"""Check the original Files view with its independent peer on a private seat.

The independently selected SDK is experimental; Aetheri's normal pin is unchanged.
Original peer modes are reused without changing their assertions or deadlines.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('aetheri-source', 'server', 'host', 'client', 'registry', 'font',
                 'terminal', 'editor', 'mono', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expect-unavailable', action='store_true')
    parser.add_argument('--document', action='store_true', help='open the original editor/Terminal, retain dirty choices and wait for original child end')
    parser.add_argument('--runtime-library-dir', type=Path, help='explicit private runtime directory for the external Terminal')
    args = parser.parse_args()
    if args.document and args.expect_unavailable:
        parser.error('--document requires the accessible positive host')
    if os.getuid() == 0:
        parser.error('Use an unprivileged private test owner')
    source = args.aetheri_source.resolve(strict=True)
    inputs = json.loads(Path(__file__).with_name('source-inputs.json').read_text())
    for record in inputs['files']:
        if digest(source / record['path']) != record['sha256']:
            raise ValueError('Original source differs: ' + record['path'])
    if args.document:
        for record in inputs['document_files']:
            if digest(source / record['path']) != record['sha256']:
                raise ValueError('Original document source differs: ' + record['path'])
    paths = {name: getattr(args, name).resolve(strict=True) for name in
             ('server', 'host', 'client', 'registry', 'font', 'terminal', 'editor', 'mono')}
    work = args.work_dir.resolve()
    work.mkdir(mode=0o700)
    runtime, folder, cache, captures = (work / name for name in ('runtime', 'documents', 'cache', 'captures'))
    for path in (runtime, folder, cache, captures):
        path.mkdir(mode=0o700)
    (folder / 'existing').mkdir(mode=0o700)
    (folder / 'seed.txt').write_bytes(b'seed')
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='owned-files',
               NEXAUI_PLATFORM='wayland', DBUS_SESSION_BUS_ADDRESS='unix:path=' + str(work / 'bus'),
               AT_SPI_BUS_ADDRESS='unix:path=' + str(work / 'bus'),
               ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'NO_AT_BRIDGE'):
        env.pop(name, None)
    if args.runtime_library_dir:
        directory = args.runtime_library_dir.resolve(strict=True)
        env['LD_LIBRARY_PATH'] = str(directory) + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    if args.document:
        env.update(LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe', LP_NUM_THREADS='2')
    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    left.settimeout(4)
    children, streams, peers, images, cleanup, actor_pins = [], [], [], [], [], []
    result = dict(status='failed', aetheri_revision=inputs['aetheri_revision'],
                  hashes={name: digest(path) for name, path in paths.items()},
                  peers=peers, images=images, cleanup=cleanup,
                  expect_unavailable=args.expect_unavailable,
                  document=args.document,
                  scope='Original app/controller and native transport; no document grant, trusted service or physical/human acceptance')

    def start(command, name, pass_fds=()):
        stream = (work / (name + '.log')).open('xb')
        streams.append(stream)
        child = subprocess.Popen(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT,
                                 stdin=subprocess.DEVNULL, env=env, cwd=work,
                                 pass_fds=pass_fds, start_new_session=True)
        children.append(child)
        return child

    def log(name='app'):
        return (work / (name + '.log')).read_text(errors='replace')

    def wait(predicate, label, allowed_exit=None):
        deadline = time.monotonic() + 5
        while not predicate():
            if any(child is not allowed_exit and child.poll() is not None for child in children) or time.monotonic() >= deadline:
                raise RuntimeError(label + '; see logs in ' + str(work))
            time.sleep(.01)

    def state(**wanted):
        values = re.findall(r'^VIEW (.+)$', log(), re.M)
        current = dict(re.findall(r'(\w+)=([^ ]+)', values[-1])) if values else {}
        return all(current.get(key) == str(value) for key, value in wanted.items())

    def event(command):
        left.sendall(command.encode())
        if left.recv(32) != b'OK':
            raise RuntimeError('Compositor rejected ' + command)

    def key(code, ctrl=False, shift=False):
        held = ([29] if ctrl else []) + ([42] if shift else [])
        for modifier in held:
            event(f'K {modifier} 1')
        event(f'K {code} 1')
        event(f'K {code} 0')
        for modifier in reversed(held):
            event(f'K {modifier} 0')

    def peer(mode):
        with (work / ('peer-' + mode + '.log')).open('w') as output:
            process = subprocess.run([str(paths['client']), str(app.pid), mode], env=env,
                                     stdout=output, stderr=subprocess.STDOUT, timeout=12)
        peers.append(dict(mode=mode, code=process.returncode))
        return process.returncode

    def document_flow():
        # Use the original peer's localized Close action while the document owns
        # native keyboard focus. An accessible close intent must not end that child.
        key(108)
        key(64)
        wait(lambda: state(german=1, selected=1), 'original localized Files selection')
        key(28)
        wait(lambda: re.search(r'^OPEN pid=(\d+) pin=1$', log(), re.M) and
             re.search(r'^READY wayland opengles2 editor=(\d+)', log(), re.M), 'original document pair')
        terminal_pid = int(re.findall(r'^OPEN pid=(\d+) pin=1$', log(), re.M)[-1])
        editor_pid = int(re.findall(r'^READY .*editor=(\d+)', log(), re.M)[-1])
        actors = []
        for pid, binary, parent in ((terminal_pid, paths['terminal'], app.pid), (editor_pid, paths['editor'], terminal_pid)):
            pin = os.pidfd_open(pid)
            actor_pins.append(pin)
            proc = Path(f'/proc/{pid}')
            fields = (proc / 'stat').read_text().rsplit(')', 1)[1].split()
            if int(fields[1]) != parent or (proc / 'exe').resolve() != binary or digest(proc / 'exe') != digest(binary):
                raise RuntimeError('Original document actor mismatch')
            mappings = (proc / 'maps').read_text()
            (work / f'actor-{pid}-maps.txt').write_text(mappings)
            actors.append(dict(pid=pid, binary=str(binary), sha256=digest(binary), parent=parent))
        result['actors'] = actors

        def ended():
            return [bool(select.select([pin], [], [], 0)[0]) for pin in actor_pins]

        def document_log():
            return log().rsplit('READY wayland opengles2 editor=', 1)[-1]

        wait(lambda: 'FOCUS 1\n' in document_log() and re.search(r'^DRAWN \d+$', document_log(), re.M),
             'original document frame and native focus')
        codes = dict(zip('abcdefghijklmnopqrstuvwxyz', (30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44)))

        def accepted():
            values = re.findall(r'^INPUT (\d+)$', document_log(), re.M)
            return int(values[-1]) if values else 0

        expected_input = accepted() + 6
        for character in 'ihello':
            key(codes[character])
        wait(lambda: accepted() == expected_input, 'original editor accepts complete input')
        key(16, ctrl=True, shift=True)
        time.sleep(.2)
        if any(ended()) or (folder / 'seed.txt').read_bytes() != b'seed':
            raise RuntimeError('Terminal close discarded dirty content or saved implicitly')
        capture('original dirty-document close choice')
        key(1)
        if peer('close'):
            raise RuntimeError('Original Files close action failed')
        wait(lambda: state(closing=1), 'Files waits for original document')
        time.sleep(.2)
        if app.poll() is not None or any(ended()) or 'DOCUMENT_END ' in log():
            raise RuntimeError('Files closed before its original document')
        capture('Files waits while original document remains editable')
        key(31, ctrl=True)
        wait(lambda: (folder / 'seed.txt').read_bytes() == b'helloseed', 'exact original selected-FD save')
        if any(ended()) or app.poll() is not None or len(re.findall(r'^OPEN pid=', log(), re.M)) != 1:
            raise RuntimeError('Save replaced or ended original actors')
        result['saved_sha256'] = digest(folder / 'seed.txt')
        key(16, ctrl=True)
        wait(lambda: all(ended()) and 'DOCUMENT_END status=0' in log(), 'original document pair ended', allowed_exit=app)
        result['document_checks'] = ['exact actor binaries and parents', 'dirty Terminal close retained',
                                     'close choice cancelled', 'Files close waits', 'exact selected-FD save',
                                     'one original launch', 'original editor and direct child ended']

    def capture(label):
        time.sleep(.08)
        sequence = len(images) // 3 + 1
        compositor.send_signal(signal.SIGUSR1)
        wait(lambda: all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)), label)
        for output in range(3):
            path = captures / f'capture-{sequence}-output-{output}.ppm'
            raw = path.read_bytes()
            magic, size, maximum, pixels = raw.split(b'\n', 3)
            width, height = map(int, size.split())
            if magic != b'P6' or maximum != b'255' or len(pixels) != width * height * 3:
                raise RuntimeError('Invalid capture')
            images.append(dict(label=label, output=output, file=path.name, sha256=digest(path)))

    try:
        compositor = start([paths['server'], '--headless', env['WAYLAND_DISPLAY'], captures, right.fileno()],
                           'server', (right.fileno(),))
        right.close()
        wait(lambda: 'READY ' in log('server'), 'compositor startup')
        start(['dbus-daemon', '--session', '--nofork', '--address=' + env['AT_SPI_BUS_ADDRESS']], 'bus')
        wait(lambda: (work / 'bus').exists(), 'private bus startup')
        start([paths['registry']], 'registry')

        def registered():
            run = subprocess.run(['dbus-send', '--bus=' + env['AT_SPI_BUS_ADDRESS'], '--type=method_call',
                                  '--print-reply', '--dest=org.freedesktop.DBus', '/org/freedesktop/DBus',
                                  'org.freedesktop.DBus.NameHasOwner', 'string:org.a11y.atspi.Registry'],
                                 capture_output=True, text=True, timeout=2)
            return run.returncode == 0 and 'boolean true' in run.stdout

        wait(registered, 'private registry startup')
        app = start([paths['host'], '--root', folder, '--font', paths['font'], '--cache', cache,
                     '--terminal', paths['terminal'], '--editor', paths['editor'],
                     '--terminal-font', paths['mono'], '--diagnostics'], 'app')
        wait(lambda: state(count=2, depth=0), 'original Files initial frame')
        mappings = Path(f'/proc/{app.pid}/maps').read_text()
        (work / 'maps.txt').write_text(mappings)
        if any(name in mappings for name in ('libSDL', 'libX11', 'libEGL', 'libvulkan')):
            raise RuntimeError('Unexpected presentation dependency')
        modes = ['initial'] if args.expect_unavailable else ['initial', 'dialog', 'text', 'cancel', 'query', 'query-clear']
        for mode in modes:
            code = peer(mode)
            if args.expect_unavailable:
                text = (work / ('peer-' + mode + '.log')).read_text()
                if code != 1 or 'target app was not registered' not in text:
                    raise RuntimeError('Provider-off negative failed at an unexpected boundary')
            elif code:
                raise RuntimeError('Independent original peer failed: ' + mode)
            if mode in ('initial', 'text', 'query'):
                capture(mode)
        if 'OPEN pid=' in log() or sorted(path.name for path in folder.iterdir()) != ['existing', 'seed.txt']:
            raise RuntimeError('View-only actions changed/opened content')
        if args.document:
            document_flow()
        else:
            key(16, ctrl=True)
        if app.wait(timeout=8) != 0 or 'CLOSED files ' not in log() or 'failed=0' not in log():
            raise RuntimeError('Original Files did not close cleanly')
        children.remove(app)
        result['app_exit'] = app.returncode
        for path in work.glob('*.log'):
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', path.read_text(errors='replace')):
                raise RuntimeError('Sanitizer diagnostic in ' + str(path))
        result.update(status='expected-unavailable' if args.expect_unavailable else 'passed', peers=peers, images=images)
    finally:
        # These pidfds refer only to the document actors opened by this host.
        # On a failing test, release those processes before their parent owner.
        for pin in reversed(actor_pins):
            try:
                signal.pidfd_send_signal(pin, signal.SIGHUP)
            except ProcessLookupError:
                pass
        for child in reversed(children):
            escalated = False
            if child.poll() is None:
                try:
                    os.killpg(child.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    escalated = True
                    try:
                        os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    child.wait(timeout=3)
            cleanup.append(dict(command=child.args, code=child.returncode, escalated=escalated))
        for stream in streams:
            stream.close()
        for pin in actor_pins:
            escalated = False
            if not select.select([pin], [], [], 1)[0]:
                escalated = True
                try:
                    signal.pidfd_send_signal(pin, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            cleanup.append(dict(actor_pin=pin, ended=bool(select.select([pin], [], [], 3)[0]), escalated=escalated))
            os.close(pin)
        left.close()
        right.close()
        (work / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
