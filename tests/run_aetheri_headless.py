"""Exercise Settings on an independently built Aetheri test compositor.

The server is Aetheri's test-only executable with an inherited input socket;
this script never connects to an existing session or injects global input.
Pass exact, separately built binaries and an existing font. The work directory
must be new and is retained with logs, screenshots and a result record.
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
import time


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--settings', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--test-compositor-loss', action='store_true',
                        help='kill only the isolated compositor and require local Settings cleanup')
    parser.add_argument('--test-apply-inflight', action='store_true',
                        help='hold a submitted Apply worker across compositor loss')
    parser.add_argument('--test-ime-v3', action='store_true',
                        help='exercise the compositor test-only text-input-v3 producer')
    parser.add_argument('--test-ime-v3-focus', action='store_true',
                        help='cancel preedit by moving focus to another control')
    args = parser.parse_args()
    if args.test_apply_inflight and not args.test_compositor_loss:
        parser.error('--test-apply-inflight requires --test-compositor-loss')
    if args.test_ime_v3 and args.test_compositor_loss:
        parser.error('--test-ime-v3 cannot be combined with compositor-loss mode')
    if args.test_ime_v3_focus and not args.test_ime_v3:
        parser.error('--test-ime-v3-focus requires --test-ime-v3')
    server_binary, app_binary, font = (p.resolve(strict=True) for p in
                                        (args.server, args.settings, args.font))
    root = args.work_dir.resolve()
    root.mkdir(mode=0o700)  # Refuse an existing directory and preserve all evidence.
    runtime, captures = root / 'runtime', root / 'captures'
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-aetheri',
               NEXAUI_PLATFORM='wayland')
    if args.test_compositor_loss:
        env['NEXAUI_SETTINGS_TEST_TRACE_CLEANUP'] = '1'
    for key in ('DISPLAY', 'WAYLAND_SOCKET'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    server = app = None
    gate_read = gate_write = None
    if args.test_apply_inflight:
        gate_read, gate_write = os.pipe()
        env['NEXAUI_SETTINGS_TEST_APPLY_GATE_FD'] = str(gate_read)
    result = {'status': 'failed', 'server_sha256': sha256(server_binary),
              'settings_sha256': sha256(app_binary), 'font_sha256': sha256(font),
              'scope': 'private headless Aetheri test compositor; virtual input; no physical or accessibility acceptance'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'settings.log').open('wb') as app_log:
            server = subprocess.Popen([str(server_binary), '--headless', env['WAYLAND_DISPLAY'],
                                       str(captures), str(issuer.fileno())], env=env,
                                      stdin=subprocess.DEVNULL, stdout=server_log,
                                      stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(),))
            issuer.close()

            def contents(name):
                return (root / name).read_text(errors='replace')

            def wait(predicate, label, need_app=True):
                end = time.monotonic() + 15
                while not predicate():
                    if server.poll() is not None or (need_app and app and app.poll() is not None):
                        raise RuntimeError('process exited before ' + label)
                    if time.monotonic() >= end:
                        raise RuntimeError('timed out waiting for ' + label)
                    time.sleep(.02)

            def event(command):
                controller.sendall(command.encode('ascii'))
                if controller.recv(32) != b'OK':
                    raise RuntimeError('input driver rejected ' + command)

            def key(code):
                event(f'K {code} 1'); event(f'K {code} 0')

            wait(lambda: 'READY ' in contents('server.log'), 'compositor readiness', False)
            if args.test_ime_v3:
                event('I on')
            mode = ('--verify-ime-aetheri-focus' if args.test_ime_v3_focus else
                    '--verify-ime-aetheri' if args.test_ime_v3 else '--verify-keyboard-controls')
            app = subprocess.Popen([str(app_binary), str(font), mode],
                                   env=env, stdin=subprocess.DEVNULL, stdout=app_log,
                                   stderr=subprocess.STDOUT,
                                   pass_fds=(gate_read,) if gate_read is not None else ())
            if gate_read is not None:
                os.close(gate_read); gate_read = None
            ready_marker = 'SETTINGS_IME_READY' if args.test_ime_v3 else 'SETTINGS_KEYBOARD_READY'
            wait(lambda: ready_marker in contents('settings.log') and
                 'MAP 0\n' in contents('server.log'), 'mapped Settings frame')
            server.send_signal(signal.SIGUSR1)
            wait(lambda: all(f'CAPTURE 1 {i}\n' in contents('server.log') for i in range(3)),
                 'three composited outputs')
            picture = (captures / 'capture-1-output-0.ppm').read_bytes()
            magic, dimensions, maximum, pixels = picture.split(b'\n', 3)
            width, height = map(int, dimensions.split())
            if magic != b'P6' or maximum != b'255' or (width, height) != (1920, 1080) or len(pixels) != width * height * 3:
                raise RuntimeError('unexpected compositor capture format')
            def pixel(x, y):
                offset = (y * width + x) * 3
                return pixels[offset:offset + 3]
            background = pixel(10, 10)
            if background != bytes((16, 20, 28)):
                raise RuntimeError('unexpected desktop background')
            changed = sum(pixel(x, y) != background for y in range(50, 390, 4)
                          for x in range(50, 670, 4))
            if changed < 2000:
                raise RuntimeError('captured Settings pixels missing')
            result['output0_sha256'] = hashlib.sha256(picture).hexdigest()
            if args.test_ime_v3:
                key(15)
                wait(lambda: 'SETTINGS_IME_FOCUSED\n' in contents('settings.log') and
                     'IME commit enabled=1 ' in contents('server.log'), 'focused native text input')
                event('I 0')
                wait(lambda: 'SETTINGS_IME_PREEDIT\n' in contents('settings.log'), 'unicode preedit')
                if args.test_ime_v3_focus:
                    key(15)
                    wait(lambda: 'SETTINGS_IME_CANCELLED\n' in contents('settings.log') and
                         'IME commit enabled=0 ' in contents('server.log'), 'cancelled preedit and disabled text input')
                    before_refocus = len(contents('server.log'))
                    event('K 42 1'); key(15); event('K 42 0')
                    wait(lambda: 'SETTINGS_IME_REFOCUSED\n' in contents('settings.log') and
                         'IME commit enabled=1 ' in contents('server.log')[before_refocus:], 'refocused text input')
                    key(30)
                else:
                    event('I 1')
                if app.wait(timeout=10) != 0:
                    raise RuntimeError('IME Settings example failed')
                trace = contents('settings.log')
                expected = 'SETTINGS_IME_AFTER_RETURN\n' if args.test_ime_v3_focus else 'SETTINGS_IME_COMMITTED\n'
                if expected not in trace or 'SETTINGS_IME_CLOSED\n' not in trace:
                    raise RuntimeError('IME commit or clean closure missing')
                if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', trace):
                    raise RuntimeError('sanitizer reported an IME error')
                result['ime_v3'] = ('preedit cancelled on control focus change; typing resumed' if args.test_ime_v3_focus
                                    else 'unicode preedit and commit without applying the draft')
                result['status'] = 'passed'
                return
            if args.test_compositor_loss:
                if args.test_apply_inflight:
                    for code in (15, 15, 57, 15, 57):
                        key(code); time.sleep(.15)
                    wait(lambda: 'SETTINGS_APPLY_WORKER_BLOCKED\n' in contents('settings.log'),
                         'held Apply worker')
                server.kill()
                if server.wait(timeout=10) != -signal.SIGKILL:
                    raise RuntimeError('unexpected compositor exit')
                if args.test_apply_inflight:
                    deadline = time.monotonic() + 10
                    while 'SETTINGS_CLEANUP_WAIT_WORKER\n' not in contents('settings.log'):
                        if app.poll() is not None or time.monotonic() >= deadline:
                            raise RuntimeError('Settings did not wait for the held worker')
                        time.sleep(.02)
                    os.write(gate_write, b'g'); os.close(gate_write); gate_write = None
                if app.wait(timeout=10) != 1:
                    raise RuntimeError('unexpected compositor or Settings exit')
                trace = contents('settings.log')
                if 'SETTINGS_CLEANUP_DONE\n' not in trace or 'Settings example failed\n' not in trace:
                    raise RuntimeError('Settings did not complete its failure cleanup')
                if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', trace):
                    raise RuntimeError('sanitizer reported an error during Settings cleanup')
                if args.test_apply_inflight:
                    if trace.count('SETTINGS_APPLY_WORKER_BLOCKED\n') != 1 or re.search(r'^KEYBOARD_TRACE .*motion=1$', trace, re.M):
                        raise RuntimeError('Apply was replayed or reported completed')
                    result['apply_inflight'] = 'one held worker joined after compositor loss'
                result['compositor_loss'] = 'local failure cleanup'
                result['status'] = 'passed'
                return
            # Test driver's native input reaches the same wlroots seat used by
            # the compositor. Its ACK only confirms injection, not app state.
            for code in (15, 15, 57, 15, 57):
                key(code)
                time.sleep(.15)
            if app.wait(timeout=15) != 0:
                raise RuntimeError('Settings keyboard flow failed')
            trace = contents('settings.log')
            for focus in (3, 4, 5):
                if not re.search(r'^KEYBOARD_TRACE .*focus=' + str(focus) + r'\b', trace, re.M):
                    raise RuntimeError('keyboard focus was not observed at control ' + str(focus))
            if not re.search(r'^KEYBOARD_TRACE .*applied_bytes=7 motion=1$', trace, re.M):
                raise RuntimeError('Settings did not observe the applied model state')
            result['status'] = 'passed'
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        if gate_write is not None:
            os.close(gate_write)
        if gate_read is not None:
            os.close(gate_read)
        if app and app.poll() is None:
            app.terminate(); app.wait(timeout=5)
        if server and server.poll() is None:
            server.send_signal(signal.SIGTERM); server.wait(timeout=10)
        issuer.close(); controller.close()
        result['settings_exit'] = app.returncode if app else None
        result['server_exit'] = server.returncode if server else None
        expected = (1, -signal.SIGKILL) if args.test_compositor_loss else (0, 0)
        if (result['settings_exit'], result['server_exit']) != expected:
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
