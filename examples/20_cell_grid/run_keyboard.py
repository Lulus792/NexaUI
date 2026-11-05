"""Drive the independent model through a private Weston virtual keyboard."""
import os
import queue
import re
import subprocess
import sys
import threading


def main():
    if len(sys.argv) != 4:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    app, font, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    host = subprocess.Popen([app, font, '--verify'], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    messages, output, errors = queue.Queue(), [], []

    def collect(stream, destination, publish=False):
        for line in stream:
            destination.append(line)
            if publish:
                messages.put(line.strip())

    threads = [threading.Thread(target=collect, args=(host.stdout, output, True), daemon=True),
               threading.Thread(target=collect, args=(host.stderr, errors), daemon=True)]
    for thread in threads:
        thread.start()

    def wait(expected):
        try:
            while messages.get(timeout=10) != expected:
                pass
        except queue.Empty:
            raise RuntimeError(f'Missing {expected}: {"".join(output)} {"".join(errors)}') from None

    try:
        wait('CELL_GRID_READY')
        log = os.environ.get('NEXAUI_TEST_WESTON_LOG')
        if not log:
            raise RuntimeError('Missing private compositor log')
        with open(log, encoding='utf-8', errors='replace') as file:
            windows = re.findall(r'x11 output .*?window id (\d+)', file.read())
        if len(windows) != 1:
            raise RuntimeError(f'Expected one private Weston output, got {len(windows)}')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
        subprocess.run([xdotool, 'type', '--delay', '50', 'Z'], env=env, check=True)
        wait('CELL_GRID_TEXT 90')
        subprocess.run([xdotool, 'key', 'Right'], env=env, check=True)
        wait('CELL_GRID_CURSOR 1')
        subprocess.run([xdotool, 'key', 'Right'], env=env, check=True)
        wait('CELL_GRID_CURSOR 3')
        if host.wait(timeout=10) != 0:
            raise RuntimeError(f'Host failed: {"".join(output)} {"".join(errors)}')
        print('Native text and cursor input changed the app model and skipped the wide-cell continuation; clean exit.')
        return 0
    finally:
        if host.poll() is None:
            host.terminate()
            try:
                host.wait(timeout=5)
            except subprocess.TimeoutExpired:
                host.kill()
                host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
