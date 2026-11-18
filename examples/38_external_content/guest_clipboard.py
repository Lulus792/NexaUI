"""Bounded clipboard text transport for the private desktop fixture."""

import base64
import hashlib
import socket
import time


CLIPBOARD_LIMIT = 192
CLIPBOARD_ROWS = 4
CLIPBOARD_COLUMNS = 32
CLIPBOARD_SEND = 1
CLIPBOARD_READ = 2


def allowed_clipboard_text(payload):
    if not 0 < len(payload) <= CLIPBOARD_LIMIT:
        return False
    try:
        text = payload.decode()
    except UnicodeError:
        return False
    rows = text.split('\n')
    if len(rows) > CLIPBOARD_ROWS or any(len(row) > CLIPBOARD_COLUMNS for row in rows):
        return False
    for ch in text:
        scalar = ord(ch)
        if ch != '\n' and (scalar < 32 or 0x7f <= scalar <= 0x9f or 0x200b <= scalar <= 0x200f or
                           0x2028 <= scalar <= 0x202e or 0x2060 <= scalar <= 0x206f or scalar == 0xfeff):
            return False
    return True


class GuestClipboard:
    def __init__(self, root):
        self.address = root / 'clip.sock'
        self.peer = None
        self.last_ticket = 0
        self.can_read = False

    def capabilities(self):
        if not self.peer:
            return 0
        return CLIPBOARD_SEND | (CLIPBOARD_READ if self.can_read else 0)

    def connect(self):
        self.peer = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.peer.settimeout(2)
        self.peer.connect(str(self.address))

    def close(self):
        if self.peer:
            self.peer.close()
            self.peer = None

    def exchange(self, ticket, request, limit):
        self.last_ticket = ticket
        deadline = time.monotonic()+4
        reply = b''
        try:
            self.peer.settimeout(4)
            self.peer.sendall(request)
            while len(reply)<limit and b'\n' not in reply:
                remaining = deadline-time.monotonic()
                if remaining<=0:
                    break
                self.peer.settimeout(remaining)
                chunk = self.peer.recv(limit-len(reply))
                if not chunk:
                    break
                reply += chunk
        except OSError:
            pass
        return reply

    def send(self, ticket, text):
        if not self.peer or ticket <= self.last_ticket or not allowed_clipboard_text(text):
            return b'R'
        token = str(ticket).encode('ascii')
        digest = hashlib.sha256(text).hexdigest().encode('ascii')
        request = b' '.join((token,digest,base64.b64encode(text))) + b'\n'
        reply = self.exchange(ticket,request,32)
        if reply == b'A ' + token + b'\n':
            return b'A'
        if reply == b'R ' + token + b'\n':
            return b'R'
        if reply == b'U ' + token + b'\n':
            return b'U'
        # Late or malformed acknowledgements cannot be correlated on this stream.
        self.close()
        return b'U'

    def read(self, ticket):
        if not self.peer or not self.can_read or ticket <= self.last_ticket:
            return b'R',b''
        token = str(ticket).encode('ascii')
        reply = self.exchange(ticket,b'Q ' + token + b'\n',352)
        if reply == b'R ' + token + b'\n':
            return b'R',b''
        fields = reply[:-1].split(b' ')
        if reply.endswith(b'\n') and len(fields)==4 and fields[:2]==[b'D',token]:
            try:
                text = base64.b64decode(fields[3],validate=True)
            except ValueError:
                pass
            else:
                if hashlib.sha256(text).hexdigest().encode('ascii')==fields[2]:
                    if allowed_clipboard_text(text):
                        return b'A',text
                    return b'R',b''
        # A truncated, corrupt or foreign data frame cannot supply a review.
        self.close()
        return b'U',b''
