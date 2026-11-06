"""Check Files text-input-v3 against Aetheri's isolated test producer."""
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
    args = parser.parse_args()
    server, binary, font = (path.resolve(strict=True) for path in (args.server, args.files, args.font))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime, directory = root / 'runtime', root / 'files'
    runtime.mkdir(mode=0o700); directory.mkdir(mode=0o700)
    contents = {'alpha': b'first', 'é': b'second'}
    for name, data in contents.items():
        (directory / name).write_bytes(data)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-files-ime',
               NEXAUI_PLATFORM='wayland', NEXAUI_FILES_NATIVE_TRACE='1', NEXAUI_FILES_IME_TEST='1')
    for key in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS', 'DBUS_SESSION_BUS_ADDRESS'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    compositor = app = None
    result = {'status': 'failed', 'server_sha256': digest(server), 'files_sha256': digest(binary),
              'font_sha256': digest(font), 'scope': 'Aetheri test-only text-input-v3 producer, virtual input'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'files.log').open('wb') as app_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], '-',
                                           str(issuer.fileno())], env=env, stdin=subprocess.DEVNULL,
                                          stdout=server_log, stderr=subprocess.STDOUT,
                                          pass_fds=(issuer.fileno(),))
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
                    raise RuntimeError('test producer rejected ' + command)

            def key(code):
                event(f'K {code} 1'); event(f'K {code} 0')

            def chord(modifier, code):
                event(f'K {modifier} 1'); key(code); event(f'K {modifier} 0')

            def has_caret():
                areas = re.findall(r'IME commit enabled=1 serial=\d+ area=(-?\d+),(-?\d+),(\d+),(\d+)',
                                   log('server'))
                return bool(areas and int(areas[-1][2]) > 0 and int(areas[-1][3]) > 0)

            wait(lambda: 'READY ' in log('server'), 'compositor')
            event('I on')
            app = subprocess.Popen([str(binary), str(directory), str(font)], env=env,
                                   stdin=subprocess.DEVNULL, stdout=app_log, stderr=subprocess.STDOUT)
            wait(lambda: 'FILES_NATIVE_READY\n' in log('files') and 'MAP 0\n' in log('server'), 'mapped Files')
            chord(29, 38)  # Ctrl+L focuses the filter.
            wait(lambda: 'FILES_NATIVE_FILTER_FOCUSED\n' in log('files') and has_caret(),
                 'native filter focus and nonempty candidate rectangle')
            event('I 0')  # Controlled multibyte preedit; it must not alter the model.
            wait(lambda: 'FILES_NATIVE_PREEDIT_BYTES 7 0\n' in log('files'), 'uncommitted preedit')
            if re.search(r'^FILES_NATIVE_FILTER [1-9]', log('files'), re.M):
                raise RuntimeError('preedit altered the model')
            app.send_signal(signal.SIGUSR1)  # Test-only clear/reset while composing.
            wait(lambda: 'FILES_NATIVE_TEST_CLEAR\n' in log('files') and
                 'IME commit enabled=0 ' in log('server'), 'native preedit reset')
            wait(lambda: log('server').rfind('IME commit enabled=1 ') >
                 log('server').rfind('IME commit enabled=0 '), 'native input re-enabled')
            event('I 1')  # Controlled single Unicode commit.
            wait(lambda: 'FILES_NATIVE_FILTER 2 1 1\n' in log('files') and
                 'FILES_NATIVE_VISIBLE é\n' in log('files'), 'committed Unicode filter')
            if log('files').count('FILES_NATIVE_FILTER 2 1 1\n') != 1:
                raise RuntimeError('Unicode filter commit was duplicated')
            chord(29, 16)  # Ctrl+Q closes the example.
            if app.wait(timeout=10) != 0 or 'FILES_NATIVE_CLOSED\n' not in log('files'):
                raise RuntimeError('Files failed to close cleanly')
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', log('files')):
                raise RuntimeError('sanitizer reported a Files error')
            if sorted(p.name for p in directory.iterdir()) != sorted(contents) or any(
                    (directory / name).read_bytes() != data for name, data in contents.items()):
                raise RuntimeError('IME editing changed the file set or contents')
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
        raise RuntimeError('IME integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
