"""Exercise a real Weston simple-IME compose sequence on a private Xvfb seat."""
import ctypes
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if (len(sys.argv) not in (4, 5, 6) or
            (len(sys.argv) == 5 and sys.argv[4] != '--cancel') or
            (len(sys.argv) == 6 and sys.argv[4] not in ('--control-cancel', '--replace'))):
        return 2
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test' or
            os.environ.get('NEXAUI_TEST_SIMPLE_IME') != '1'):
        print('Requires private Weston with simple-IME and compose:ralt on its private Xvfb.')
        return 77
    app, font, xdotool = sys.argv[1:4]
    cancel = len(sys.argv) == 5
    control_cancel = len(sys.argv) == 6 and sys.argv[4] == '--control-cancel'
    replace = len(sys.argv) == 6 and sys.argv[4] == '--replace'
    focus_client = sys.argv[5] if control_cancel or replace else None
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', WAYLAND_DEBUG='1')
    mode = '--verify-ime-replace' if replace else '--verify-ime-control-cancel' if control_cancel else '--verify-ime-focus-cancel' if cancel else '--verify-ime-compose'
    host = subprocess.Popen([app, font, mode], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    x11 = display = None
    peer = 0
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

    def wait_trace(pattern, start=0, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            match = re.search(pattern, ''.join(errors)[start:])
            if match:
                return start + match.end()
            if host.poll() is not None:
                break
            time.sleep(.01)
        raise RuntimeError('Missing Wayland event: ' + pattern)

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
        if replace:
            subprocess.run([focus_client, str(host.pid), 'Display name', '--select'], env=env, check=True)
            wait_trace(r'set_surrounding_text\("Aetheri", (?:4, 1|1, 4)\)')
        # X11 keycode 108 is RAlt on this explicitly configured evdev/Xvfb map.
        for key in ('108', 'apostrophe'):
            subprocess.run([xdotool, 'key', key], env=env, check=True)
            time.sleep(.15)
        wait_for('SETTINGS_IME_PREEDIT')
        if cancel:
            # This second X11 window belongs to the private Xvfb fixture only.
            x11 = ctypes.CDLL('libX11.so.6')
            x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
            x11.XOpenDisplay.restype = ctypes.c_void_p
            x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
            x11.XDefaultRootWindow.restype = ctypes.c_ulong
            x11.XCreateSimpleWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                ctypes.c_int, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_ulong]
            x11.XCreateSimpleWindow.restype = ctypes.c_ulong
            for name in ('XMapWindow', 'XDestroyWindow'):
                getattr(x11, name).argtypes = [ctypes.c_void_p, ctypes.c_ulong]
            x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
            x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
            display = x11.XOpenDisplay(None)
            if not display:
                raise RuntimeError('Cannot open private Xvfb display')
            peer = x11.XCreateSimpleWindow(display, x11.XDefaultRootWindow(display),
                                           0, 0, 32, 32, 0, 0, 0)
            if not peer:
                raise RuntimeError('Cannot create private focus peer')
            x11.XMapWindow(display, peer)
            x11.XSync(display, 0)
            subprocess.run([xdotool, 'windowfocus', str(peer)], env=env, check=True)
            wait_for('SETTINGS_IME_CANCELLED')
            preedit_end = wait_trace(r'preedit_string\(\d+, "\'", "\'"\)')
            leave_end = wait_trace(r'wl_keyboard#\d+\.leave\(', preedit_end)
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
            enter_end = wait_trace(r'wl_keyboard#\d+\.enter\(', leave_end)
            wait_trace(r'zwp_text_input_v1#\d+\.enter\(', enter_end)
            subprocess.run([xdotool, 'key', 'a'], env=env, check=True)
            wait_for('SETTINGS_IME_AFTER_RETURN')
        elif control_cancel:
            preedit_end = wait_trace(r'preedit_string\(\d+, "\'", "\'"\)')
            subprocess.run([focus_client, str(host.pid), 'Reduce motion'], env=env, check=True)
            wait_for('SETTINGS_IME_CANCELLED')
            deactivate_end = wait_trace(r'zwp_text_input_v1#\d+\.deactivate\(', preedit_end)
            if re.search(r'wl_keyboard#\d+\.leave\(', ''.join(errors)[preedit_end:deactivate_end]):
                raise RuntimeError('The window lost keyboard focus during the control focus test')
            subprocess.run([focus_client, str(host.pid), 'Display name'], env=env, check=True)
            wait_for('SETTINGS_IME_REFOCUSED')
            wait_trace(r'zwp_text_input_v1#\d+\.enter\(', deactivate_end)
            subprocess.run([xdotool, 'key', 'a'], env=env, check=True)
            wait_for('SETTINGS_IME_AFTER_RETURN')
        else:
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
        preedit = re.search(r'preedit_string\(\d+, "\'", "\'"\)', trace)
        after = trace[preedit.end():] if preedit else ''
        outcome = (re.search(r'wl_keyboard#\d+\.leave\(', after) and
                   re.search(r'zwp_text_input_v1#\d+\.deactivate\(', after) and
                   re.search(r'commit_string\(\d+, "a"\)', after) and
                   not re.search(r'commit_string\(\d+, "á"\)', after)
                   if cancel else
                   re.search(r'zwp_text_input_v1#\d+\.deactivate\(', after) and
                   re.search(r'commit_string\(\d+, "a"\)', after) and
                   not re.search(r'commit_string\(\d+, "á"\)', after)
                   if control_cancel else re.search(r'commit_string\(\d+, "á"\)', after))
        if host.returncode or 'Validation Error' in trace or 'VUID-' in trace or not preedit or not outcome:
            raise RuntimeError(f'Invalid exit/IME wire trace: {host.returncode}\n{trace}')
        print('Real simple-IME preedit and ' + ('focus cancellation' if cancel or control_cancel else 'Unicode commit') +
              ' reached Settings; model stayed unapplied and host closed.')
        return 0
    except (RuntimeError, subprocess.SubprocessError) as problem:
        print(f'IME integration failed: {problem}\n{"".join(lines)}\n{"".join(errors)}', file=sys.stderr)
        return 1
    finally:
        if display:
            if peer:
                x11.XDestroyWindow(display, peer)
            x11.XCloseDisplay(display)
        if host.poll() is None:
            host.terminate()
            try:
                host.wait(timeout=5)
            except subprocess.TimeoutExpired:
                host.kill()
                host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
