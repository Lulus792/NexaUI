"""Exercise an installed Files model/view on an isolated Aetheri compositor."""
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
    for name in ('server', 'files', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--key-delay-ms', type=int, choices=range(0, 51), default=0,
                        help='optional per-key delay for comparison; default remains unpaced')
    args = parser.parse_args()
    server, app_binary, font = (p.resolve(strict=True) for p in (args.server, args.files, args.font))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime, captures, directory = root / 'runtime', root / 'captures', root / 'files'
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700); directory.mkdir(mode=0o700)
    for index in range(20):
        (directory / f'item{index:02d}').write_bytes(f'content-{index:02d}'.encode('ascii'))
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-files',
               NEXAUI_PLATFORM='wayland', NEXAUI_FILES_NATIVE_TRACE='1')
    for key in ('DISPLAY', 'WAYLAND_SOCKET'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    compositor = app = None
    result = {'status': 'failed', 'server_sha256': digest(server), 'files_sha256': digest(app_binary),
              'key_delay_ms': args.key_delay_ms,
              'font_sha256': digest(font), 'scope': 'isolated Aetheri test compositor and virtual input'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'files.log').open('wb') as app_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'],
                                           str(captures), str(issuer.fileno())], env=env,
                                          stdin=subprocess.DEVNULL, stdout=server_log,
                                          stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(),))
            issuer.close()

            def log(name):
                return (root / (name + '.log')).read_text(errors='replace')

            def wait(predicate, label):
                deadline = time.monotonic() + 15
                while not predicate():
                    if compositor.poll() is not None or (app and app.poll() is not None):
                        raise RuntimeError('process exited before ' + label)
                    if time.monotonic() >= deadline:
                        raise RuntimeError('timed out waiting for ' + label)
                    time.sleep(.02)

            def event(command):
                controller.sendall(command.encode('ascii'))
                if controller.recv(32) != b'OK':
                    raise RuntimeError('input driver rejected ' + command)

            def key(code):
                event(f'K {code} 1'); event(f'K {code} 0')

            def chord(modifier, code):
                event(f'K {modifier} 1'); key(code); event(f'K {modifier} 0')

            def expect(line, after=0):
                wait(lambda: line + '\n' in log('files')[after:], line)

            wait(lambda: 'READY ' in log('server'), 'compositor')
            app = subprocess.Popen([str(app_binary), str(directory), str(font)], env=env,
                                   stdin=subprocess.DEVNULL, stdout=app_log, stderr=subprocess.STDOUT)
            expect('FILES_NATIVE_READY'); expect('FILES_NATIVE_SELECTED item00')
            compositor.send_signal(signal.SIGUSR1)
            wait(lambda: all(f'CAPTURE 1 {i}\n' in log('server') for i in range(3)), 'captured outputs')
            picture = (captures / 'capture-1-output-0.ppm').read_bytes()
            magic, dimensions, maximum, pixels = picture.split(b'\n', 3)
            if (magic, dimensions, maximum) != (b'P6', b'1920 1080', b'255') or len(pixels) != 1920*1080*3:
                raise RuntimeError('unexpected capture format')
            if pixels[(60*1920+60)*3:(60*1920+60)*3+3] == bytes((16,20,28)):
                raise RuntimeError('Files surface missing from captured output')
            result['capture_sha256'] = hashlib.sha256(picture).hexdigest()
            key(108); expect('FILES_NATIVE_SELECTED item01')
            key(107); expect('FILES_NATIVE_SELECTED item19')
            before = len(log('files')); key(102); expect('FILES_NATIVE_SELECTED item00', before)
            chord(29, 19); expect('FILES_NATIVE_REFRESHED')
            chord(29, 38); expect('FILES_NATIVE_FILTER_FOCUSED')
            for code in (23, 20, 18, 50, 11, 2):
                key(code)
            expect('FILES_NATIVE_FILTER 6 1 1'); expect('FILES_NATIVE_VISIBLE item01')
            for _ in range(256):
                key(45)  # x: exceed the original model's whole-query bound.
                if args.key_delay_ms:
                    time.sleep(args.key_delay_ms / 1000)
            expect('FILES_NATIVE_DRAFT 262 0')
            filters = re.findall(r'^FILES_NATIVE_FILTER (\d+) (\d+) ([01])$', log('files'), re.M)
            if len(filters) < 2 or filters[-1][1:] != ('0', '0') or filters[-2] != (filters[-1][0], '0', '1'):
                raise RuntimeError('rejected query did not retain the last accepted model query')
            for _ in range(10):
                key(14)
            expect('FILES_NATIVE_DRAFT 252 0')
            chord(29, 16)
            if app.wait(timeout=10) != 0 or 'FILES_NATIVE_CLOSED\n' not in log('files'):
                raise RuntimeError('Files failed to close cleanly')
            if any(s in log('files') for s in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')):
                raise RuntimeError('sanitizer reported a Files error')
            if sorted(p.name for p in directory.iterdir()) != [f'item{i:02d}' for i in range(20)]:
                raise RuntimeError('navigation or filtering changed the file set')
            if any((directory / f'item{i:02d}').read_bytes() != f'content-{i:02d}'.encode('ascii') for i in range(20)):
                raise RuntimeError('navigation or filtering changed file content')
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
        result['files_exit'] = app.returncode if app else None
        result['server_exit'] = compositor.returncode if compositor else None
        if (result['files_exit'], result['server_exit']) != (0, 0):
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
