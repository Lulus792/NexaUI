"""Exercise a visible numeric Apply action with native keyboard input."""
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
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test'):
        return 77
    host_path, xdotool, font = sys.argv[1:4]
    ime = len(sys.argv) == 5 and sys.argv[4] in ('--ime', '--preedit')
    preedit = ime and sys.argv[4] == '--preedit'
    if len(sys.argv) == 5 and not ime:
        return 2
    if ime and os.environ.get('NEXAUI_TEST_SIMPLE_IME') != '1':
        return 77
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    if ime:
        env.update(NEXAUI_NUMERIC_POINTER_TRACE='1', WAYLAND_DEBUG='1')
    host = subprocess.Popen([host_path, '--apply-ime' if ime else '--apply', font], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    messages = queue.Queue()
    errors = []
    transcript = []
    def read_output():
        for line in host.stdout:
            transcript.append(line.strip())
            messages.put(line.strip())
    threading.Thread(target=read_output, daemon=True).start()
    error_thread = threading.Thread(target=lambda: errors.extend(host.stderr), daemon=True)
    error_thread.start()

    def wait_for(expected):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            try:
                if messages.get(timeout=deadline-time.monotonic()) == expected:
                    return
            except queue.Empty:
                break
        raise RuntimeError('Missing ' + expected + '; observed: ' + repr(transcript[-25:]))

    def key(name):
        subprocess.run([xdotool, 'key', name], env=env, check=True, timeout=5)

    try:
        wait_for('READY')
        with open(env['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows = re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows) != 1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
        time.sleep(1)
        key('Tab'); wait_for('STATE 1234 1234 1 1')
        key('5'); wait_for('STATE 12345 1234 1 1')
        if preedit:
            key('108'); key('apostrophe'); wait_for('COMPOSITION_ACTIVE 1')
        if ime:
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
                        match = re.fullmatch(r'POINTER ([\d.]+) ([\d.]+)', message)
                        if match:
                            pointer = (x, y, float(match[1]), float(match[2])); break
                    if pointer:
                        break
                if pointer:
                    break
            if not pointer:
                raise RuntimeError('Could not locate numeric surface on private output')
            x, y, local_x, local_y = pointer
            subprocess.run([xdotool, 'mousemove', '--window', windows[0],
                            str(round(x+80-local_x)), str(round(y+88-local_y)), 'click', '1'],
                           env=env, check=True, timeout=5)
            if preedit:
                wait_for('AFTER_CLICK 1 1234 1')
                key('a'); wait_for('STATE 12345á 1234 0 1')
                subprocess.run([xdotool, 'click', '1'], env=env, check=True, timeout=5)
                wait_for('APPLY 0 1234')
            else:
                wait_for('APPLY 1 12345')
        else:
            key('Tab'); wait_for('BUTTON_FOCUS 1')
            key('Return'); wait_for('APPLY 1 12345'); wait_for('STATE 12345 12345 1 0')
            key('shift+Tab'); wait_for('STATE 12345 12345 1 1')
            key('ctrl+a'); key('minus'); wait_for('STATE - 12345 0 1')
            key('Tab'); wait_for('BUTTON_FOCUS 1')
            key('Return'); wait_for('APPLY 0 12345')
        key('Escape')
        if host.wait(timeout=5):
            raise RuntimeError('Numeric host failed during close')
        error_thread.join(timeout=2)
        if ime and not re.search(r'commit_string\(\d+, "5"\)', ''.join(errors)):
            raise RuntimeError('The IME did not commit the expected digit')
        if preedit and (not re.search(r'preedit_string\(\d+, "\'", "\'"\)', ''.join(errors)) or
                        not re.search(r'commit_string\(\d+, "á"\)', ''.join(errors))):
            raise RuntimeError('The expected preedit and subsequent commit were not observed')
        if preedit:
            print('Disabled Apply preserved native IME preedit and focus; invalid draft remained unconfirmed.')
        elif ime:
            print('Native pointer after IME input activated the labeled Apply button.')
        else:
            print('Native keyboard focused and activated the labeled Apply button.')
        return 0
    finally:
        if host.poll() is None:
            host.terminate()
            try:
                host.wait(timeout=5)
            except subprocess.TimeoutExpired:
                host.kill(); host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
