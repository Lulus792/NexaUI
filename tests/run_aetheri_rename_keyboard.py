"""Exercise rename review with virtual keys on an isolated Aetheri compositor."""
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
    for name in ('server', 'host', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--mode', choices=('cancel', 'confirm', 'long'), required=True)
    args = parser.parse_args()
    server, host, font = (p.resolve(strict=True) for p in (args.server, args.host, args.font))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime, directory = root / 'runtime', root / 'files'
    runtime.mkdir(mode=0o700); directory.mkdir(mode=0o700)
    old, new = directory / 'old.txt', directory / ('x' * 255 if args.mode == 'long' else 'new.txt')
    old.write_bytes(b'original')
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-rename',
               NEXAUI_PLATFORM='wayland', NEXAUI_TEST_PRIVATE_WAYLAND='1', NEXAUI_RENAME_TRACE='1')
    for key in ('DISPLAY', 'WAYLAND_SOCKET', 'DBUS_SESSION_BUS_ADDRESS', 'AT_SPI_BUS_ADDRESS', 'NEXAUI_RENAME_LONG_TEST'):
        env.pop(key, None)
    if args.mode == 'long':
        env['NEXAUI_RENAME_LONG_TEST'] = '1'
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    compositor = app = None
    result = {'status': 'failed', 'server_sha256': digest(server), 'host_sha256': digest(host),
              'font_sha256': digest(font), 'mode': args.mode,
              'scope': 'isolated Aetheri test compositor, virtual keys and injected post-rename flush failure'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'rename.log').open('wb') as app_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], '-', str(issuer.fileno())],
                env=env, stdin=subprocess.DEVNULL, stdout=server_log, stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(),))
            issuer.close()

            def log(name):
                return (root / (name + '.log')).read_text(errors='replace')

            def wait(marker, name='rename', after=0):
                deadline = time.monotonic() + 15
                while marker + '\n' not in log(name)[after:]:
                    if compositor.poll() is not None or (app and app.poll() is not None) or time.monotonic() >= deadline:
                        raise RuntimeError('failed waiting for ' + marker)
                    time.sleep(.02)

            def key(code):
                for state in (1, 0):
                    controller.sendall(f'K {code} {state}'.encode('ascii'))
                    if controller.recv(32) != b'OK':
                        raise RuntimeError('private input driver rejected key')

            wait('READY nexaui-rename outputs=3 renderer=pixman', 'server')
            app = subprocess.Popen([str(host), str(directory), str(font)], env=env,
                stdin=subprocess.DEVNULL, stdout=app_log, stderr=subprocess.STDOUT)
            wait('RENAME_NATIVE_READY'); wait('MAP 0', 'server')
            wait('RENAME_NATIVE_FOCUS 31' if args.mode == 'long' else 'RENAME_NATIVE_FOCUS 10')
            if args.mode == 'cancel':
                key(1); wait('RENAME_NATIVE_CLOSED 0 0')
            else:
                if args.mode == 'long':
                    wait('RENAME_NATIVE_CONFIRM_ENABLED 0')
                    key(107); wait('RENAME_NATIVE_CONFIRM_ENABLED 1')
                    if not old.exists() or new.exists():
                        raise RuntimeError('review changed the files before confirmation')
                    key(15); wait('RENAME_NATIVE_FOCUS 10')
                if not old.exists() or new.exists() or old.read_bytes() != b'original':
                    raise RuntimeError('files changed before explicit confirmation')
                key(15); wait('RENAME_NATIVE_FOCUS 11')
                key(28); wait('RENAME_NATIVE_REOPENED_UNKNOWN')
                start = log('rename').index('RENAME_NATIVE_REOPENED_UNKNOWN')
                wait('RENAME_NATIVE_CONFIRM_ENABLED 0', after=start)
                wait('RENAME_NATIVE_FOCUS 31' if args.mode == 'long' else 'RENAME_NATIVE_FOCUS 10', after=start)
                key(15)  # The disabled Confirm must be skipped after unknown.
                if args.mode == 'long':
                    wait('RENAME_NATIVE_FOCUS 10', after=start)
                key(28); wait('RENAME_NATIVE_CLOSED 1 1')
            if app.wait(timeout=10) != 0:
                raise RuntimeError('rename host failed')
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', log('rename')):
                raise RuntimeError('sanitizer reported a rename error')
            expected = old if args.mode == 'cancel' else new
            if sorted(p.name for p in directory.iterdir()) != [expected.name] or expected.read_bytes() != b'original':
                raise RuntimeError('unexpected file set or content')
            result['status'] = 'passed'
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        if app and app.poll() is None:
            app.terminate(); app.wait(timeout=5)
        if compositor and compositor.poll() is None:
            compositor.send_signal(signal.SIGTERM); compositor.wait(timeout=10)
        issuer.close(); controller.close()
        result['host_exit'] = app.returncode if app else None
        result['server_exit'] = compositor.returncode if compositor else None
        if (result['host_exit'], result['server_exit']) != (0, 0):
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('rename keyboard integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
