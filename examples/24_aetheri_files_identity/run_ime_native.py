"""Exercise one native simple-IME composition in the Files filter."""
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
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test' or not os.environ.get('NEXAUI_FILES_IME_TEST')):
        return 77
    app, font, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_FILES_NATIVE_TRACE='1')
    with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as stream:
        windows = re.findall(r'x11 output .*?window id (\d+)', stream.read())
    if len(windows) != 1:
        raise RuntimeError('Expected one isolated Weston output')
    with tempfile.TemporaryDirectory(prefix='nexaui-files-ime-') as directory:
        Path(directory, 'alpha').touch(exist_ok=False)
        Path(directory, 'á').touch(exist_ok=False)
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
            while True:
                remaining = deadline-time.monotonic()
                if remaining <= 0:
                    break
                try:
                    message = messages.get(timeout=remaining)
                    if message in expected:
                        return message
                except queue.Empty:
                    break
            raise RuntimeError(f'Missing one of {expected}: {"".join(output)} {"".join(errors)[-6000:]}')
        def wait(expected):
            return wait_any({expected})
        def key(value):
            subprocess.run([xdotool, 'key', value], env=env, check=True, timeout=5)
        def wait_trace(pattern):
            deadline = time.monotonic()+10
            while time.monotonic()<deadline:
                if re.search(pattern, ''.join(errors)):
                    return
                if host.poll() is not None:
                    break
                time.sleep(.01)
            raise RuntimeError('Missing Wayland IME event: '+pattern)
        try:
            wait('FILES_NATIVE_READY')
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
            key('ctrl+l');wait('FILES_NATIVE_FILTER_FOCUSED')
            wait_trace(r'zwp_text_input_v1#\d+\.enter\(')
            key('108');key('apostrophe');wait('FILES_NATIVE_PREEDIT 1 0')
            key('a');wait('FILES_NATIVE_FILTER 2 1 1');wait('FILES_NATIVE_VISIBLE á')
            # A provider may forward Ctrl+Q as a key, or consume Q and commit
            # text instead. The latter must never be guessed into an action.
            trace_start = len(errors)
            key('ctrl+q')
            outcome = wait_any({'FILES_NATIVE_CLOSED', 'FILES_NATIVE_FILTER 3 0 1'})
            if outcome != 'FILES_NATIVE_CLOSED':
                wait_trace(r'commit_string\(\d+, "q"\)')
                trace_after_shortcut = ''.join(errors[trace_start:])
                if not re.search(r'commit_string\(\d+, "q"\)', trace_after_shortcut):
                    raise RuntimeError('Changed filter lacked the IME q commit')
                if host.poll() is not None:
                    raise RuntimeError('IME text commit unexpectedly closed the window')
                print('IME consumed shortcut key and committed text', flush=True)
                pointer = None
                for y in range(50, 1200, 100):
                    for x in range(50, 1600, 100):
                        subprocess.run([xdotool, 'mousemove', '--window', windows[0], str(x), str(y)],
                                       env=env, check=True, timeout=5)
                        deadline = time.monotonic()+.04
                        while time.monotonic() < deadline:
                            try:
                                message = messages.get(timeout=max(.001, deadline-time.monotonic()))
                            except queue.Empty:
                                break
                            match = re.fullmatch(r'FILES_NATIVE_POINTER_AT ([\d.]+) ([\d.]+)', message)
                            if match:
                                pointer = (x, y, float(match[1]), float(match[2]))
                                break
                        if pointer:
                            break
                    if pointer:
                        break
                if not pointer:
                    raise RuntimeError('Could not locate the Files window on the private output')
                x, y, local_x, local_y = pointer
                subprocess.run([xdotool, 'mousemove', '--window', windows[0],
                                str(round(x+252-local_x)), str(round(y+18-local_y)), 'click', '1'],
                               env=env, check=True, timeout=5)
                wait('FILES_NATIVE_FILTER 0 2 1')
                subprocess.run([xdotool, 'mousemove', '--window', windows[0],
                                str(round(x+324-local_x)), str(round(y+18-local_y)), 'click', '1'],
                               env=env, check=True, timeout=5)
                wait('FILES_NATIVE_CLOSED')
            else:
                print('IME forwarded close shortcut', flush=True)
            if host.wait(timeout=10) != 0:
                raise RuntimeError('Native Files consumer failed')
            trace = ''.join(errors)
            if not re.search(r'preedit_string\(\d+, "\'", "\'"\)', trace):
                raise RuntimeError('Missing separate simple-IME preedit')
            if len(re.findall(r'commit_string\(\d+, "á"\)', trace)) != 1:
                raise RuntimeError('Expected exactly one simple-IME Unicode commit')
            print('Native Files: IME composition and shortcut/commit routing passed')
            return 0
        finally:
            if host.poll() is None:
                host.terminate()
                try:
                    host.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    host.kill();host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
