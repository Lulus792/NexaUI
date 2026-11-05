"""Exercise the installed Files probe on a private Weston seat."""
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
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    app, font, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_FILES_NATIVE_TRACE='1')
    with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as stream:
        windows = re.findall(r'x11 output .*?window id (\d+)', stream.read())
    if len(windows) != 1:
        raise RuntimeError('Expected one private Weston output')
    with tempfile.TemporaryDirectory(prefix='nexaui-files-native-') as directory:
        for index in range(20):
            Path(directory, f'item{index:02d}').touch(exist_ok=False)
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
        def wait(expected):
            deadline = time.monotonic()+12
            while True:
                remaining = deadline-time.monotonic()
                if remaining <= 0:
                    break
                try:
                    if messages.get(timeout=remaining) == expected:
                        return
                except queue.Empty:
                    break
            raise RuntimeError(f'Missing {expected}: {"".join(output)} {"".join(errors)}')
        def key(value):
            subprocess.run([xdotool, 'key', value], env=env, check=True, timeout=5)
        try:
            wait('FILES_NATIVE_READY')
            wait('FILES_NATIVE_SELECTED item00')
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
            key('Down');wait('FILES_NATIVE_SELECTED item01')
            key('End');wait('FILES_NATIVE_SELECTED item19')
            entered = False
            for y in range(50, 1200, 100):
                for x in range(50, 1600, 100):
                    subprocess.run([xdotool, 'mousemove', '--window', windows[0], str(x), str(y)],
                                   env=env, check=True, timeout=5)
                    deadline = time.monotonic()+.04
                    while True:
                        remaining = deadline-time.monotonic()
                        if remaining <= 0:
                            break
                        try:
                            if messages.get(timeout=remaining) == 'FILES_NATIVE_POINTER':
                                entered = True
                                break
                        except queue.Empty:
                            break
                    if entered:
                        break
                if entered:
                    break
            if not entered:
                raise RuntimeError(f'Virtual pointer did not enter the Files surface: {"".join(output)} {"".join(errors)}')
            subprocess.run([xdotool, 'click', '4'], env=env, check=True, timeout=5)
            wait('FILES_NATIVE_SCROLL_UP');wait('FILES_NATIVE_VISIBLE item12')
            key('Home');wait('FILES_NATIVE_SELECTED item00')
            key('ctrl+r');wait('FILES_NATIVE_REFRESHED')
            key('ctrl+l')
            subprocess.run([xdotool, 'type', '--clearmodifiers', '--delay', '2', 'item01'], env=env, check=True, timeout=10)
            wait('FILES_NATIVE_FILTER 6 1 1');wait('FILES_NATIVE_VISIBLE item01')
            subprocess.run([xdotool, 'type', '--clearmodifiers', '--delay', '1', 'x'*256], env=env, check=True, timeout=20)
            wait('FILES_NATIVE_DRAFT 262 0')
            for _ in range(10):
                key('BackSpace')
            wait('FILES_NATIVE_DRAFT 252 0')
            key('ctrl+l');wait('FILES_NATIVE_FILTER 0 20 1')
            key('Tab');key('Return');wait('FILES_NATIVE_CLOSED')
            if host.wait(timeout=10) != 0:
                raise RuntimeError(f'Consumer failed: {"".join(errors)}')
            print('Native Files: keyboard, wheel, guarded filter, refresh and button close passed')
            return 0
        finally:
            if host.poll() is None:
                host.terminate()
                try:
                    host.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    host.kill(); host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
