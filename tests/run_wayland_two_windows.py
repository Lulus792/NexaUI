"""Exercise two Wayland connections with real compositor focus and keyboard input."""
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time


def main():
    if len(sys.argv) != 3:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    host, xdotool = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix='nexaui-two-windows-') as tmp:
        close = Path(tmp) / 'close'
        done = Path(tmp) / 'done'
        env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_W02_CLOSE_FILE=str(close), NEXAUI_W02_DONE_FILE=str(done))
        process = subprocess.Popen([host], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
        messages = queue.Queue()
        output = []
        def collect():
            for line in process.stdout:
                output.append(line.rstrip())
                messages.put(line.rstrip())
        thread = threading.Thread(target=collect, daemon=True)
        thread.start()
        def wait(prefix, timeout=8):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                try:
                    line = messages.get(timeout=max(.01, deadline-time.monotonic()))
                except queue.Empty:
                    break
                if line.startswith(prefix):
                    return line
            detail = process.stderr.read() if process.poll() is not None else 'host still running'
            raise RuntimeError(f'Missing {prefix}; output={output}; {detail}')
        def key(value):
            subprocess.run([xdotool, 'key', value], env=env, check=True, timeout=5)
        try:
            wait('WINDOWS_OPEN')
            log = Path(env['NEXAUI_TEST_WESTON_LOG']).read_text(encoding='utf-8', errors='replace')
            windows = re.findall(r'x11 output .*?window id (\d+)', log)
            if len(windows)!=1:
                raise RuntimeError(f'Expected a single private compositor window: {windows}')
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
            time.sleep(.8)
            key('a')
            first = wait('TEXT ').split()
            if first[2] != '97':
                raise RuntimeError(f'Unexpected initial input: {first}')
            key('super+Tab')
            time.sleep(.5)
            key('b')
            second = wait('TEXT ').split()
            if second[2] != '98' or second[1] == first[1]:
                raise RuntimeError(f'Window focus did not switch: {first}, {second}')
            request = Path(tmp) / 'request'
            request.write_text(second[1], encoding='ascii')
            request.replace(close)
            wait('CLOSED ' + second[1])
            closed_at=output.index('CLOSED ' + second[1])
            wait('SURVIVOR_PRESENTED')
            if 'FOCUS ' + first[1] + ' 1' not in output[closed_at+1:]:
                wait('FOCUS ' + first[1] + ' 1')
            key('c')
            survivor = wait('TEXT ').split()
            if survivor[2] != '99' or survivor[1] != first[1]:
                raise RuntimeError(f'Input reached the wrong survivor: {survivor}')
            done.write_text('done', encoding='ascii')
            wait('WINDOWS_CLOSED')
            if process.wait(timeout=5):
                raise RuntimeError('Host failed: ' + process.stderr.read())
            thread.join(timeout=2)
            texts = [line for line in output if line.startswith('TEXT ')]
            expected = [f'TEXT {first[1]} 97', f'TEXT {second[1]} 98', f'TEXT {first[1]} 99']
            if texts != expected:
                raise RuntimeError(f'Duplicated or misrouted input: {texts}')
            print(f'Independent Wayland windows {first[1]} and {second[1]} received distinct input; {first[1]} survived close.')
            return 0
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            thread.join(timeout=2)


if __name__ == '__main__':
    raise SystemExit(main())
