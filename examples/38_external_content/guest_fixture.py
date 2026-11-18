"""Private QEMU guest owner for the native console workflow."""

from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import time

from guest_clipboard import CLIPBOARD_LIMIT, GuestClipboard
from guest_export import GuestExport, private_export_identity
from guest_network import GuestNetwork
from guest_reset import (RESET_APPLIED, RESET_REJECTED, RESET_SCOPE_BYTES, RESET_UNKNOWN, RESET_UNREQUESTED,
                         WORK_DISK_BYTES, image_digest, image_identity, image_snapshot, observe_reset,
                         record_reset, sync_directory)

PNG_LIMIT = 4 * 1024 * 1024
INPUT_LIMIT = 128
INPUT_RECORD_SIZE = 5
INPUT_KEY, INPUT_BUTTON, INPUT_MOTION = 1, 2, 3
QMP_LIMIT = 16384
DESKTOP_BOOT_TIMEOUT = 120
RESET_RESTART_EXIT = 86
SHARE_PATH_LIMIT = 96
FILE_EXPORT_ID = 'nexaui-export'
FILE_EXPORT_QOM = f'/machine/peripheral/{FILE_EXPORT_ID}'
SHARE_ACTIVE, SHARE_PENDING, SHARE_REVOKED, SHARE_UNKNOWN, SHARE_OFFERED, SHARE_ATTACHING = range(1,7)
KEY_CODES = {k:chr(k) for k in b'abcdefghijklmnopqrstuvwxyz0123456789'}
KEY_CODES.update({1:'ctrl', 2:'ctrl_r', 3:'shift', 4:'shift_r', 5:'alt', 6:'alt_r',
                  7:'ret', 8:'tab', 9:'spc', 10:'esc', 11:'backspace', 12:'left',
                  13:'right', 14:'up', 15:'down', 16:'delete', 17:'insert', 18:'home',
                  19:'end', 20:'pgup', 21:'pgdn', 22:'meta_l'})
KEY_CODES.update({63+n:f'f{n}' for n in range(1,13)})
BUTTONS = {1:('left',b'L'), 2:('right',b'M'), 3:('middle',b'N')}

class Guest:
    def __init__(self, engine, firmware, boot, root, initrd=None, disk=None, writable=False, checkpoint=None,
                 reset_enabled=False, file_share=None, revoke_share=False, grant_share=False, export=None,
                 clipboard=False, network=False):
        self.root = root
        self.desktop = initrd is not None
        self.disk = Path(disk) if disk else None
        self.checkpoint = Path(checkpoint) if checkpoint else None
        self.reset_enabled = reset_enabled
        self.reset_state = RESET_UNREQUESTED
        self.close_result = None
        self.file_share = file_share
        self.export = export
        if clipboard and not self.desktop:
            raise ValueError('Clipboard requires the desktop guest')
        if network and not self.desktop:
            raise ValueError('Network requires the desktop guest')
        self.clipboard = GuestClipboard(root) if clipboard else None
        self.network = GuestNetwork(root) if network else None
        if self.clipboard or self.network or self.checkpoint:
            self.environment = os.urandom(16)
        if self.export and (not grant_share or self.export.source!=file_share):
            raise ValueError('The file backend requires the private runtime grant')
        self.grant_share = grant_share
        self.revoke_share = revoke_share or grant_share
        self.share_state = SHARE_OFFERED if grant_share else SHARE_ACTIVE
        self.share_requested = False
        self.share_granted = not grant_share
        self.share_deleted = False
        if self.file_share:
            if not (reset_enabled and self.disk and self.file_share==self.disk.parent/'host-export'):
                raise ValueError('File sharing requires the private reset fixture export')
            self.share_path = os.fsencode(self.file_share)
            if len(self.share_path)>SHARE_PATH_LIMIT or not re.fullmatch(rb'/[a-zA-Z0-9_./-]+',self.share_path):
                raise ValueError('Invalid private export path')
            self.share_identity = self.export_identity()
            if self.export and self.export.source_identity!=self.share_identity:
                raise ValueError('The private export directory changed during setup')
        if self.checkpoint:
            if not (self.desktop and self.disk and writable):
                raise ValueError('Reset preview requires an owned writable disk')
            self.reset_binding = os.urandom(16) if grant_share else self.environment
            self.disk_identity = image_identity(self.disk.stat(follow_symlinks=False))
            self.checkpoint_identity = image_identity(self.checkpoint.stat(follow_symlinks=False))
            self.checkpoint_hash = image_digest(self.checkpoint)
            if self.checkpoint_identity==self.disk_identity or self.checkpoint_hash!=image_digest(self.disk):
                raise ValueError('Work disk and baseline are inconsistent')
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
            if self.clipboard:
                cmdline += ' nexaui.clipboard=on'
            if self.network:
                cmdline += ' nexaui.network=probe'
            if self.file_share:
                cmdline += ' nexaui.export=' + ('grantable' if grant_share else 'revocable' if revoke_share else 'write')
            boot_options = [
                '-m', '768', '-device', 'virtio-vga,id=guest-gpu,xres=800,yres=600',
                '-kernel', boot, '-initrd', initrd, '-append', cmdline,
                '-serial', f'file:{self.trace}']
            if self.clipboard:
                boot_options += ['-chardev',f'socket,id=clipboard,path={self.clipboard.address},server=on,wait=off',
                                 '-serial','chardev:clipboard']
            if disk:
                node = {'driver':'raw','node-name':'work','read-only':not writable,
                        'file':{'driver':'file','filename':str(disk)}}
                boot_options += ['-blockdev',json.dumps(node),'-device','virtio-blk-pci,drive=work,id=work-disk']
            if self.file_share:
                export_path = self.export.mount if self.export else self.file_share
                boot_options += [
                    '-fsdev',f'local,id={FILE_EXPORT_ID},path={export_path},security_model=mapped-xattr,writeout=immediate']
                if not grant_share:
                    device = f'virtio-9p-pci,id={FILE_EXPORT_ID},fsdev={FILE_EXPORT_ID},mount_tag={FILE_EXPORT_ID}'
                    if revoke_share:
                        device = json.dumps(dict(driver='virtio-9p-pci',id=FILE_EXPORT_ID,fsdev=FILE_EXPORT_ID,
                                                 mount_tag=FILE_EXPORT_ID))
                    boot_options += ['-device',device]
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
        if self.clipboard:
            self.clipboard.connect()
        if self.desktop:
            deadline = time.monotonic() + DESKTOP_BOOT_TIMEOUT
            while True:
                if self.trace.exists():
                    with self.trace.open('rb') as stream:
                        data=stream.read(65537)
                    lines = data.splitlines()
                    if (b'NEXAUI_DESKTOP_READY' in lines and
                            (not self.clipboard or b'NEXAUI_CLIPBOARD_READY' in lines)):
                        if self.clipboard:
                            self.clipboard.can_read = b'NEXAUI_CLIPBOARD_READ_READY' in lines
                        break
                    if b'NEXAUI_STARTUP_FAILED ' in data or b'Kernel panic - not syncing' in data:
                        raise RuntimeError('Guest startup failed before desktop readiness')
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
            if reply.get('event')=='DEVICE_DELETED':
                data = reply.get('data',{})
                if data.get('device')==FILE_EXPORT_ID and data.get('path')==FILE_EXPORT_QOM:
                    self.share_deleted = True
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

    def work_device(self):
        device, = [entry for entry in self.command('query-block')
                   if entry.get('inserted',{}).get('node-name')=='work']
        return device

    def resources(self):
        state = {'running':1,'paused':2}[self.command('query-status')['status']]
        cpus = len(self.command('query-cpus-fast'))
        memory = self.command('query-memory-size-summary')
        ram = memory['base-memory'] + memory.get('plugged-memory',0)
        flags = capacity = read = written = 0
        if self.disk:
            device = self.work_device()
            block = device['inserted']
            stats, = [entry['stats'] for entry in self.command('query-blockstats')
                      if entry.get('qdev')==device['qdev']]
            flags = 1 if block['ro'] else 3
            capacity = block['image']['virtual-size']
            read, written = stats['rd_bytes'], stats['wr_bytes']
        print(f'GUEST_RESOURCES state={state} flags={flags} cpus={cpus} ram={ram} '
              f'capacity={capacity} read={read} written={written}',flush=True)
        return struct.pack('>4sBBHQQQQ',b'NXR1',state,flags,cpus,ram,capacity,read,written)

    def export_identity(self):
        return private_export_identity(self.file_share)

    def file_share_scope(self):
        if not self.file_share:
            return b''
        if self.export_identity()!=self.share_identity:
            raise ValueError('The file grant changed')
        state = self.observe_share_state()
        if state==SHARE_OFFERED:
            return b''
        if state not in (SHARE_ACTIVE,SHARE_REVOKED):
            raise ValueError('File access scope is unconfirmed')
        if state==SHARE_ACTIVE and self.command('qom-get',path=FILE_EXPORT_QOM,property='mount_tag')!=FILE_EXPORT_ID:
            raise ValueError('The file grant changed')
        return self.share_path

    def observe_share_state(self):
        if self.export:
            backend = self.export.status()
            if backend=='cut off' and self.share_requested:
                if self.share_state!=SHARE_REVOKED:
                    print('GUEST_SHARE_BACKEND_CUTOFF',flush=True)
                self.share_state = SHARE_REVOKED
                return self.share_state
            if backend!='running':
                return SHARE_UNKNOWN
        if self.share_state!=SHARE_ACTIVE:
            devices = self.command('qom-list',path='/machine/peripheral')
            present = any(item['name']==FILE_EXPORT_ID for item in devices)
            if self.grant_share and not self.share_granted:
                if present:
                    if self.share_requested and self.command('qom-get',path=FILE_EXPORT_QOM,
                            property='mount_tag')==FILE_EXPORT_ID:
                        self.share_granted = True
                        self.share_state = SHARE_ACTIVE
                        print(f'GUEST_SHARE_ATTACHED device={FILE_EXPORT_ID}',flush=True)
                    else:
                        self.share_state = SHARE_UNKNOWN
            elif self.share_deleted and not present:
                if self.share_state!=SHARE_REVOKED:
                    print(f'GUEST_SHARE_REMOVED device={FILE_EXPORT_ID}',flush=True)
                self.share_state = SHARE_REVOKED
            elif self.share_state==SHARE_REVOKED:
                self.share_state = SHARE_UNKNOWN
        return self.share_state

    def grant_files(self, environment):
        if not self.file_share or not self.grant_share or environment!=self.environment:
            return b'U'
        if self.share_requested or self.share_state!=SHARE_OFFERED:
            return self.sharing()
        try:
            if self.file_share_scope() or self.share_state!=SHARE_OFFERED:
                return b'U'
        except (OSError,ValueError,RuntimeError,EOFError,KeyError):
            return b'U'
        self.reset_binding = os.urandom(16)
        self.share_requested = True
        self.share_state = SHARE_UNKNOWN
        try:
            self.command('device_add',driver='virtio-9p-pci',id=FILE_EXPORT_ID,bus='pci.0',
                         fsdev=FILE_EXPORT_ID,mount_tag=FILE_EXPORT_ID)
            self.share_state = SHARE_ATTACHING
            print(f'GUEST_SHARE_GRANT_REQUEST device={FILE_EXPORT_ID}',flush=True)
        except (OSError,ValueError,RuntimeError,EOFError,KeyError):
            pass
        return self.sharing_reply(self.share_state)

    def sharing_reply(self, state):
        path = self.share_path if self.file_share else b''
        clipboard = self.clipboard.capabilities() if self.clipboard else 0
        if self.network:
            return struct.pack('>4sBBBx16sI',b'NXS4',state,clipboard,self.network.state,
                               self.environment,len(path))+path
        if self.clipboard:
            return struct.pack('>4sBB2x16sI',b'NXS3',state,clipboard,self.environment,len(path))+path
        return struct.pack('>4sB3x16sI',b'NXS2',state,self.environment,len(path))+path

    def sharing(self, observe_network=True):
        if self.network and observe_network:
            self.network.observe(self.command)
        try:
            if self.file_share and self.revoke_share and self.share_state!=SHARE_ACTIVE:
                if self.export_identity()!=self.share_identity:
                    return b'U'
                return self.sharing_reply(self.observe_share_state())
            path = self.file_share_scope()
        except (OSError,ValueError,RuntimeError,EOFError,KeyError):
            return b'U'
        print(f'GUEST_SHARING files={path.decode() or "unavailable"}',flush=True)
        if self.file_share and self.revoke_share:
            return self.sharing_reply(self.share_state)
        if self.clipboard or self.network:
            return self.sharing_reply(0)
        return struct.pack('>4sI',b'NXS1',len(path))+path

    def network_action(self, scope, grant):
        if not self.network or scope!=self.environment:
            return b'U'
        if grant:
            try:
                if self.command('query-status')['status']!='running':
                    return b'U'
            except (OSError,ValueError,RuntimeError,EOFError,KeyError):
                return b'U'
            self.network.grant(self.command)
        else:
            self.network.revoke(self.command)
        return self.sharing(observe_network=False)

    def clipboard_ticket(self, payload):
        if not self.clipboard or payload[:16]!=self.environment:
            return 0
        if self.held or self.buttons:
            return 0
        try:
            if self.command('query-status')['status']!='running':
                return 0
        except (OSError,ValueError,RuntimeError,EOFError,KeyError):
            return 0
        return int.from_bytes(payload[16:24],'big')

    def send_clipboard(self, payload):
        ticket = self.clipboard_ticket(payload)
        if not ticket:
            return b'R'
        outcome = self.clipboard.send(ticket,payload[24:])
        if outcome==b'A':
            print('GUEST_CLIPBOARD acknowledged',flush=True)
        return outcome

    def read_clipboard(self, payload):
        ticket = self.clipboard_ticket(payload)
        if not ticket:
            return b'R'
        outcome, text = self.clipboard.read(ticket)
        if outcome!=b'A':
            return outcome
        print('GUEST_CLIPBOARD guest_snapshot',flush=True)
        return struct.pack('>4s16sQI',b'NXC1',self.environment,ticket,len(text))+text

    def revoke_files(self, environment):
        if not self.file_share or not self.revoke_share or environment!=self.environment:
            return b'U'
        if self.share_state!=SHARE_ACTIVE:
            return self.sharing() if self.share_granted else b'U'
        try:
            self.file_share_scope()
        except (OSError,ValueError,RuntimeError,EOFError,KeyError):
            return b'U'
        if self.share_state!=SHARE_ACTIVE:
            return self.sharing_reply(self.share_state)
        self.share_state = SHARE_UNKNOWN
        self.share_deleted = False
        try:
            if self.export:
                self.export.cut_off()
            else:
                self.command('device_del',id=FILE_EXPORT_ID)
            self.share_state = SHARE_PENDING
            mechanism = 'backend' if self.export else f'device={FILE_EXPORT_ID}'
            print(f'GUEST_SHARE_REVOKE_REQUEST {mechanism}',flush=True)
        except (OSError,ValueError,RuntimeError,EOFError,KeyError):
            pass
        return self.sharing_reply(self.share_state)

    def check_reset_scope(self):
        if not self.checkpoint:
            raise ValueError('The owner has no reset baseline')
        block = self.work_device()['inserted']
        if (block['ro'] or block['drv']!='raw' or block['image']['filename']!=str(self.disk) or
                block['image']['virtual-size']!=WORK_DISK_BYTES or
                image_identity(self.disk.stat(follow_symlinks=False))!=self.disk_identity or
                image_identity(self.checkpoint.stat(follow_symlinks=False))!=self.checkpoint_identity or
                image_digest(self.checkpoint)!=self.checkpoint_hash or
                image_identity(self.checkpoint.stat(follow_symlinks=False))!=self.checkpoint_identity):
            raise ValueError('The reset baseline or work disk changed')
        return self.file_share_scope()

    def reset_preview(self):
        try:
            export = self.check_reset_scope()
        except (OSError,ValueError,KeyError,RuntimeError,EOFError):
            print('GUEST_RESET_PREVIEW unavailable',flush=True)
            return b'U'
        print(f'GUEST_RESET_PREVIEW environment={self.environment.hex()} checkpoint={self.checkpoint_hash.hex()} '
              f'capacity={WORK_DISK_BYTES} grants={int(bool(export))}',flush=True)
        if self.file_share and self.grant_share:
            return struct.pack('>4s16s32sQI16s',b'NXV3',self.environment,self.checkpoint_hash,WORK_DISK_BYTES,
                               len(export),self.reset_binding)+export
        magic = b'NXV2' if export else b'NXV1'
        return struct.pack('>4s16s32sQI',magic,self.environment,self.checkpoint_hash,WORK_DISK_BYTES,len(export))+export

    def reset_matches(self, scope):
        if not self.reset_enabled:
            return False
        binding = self.reset_binding if self.file_share and self.grant_share else self.environment
        return scope==binding+self.checkpoint_hash

    def reset_status(self, scope):
        state=self.reset_state if self.reset_matches(scope) else RESET_REJECTED
        if state==RESET_APPLIED:
            state=observe_reset(self.disk.parent,scope)
        print(f'GUEST_RESET_STATUS state={state}',flush=True)
        return state

    def apply_reset(self, scope):
        if not self.reset_matches(scope):
            return RESET_REJECTED
        if self.reset_state!=RESET_UNREQUESTED:
            return self.reset_status(scope)
        if self.held or self.buttons:
            return RESET_REJECTED
        try:
            self.check_reset_scope()
        except (OSError,ValueError,KeyError,RuntimeError,EOFError):
            return RESET_REJECTED

        self.reset_state=RESET_UNKNOWN
        staged=None
        installed=False
        try:
            with tempfile.NamedTemporaryFile(dir=self.disk.parent,prefix='reset-',delete=False) as output:
                staged=Path(output.name)
                with self.checkpoint.open('rb') as source:
                    shutil.copyfileobj(source,output)
                output.flush()
                os.fsync(output.fileno())
            replacement, digest = image_snapshot(staged)
            if digest!=self.checkpoint_hash:
                raise ValueError('Invalid staged baseline')
            if self.close()!=0:
                raise RuntimeError('The old guest failed to stop')
            if image_identity(self.disk.stat(follow_symlinks=False))!=self.disk_identity:
                raise ValueError('The stopped work disk changed identity')
            if self.file_share and self.export_identity()!=self.share_identity:
                raise ValueError('The stopped guest export changed identity')
            previous=self.disk.with_name('work-before-reset.img')
            os.link(self.disk,previous,follow_symlinks=False)
            saved_identity, saved = image_snapshot(previous)
            if saved_identity!=self.disk_identity:
                raise ValueError('The retained work disk changed identity')
            os.replace(staged,self.disk)
            installed=True
            sync_directory(self.disk.parent)
            if image_snapshot(self.disk)!=(replacement,self.checkpoint_hash):
                raise ValueError('The installed work disk does not match the baseline')
            record_reset(self.disk.parent,scope,replacement,self.checkpoint_identity,saved_identity,saved)
            self.reset_state=RESET_APPLIED
            print(f'GUEST_RESET_APPLIED environment={self.environment.hex()} '
                  f'checkpoint={self.checkpoint_hash.hex()} prior={saved.hex()}',flush=True)
        except (OSError,ValueError,RuntimeError):
            self.reset_state=RESET_UNKNOWN if installed else RESET_REJECTED
            print(f'GUEST_RESET_FAILED state={self.reset_state}',flush=True)
        finally:
            if staged:
                staged.unlink(missing_ok=True)
        return self.reset_state

    def close(self):
        if self.close_result is not None:
            return self.close_result
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
                        for export in re.findall(rb'^NEXAUI_FILE_SHARE (\w+)\r?$',output,re.M):
                            print(f'GUEST_FILE_SHARE {export.decode()}',flush=True)
                        if b'NEXAUI_CLIPBOARD_INDEPENDENT_OK' in output.splitlines():
                            print('GUEST_CLIPBOARD independent_wayland_read',flush=True)
                        for network in re.findall(rb'^NEXAUI_NETWORK_(OFF|CONNECTED|CUTOFF|CANARY_BLOCKED)\r?$',output,re.M):
                            print(f'GUEST_NETWORK guest_{network.decode().lower()}',flush=True)
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
                self.close_result=self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.close_result=self.process.wait(timeout=2)
            return self.close_result
        finally:
            if self.network:
                self.network.close()
            if self.clipboard:
                self.clipboard.close()
            if self.stream:
                self.stream.close()
            self.peer.close()


def read_request(peer, desktop):
    peer.settimeout(1)
    with peer.makefile('rb') as stream:
        line = stream.readline(96)
        if not line.endswith(b'\n'):
            raise ValueError('Invalid guest frame request')
        fields = tuple(map(int, line.split()))
        generation, resize, width, height, kind = fields[:5]
        if not (0 < generation < 2**64 and 0 < resize < 2**64 and
                0 < width <= 2048 and 0 < height <= 2048 and width*height*4 <= PNG_LIMIT and
                kind in range(18 if desktop else 6) and len(fields)==(6 if kind in (5,9,10,12,13,14,15,16,17) else 5)):
            raise ValueError('Guest request outside the fixture limits')
        size = fields[5] if len(fields)==6 else 0
        if not 0 <= size <= INPUT_LIMIT*INPUT_RECORD_SIZE:
            raise ValueError('Guest input batch too large')
        if kind in (9,10) and size!=RESET_SCOPE_BYTES:
            raise ValueError('Invalid guest reset scope length')
        if kind in (12,13,16,17) and size!=16:
            raise ValueError('Invalid sharing grant environment length')
        if kind==14 and not 24<size<=24+CLIPBOARD_LIMIT:
            raise ValueError('Invalid guest clipboard length')
        if kind==15 and size!=24:
            raise ValueError('Invalid guest clipboard read scope')
        payload = stream.read(size)
        if len(payload)!=size or stream.read(1):
            raise ValueError('Invalid guest request framing')
    return generation, resize, kind, payload


def interrupt(*_):
    raise KeyboardInterrupt


def require_reset_root(address):
    info = address.parent.stat(follow_symlinks=False)
    if (not address.parent.name.startswith('nexaui-guest-') or not stat.S_ISDIR(info.st_mode) or
            stat.S_IMODE(info.st_mode)!=0o700 or info.st_uid!=os.geteuid()):
        raise ValueError('Reset requires the private test harness directory')


def serve(engine, firmware, boot, address, initrd=None, disk=None, checkpoint=None, reset_mode=None, listener_fd=None):
    signal.signal(signal.SIGTERM, interrupt)
    if reset_mode:
        require_reset_root(address)
        if (reset_mode not in ('drop-response','restart-before-reply') or not disk or not checkpoint or
                Path(disk)!=address.parent/'work.img' or Path(checkpoint)!=address.parent/'baseline.img'):
            raise ValueError('Reset requires the private test harness images')
    file_share = address.parent/'host-export' if reset_mode and os.environ.get('NEXAUI_CONTENT_TEST_FILE_SHARE')=='1' else None
    clipboard = os.environ.get('NEXAUI_CONTENT_TEST_GUEST_CLIPBOARD')=='1'
    network = os.environ.get('NEXAUI_CONTENT_TEST_GUEST_NETWORK')=='1'
    if clipboard or network:
        require_reset_root(address)
    grant = bool(file_share and os.environ.get('NEXAUI_CONTENT_TEST_FILE_GRANT')=='1')
    bindfs = os.environ.get('NEXAUI_CONTENT_TEST_FILE_BINDFS')
    if bindfs and not grant:
        raise ValueError('Private file backend requires a runtime grant')
    with tempfile.TemporaryDirectory(prefix='qemu-guest-', dir=address.parent) as temp, ExitStack() as services:
        export = services.enter_context(GuestExport(bindfs,file_share,Path(temp)/'file-export')) if bindfs else None
        guest = Guest(engine, firmware, boot, Path(temp), initrd, disk, writable=bool(disk), checkpoint=checkpoint,
                      reset_enabled=bool(reset_mode),file_share=file_share,
                      revoke_share=bool(file_share and os.environ.get('NEXAUI_CONTENT_TEST_FILE_REVOKE')=='1'),
                      grant_share=grant,export=export,clipboard=clipboard,network=network)
        drop_response=reset_mode=='drop-response'
        try:
            status = guest.wait_ready()
            if status != 'running':
                raise RuntimeError(f'Unexpected guest state {status}')
            version = '.'.join(str(guest.version[k]) for k in ('major','minor','micro'))
            identity = 'desktop nic=none ram=768MiB' if guest.desktop else 'trace=R nic=none boot=readonly ram=32MiB'
            print(f'GUEST_READY qemu={version} status={status} {identity}', flush=True)
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, fileno=listener_fd) as listener:
                if listener_fd is None:
                    listener.bind(str(address))
                    listener.listen(1)
                else:
                    address.with_suffix('.ready').touch(exist_ok=False)
                while True:
                    peer, _ = listener.accept()
                    with peer:
                        generation, resize, kind, payload = read_request(peer,guest.desktop)
                        if kind in (9,10):
                            state = guest.apply_reset(payload) if kind==9 else guest.reset_status(payload)
                            if kind==9 and reset_mode=='restart-before-reply' and state==RESET_APPLIED:
                                print(f'GUEST_RESET_OWNER_EXIT pid={os.getpid()}',flush=True)
                                return RESET_RESTART_EXIT
                            if kind==9 and drop_response and state==RESET_APPLIED:
                                drop_response=False
                                print('GUEST_RESET_RESPONSE dropped',flush=True)
                            else:
                                peer.sendall(b'NXA1'+bytes((state,)))
                            continue
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
                        if kind == 8:
                            peer.sendall(guest.reset_preview())
                            continue
                        if kind == 11:
                            peer.sendall(guest.sharing())
                            continue
                        if kind == 12:
                            peer.sendall(guest.revoke_files(payload))
                            continue
                        if kind == 13:
                            peer.sendall(guest.grant_files(payload))
                            continue
                        if kind in (14,15):
                            outcome = guest.send_clipboard(payload) if kind==14 else guest.read_clipboard(payload)
                            try:
                                peer.sendall(outcome)
                            except (BrokenPipeError,ConnectionResetError):
                                pass
                            continue
                        if kind in (16,17):
                            outcome = guest.network_action(payload,grant=kind==16)
                            try:
                                peer.sendall(outcome)
                            except (BrokenPipeError,ConnectionResetError):
                                pass
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


def serve_reset_status(address, listener_fd):
    require_reset_root(address)
    signal.signal(signal.SIGTERM, interrupt)
    print(f'GUEST_RESET_RECOVERY_READY pid={os.getpid()}',flush=True)
    try:
        with socket.socket(fileno=listener_fd) as listener:
            while True:
                peer, _ = listener.accept()
                with peer:
                    try:
                        _, _, kind, scope = read_request(peer,True)
                        state = observe_reset(address.parent,scope) if kind==10 else RESET_REJECTED
                        print(f'GUEST_RESET_RECOVERY kind={kind} state={state}',flush=True)
                        peer.sendall(b'NXA1'+bytes((state,)))
                    except (OSError,ValueError):
                        continue
    except KeyboardInterrupt:
        pass


def supervise_reset_restart(arguments):
    address = Path(arguments[3])
    require_reset_root(address)
    signal.signal(signal.SIGTERM, interrupt)
    child = None
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
        listener.bind(str(address))
        listener.listen(1)
        fd = listener.fileno()
        script = [sys.executable, __file__]
        try:
            child = subprocess.Popen([*script, *arguments, str(fd)],pass_fds=(fd,))
            if child.wait()!=RESET_RESTART_EXIT:
                raise RuntimeError('Expected the guest owner to exit after committing the reset')
            child = subprocess.Popen([*script, '--recover-reset', str(address), str(fd)],pass_fds=(fd,))
            if child.wait()!=0:
                raise RuntimeError('The reset status owner failed')
        except KeyboardInterrupt:
            signal.signal(signal.SIGTERM,signal.SIG_IGN)
        finally:
            if child and child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=6)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
                    raise


if __name__ == '__main__':
    args = sys.argv[1:]
    if len(args)==3 and args[0]=='--recover-reset':
        serve_reset_status(Path(args[1]),int(args[2]))
    elif len(args)==8 and args[-1]=='restart-before-reply':
        supervise_reset_restart(args)
    elif 4<=len(args)<=8 or (len(args)==9 and args[7]=='restart-before-reply'):
        sys.exit(serve(*args[:3], Path(args[3]), *args[4:8], listener_fd=int(args[8]) if len(args)==9 else None))
    else:
        raise SystemExit('Invalid guest fixture arguments')
