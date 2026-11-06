"""Check a declared removal preview with an independent native assistive client.

Owns a private Aetheri test compositor, registry, host and optional virtual keys.
No removal service, catalogue, installation or live Aetheri checkout is touched.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'host', 'client', 'registry', 'runner', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--keyboard', action='store_true')
    parser.add_argument('--incomplete', action='store_true')
    parser.add_argument('--expect-unavailable', action='store_true')
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--large', action='store_true')
    parser.add_argument('--filter', action='store_true', help='edit/apply/cancel the native name filter')
    parser.add_argument('--filter-ime', action='store_true', help='controlled text-input-v3 preedit/cancel/commit in the small filter')
    refresh = parser.add_mutually_exclusive_group()
    refresh.add_argument('--refresh-before', action='store_true', help='review a newer snapshot before requesting')
    refresh.add_argument('--refresh-draft', action='store_true', help='preserve editor/undo and cancel preedit across refresh')
    refresh.add_argument('--refresh-issued', action='store_true', help='refresh after an issued request without rearming it')
    args = parser.parse_args()
    if args.keyboard and (args.incomplete or args.expect_unavailable):
        parser.error('--keyboard requires the complete positive fixture')
    if args.capture and not args.keyboard:
        parser.error('--capture requires --keyboard')
    if args.filter_ime and (args.large or args.incomplete or args.expect_unavailable):
        parser.error('--filter-ime requires a complete small positive fixture')
    if args.filter_ime:
        args.filter = True
    refresh_mode = 'draft' if args.refresh_draft else 'before' if args.refresh_before else 'issued' if args.refresh_issued else ''
    if refresh_mode and (args.incomplete or args.expect_unavailable or args.filter_ime):
        parser.error('refresh modes require a complete positive fixture without --filter-ime')
    if args.refresh_draft:
        args.filter = True
    server, host, client, registry, runner, font = [p.resolve(strict=True) for p in
        (args.server, args.host, args.client, args.registry, args.runner, args.font)]
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    captures = root / 'captures'
    if args.capture:
        captures.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-removal',
               NEXAUI_PLATFORM='wayland', NEXAUI_TEST_PRIVATE_WAYLAND='1',
               NEXAUI_REMOVAL_FONT=str(font), NEXAUI_REMOVAL_TEST_CONTROL='1')
    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS', 'DBUS_SESSION_BUS_ADDRESS',
                 'NEXAUI_REMOVAL_KEYBOARD', 'NEXAUI_REMOVAL_INCOMPLETE', 'NEXAUI_REMOVAL_LARGE',
                 'NEXAUI_REMOVAL_FILTER', 'NEXAUI_REMOVAL_FILTER_IME',
                 'NEXAUI_REMOVAL_REFRESH_MODE', 'NEXAUI_REMOVAL_REFRESH_PREEDIT'):
        env.pop(name, None)
    if args.keyboard:
        env['NEXAUI_REMOVAL_KEYBOARD'] = '1'
    if args.incomplete:
        env['NEXAUI_REMOVAL_INCOMPLETE'] = '1'
    if args.large:
        env['NEXAUI_REMOVAL_LARGE'] = '1'
    if args.filter:
        env['NEXAUI_REMOVAL_FILTER'] = '1'
    if args.filter_ime:
        env['NEXAUI_REMOVAL_FILTER_IME'] = '1'
    if refresh_mode:
        env['NEXAUI_REMOVAL_REFRESH_MODE'] = refresh_mode
    if args.refresh_draft:
        env['NEXAUI_REMOVAL_REFRESH_PREEDIT'] = '1'
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    compositor = test = None
    result = dict(status='failed', server_sha256=digest(server), host_sha256=digest(host),
                  client_sha256=digest(client), font_sha256=digest(font), keyboard=args.keyboard,
                  incomplete=args.incomplete, large=args.large, filter=args.filter, filter_ime=args.filter_ime, refresh=refresh_mode,
                  scope='Declared preview only; independent AT-SPI and virtual input, no real removal/authorization or physical/human acceptance')
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'atspi.log').open('wb') as test_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], str(captures) if args.capture else '-', str(issuer.fileno())],
                env=env, stdin=subprocess.DEVNULL, stdout=server_log, stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(),))
            issuer.close(); deadline = time.monotonic() + 5
            while 'READY' not in (root / 'server.log').read_text(errors='replace'):
                if compositor.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('private compositor did not start')
                time.sleep(.02)
            if args.filter_ime or args.refresh_draft:
                # Advertise text-input-v3 before the host binds, using the
                # pinned producer's explicit test-only switch.
                controller.sendall(b'I on')
                if controller.recv(32) != b'OK':
                    raise RuntimeError('test producer rejected IME enable')
            test = subprocess.Popen([sys.executable, str(runner), str(registry), str(client), str(host)],
                env=env, stdin=subprocess.DEVNULL, stdout=test_log, stderr=subprocess.STDOUT)
            injected = set(); deadline = time.monotonic() + 50; capture_started = captured = False
            while test.poll() is None:
                log = (root / 'atspi.log').read_text(errors='replace')
                if args.refresh_draft:
                    for marker, messages in [('REMOVAL_REFRESH_PREEDIT', ['I 0']),
                                             ('REMOVAL_REFRESH_UNDO', ['K 29 1', 'K 44 1', 'K 44 0', 'K 29 0'])]:
                        if marker + '\n' in log and marker not in injected:
                            for message in messages:
                                controller.sendall(message.encode('ascii'))
                                if controller.recv(32) != b'OK':
                                    raise RuntimeError('test producer rejected ' + message)
                            injected.add(marker)
                if args.filter_ime:
                    ime_commands = [('REMOVAL_FILTER_PREEDIT', [('I', 0)]),
                                    ('REMOVAL_FILTER_CANCEL_PREEDIT', [('K', 15)]),
                                    ('REMOVAL_FILTER_REFOCUS', [('K', 'backtab')]),
                                    ('REMOVAL_FILTER_COMMIT', [('I', 1)])]
                    for marker, commands in ime_commands:
                        if marker + '\n' not in log or marker in injected:
                            continue
                        for kind, code in commands:
                            if kind == 'I':
                                messages = [f'I {code}']
                            elif code == 'backtab':
                                messages = ['K 42 1', 'K 15 1', 'K 15 0', 'K 42 0']
                            else:
                                messages = [f'K {code} 1', f'K {code} 0']
                            for message in messages:
                                controller.sendall(message.encode('ascii'))
                                if controller.recv(32) != b'OK':
                                    raise RuntimeError('test producer rejected ' + message)
                        injected.add(marker)
                if args.keyboard:
                    keys = [('REMOVAL_KEY_CHECK', 57), ('REMOVAL_KEY_FOCUS_CLOSE', 15),
                            ('REMOVAL_KEY_FOCUS_REQUEST', 15), ('REMOVAL_KEY_REQUEST', 28), ('REMOVAL_KEY_CLOSE', 28)]
                    if args.large:
                        keys = [('REMOVAL_KEY_LARGE_FOCUS_SCROLL', 'backtab'), ('REMOVAL_KEY_LARGE_END', 107),
                                ('REMOVAL_KEY_LARGE_FOCUS_ACK', 15)] + keys
                    for marker, code in keys:
                        if marker + '\n' in log and marker not in injected:
                            if args.capture and marker == 'REMOVAL_KEY_CHECK' and not captured:
                                if not capture_started:
                                    compositor.send_signal(signal.SIGUSR1); capture_started = True
                                server_output = (root / 'server.log').read_text(errors='replace')
                                if not all(f'CAPTURE 1 {i}\n' in server_output for i in range(3)):
                                    continue
                                picture = (captures / 'capture-1-output-0.ppm').read_bytes()
                                magic, dimensions, maximum, pixels = picture.split(b'\n', 3)
                                if (magic, dimensions, maximum) != (b'P6', b'1920 1080', b'255') or len(pixels) != 1920*1080*3:
                                    raise RuntimeError('unexpected native capture format')
                                result['capture_sha256'] = hashlib.sha256(picture).hexdigest(); captured = True
                            sequence = [(42, 1), (15, 1), (15, 0), (42, 0)] if code == 'backtab' else [(code, 1), (code, 0)]
                            for key, state in sequence:
                                controller.sendall(f'K {key} {state}'.encode('ascii'))
                                if controller.recv(32) != b'OK':
                                    raise RuntimeError('private key driver rejected input')
                            injected.add(marker)
                if time.monotonic() >= deadline:
                    raise RuntimeError('native removal test timed out')
                time.sleep(.02)
            output = (root / 'atspi.log').read_text(errors='replace')
            if 'MAP 0\n' not in (root / 'server.log').read_text(errors='replace'):
                raise RuntimeError('preview was not mapped')
            if args.expect_unavailable:
                if test.returncode == 0 or not re.search(r'removal-atspi:\d+: reviewed\n', output):
                    raise RuntimeError('provider negative did not fail at review discovery')
                result['status'] = 'expected-unavailable'
            else:
                expected = 'HOST REMOVAL_CLOSED requests=0 reopened=0\n' if args.incomplete else 'HOST REMOVAL_CLOSED requests=1 reopened=1\n'
                if test.returncode or expected not in output:
                    raise RuntimeError('independent native client failed; see atspi.log')
                expected_inputs = ((8 if args.large else 5) if args.keyboard else 0) + (4 if args.filter_ime else 0) + (2 if args.refresh_draft else 0)
                if len(injected) != expected_inputs:
                    raise RuntimeError('keyboard sequence incomplete')
                if args.capture and not captured:
                    raise RuntimeError('native capture incomplete')
                result['status'] = 'passed'
    finally:
        for process in (test, compositor):
            if process is not None:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait(timeout=5)
                else:
                    process.wait()
        controller.close(); issuer.close()
        result['test_exit'] = test.returncode if test is not None else None
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
