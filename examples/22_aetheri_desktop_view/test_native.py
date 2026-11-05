"""Exercise the preview only inside NexaUI's private Weston/Xvfb fixture."""
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) != 4:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    binary, font, xdotool = sys.argv[1:]
    process = subprocess.Popen([binary, '--diagnostics', font], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True, env=dict(os.environ, NEXAUI_PLATFORM='wayland'))
    messages = queue.Queue()
    output = []

    def collect():
        for line in process.stdout:
            output.append(line.rstrip())
            messages.put(line.rstrip())

    threading.Thread(target=collect, daemon=True).start()

    def wait(prefix, seconds=12):
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            try:
                line = messages.get(timeout=max(.01, end-time.monotonic()))
            except queue.Empty:
                break
            if line.startswith(prefix):
                return line
        raise RuntimeError(f'missing {prefix!r}; output={output}')

    try:
        wait('READY')
        log = Path(os.environ['NEXAUI_TEST_WESTON_LOG']).read_text(encoding='utf-8', errors='replace')
        windows = re.findall(r'x11 output .*?window id (\d+)', log)
        if len(windows) != 1:
            raise RuntimeError(f'expected one Weston window, found {windows}')
        subprocess.run([xdotool, 'windowfocus', windows[0]], check=True, timeout=5)
        subprocess.run([xdotool, 'type', '--clearmodifiers', '--delay', '30', 'Terminal'], check=True, timeout=8)
        wait('QUERY Terminal')
        subprocess.run([xdotool, 'key', 'Return'], check=True, timeout=5)
        wait('INTENT kind=2 app=2 window=0')
        subprocess.run([xdotool, 'key', 'Escape'], check=True, timeout=5)
        wait('CLOSED 1')
        return process.wait(timeout=5)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'desktop preview: {error}', file=sys.stderr)
        return 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if process.stderr:
            sys.stderr.write(process.stderr.read())


if __name__ == '__main__':
    raise SystemExit(main())
