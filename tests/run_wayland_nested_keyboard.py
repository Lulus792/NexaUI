"""Run a nested-dialog keyboard workflow on a private Weston seat."""
import os
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) not in (4, 5):
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        print('Requires the private Weston fixture.')
        return 77
    host_path, xdotool, font = sys.argv[1:4]
    atspi_probe = sys.argv[4] if len(sys.argv)==5 else None
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    if atspi_probe:
        env['NEXAUI_NESTED_ATSPI']='1'
    host = subprocess.Popen([host_path, font], env=env, stdout=subprocess.PIPE, text=True)
    messages = queue.Queue()
    threading.Thread(target=lambda: [messages.put(line.strip()) for line in host.stdout], daemon=True).start()

    def wait_for(expected):
        deadline = time.monotonic()+8
        while time.monotonic()<deadline:
            try:
                if messages.get(timeout=deadline-time.monotonic()) == expected:
                    return
            except queue.Empty:
                break
        raise RuntimeError('Missing ' + expected)

    def key(name):
        subprocess.run([xdotool, 'key', name], env=env, check=True, timeout=5)

    def inspect(stage):
        if atspi_probe:
            subprocess.run([atspi_probe, str(host.pid), stage], env=env, check=True, timeout=10)

    try:
        wait_for('READY')
        with open(env['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows = re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows)!=1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
        time.sleep(1)
        key('Tab');wait_for('STATE 0 2 0')
        key('Return');wait_for('STATE 1 12 0');inspect('dialog')
        key('Return');wait_for('STATE 2 22 0');inspect('menu')
        key('Tab');wait_for('STATE 2 23 0')
        key('Return');wait_for('STATE 2 23 1')
        key('Escape');wait_for('STATE 1 12 1');inspect('dialog')
        key('Escape');wait_for('DONE')
        if host.wait(timeout=5):
            raise RuntimeError('Host failed during cleanup')
        print('Native keyboard opened dialog and menu, activated an item and restored both focus levels.')
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
