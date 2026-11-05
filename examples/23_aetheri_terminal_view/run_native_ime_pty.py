"""Exercise Weston simple-IME with a disposable Aetheri shell on private Xvfb."""
import ctypes
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
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test' or
            os.environ.get('NEXAUI_TEST_SIMPLE_IME') != '1'):
        return 77
    app, font, shell, xdotool = sys.argv[1:]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', WAYLAND_DEBUG='1')
    host = subprocess.Popen([app, font, shell], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    messages, lines, errors, seen = queue.Queue(), [], [], set()
    x11 = display = None
    peer = 0

    def collect(stream, output, notify=False):
        for line in stream:
            output.append(line)
            if notify:
                messages.put(line.strip())

    threads = [threading.Thread(target=collect, args=(host.stdout, lines, True), daemon=True),
               threading.Thread(target=collect, args=(host.stderr, errors), daemon=True)]
    for thread in threads:
        thread.start()

    def wait_for(expected, timeout=12):
        deadline = time.monotonic() + timeout
        while expected not in seen:
            remaining = deadline-time.monotonic()
            if remaining <= 0:
                raise RuntimeError('Missing '+expected)
            try:
                seen.add(messages.get(timeout=remaining))
            except queue.Empty:
                raise RuntimeError('Missing '+expected) from None

    def wait_trace(pattern, timeout=10, start=0):
        deadline = time.monotonic()+timeout
        while time.monotonic()<deadline:
            if re.search(pattern, ''.join(errors)[start:]):
                return
            if host.poll() is not None:
                break
            time.sleep(.01)
        raise RuntimeError('Missing Wayland event: '+pattern)

    try:
        wait_for('TERMINAL_IME_READY')
        with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as stream:
            windows = re.findall(r'x11 output .*?window id (\d+)', stream.read())
        if len(windows) != 1:
            raise RuntimeError('Expected one private Weston output')
        wait_for('TERMINAL_IME_FOCUSED')
        wait_trace(r'zwp_text_input_v1#\d+\.enter\(')
        if os.environ.get('NEXAUI_TEST_FORCE_FOCUS') == '1':
            start = len(''.join(errors))
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
            deadline = time.monotonic()+10
            while lines.count('TERMINAL_IME_FOCUSED\n') < 2 and time.monotonic()<deadline:
                if host.poll() is not None:
                    break
                time.sleep(.01)
            if lines.count('TERMINAL_IME_FOCUSED\n') < 2:
                raise RuntimeError('No focus transition after refocus request')
            wait_trace(r'zwp_text_input_v1#\d+\.enter\(', start=start)
        for key in ('108', 'apostrophe'):
            subprocess.run([xdotool, 'key', key], env=env, check=True)
        wait_for('TERMINAL_IME_PREEDIT')
        cancel = os.environ.get('NEXAUI_TEST_FOCUS_CANCEL') == '1'
        if cancel:
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
            start = len(''.join(errors))
            subprocess.run([xdotool, 'windowfocus', str(peer)], env=env, check=True)
            wait_for('TERMINAL_IME_CANCELLED')
            wait_trace(r'wl_keyboard#\d+\.leave\(', start=start)
            start = len(''.join(errors))
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
            deadline = time.monotonic()+10
            while lines.count('TERMINAL_IME_FOCUSED\n') < 2 and time.monotonic()<deadline:
                if host.poll() is not None:
                    break
                time.sleep(.01)
            if lines.count('TERMINAL_IME_FOCUSED\n') < 2:
                raise RuntimeError('No focus return after cancellation')
            wait_trace(r'zwp_text_input_v1#\d+\.enter\(', start=start)
        subprocess.run([xdotool, 'key', 'z' if cancel else 'a'], env=env, check=True)
        wait_for('TERMINAL_IME_PTY_COMMITTED')
        wait_for('TERMINAL_IME_CLOSED')
        if host.wait(timeout=15):
            raise RuntimeError('Terminal consumer failed')
        for thread in threads:
            thread.join(timeout=3)
        trace = ''.join(errors)
        if not re.search(r'preedit_string\(\d+, "\'", "\'"\)', trace):
            raise RuntimeError('No simple-IME apostrophe preedit in protocol trace')
        expected = 'z' if cancel else 'á'
        if len(re.findall(r'commit_string\(\d+, "'+expected+r'"\)', trace)) != 1:
            raise RuntimeError('Expected exactly one current IME commit')
        if cancel and re.search(r'commit_string\(\d+, "á"\)', trace):
            raise RuntimeError('Cancelled composition was committed after focus return')
        if not re.search(r'set_cursor_rectangle\(', trace):
            raise RuntimeError('No native candidate rectangle')
        print('Private Weston simple-IME preedit, '+('focus cancellation' if cancel else 'Unicode commit')+
              ' and original PTY output passed')
        return 0
    except (KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'Terminal IME fixture failed: {error}\n{"".join(lines)}\n{"".join(errors)}', file=sys.stderr)
        return 1
    finally:
        if display and x11:
            if peer:
                x11.XDestroyWindow(display, peer)
            x11.XCloseDisplay(display)
        if host.poll() is None:
            host.terminate()
            try:
                host.wait(timeout=30)
            except subprocess.TimeoutExpired:
                host.kill()
                host.wait()


if __name__ == '__main__':
    raise SystemExit(main())
