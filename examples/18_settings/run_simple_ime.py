"""Exercise a real Weston simple-IME compose sequence on a private Xvfb seat."""
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
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test' or
            os.environ.get('NEXAUI_TEST_SIMPLE_IME') != '1'):
        print('Requires private Weston with simple-IME and compose:ralt on its private Xvfb.')
        return 77
    app, font, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', WAYLAND_DEBUG='1')
    host = subprocess.Popen([app, font, '--verify-ime-compose'], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    lines, errors, messages = [], [], queue.Queue()

    def collect(stream, output, notify=False):
        for line in stream:
            output.append(line)
            if notify:
                messages.put(line.strip())

    threads = [threading.Thread(target=collect, args=(host.stdout, lines, True), daemon=True),
               threading.Thread(target=collect, args=(host.stderr, errors), daemon=True)]
    for thread in threads:
        thread.start()

    def wait_for(expected, timeout=10):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError('Missing ' + expected)
            try:
                if messages.get(timeout=remaining) == expected:
                    return
            except queue.Empty:
                raise RuntimeError('Missing ' + expected) from None

    try:
        wait_for('SETTINGS_IME_READY', 15)
        log = os.environ.get('NEXAUI_TEST_WESTON_LOG')
        if not log:
            raise RuntimeError('Private compositor log unavailable')
        with open(log, encoding='utf-8', errors='replace') as file:
            windows = re.findall(r'x11 output .*?window id (\d+)', file.read())
        if len(windows) != 1:
            raise RuntimeError(f'Expected one private Weston output, got {len(windows)}')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
        time.sleep(1.2)
        subprocess.run([xdotool, 'key', 'Tab'], env=env, check=True)
        wait_for('SETTINGS_IME_FOCUSED')
        # X11 keycode 108 is RAlt on this explicitly configured evdev/Xvfb map.
        for key in ('108', 'apostrophe'):
            subprocess.run([xdotool, 'key', key], env=env, check=True)
            time.sleep(.15)
        wait_for('SETTINGS_IME_PREEDIT')
        subprocess.run([xdotool, 'key', 'a'], env=env, check=True)
        wait_for('SETTINGS_IME_COMMITTED')
        wait_for('SETTINGS_IME_CLOSED')
        host.wait(timeout=10)
        for thread in threads:
            thread.join(timeout=3)
        trace = ''.join(errors)
        trace_path = os.environ.get('NEXAUI_TEST_IME_TRACE')
        if trace_path:
            Path(trace_path).write_text(trace, encoding='utf-8')
        if (host.returncode or 'Validation Error' in trace or 'VUID-' in trace or
                not re.search(r'preedit_string\(\d+, "\'", "\'"\)', trace) or
                not re.search(r'commit_string\(\d+, "á"\)', trace)):
            raise RuntimeError(f'Invalid exit/IME wire trace: {host.returncode}\n{trace}')
        print('Real simple-IME preedit and Unicode commit reached Settings; model stayed unapplied and host closed.')
        return 0
    except (RuntimeError, subprocess.SubprocessError) as problem:
        print(f'IME integration failed: {problem}\n{"".join(lines)}\n{"".join(errors)}', file=sys.stderr)
        return 1
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
