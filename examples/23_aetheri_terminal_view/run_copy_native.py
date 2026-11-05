"""Verify original-model copy with independent Wayland clipboard peers."""
import os
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) != 6:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    app, font, xdotool, copy, paste = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    expected = 'A界e\u0301\nSecond'.encode('utf-8')
    marker = b'independent clipboard marker'
    host = subprocess.Popen([app, font], env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    owner = None
    messages, output, errors = queue.Queue(), [], []
    def collect(stream, target, publish=False):
        for line in stream:
            target.append(line)
            if publish:
                messages.put(line.strip())
    for stream, target, publish in ((host.stdout, output, True), (host.stderr, errors, False)):
        threading.Thread(target=collect, args=(stream, target, publish), daemon=True).start()
    def wait(expected_line):
        deadline = time.monotonic()+12
        while time.monotonic() < deadline:
            try:
                if messages.get(timeout=deadline-time.monotonic()) == expected_line:
                    return
            except queue.Empty:
                break
        raise RuntimeError(f'Missing {expected_line}: {"".join(output)} {"".join(errors)}')
    def read_clipboard(expected_bytes):
        for _ in range(30):
            result = subprocess.run([paste, '--no-newline'], env=env, capture_output=True, timeout=3)
            if result.returncode == 0 and result.stdout == expected_bytes:
                return
            if host.poll() is not None:
                raise RuntimeError('Copy owner exited before clipboard read')
            time.sleep(.05)
        raise RuntimeError('Independent client did not read the expected full selection')
    try:
        wait('NATIVE_COPY_READY')
        with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as stream:
            windows = re.findall(r'x11 output .*?window id (\d+)', stream.read())
        if len(windows) != 1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
        subprocess.run([xdotool, 'key', 'ctrl+shift+a'], env=env, check=True)
        wait('NATIVE_COPY_SELECTION')
        subprocess.run([xdotool, 'key', 'ctrl+shift+c'], env=env, check=True)
        wait('NATIVE_COPY_PUBLISHED'); read_clipboard(expected)
        time.sleep(.8)
        subprocess.run([xdotool, 'key', 'Escape'], env=env, check=True); wait('NATIVE_COPY_CLEARED')
        owner = subprocess.Popen([copy, '--foreground'], env=env, stdin=subprocess.PIPE,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        owner.stdin.write(marker); owner.stdin.close(); read_clipboard(marker)
        time.sleep(.8)
        subprocess.run([xdotool, 'key', 'ctrl+c'], env=env, check=True)
        subprocess.run([xdotool, 'key', 'ctrl+shift+c'], env=env, check=True)
        wait('NATIVE_COPY_EMPTY'); read_clipboard(marker)
        time.sleep(.8)
        subprocess.run([xdotool, 'key', 'Escape'], env=env, check=True)
        if host.wait(timeout=10) != 0:
            raise RuntimeError(f'Consumer failed: {"".join(errors)}')
        print('Independent Wayland copy received full model selection; empty copy preserved peer selection')
        return 0
    finally:
        for process in (host, owner):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()


if __name__ == '__main__':
    raise SystemExit(main())
