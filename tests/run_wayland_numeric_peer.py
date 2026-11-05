"""Exercise a numeric field with separate wl-clipboard clients on private Weston."""
import os
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) != 5:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        print('Requires the private Weston fixture.')
        return 77
    host_path, xdotool, copy, paste = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    host = subprocess.Popen([host_path], env=env, stdout=subprocess.PIPE, text=True)
    output = queue.Queue()
    threading.Thread(target=lambda: [output.put(line.strip()) for line in host.stdout], daemon=True).start()
    peer = None

    def await_line(expected):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            try:
                line = output.get(timeout=deadline-time.monotonic())
            except queue.Empty:
                break
            if line == expected:
                return
        raise RuntimeError(f'Host did not report {expected}')

    def key(name):
        subprocess.run([xdotool, 'key', name], env=env, check=True, timeout=5)

    def own(value):
        nonlocal peer
        if peer:
            peer.terminate(); peer.wait(timeout=5)
        peer = subprocess.Popen([copy, '--foreground'], env=env, stdin=subprocess.PIPE)
        peer.stdin.write(value.encode()); peer.stdin.close()
        for _ in range(30):
            result = subprocess.run([paste, '--no-newline'], env=env, capture_output=True, timeout=3)
            if result.returncode == 0 and result.stdout == value.encode():
                return
            time.sleep(.05)
        raise RuntimeError('Clipboard peer did not offer the expected bytes')

    def await_copied(value):
        for _ in range(30):
            result = subprocess.run([paste, '--no-newline'], env=env, capture_output=True, timeout=3)
            if result.returncode == 0 and result.stdout == value:
                return
            time.sleep(.05)
        raise RuntimeError('Independent paste client did not receive the copied bytes')

    try:
        await_line('READY')
        with open(env['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows = re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows) != 1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
        time.sleep(1)
        key('Tab'); await_line('STATE 1234 1234 1 1')
        key('ctrl+a'); key('ctrl+c')
        await_copied(b'1234')
        own('bad'); key('ctrl+a'); key('ctrl+v'); await_line('STATE bad 1234 0 1')
        own('42'); key('ctrl+a'); key('ctrl+v'); await_line('STATE 42 1234 1 1')
        key('Return'); await_line('STATE 42 42 1 1')
        key('Escape')
        if host.wait(timeout=8):
            raise RuntimeError('Numeric host failed during close')
        print('Independent Wayland clipboard clients copied, pasted and committed the numeric draft.')
        return 0
    finally:
        if peer and peer.poll() is None:
            peer.terminate(); peer.wait(timeout=5)
        if host.poll() is None:
            host.terminate(); host.wait(timeout=5)


if __name__ == '__main__':
    raise SystemExit(main())
