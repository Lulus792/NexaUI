"""Exercise two public NexaUI windows on an isolated Aetheri test compositor."""
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
    for name in ('server', 'consumer', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--close-window', choices=('A', 'B'), default='B',
                        help='close the selected focused window and test the survivor')
    parser.add_argument('--test-clipboard', action='store_true',
                        help='copy Unicode from A and asynchronously read it in B')
    args = parser.parse_args()
    server, consumer = (p.resolve(strict=True) for p in (args.server, args.consumer))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-two',
               NEXAUI_PLATFORM='wayland', NEXAUI_W02_POLL_MANY='1')
    for key in ('DISPLAY', 'WAYLAND_SOCKET', 'NEXAUI_W02_CLOSE_FILE', 'NEXAUI_W02_DONE_FILE'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    read_fd, write_fd = os.pipe()
    env['NEXAUI_W02_CONTROL_FD'] = str(read_fd)
    compositor = app = None
    result = {'status': 'failed', 'server_sha256': digest(server),
              'consumer_sha256': digest(consumer),
              'scope': 'isolated Aetheri compositor and virtual input; no physical or human acceptance'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'consumer.log').open('wb') as app_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], '-', str(issuer.fileno())],
                                          env=env, stdin=subprocess.DEVNULL, stdout=server_log,
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

            def event(value):
                controller.sendall(value.encode('ascii'))
                if controller.recv(32) != b'OK':
                    raise RuntimeError('Aetheri input driver rejected ' + value)

            def key(code):
                event(f'K {code} 1'); event(f'K {code} 0')

            def click(x, y):
                event(f'M {x} {y}'); event('B 272 1'); event('B 272 0')

            wait(lambda: 'READY ' in log('server'), 'compositor')
            app = subprocess.Popen([str(consumer)], env=env, stdin=subprocess.DEVNULL,
                                   stdout=app_log, stderr=subprocess.STDOUT, pass_fds=(read_fd,))
            os.close(read_fd); read_fd = -1
            wait(lambda: 'WINDOWS_OPEN\n' in log('consumer'), 'both presented windows')
            # The fixture assigns the first and second toplevel to outputs 0 and 1,
            # respectively, with a 40-pixel inset on its unscaled virtual outputs.
            click(60, 60)
            wait(lambda: 'FOCUS A 1\n' in log('consumer'), 'first window focus')
            key(30)
            wait(lambda: 'TEXT A 97\n' in log('consumer'), 'first window input')
            if args.test_clipboard:
                os.write(write_fd, b'C')
                wait(lambda: 'CLIPBOARD_SET A\n' in log('consumer'), 'clipboard ownership')
            start = len(log('consumer'))
            click(1980, 60)
            wait(lambda: 'FOCUS B 1\n' in log('consumer')[start:], 'second window focus')
            key(48)
            wait(lambda: 'TEXT B 98\n' in log('consumer'), 'second window input')
            if args.test_clipboard:
                os.write(write_fd, b'P')
                wait(lambda: 'CLIPBOARD_READ B\n' in log('consumer') and
                     'CLIPBOARD_MATCH B\n' in log('consumer'), 'cross-window clipboard transfer')
                result['clipboard'] = 'Unicode copied from A and read by B'
            closed = args.close_window
            survivor = 'A' if closed == 'B' else 'B'
            if closed == 'A':
                start = len(log('consumer')); click(60, 60)
                wait(lambda: 'FOCUS A 1\n' in log('consumer')[start:], 'first window focused again')
            start = len(log('consumer')); os.write(write_fd, closed.encode('ascii'))
            wait(lambda: f'CLOSED {closed}\n' in log('consumer')[start:] and
                 'SURVIVOR_PRESENTED\n' in log('consumer')[start:] and
                 f'FOCUS {survivor} 1\n' in log('consumer')[start:], 'surviving window after close')
            key(46)
            wait(lambda: f'TEXT {survivor} 99\n' in log('consumer'), 'survivor input')
            if args.test_clipboard and closed == 'A':
                os.write(write_fd, b'L')
                wait(lambda: 'CLIPBOARD_LOST B\n' in log('consumer'), 'clipboard owner loss')
                result['clipboard_owner_loss'] = 'read rejected after owner closed'
            os.write(write_fd, b'D')
            if app.wait(timeout=10) != 0:
                raise RuntimeError('consumer failed during cleanup')
            trace = log('consumer')
            if re.findall(r'^TEXT [AB] \d+$', trace, re.M) != ['TEXT A 97', 'TEXT B 98', f'TEXT {survivor} 99']:
                raise RuntimeError('text was duplicated or routed to the wrong window')
            if 'WINDOWS_CLOSED\n' not in trace or re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', trace):
                raise RuntimeError('cleanup or sanitizer check failed')
            result['closed_window'] = closed
            result['status'] = 'passed'
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        if read_fd >= 0:
            os.close(read_fd)
        os.close(write_fd)
        if app and app.poll() is None:
            app.terminate(); app.wait(timeout=5)
        if compositor and compositor.poll() is None:
            compositor.send_signal(signal.SIGTERM); compositor.wait(timeout=10)
        issuer.close(); controller.close()
        result['consumer_exit'] = app.returncode if app else None
        result['server_exit'] = compositor.returncode if compositor else None
        if (result['consumer_exit'], result['server_exit']) != (0, 0):
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
