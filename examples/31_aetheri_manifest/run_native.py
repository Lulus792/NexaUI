"""Controlled async producer and independent native UI peer on a private seat."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'registry', 'host', 'client', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--aetheri-source', type=Path, required=True)
    for name in ('capture', 'german', 'large', 'small', 'light', 'contrast', 'automatic'):
        parser.add_argument('--' + name, action='store_true')
    args = parser.parse_args()
    if os.getuid() == 0:
        parser.error('Use an unprivileged private test owner')
    paths = {name: getattr(args, name).resolve(strict=True) for name in ('server', 'registry', 'host', 'client', 'font')}
    work = args.work_dir.absolute()
    work.mkdir(mode=0o700)
    runtime, captures = work / 'r', work / 'captures'
    runtime.mkdir(mode=0o700)
    captures.mkdir(mode=0o700)
    if len(os.fsencode(runtime / 'bus')) > 90:
        raise ValueError('Use a shorter new work directory')
    address = 'unix:path=' + str(runtime / 'bus')
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='async-tree',
               NEXAUI_PLATFORM='wayland', DBUS_SESSION_BUS_ADDRESS=address, AT_SPI_BUS_ADDRESS=address,
               ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'NO_AT_BRIDGE'):
        env.pop(name, None)
    for name in ('german', 'large', 'small', 'light', 'contrast'):
        env.pop('AH_TEST_' + name.upper(), None)
        if getattr(args, name):
            env['AH_TEST_' + name.upper()] = '1'
    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    left.settimeout(4)
    host_read, owner_write = os.pipe2(os.O_NONBLOCK | os.O_CLOEXEC)
    children, streams, cleanup, checks, images = [], [], [], [], []
    expected_done = set()
    result = dict(status='failed', inputs={name: digest(path) for name, path in paths.items()},
                  cleanup=cleanup, checks=checks, images=images,
                  configuration={name: getattr(args, name) for name in ('german', 'large', 'small', 'light', 'contrast')},
                  scope='Original canonical manifest parser/hash and complete declared paths through held real workers/native UI; no authority, installation, normal Aetheri adoption or physical/human acceptance')

    def start(command, name, pass_fds=(), interactive=False):
        stream = (work / (name + '.log')).open('xb')
        streams.append(stream)
        child = subprocess.Popen(list(map(str, command)), stdin=subprocess.PIPE if interactive else subprocess.DEVNULL,
                                 stdout=stream, stderr=subprocess.STDOUT, env=env, cwd=work,
                                 pass_fds=pass_fds, start_new_session=True)
        children.append((name, child))
        return child

    def log(name):
        return (work / (name + '.log')).read_text(errors='replace')

    def wait(predicate, label, allowed_exit=None):
        deadline = time.monotonic() + 7
        while not predicate():
            if any(child not in expected_done and child is not allowed_exit and child.poll() is not None for _, child in children):
                raise RuntimeError(label + ': child ended; inspect ' + str(work))
            if time.monotonic() >= deadline:
                raise RuntimeError(label + ': timeout; inspect ' + str(work))
            time.sleep(.01)

    def event(command):
        left.sendall(command.encode('ascii'))
        if left.recv(64) != b'OK':
            raise RuntimeError('Compositor rejected ' + command)

    def key(code):
        event(f'K {code} 1')
        event(f'K {code} 0')

    def view():
        frames = re.findall(r'^VIEW (.+)$', log('host'), re.M)
        return dict(re.findall(r'(\w+)=(\d+)', frames[-1])) if frames else {}

    def requests():
        return [int(ticket) for ticket in re.findall(r'^REQUEST ticket=(\d+).* status=0 submitted=\d+$', log('host'), re.M)]

    def permit(ticket):
        wait(lambda: f'TAKEN ticket={ticket} ' in log('host'), 'worker took exact ticket')
        os.write(owner_write, f'F {ticket}\n'.encode('ascii'))
        wait(lambda: re.search(rf'^OBSERVE ticket={ticket} accepted=', log('host'), re.M), 'completion reached owner')

    def capture(label):
        if not args.capture:
            return
        sequence = len(images) // 3 + 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)), 'complete capture ' + label)
        for output in range(3):
            path = captures / f'capture-{sequence}-output-{output}.ppm'
            magic, extent, maximum, pixels = path.read_bytes().split(b'\n', 3)
            width, height = map(int, extent.split())
            if magic != b'P6' or maximum != b'255' or len(pixels) != width * height * 3:
                raise RuntimeError('Incomplete capture')
            images.append(dict(label=label, output=output, file=path.name, sha256=digest(path)))

    try:
        server = start([paths['server'], '--headless', 'async-tree', captures if args.capture else '-', right.fileno()], 'server', (right.fileno(),))
        right.close()
        wait(lambda: 'READY ' in log('server'), 'compositor startup')
        start(['dbus-daemon', '--session', '--nofork', '--address=' + address], 'bus')
        wait(lambda: (runtime / 'bus').exists(), 'bus startup')
        start([paths['registry']], 'registry')

        def registered():
            reply = subprocess.run(['dbus-send', '--bus=' + address, '--print-reply', '--dest=org.freedesktop.DBus',
                                    '/org/freedesktop/DBus', 'org.freedesktop.DBus.NameHasOwner', 'string:org.a11y.atspi.Registry'],
                                   env=env, capture_output=True, text=True, timeout=2)
            return reply.returncode == 0 and 'boolean true' in reply.stdout
        wait(registered, 'registry startup')
        from run_source import seed
        records, _ = seed(args.aetheri_source.resolve(strict=True), work)
        manifest = work / 'selected.manifest'
        manifest.write_bytes(records['maximum'] if args.automatic else records['frozen'])
        expected = hashlib.sha256(records['maximum']).hexdigest()
        if args.automatic:
            host = start([paths['host'], paths['font'], manifest, expected], 'host')
        else:
            host = start([paths['host'], paths['font'], manifest, expected, host_read, owner_write], 'host', (host_read, owner_write))
        os.close(host_read)
        host_read = None
        wait(lambda: 'ASYNC_READY\n' in log('host'), 'mapped manifest view')
        result['host_pid'] = host.pid
        result['host_maps'] = Path(f'/proc/{host.pid}/maps').read_text()
        if any(name in result['host_maps'] for name in ('libSDL', 'libX11', 'libEGL', 'libvulkan')):
            raise RuntimeError('Unexpected presentation dependency')
        peer = start([paths['client'], host.pid, work / 'paths.txt'] + (['--german'] if args.german else []) +
                     (['--automatic'] if args.automatic else []), 'client', interactive=True)
        keys = {0:[106], 2:[28], 4:[63], 5:[105], 6:[106], 9:[62], 10:[106]}
        requested = {0:1, 2:2, 4:3, 6:4, 10:5}
        completed = {1:0, 3:1, 7:3, 8:2}
        for step in range(1 if args.automatic else 11):
            wait(lambda: f'STEP {step}\n' in log('client'), f'peer step {step}')
            before = view()
            if step in (2, 6):
                manifest.write_bytes(records['maximum'])
            if step == 4:
                manifest.write_bytes(records['frozen'])
            codes = keys.get(step, [])
            expected_keys = int(before.get('keys', '0')) + 2 * len(codes)
            for code in codes:
                key(code)
            if codes:
                wait(lambda: int(view().get('keys', '0')) >= expected_keys, f'published key batch {step}')
            if step in requested:
                wait(lambda: len(requests()) == requested[step] and f'TAKEN ticket={requests()[-1]} ' in log('host'), f'worker request {step}')
                if not args.automatic:
                    time.sleep(.2)
                    if view().get('phase') != '1':
                        raise RuntimeError('Held original result was published early')
            if step in completed:
                permit(requests()[completed[step]])
            peer.stdin.write(f'CHECK {step}\n'.encode('ascii'))
            peer.stdin.flush()
            wait(lambda: f'ACK {step}\n' in log('client') or f'READ_TOP {step} ' in log('client'),
                 f'native row reading {step}', allowed_exit=peer)
            reading = re.search(rf'^READ_TOP {step} (\d+)$', log('client'), re.M)
            if reading:
                pages = int(reading[1])
                if not 0 < pages <= 64:
                    raise RuntimeError('Invalid native reading page count')
                capture('read-top-' + str(step))
                before_keys = int(view().get('keys', '0'))
                for _ in range(pages):
                    key(109)
                wait(lambda: int(view().get('keys', '0')) >= before_keys + 2 * pages, 'published PageDown reading')
                peer.stdin.write(f'READ {step}\n'.encode('ascii'))
                peer.stdin.flush()
            wait(lambda: f'ACK {step}\n' in log('client'), f'independent manifest state {step}', allowed_exit=peer)
            if args.automatic or step == 10:
                expected_done.add(peer)
            after = view()
            if step == 8 and (after.get('revision'), after.get('ticket'), after.get('phase'), after.get('selected')) != (before.get('revision'), before.get('ticket'), before.get('phase'), before.get('selected')):
                raise RuntimeError('Stale original failure changed the current snapshot')
            checks.append(dict(step=step, before=before, after=after, tickets=requests()))
            if step in (1, 3, 7) or args.automatic:
                capture('step-' + str(step))
        expected_done.add(peer)
        wait(lambda: peer.poll() is not None, 'manifest peer exit', allowed_exit=peer)
        if peer.returncode or 'MANIFEST_NATIVE_VERIFIED' not in log('client'):
            raise RuntimeError('Independent manifest peer failed')
        peer.stdin.close()
        key(1)
        wait(lambda: host.poll() is not None, 'close with original result held', allowed_exit=host)
        totals = 'submitted=1 posted=1 adopted=1 ignored=0 drained=0' if args.automatic else 'submitted=5 posted=5 adopted=3 ignored=1 drained=1'
        if host.returncode or 'ASYNC_CLOSED ' + totals + ' joined=2 failed=0' not in log('host'):
            raise RuntimeError('Manifest owner did not close cleanly')
        last = requests()[-1]
        if not args.automatic and not log('host').index('GUI_GONE\n') < log('host').index(f'POSTED ticket={last} ') < log('host').index('ASYNC_CLOSED '):
            raise RuntimeError('Original worker did not finish after GUI teardown')
        expected_done.add(host)
        for path in work.glob('*.log'):
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', path.read_text(errors='replace')):
                raise RuntimeError('Sanitizer diagnostic: ' + str(path))
        result.update(status='passed', host_exit=host.returncode, client_exit=peer.returncode, tickets=requests())
    finally:
        for name, child in reversed(children):
            forced = False
            if child.poll() is None:
                try:
                    os.killpg(child.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    forced = True
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait(timeout=3)
            cleanup.append(dict(name=name, exit=child.returncode, forced=forced))
        if host_read is not None:
            os.close(host_read)
        os.close(owner_write)
        left.close()
        right.close()
        for stream in streams:
            stream.close()
        (work / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
