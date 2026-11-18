"""Private QEMU guest owner for the native console workflow."""

import json
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

PNG_LIMIT = 4 * 1024 * 1024
INPUT_LIMIT = 128
INPUT_RECORD_SIZE = 5
INPUT_KEY, INPUT_BUTTON, INPUT_MOTION = 1, 2, 3
QMP_LIMIT = 16384
KEY_CODES = {k:chr(k) for k in b'abcdefghijklmnopqrstuvwxyz0123456789'}
KEY_CODES.update({1:'ctrl', 2:'ctrl_r', 3:'shift', 4:'shift_r', 5:'alt', 6:'alt_r',
                  7:'ret', 8:'tab', 9:'spc', 10:'esc', 11:'backspace', 12:'left',
                  13:'right', 14:'up', 15:'down', 16:'delete', 17:'insert', 18:'home',
                  19:'end', 20:'pgup', 21:'pgdn', 22:'meta_l'})
KEY_CODES.update({63+n:f'f{n}' for n in range(1,13)})
BUTTONS = {1:('left',b'L'), 2:('right',b'M'), 3:('middle',b'N')}


class Guest:
    def __init__(self, engine, firmware, boot, root):
        self.root = root
        self.trace = root / 'guest.trace'
        self.address = root / 'qmp.sock'
        self.held = set()
        self.buttons = set()
        self.trace_offset = 0
        self.trace_cursor = b''
        self.serial = 0
        self.peer = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.stream = None
        try:
            with (root / 'qemu.stderr').open('wb') as log:
                self.process = subprocess.Popen(
                    [engine, '-L', firmware, '-machine', 'pc', '-accel', 'tcg',
                     '-m', '32', '-no-user-config', '-nodefaults', '-no-reboot',
                     '-nic', 'none', '-display', 'none', '-monitor', 'none',
                     '-serial', 'none', '-parallel', 'none', '-device', 'VGA',
                     '-drive', f'file={boot},if=floppy,format=raw,readonly=on', '-boot', 'order=a',
                     '-debugcon', f'file:{self.trace}',
                     '-qmp', f'unix:{self.address},server=on,wait=off'],
                    stdin=subprocess.DEVNULL, stdout=log, stderr=log)
        except BaseException:
            self.peer.close()
            raise

    def wait_ready(self):
        deadline = time.monotonic() + 8
        while not self.address.exists():
            self.progress(deadline)
        self.peer.settimeout(4)
        self.peer.connect(str(self.address))
        self.stream = self.peer.makefile('rb')
        self.version = self.message(deadline)['QMP']['version']['qemu']
        self.command('qmp_capabilities')
        self.wait_trace(b'R')
        return self.command('query-status')['status']

    def progress(self, deadline):
        if self.process.poll() is not None:
            raise RuntimeError(f'QEMU exited with status {self.process.returncode}')
        if time.monotonic() >= deadline:
            raise TimeoutError('QEMU guest response')
        time.sleep(.01)

    def message(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('QMP response')
        self.peer.settimeout(remaining)
        line = self.stream.readline(QMP_LIMIT + 1)
        if not line:
            raise EOFError('QMP closed')
        if len(line) > QMP_LIMIT or not line.endswith(b'\n'):
            raise ValueError('Invalid QMP response framing')
        return json.loads(line)

    def command(self, method, **arguments):
        self.serial += 1
        packet = {'execute': method, 'id': self.serial}
        if arguments:
            packet['arguments'] = arguments
        deadline = time.monotonic() + 3
        self.peer.settimeout(3)
        self.peer.sendall(json.dumps(packet, separators=(',', ':')).encode() + b'\n')
        while True:
            reply = self.message(deadline)
            if reply.get('id') == self.serial:
                if 'error' in reply:
                    raise RuntimeError(f'{method}: {reply["error"]["desc"]}')
                return reply['return']

    def wait_trace(self, expected):
        if not expected:
            return
        deadline = time.monotonic() + 2
        while True:
            if self.trace.exists():
                with self.trace.open('rb') as stream:
                    stream.seek(self.trace_offset)
                    data = stream.read(len(expected) + 1)
                if data == expected:
                    self.trace_offset += len(expected)
                    self.trace_cursor = (self.trace_cursor + data)[-256:]
                    return
                if not expected.startswith(data):
                    raise RuntimeError('Unexpected boot or input transition in guest')
            self.progress(deadline)

    def input(self, payload):
        if not payload or len(payload)%INPUT_RECORD_SIZE or len(payload) > INPUT_LIMIT*INPUT_RECORD_SIZE:
            raise ValueError('Invalid guest input batch')
        keys, buttons = self.held.copy(), self.buttons.copy()
        commands = []
        for kind, code, down, lo, hi in struct.iter_unpack('5B', payload):
            if kind == INPUT_MOTION:
                dx, dy = struct.unpack('<hh', bytes((code, down, lo, hi)))
                if not (dx or dy) or max(abs(dx), abs(dy)) >= 2048:
                    raise ValueError('Invalid guest pointer motion')
                # The PS/2 guest receives one packet per impulse.
                while dx or dy:
                    steps = [max(-127,min(127,value)) for value in (dx,dy)]
                    events = [{'type':'rel','data':{'axis':axis,'value':step}}
                              for axis, step in zip('xy',steps) if step]
                    commands.append((events,b'P'))
                    dx, dy = dx-steps[0], dy-steps[1]
                continue
            if kind not in (INPUT_KEY, INPUT_BUTTON) or lo or hi:
                raise ValueError('Invalid guest input record')
            allowed, held = (KEY_CODES,keys) if kind == INPUT_KEY else (BUTTONS,buttons)
            if code not in allowed or down not in (0,1) or (code in held)==bool(down):
                raise ValueError('Invalid guest button or key transition')
            if down:
                held.add(code)
            else:
                held.remove(code)
            if kind == INPUT_KEY:
                token = (b'D' if down else b'U') if code==ord('a') else b''
                event = {'type':'key','data':{'key':{'type':'qcode','data':allowed[code]},'down':bool(down)}}
            else:
                button, token = allowed[code]
                token = token if down else token.lower()
                event = {'type':'btn','data':{'button':button,'down':bool(down)}}
            commands.append(([event],token))
        deadline = time.monotonic() + 3.5
        for events, token in commands:
            if time.monotonic() >= deadline:
                raise TimeoutError('Guest input batch response')
            self.command('input-send-event', events=events)
            self.wait_trace(token)
        self.held, self.buttons = keys, buttons
        print(f'GUEST_INPUT count={len(payload)//INPUT_RECORD_SIZE} held={len(keys)} '
              f'buttons={len(buttons)} trace={self.trace_cursor.decode()}', flush=True)

    def frame(self):
        path = self.root / 'screen.png'
        path.unlink(missing_ok=True)
        self.command('screendump', filename=str(path), format='png')
        with path.open('rb') as stream:
            data = stream.read(PNG_LIMIT + 1)
        if not data.startswith(b'\x89PNG\r\n\x1a\n') or len(data) > PNG_LIMIT:
            raise ValueError('Invalid guest screendump')
        return data

    def close(self):
        try:
            if self.process.poll() is None and self.stream:
                try:
                    status = self.command('query-status')['status']
                    print(f'GUEST_CLOSE status={status} held={len(self.held)} buttons={len(self.buttons)} '
                          f'trace={self.trace_cursor.decode()}',flush=True)
                    self.command('quit')
                except EOFError:
                    pass  # QEMU may close QMP immediately after quit.
                except (OSError, RuntimeError):
                    self.process.terminate()
            if self.process.poll() is None and not self.stream:
                self.process.terminate()
            try:
                return self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                return self.process.wait(timeout=2)
        finally:
            if self.stream:
                self.stream.close()
            self.peer.close()


def serve(engine, firmware, boot, address):
    def interrupt(*_):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupt)
    with tempfile.TemporaryDirectory(prefix='qemu-guest-', dir=address.parent) as temp:
        guest = Guest(engine, firmware, boot, Path(temp))
        try:
            status = guest.wait_ready()
            if status != 'running':
                raise RuntimeError(f'Unexpected guest state {status}')
            version = '.'.join(str(guest.version[k]) for k in ('major','minor','micro'))
            print(f'GUEST_READY qemu={version} status={status} trace=R nic=none boot=readonly ram=32MiB', flush=True)
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(str(address))
                listener.listen(1)
                while True:
                    peer, _ = listener.accept()
                    with peer:
                        peer.settimeout(1)
                        with peer.makefile('rb') as stream:
                            line = stream.readline(96)
                            if not line.endswith(b'\n'):
                                raise ValueError('Invalid guest frame request')
                            fields = tuple(map(int, line.split()))
                            generation,resize,width,height,kind = fields[:5]
                            if not (0 < generation < 2**64 and 0 < resize < 2**64 and
                                    0 < width <= 2048 and 0 < height <= 2048 and width*height*4 <= PNG_LIMIT and
                                    kind in range(6) and len(fields)==(6 if kind==5 else 5)):
                                raise ValueError('Guest request outside the fixture limits')
                            size = fields[5] if kind == 5 else 0
                            if not 0 <= size <= INPUT_LIMIT*INPUT_RECORD_SIZE:
                                raise ValueError('Guest input batch too large')
                            payload = stream.read(size)
                            if len(payload)!=size or stream.read(1):
                                raise ValueError('Invalid guest request framing')
                        if kind == 2:
                            guest.command('stop')
                            if guest.command('query-status')['status'] != 'paused':
                                raise RuntimeError('Guest did not pause')
                            print('GUEST_STATUS paused',flush=True)
                            peer.sendall(b'P')
                            continue
                        if kind == 3:
                            guest.command('cont')
                            if guest.command('query-status')['status'] != 'running':
                                raise RuntimeError('Guest did not resume')
                            print('GUEST_STATUS running',flush=True)
                        if kind == 5:
                            guest.input(payload)
                        image = guest.frame()
                        peer.sendall(image)
                        print(f'GUEST_FRAME kind={kind} generation={generation} resize={resize} bytes={len(image)}',flush=True)
        except KeyboardInterrupt:
            pass
        except Exception:
            with (guest.root/'qemu.stderr').open('rb') as log:
                length = log.seek(0, 2)
                log.seek(max(0, length - 4000))
                print(log.read().decode(errors='replace'),file=sys.stderr)
            raise
        finally:
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
            result = guest.close()
            print(f'GUEST_EXIT result={result}',flush=True)
            if result:
                raise RuntimeError('QEMU failed to exit cleanly')


if __name__ == '__main__':
    serve(*sys.argv[1:4], Path(sys.argv[4]))
