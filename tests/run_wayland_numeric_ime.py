"""Exercise numeric IME composition and observe simple-IME shortcut handling."""
import os
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) != 3:
        return 2
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test' or
            os.environ.get('NEXAUI_TEST_SIMPLE_IME') != '1'):
        print('Requires private Weston with simple-IME and compose:ralt.')
        return 77
    host_path, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', WAYLAND_DEBUG='1')
    host = subprocess.Popen([host_path, '--ime'], env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    messages, errors = queue.Queue(), []
    def read_lines(stream, target):
        for line in stream:
            target(line)
    threads = [threading.Thread(target=read_lines, args=(host.stdout, lambda line: messages.put(line.strip())), daemon=True),
               threading.Thread(target=read_lines, args=(host.stderr, errors.append), daemon=True)]
    for thread in threads:
        thread.start()

    def wait_for(expected):
        expected = {expected} if isinstance(expected, str) else set(expected)
        deadline = time.monotonic()+10
        while time.monotonic()<deadline:
            try:
                line = messages.get(timeout=deadline-time.monotonic())
                if line in expected:
                    return line
            except queue.Empty:
                break
        raise RuntimeError('Missing ' + ' or '.join(sorted(expected)))

    def key(value):
        subprocess.run([xdotool, 'key', value], env=env, check=True, timeout=5)

    try:
        wait_for('READY')
        with open(env['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows = re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows)!=1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True, timeout=5)
        time.sleep(1.2)
        key('Tab');wait_for('STATE 1234 1234 1 1')
        key('108');time.sleep(.15);key('apostrophe');wait_for('PREEDIT')
        key('a');wait_for('INVALID')
        key('ctrl+z');shortcut=wait_for(('SHORTCUT_AS_TEXT','UNDONE'))
        if host.wait(timeout=5):
            raise RuntimeError('Numeric host failed during cleanup')
        for thread in threads:
            thread.join(timeout=2)
        trace=''.join(errors)
        if (not re.search(r'preedit_string\(\d+, "\'", "\'"\)',trace) or
                not re.search(r'commit_string\(\d+, "á"\)',trace) or
                (shortcut=='SHORTCUT_AS_TEXT' and not re.search(r'commit_string\(\d+, "z"\)',trace))):
            raise RuntimeError('Expected native IME preedit and corresponding text commits on the wire')
        print('Numeric simple-IME preedit and invalid commit passed; ' +
              ('Ctrl+Z was committed as z, not undo.' if shortcut=='SHORTCUT_AS_TEXT' else 'Ctrl+Z undid the commit.'))
        return 0
    except (RuntimeError, subprocess.SubprocessError) as problem:
        print(f'Numeric IME integration failed: {problem}\n{"".join(errors)}', file=sys.stderr)
        return 1
    finally:
        if host.poll() is None:
            host.terminate()
            try:
                host.wait(timeout=5)
            except subprocess.TimeoutExpired:
                host.kill();host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
