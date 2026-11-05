"""Exercise native rename cancellation and confirmation on a private Weston seat."""
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


def main():
    if len(sys.argv) != 4:
        return 2
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test'):
        return 77
    app, font, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_RENAME_TRACE='1')
    with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as stream:
        windows = re.findall(r'x11 output .*?window id (\d+)', stream.read())
    if len(windows) != 1:
        raise RuntimeError('Expected one private Weston output')
    for confirm in (False, True):
        with tempfile.TemporaryDirectory(prefix='nexaui-rename-key-') as directory:
            old, new = Path(directory, 'old.txt'), Path(directory, 'new.txt')
            old.write_bytes(b'original')
            host = subprocess.Popen([app, directory, font], env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            messages, output, errors = queue.Queue(), [], []
            def collect(stream, target, publish=False):
                for line in stream:
                    target.append(line)
                    if publish:
                        messages.put(line.strip())
            for stream, target, publish in ((host.stdout, output, True), (host.stderr, errors, False)):
                threading.Thread(target=collect, args=(stream, target, publish), daemon=True).start()
            def wait_any(expected):
                deadline = time.monotonic()+12
                while time.monotonic() < deadline:
                    try:
                        message = messages.get(timeout=max(.001, deadline-time.monotonic()))
                        if message in expected:
                            return message
                    except queue.Empty:
                        break
                raise RuntimeError(f'Missing one of {expected}: {"".join(output)} {"".join(errors)}')
            def wait(expected):
                return wait_any({expected})
            def key(value):
                subprocess.run([xdotool, 'key', value], env=env, check=True, timeout=5)
            try:
                wait('RENAME_NATIVE_READY');wait('RENAME_NATIVE_FOCUS 10')
                subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
                if confirm:
                    key('Tab');wait('RENAME_NATIVE_FOCUS 11')
                    key('Return');wait('RENAME_NATIVE_REOPENED_UNKNOWN');wait('RENAME_NATIVE_FOCUS 10')
                    if old.exists() or not new.exists() or new.read_bytes() != b'original':
                        raise RuntimeError('Rename result did not match the original temporary object')
                    key('Tab');key('Return')
                    outcome = wait_any({'RENAME_NATIVE_CLOSED 1 1', 'RENAME_NATIVE_FOCUS 11'})
                    if outcome != 'RENAME_NATIVE_CLOSED 1 1':
                        raise RuntimeError('Unknown operation allowed focus on Confirm')
                else:
                    key('Escape');wait('RENAME_NATIVE_CLOSED 0 0')
                    if not old.exists() or new.exists() or old.read_bytes() != b'original':
                        raise RuntimeError('Cancel changed the temporary files')
                if host.wait(timeout=10) != 0:
                    raise RuntimeError(f'Native rename consumer failed: {"".join(errors)}')
            finally:
                if host.poll() is None:
                    host.terminate()
                    try:
                        host.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        host.kill();host.wait()
    print('Native rename: keyboard cancellation, confirmation and disabled replay passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
