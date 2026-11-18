"""Private QEMU guest owner for the native console workflow."""

import json
from pathlib import Path
import re
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
    def __init__(self, engine, firmware, boot, root, initrd=None, disk=None, writable=False):
        self.root = root
        self.desktop = initrd is not None
        self.disk = disk is not None
        self.trace = root / 'guest.trace'
        self.address = root / 'qmp.sock'
        self.held = set()
        self.buttons = set()
        self.trace_offset = 0
        self.trace_cursor = b''
        self.serial = 0
        self.peer = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.stream = None
        if self.desktop:
            cmdline = 'console=ttyS0 quiet rootfstype=ramfs'
            if disk:
                cmdline += ' nexaui.work=' + ('rw' if writable else 'ro')
            boot_options = [
                '-m', '768', '-device', 'virtio-vga,id=guest-gpu,xres=800,yres=600',
                '-kernel', boot, '-initrd', initrd, '-append', cmdline,
                '-serial', f'file:{self.trace}']
            if disk:
                node = {'driver':'raw','node-name':'work','read-only':not writable,
                        'file':{'driver':'file','filename':str(disk)}}
                boot_options += ['-blockdev',json.dumps(node),'-device','virtio-blk-pci,drive=work,id=work-disk']
        else:
            boot_options = [
                '-m', '32', '-device', 'VGA', '-serial', 'none',
                '-drive', f'file={boot},if=floppy,format=raw,readonly=on', '-boot', 'order=a',
                '-debugcon', f'file:{self.trace}']
        try:
            with (root / 'qemu.stderr').open('wb') as log:
                self.process = subprocess.Popen(
                    [engine, '-L', firmware, '-machine', 'pc', '-accel', 'tcg',
                     '-no-user-config', '-nodefaults', '-no-reboot',
                     '-nic', 'none', '-display', 'none', '-monitor', 'none',
                     '-parallel', 'none', *boot_options,
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
        if self.desktop:
            deadline = time.monotonic() + 28
            while True:
                if self.trace.exists():
                    with self.trace.open('rb') as stream:
                        data=stream.read(65537)
                    if b'NEXAUI_DESKTOP_READY\r\n' in data or b'NEXAUI_DESKTOP_READY\n' in data:
                        break
                    if len(data) > 65536:
                        raise RuntimeError('Guest boot output exceeded its limit')
                self.progress(deadline)
            # A Wayland client can start before the DRM page flip reaches QEMU.
            # Wait for the visible terminal decoration on the designated head.
            header = b'P6\n800 600\n255\n'
            sample = len(header) + (42*800+200)*3
            while True:
                frame = self.capture('ppm')
                if (frame.startswith(header) and len(frame)==len(header)+800*600*3 and
                        min(frame[sample:sample+3]) >= 240):
                    break
                self.progress(deadline, .08)
        else:
            self.wait_trace(b'R')
        return self.command('query-status')['status']

    def progress(self, deadline, interval=.01):
        if self.process.poll() is not None:
            raise RuntimeError(f'QEMU exited with status {self.process.returncode}')
        if time.monotonic() >= deadline:
            raise TimeoutError('QEMU guest response')
        time.sleep(interval)

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
            if not self.desktop:
                self.wait_trace(token)
        self.held, self.buttons = keys, buttons
        print(f'GUEST_INPUT count={len(payload)//INPUT_RECORD_SIZE} held={len(keys)} '
              f'buttons={len(buttons)} trace={self.trace_cursor.decode()}', flush=True)

    def capture(self, format):
        path = self.root / f'screen.{format}'
        path.unlink(missing_ok=True)
        display = {'device':'guest-gpu','head':0} if self.desktop else {}
        self.command('screendump', filename=str(path), format=format, **display)
        with path.open('rb') as stream:
            data = stream.read(PNG_LIMIT + 1)
        if len(data) > PNG_LIMIT:
            raise ValueError('Invalid guest screendump')
        return data

    def frame(self):
        data = self.capture('png')
        if not data.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError('Invalid guest screendump')
        return data

    def resources(self):
        state = {'running':1,'paused':2}[self.command('query-status')['status']]
        cpus = len(self.command('query-cpus-fast'))
        memory = self.command('query-memory-size-summary')
        ram = memory['base-memory'] + memory.get('plugged-memory',0)
        flags = capacity = read = written = 0
        if self.disk:
            device, = [entry for entry in self.command('query-block')
                        if entry.get('inserted',{}).get('node-name')=='work']
            block = device['inserted']
            stats, = [entry['stats'] for entry in self.command('query-blockstats')
                      if entry.get('qdev')==device['qdev']]
            flags = 1 if block['ro'] else 3
            capacity = block['image']['virtual-size']
            read, written = stats['rd_bytes'], stats['wr_bytes']
        print(f'GUEST_RESOURCES state={state} flags={flags} cpus={cpus} ram={ram} '
              f'capacity={capacity} read={read} written={written}',flush=True)
        return struct.pack('>4sBBHQQQQ',b'NXR1',state,flags,cpus,ram,capacity,read,written)

    def close(self):
        try:
            if self.process.poll() is None and self.stream:
                try:
                    status = self.command('query-status')['status']
                    if self.desktop and self.trace.exists():
                        with self.trace.open('rb') as boot_log:
                            output = boot_log.read(65536)
                        typed = output.count(b'NEXAUI_DESKTOP_INPUT')
                        motion = len(re.findall(rb'\(EV_REL\).*\(REL_[XY]\), value -?\d+', output))
                        buttons = re.findall(rb'\(BTN_LEFT\), value ([01])', output)
                        print(f'GUEST_DESKTOP_INPUT terminal={typed} motion={motion} '
                              f'left={b"".join(buttons).decode()}',flush=True)
                        for storage in re.findall(rb'^NEXAUI_STORAGE (\w+)\r?$',output,re.M):
                            print(f'GUEST_STORAGE {storage.decode()}',flush=True)
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


def serve(engine, firmware, boot, address, initrd=None, disk=None):
    def interrupt(*_):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupt)
    with tempfile.TemporaryDirectory(prefix='qemu-guest-', dir=address.parent) as temp:
        guest = Guest(engine, firmware, boot, Path(temp), initrd, disk, writable=bool(disk))
        try:
            status = guest.wait_ready()
            if status != 'running':
                raise RuntimeError(f'Unexpected guest state {status}')
            version = '.'.join(str(guest.version[k]) for k in ('major','minor','micro'))
            identity = 'desktop nic=none ram=768MiB' if guest.desktop else 'trace=R nic=none boot=readonly ram=32MiB'
            print(f'GUEST_READY qemu={version} status={status} {identity}', flush=True)
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
                                    kind in range(8 if guest.desktop else 6) and len(fields)==(6 if kind==5 else 5)):
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
                        if kind == 7:
                            peer.sendall(guest.resources())
                            continue
                        image = guest.frame()
                        peer.sendall(image)
                        print(f'GUEST_FRAME kind={kind} generation={generation} resize={resize} bytes={len(image)}',flush=True)
        except KeyboardInterrupt:
            pass
        except Exception:
            if guest.desktop and guest.trace.exists():
                with guest.trace.open('rb') as boot_log:
                    print(boot_log.read(65536).decode(errors='replace'),file=sys.stderr)
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
    serve(*sys.argv[1:4], Path(sys.argv[4]), *sys.argv[5:])
