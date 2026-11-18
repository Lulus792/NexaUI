"""Read-only accessibility transport for the private QEMU desktop fixture."""

from pathlib import Path
import socket
import struct
import subprocess
import time


AX_LIMIT = 4 + 32 * (22 + 256)
AX_PREPARE_TIMEOUT = 45
AX_REPLY_TIMEOUT = 1.4
REQUEST = struct.Struct('>Q')
REPLY = struct.Struct('>QI')


class GuestAccessibility:
    def __init__(self, root):
        self.address = root / 'ax.sock'
        self.peer = None
        self.ticket = 0
        self.awaiting_reply = False
        self.pending = bytearray()

    def connect(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
            peer.settimeout(2)
            peer.connect(str(self.address))
            self.peer = peer.dup()

    def close(self):
        self.pending.clear()
        self.awaiting_reply = False
        if self.peer:
            self.peer.close()
            self.peer = None

    def receive(self, size, deadline):
        while len(self.pending) < size:
            timeout = 0 if deadline is None else deadline - time.monotonic()
            if timeout <= 0 and deadline is not None:
                raise TimeoutError('Guest accessibility reply')
            self.peer.settimeout(timeout)
            chunk = self.peer.recv(size - len(self.pending))
            if not chunk:
                raise EOFError('Guest accessibility channel closed')
            self.pending.extend(chunk)

    def receive_reply(self, deadline):
        self.receive(REPLY.size, deadline)
        ticket, size = REPLY.unpack_from(self.pending)
        if ticket != self.ticket or size > AX_LIMIT:
            raise ValueError('Unmatched guest accessibility frame')
        self.receive(REPLY.size + size, deadline)
        payload = bytes(self.pending[REPLY.size:])
        self.pending.clear()
        self.awaiting_reply = False
        return payload

    def read(self):
        if not self.peer:
            return b''
        try:
            if self.awaiting_reply:
                # Drain the old reply without delaying video or accepting stale semantics.
                try:
                    self.receive_reply(None)
                except BlockingIOError:
                    return b''
            self.ticket += 1
            deadline = time.monotonic() + AX_REPLY_TIMEOUT
            self.peer.settimeout(AX_REPLY_TIMEOUT)
            self.peer.sendall(REQUEST.pack(self.ticket))
            self.awaiting_reply = True
            try:
                return self.receive_reply(deadline)
            except TimeoutError:
                print('GUEST_AX snapshot=timed-out', flush=True)
                return b''
        except (OSError, ValueError, EOFError) as error:
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
    prepared = Path('/run/nexaui-guest-ax-prepared')
    shown = Path('/run/nexaui-guest-ax-shown')

    def launch():
        prepared.unlink(missing_ok=True)
        shown.unlink(missing_ok=True)
        return subprocess.Popen(['/usr/bin/python3', '/guest_ax_fixture.py'], stdin=subprocess.PIPE)

    epoch = 1
    last_ticket = 0
    with open('/dev/ttyS2', 'r+b', buffering=0) as port:
        tty.setraw(port.fileno())
        app = launch()
        try:
            deadline = time.monotonic() + AX_PREPARE_TIMEOUT
            while not prepared.exists():
                if app.poll() is not None or time.monotonic() >= deadline:
                    print('NEXAUI_STARTUP_FAILED guest accessibility application', flush=True)
                    return
                time.sleep(.05)
            print('NEXAUI_GUEST_AX_READY', flush=True)
            while (ticket := read_ticket(port)) > last_ticket:
                last_ticket = ticket
                if app and app.poll() is not None:
                    app.stdin.close()
                    shown.unlink(missing_ok=True)
                    if app.returncode:
                        print(f'NEXAUI_GUEST_AX_APP_FAILED {app.returncode}', flush=True)
                        app = None
                    elif epoch < 65535:
                        epoch += 1
                        app = launch()
                    else:
                        app = None
                if trigger.exists():
                    trigger.unlink()
                    if not app and epoch < 65535:
                        epoch += 1
                        app = launch()
                    if app and not app.stdin.closed:
                        try:
                            with app.stdin:
                                app.stdin.write(b'1')
                        except BrokenPipeError:
                            pass
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
            if app:
                app.stdin.close()
                if app.poll() is None:
                    app.terminate()
                    try:
                        app.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        app.kill()
                        app.wait()


if __name__ == '__main__':
    serve_guest()
