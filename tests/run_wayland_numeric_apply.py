"""Exercise a visible numeric Apply action with native keyboard input."""
import os
import queue
import re
import signal
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
    ime = len(sys.argv) == 5 and sys.argv[4] in ('--ime', '--preedit', '--batch', '--batch-ax')
    preedit = ime and sys.argv[4] in ('--preedit', '--batch', '--batch-ax')
    batch = ime and sys.argv[4] in ('--batch', '--batch-ax')
    ax = ime and sys.argv[4] == '--batch-ax'
    if len(sys.argv) == 5 and not ime:
        return 2
    if ime and os.environ.get('NEXAUI_TEST_SIMPLE_IME') != '1':
        return 77
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    if ime:
        env.update(NEXAUI_NUMERIC_POINTER_TRACE='1', WAYLAND_DEBUG='1')
    if ax:
        env['NEXAUI_NUMERIC_ATSPI']='1'
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
    probe = None
    probe_messages = queue.Queue()

    def probe_line():
        try:
            return probe_messages.get(timeout=8)
        except queue.Empty:
            raise RuntimeError('AT-SPI probe timed out')

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
        if ax:
            probe = subprocess.Popen([env['NEXAUI_NUMERIC_ATSPI_PROBE'], str(host.pid)], env=env,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
            threading.Thread(target=lambda: [probe_messages.put(line.strip()) for line in probe.stdout],
                             daemon=True).start()
            if probe_line() != 'READY':
                raise RuntimeError('AT-SPI probe did not find Apply')
        with open(env['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows = re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows) != 1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
        time.sleep(1)
        key('Tab'); wait_for('STATE 1234 1234 1 1')
        key('5'); wait_for('STATE 12345 1234 1 1')
        if preedit and not batch:
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
            if batch:
                log_path = env['NEXAUI_TEST_WESTON_LOG']
                deadline = time.monotonic()+5
                while time.monotonic() < deadline:
                    with open(log_path, encoding='utf-8', errors='replace') as log:
                        if 'Surrounding text updated: 12345' in log.read():
                            break
                    time.sleep(.01)
                else:
                    raise RuntimeError('IME did not acknowledge the current numeric draft')
                offset = os.path.getsize(log_path)
                host.send_signal(signal.SIGSTOP)
                key('108'); key('apostrophe')
                deadline = time.monotonic()+5
                while time.monotonic() < deadline:
                    with open(log_path, encoding='utf-8', errors='replace') as log:
                        log.seek(offset)
                        if re.search(r'-> zwp_text_input_v1#\d+\.preedit_string\(\d+, "\'", "\'"\)', log.read()):
                            break
                    time.sleep(.01)
                else:
                    raise RuntimeError('IME did not send preedit before the pointer press')
                if ax:
                    probe.stdin.write('GO\n');probe.stdin.flush()
                    if probe_line() != 'SENT':
                        raise RuntimeError('AT-SPI request was not routed')
            subprocess.run([xdotool, 'mousemove', '--window', windows[0],
                            str(round(x+80-local_x)), str(round(y+88-local_y)), 'click', '1'],
                           env=env, check=True, timeout=5)
            if batch:
                host.send_signal(signal.SIGCONT)
                wait_for('SAME_BATCH')
            if preedit:
                wait_for('AFTER_CLICK 1 1234 1')
                if ax:
                    result=probe_line()
                    if result != 'RESULT 1' or probe_line() != 'DISABLED' or probe.wait(timeout=5):
                        raise RuntimeError('AT-SPI queued action or disabled-state check failed: '+result)
                    if any(line.startswith('APPLY ') for line in transcript):
                        raise RuntimeError('A pending AT-SPI action changed the draft during preedit')
                    print('Pending AT-SPI action reply: '+result)
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
            if batch:
                host.send_signal(signal.SIGCONT)
            host.terminate()
            try:
                host.wait(timeout=5)
            except subprocess.TimeoutExpired:
                host.kill(); host.wait()
        if probe and probe.poll() is None:
            probe.terminate();probe.wait(timeout=5)


if __name__ == '__main__':
    raise SystemExit(main())
