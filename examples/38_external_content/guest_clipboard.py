"""Bounded host-to-guest text transport for the private desktop fixture."""

import base64
import hashlib
import socket
import time


CLIPBOARD_LIMIT = 192
CLIPBOARD_ROWS = 4
CLIPBOARD_COLUMNS = 32


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

    def connect(self):
        self.peer = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.peer.settimeout(2)
        self.peer.connect(str(self.address))

    def close(self):
        if self.peer:
            self.peer.close()
            self.peer = None

    def send(self, ticket, text):
        if not self.peer or ticket <= self.last_ticket or not allowed_clipboard_text(text):
            return b'R'
        self.last_ticket = ticket
        token = str(ticket).encode('ascii')
        digest = hashlib.sha256(text).hexdigest().encode('ascii')
        request = b' '.join((token,digest,base64.b64encode(text))) + b'\n'
        deadline = time.monotonic()+4
        reply = b''
        try:
            self.peer.settimeout(4)
            self.peer.sendall(request)
            while len(reply)<32 and not reply.endswith(b'\n'):
                remaining = deadline-time.monotonic()
                if remaining<=0:
                    break
                self.peer.settimeout(remaining)
                chunk = self.peer.recv(32-len(reply))
                if not chunk:
                    break
                reply += chunk
        except OSError:
            pass
        if reply == b'A ' + token + b'\n':
            return b'A'
        if reply == b'R ' + token + b'\n':
            return b'R'
        if reply == b'U ' + token + b'\n':
            return b'U'
        # Late or malformed acknowledgements cannot be correlated on this stream.
        self.close()
        return b'U'
