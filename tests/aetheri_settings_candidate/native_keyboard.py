"""Drive the original Aetheri Settings process on a private Weston seat."""
import os
from pathlib import Path
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
    log = Path(os.environ['NEXAUI_TEST_WESTON_LOG']).read_text(encoding='utf-8', errors='replace')
    windows = re.findall(r'x11 output .*?window id (\d+)', log)
    if len(windows) != 1:
        raise RuntimeError('Expected exactly one private Weston output')
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    process = subprocess.Popen([binary, '--font', font, '--fixture-displays', '--diagnostics'],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
    condition = threading.Condition()
    observed = {'focus': None, 'model': None, 'settled': 0}
    lines = []

    def read_output():
        for line in process.stdout:
            with condition:
                lines.append(line.rstrip())
                match = re.fullmatch(r'FOCUS (\d+)\n', line)
                if match:
                    observed['focus'] = int(match.group(1))
                match = re.fullmatch(r'SETTLED theme=(\d+) german=(\d+) text=(\d+) page=(\d+) [\d.]+ [\d.]+\n', line)
                if match:
                    observed['model'] = tuple(map(int, match.groups()))
                    observed['settled'] += 1
                condition.notify_all()

    threading.Thread(target=read_output, daemon=True).start()

    def wait(predicate, description):
        deadline = time.monotonic() + 12
        with condition:
            while not predicate():
                if process.poll() is not None:
                    raise RuntimeError(f'Process exited during {description}: {lines[-30:]}')
                left = deadline - time.monotonic()
                if left <= 0:
                    raise RuntimeError(f'Timed out during {description}: {observed}, {lines[-30:]}')
                condition.wait(left)

    def send(key):
        subprocess.run([xdotool, 'key', '--delay', '35', key], check=True, timeout=5, env=env)

    def focus(target):
        for _ in range(24):
            with condition:
                if observed['focus'] == target:
                    return
                before = observed['focus']
            send('Tab')
            wait(lambda: observed['focus'] != before, f'focus after Tab from {before}')
        raise RuntimeError(f'Focus {target} was not reachable: {observed}')

    try:
        wait(lambda: observed['settled'] > 0, 'initial Settings frame')
        subprocess.run([xdotool, 'windowfocus', windows[0]], check=True, timeout=5, env=env)
        focus(51)
        send('space')
        wait(lambda: observed['model'] == (0, 0, 100, 1), 'Appearance')
        focus(102)
        send('space')
        wait(lambda: observed['model'] == (2, 0, 100, 1), 'high contrast')
        focus(112)
        send('space')
        wait(lambda: observed['model'] == (2, 0, 200, 1), '200% text')
        focus(52)
        send('space')
        wait(lambda: observed['model'] == (2, 0, 200, 2), 'Language')
        focus(121)
        send('space')
        wait(lambda: observed['model'] == (2, 1, 200, 2), 'German')
        subprocess.run([xdotool, 'key', 'Escape'], check=True, timeout=5, env=env)
        if process.wait(timeout=8) != 0:
            raise RuntimeError(f'Settings process failed: {lines[-30:]}')
        print('Aetheri-owned Settings: native keyboard focus, model changes and close passed')
        return 0
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
