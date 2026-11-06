"""Exercise the original-catalogue observer on an isolated Aetheri compositor.

The independent libatspi client owns the host and its test-control pipe. Optional
virtual keys operate the same native controls while that client verifies focus.
No live Aetheri checkout, service or user catalogue is changed.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import socket
import sqlite3
import subprocess
import sys
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'host', 'client', 'registry', 'runner', 'font', 'aetheri-source', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--keyboard', action='store_true')
    parser.add_argument('--capture', action='store_true', help='capture private outputs before keyboard Refresh')
    parser.add_argument('--expect-unavailable', action='store_true')
    args = parser.parse_args()
    if args.capture and (not args.keyboard or args.expect_unavailable):
        parser.error('--capture requires a positive --keyboard run')
    server, host, client, registry, runner, font, source = [p.resolve(strict=True) for p in
        (args.server, args.host, args.client, args.registry, args.runner, args.font, args.aetheri_source)]
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    captures = root / 'captures'
    if args.capture:
        captures.mkdir(mode=0o700)
    example = Path(__file__).resolve().parents[1] / 'examples/27_aetheri_operation/run.py'
    spec = importlib.util.spec_from_file_location('operation_fixture_inputs', example)
    inputs = importlib.util.module_from_spec(spec); spec.loader.exec_module(inputs)
    manifest, expected, catalogue = inputs.seed(source, root)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-operation',
               NEXAUI_PLATFORM='wayland', NEXAUI_TEST_PRIVATE_WAYLAND='1',
               NEXAUI_OPERATION_MANIFEST=str(manifest), NEXAUI_OPERATION_EXPECTED=str(expected),
               NEXAUI_OPERATION_CATALOGUE=str(catalogue), NEXAUI_OPERATION_FONT=str(font))
    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS', 'DBUS_SESSION_BUS_ADDRESS',
                 'NEXAUI_OPERATION_KEYBOARD'):
        env.pop(name, None)
    if args.keyboard:
        env['NEXAUI_OPERATION_KEYBOARD'] = '1'
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    compositor = test = None
    result = {'status': 'failed', 'server_sha256': digest(server), 'host_sha256': digest(host),
              'client_sha256': digest(client), 'font_sha256': digest(font), 'keyboard': args.keyboard,
              'scope': 'Original private catalogue, independent AT-SPI and Aetheri headless compositor; no payload installation or physical/human acceptance'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'atspi.log').open('wb') as test_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], str(captures) if args.capture else '-', str(issuer.fileno())],
                env=env, stdin=subprocess.DEVNULL, stdout=server_log, stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(),))
            issuer.close()
            deadline = time.monotonic() + 15
            while 'READY ' not in (root / 'server.log').read_text(errors='replace'):
                if compositor.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('private compositor did not start')
                time.sleep(.02)
            test = subprocess.Popen([sys.executable, str(runner), str(registry), str(client), str(host)],
                env=env, stdin=subprocess.DEVNULL, stdout=test_log, stderr=subprocess.STDOUT)
            injected = set()
            capture_started = captured = False
            deadline = time.monotonic() + 50
            while test.poll() is None:
                log = (root / 'atspi.log').read_text(errors='replace')
                if args.keyboard:
                    keys = [('OPERATION_KEYBOARD_REFRESH', 57), ('OPERATION_KEYBOARD_FOCUS_CLOSE', 15),
                            ('OPERATION_KEYBOARD_ACTIVATE_CLOSE', 28)]
                    for marker, code in keys:
                        if marker + '\n' in log and marker not in injected:
                            if args.capture and code == 57 and not captured:
                                if not capture_started:
                                    compositor.send_signal(signal.SIGUSR1); capture_started = True
                                server_output = (root / 'server.log').read_text(errors='replace')
                                if not all(f'CAPTURE 1 {i}\n' in server_output for i in range(3)):
                                    continue
                                picture = (captures / 'capture-1-output-0.ppm').read_bytes()
                                magic, dimensions, maximum, pixels = picture.split(b'\n', 3)
                                if (magic, dimensions, maximum) != (b'P6', b'1920 1080', b'255') or len(pixels) != 1920*1080*3:
                                    raise RuntimeError('unexpected native capture format')
                                result['capture_sha256'] = hashlib.sha256(picture).hexdigest()
                                captured = True
                            for state in (1, 0):
                                controller.sendall(f'K {code} {state}'.encode('ascii'))
                                if controller.recv(32) != b'OK':
                                    raise RuntimeError('private key driver rejected input')
                            injected.add(marker)
                if time.monotonic() >= deadline:
                    raise RuntimeError('native operation test timed out')
                time.sleep(.02)
            output = (root / 'atspi.log').read_text(errors='replace')
            if 'MAP 0\n' not in (root / 'server.log').read_text(errors='replace'):
                raise RuntimeError('operation view was not mapped')
            if args.expect_unavailable:
                if test.returncode == 0 or not re.search(r'operation-atspi:\d+: status\n', output):
                    raise RuntimeError('provider negative did not fail at status discovery')
                result['status'] = 'expected-unavailable'
            else:
                if test.returncode or 'HOST OBS_CLOSED accepts=1 refreshes=1 reopened=1\n' not in output:
                    raise RuntimeError('independent native client failed; see atspi.log')
                if args.keyboard and len(injected) != 3:
                    raise RuntimeError('keyboard sequence incomplete')
                if args.capture and not captured:
                    raise RuntimeError('native capture incomplete')
                with sqlite3.connect((catalogue / 'catalogue.sqlite3').as_uri() + '?mode=ro', uri=True) as db:
                    rows = db.execute('SELECT state,phase,outcome,hex(operation_id) FROM operations').fetchall()
                if rows != [('blocked', 'stage', None, '0000000000004000800000000000000B')]:
                    raise RuntimeError('original operation metadata changed unexpectedly: ' + repr(rows))
                result['operation_count'] = len(rows)
                result['status'] = 'passed'
    finally:
        if test and test.poll() is None:
            test.terminate(); test.wait(timeout=5)
        if compositor and compositor.poll() is None:
            compositor.send_signal(signal.SIGTERM); compositor.wait(timeout=10)
        issuer.close(); controller.close()
        result['client_exit'] = test.returncode if test else None
        result['server_exit'] = compositor.returncode if compositor else None
        if result['server_exit'] != 0:
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] == 'failed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
