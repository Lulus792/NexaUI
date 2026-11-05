"""Exercise the real Settings window with a private Weston/Xvfb virtual seat."""
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
        print('This test requires the private Weston fixture.')
        return 77
    app, font, xdotool = sys.argv[1:4]
    controls = len(sys.argv) == 5 and sys.argv[4] == '--controls'
    minimum = len(sys.argv) == 5 and sys.argv[4] == '--minimum'
    small = minimum or (len(sys.argv) == 5 and sys.argv[4] == '--small')
    clipboard = len(sys.argv) == 5 and sys.argv[4] == '--clipboard'
    close_pending = len(sys.argv) == 5 and sys.argv[4] == '--close-pending'
    if len(sys.argv) == 5 and not (controls or small or clipboard or close_pending):
        return 2
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    if os.environ.get('NEXAUI_TEST_WAYLAND_TRACE') == '1':
        env['WAYLAND_DEBUG'] = '1'
    mode = '--verify-pending-close' if close_pending else '--verify-keyboard-controls' if controls else '--verify-keyboard-minimum' if minimum else '--verify-keyboard-small' if small else '--verify-keyboard'
    host = subprocess.Popen([app, font, mode], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout_lines, stderr_lines = [], []
    output_queue = queue.Queue()
    def collect(stream, lines, publish=False):
        for line in stream:
            lines.append(line)
            if publish:
                output_queue.put(line.strip())
    stdout_thread = threading.Thread(target=collect, args=(host.stdout, stdout_lines, True), daemon=True)
    stderr_thread = threading.Thread(target=collect, args=(host.stderr, stderr_lines), daemon=True)
    stdout_thread.start()
    stderr_thread.start()
    def await_line(expected, timeout):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError(f'Settings did not report {expected}')
            try:
                if output_queue.get(timeout=remaining) == expected:
                    return
            except queue.Empty:
                raise RuntimeError(f'Settings did not report {expected}') from None
    def await_draft(length, timeout=5):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError(f'Settings did not report draft length {length}')
            try:
                line = output_queue.get(timeout=remaining)
            except queue.Empty:
                raise RuntimeError(f'Settings did not report draft length {length}') from None
            if re.search(rf'^KEYBOARD_TRACE .*focus=3 draft_bytes={length}(?:\s|$)', line):
                return
    try:
        await_line('SETTINGS_KEYBOARD_READY', 15)
        log = os.environ.get('NEXAUI_TEST_WESTON_LOG')
        if not log:
            raise RuntimeError('Private compositor log unavailable')
        with open(log, encoding='utf-8', errors='replace') as f:
            windows = re.findall(r'x11 output .*?window id (\d+)', f.read())
        if len(windows) != 1:
            raise RuntimeError(f'Expected one private Weston X11 window, found {len(windows)}')
        subprocess.run([xdotool, 'getwindowgeometry', windows[0]], env=env,
                       capture_output=True, check=True)
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
        # Let the initial focus reach the Wayland client, then let it become idle.
        time.sleep(1.2)
        steps = ([['key', 'Tab'], ['key', 'Tab'], ['key', 'space'], ['key', 'Tab'], ['key', 'space']]
                 if controls else [['key', 'Tab'], ['key', 'ctrl+a'], ['type', '--delay', '40', 'Ada'],
                                   ['key', 'Tab'], ['key', 'space'], ['key', 'Tab'], ['key', 'space']])
        if clipboard:
            for args in [['key', 'Tab'], ['key', 'ctrl+a'], ['type', '--delay', '40', 'Ada']]:
                subprocess.run([xdotool, *args], env=env, check=True)
            await_draft(3)
            for args in [['key', 'ctrl+a'], ['key', 'ctrl+c'], ['key', 'BackSpace']]:
                subprocess.run([xdotool, *args], env=env, check=True)
            await_draft(0)
            subprocess.run([xdotool, 'key', 'ctrl+v'], env=env, check=True)
            await_draft(3)
            steps = [['key', 'Tab'], ['key', 'space'], ['key', 'Tab'], ['key', 'space']]
        focus_ids = iter((3, 4, 5))
        space_count = 0
        for args in steps:
            if small and args == ['key', 'Tab']:
                # Require visibility before key release or another user input.
                subprocess.run([xdotool, 'keydown', 'Tab'], env=env, check=True)
                await_line(f'SETTINGS_FOCUS_VISIBLE {next(focus_ids)}', 5)
                subprocess.run([xdotool, 'keyup', 'Tab'], env=env, check=True)
            else:
                subprocess.run([xdotool, *args], env=env, check=True)
            if minimum and args == ['key', 'space']:
                space_count += 1
                if space_count == 1:
                    for _ in range(10):
                        subprocess.run([xdotool, 'key', 'Next'], env=env, check=True)
                        time.sleep(.1)
                    try:
                        await_line('SETTINGS_STATUS_VISIBLE', 5)
                    except RuntimeError as problem:
                        raise RuntimeError(f'{problem}: {"".join(stdout_lines)}') from problem
            time.sleep(.15)
        try:
            host.wait(timeout=12)
        except subprocess.TimeoutExpired:
            host.terminate()
            host.wait(timeout=5)
            raise RuntimeError(f'Settings did not apply keyboard input: {"".join(stdout_lines)} {"".join(stderr_lines)}')
        stdout_thread.join(timeout=5)
        stderr_thread.join(timeout=5)
        if host.returncode:
            raise RuntimeError(f'Settings exit {host.returncode}: {"".join(stdout_lines)} {"".join(stderr_lines)}')
        if close_pending and not any(line.strip() == 'SETTINGS_PENDING_CLOSED' for line in stdout_lines):
            raise RuntimeError('Pending worker did not close through normal cleanup')
        if small:
            for line in stdout_lines:
                if line.startswith('SETTINGS_FOCUS_VISIBLE '):
                    print(line, end='')
        if close_pending:
            print('Native virtual keyboard started a pending request; host joined the worker and closed cleanly.')
        else:
            print('Native virtual keyboard changed and applied the Settings model; host closed cleanly.')
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
