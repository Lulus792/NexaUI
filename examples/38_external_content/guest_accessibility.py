"""Read-only accessibility transport for the private QEMU desktop fixture."""

from pathlib import Path
import socket
import struct
import subprocess
import time


AX_LIMIT = 4 + 32 * (22 + 256)
REQUEST = struct.Struct('>Q')
REPLY = struct.Struct('>QI')


class GuestAccessibility:
    def __init__(self, root):
        self.address = root / 'ax.sock'
        self.peer = None
        self.ticket = 0

    def connect(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
            peer.settimeout(2)
            peer.connect(str(self.address))
            self.peer = peer.dup()

    def close(self):
        if self.peer:
            self.peer.close()
            self.peer = None

    def receive(self, size, deadline):
        packet = bytearray()
        while len(packet) < size:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError('Guest accessibility reply')
            self.peer.settimeout(remaining)
            chunk = self.peer.recv(size - len(packet))
            if not chunk:
                raise EOFError('Guest accessibility channel closed')
            packet.extend(chunk)
        return bytes(packet)

    def read(self):
        if not self.peer:
            return b''
        self.ticket += 1
        deadline = time.monotonic() + 1.4
        try:
            self.peer.settimeout(1.4)
            self.peer.sendall(REQUEST.pack(self.ticket))
            ticket, size = REPLY.unpack(self.receive(REPLY.size, deadline))
            if ticket != self.ticket or size > AX_LIMIT:
                raise ValueError('Unmatched guest accessibility frame')
            return self.receive(size, deadline)
        except (OSError, ValueError, EOFError) as error:
            # A partial response cannot be the start of the next revision.
            print(f'GUEST_AX channel=closed reason={type(error).__name__}', flush=True)
            self.close()
            return b''


def read_ticket(port):
    request = bytearray()
    while len(request) < REQUEST.size:
        block = port.read(REQUEST.size - len(request))
        if not block:
            return 0
        request.extend(block)
    return REQUEST.unpack(request)[0]


def serve_guest():
    import tty

    trigger = Path('/run/nexaui-guest-ax-open')
    shown = Path('/run/nexaui-guest-ax-shown')
    app = None
    epoch = 0
    last_ticket = 0
    with open('/dev/ttyS2', 'r+b', buffering=0) as port:
        tty.setraw(port.fileno())
        print('NEXAUI_GUEST_AX_READY', flush=True)
        try:
            while (ticket := read_ticket(port)) > last_ticket:
                last_ticket = ticket
                if app and app.poll() is not None:
                    if app.returncode:
                        print(f'NEXAUI_GUEST_AX_APP_FAILED {app.returncode}', flush=True)
                    app = None
                if trigger.exists():
                    trigger.unlink()
                    if not app and epoch < 65535:
                        epoch += 1
                        shown.unlink(missing_ok=True)
                        app = subprocess.Popen(['/usr/bin/python3', '/guest_ax_fixture.py'], stdin=subprocess.DEVNULL)
                payload = b''
                if app and shown.exists():
                    try:
                        snapshot = subprocess.run(['/content_guest_ax_reader', str(app.pid), str(epoch), '800', '600'],
                                                  stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, timeout=1.1)
                        if snapshot.returncode == 0 and app.poll() is None and 0 < len(snapshot.stdout) <= AX_LIMIT:
                            payload = snapshot.stdout
                    except subprocess.TimeoutExpired:
                        pass
                pending = memoryview(REPLY.pack(ticket, len(payload)) + payload)
                while pending:
                    sent = port.write(pending)
                    if not sent:
                        return
                    pending = pending[sent:]
        finally:
            if app and app.poll() is None:
                app.terminate()
                try:
                    app.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    app.kill()
                    app.wait()


if __name__ == '__main__':
    serve_guest()
