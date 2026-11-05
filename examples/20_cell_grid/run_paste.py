"""Drive a bounded app-owned paste from an independent Wayland clipboard owner."""
import os
import ctypes
import queue
import re
import signal
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
    host = subprocess.Popen([app, font, '--verify-paste'], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    messages, output, errors = queue.Queue(), [], []
    def collect(stream, target, notify=False):
        for line in stream:
            target.append(line)
            if notify:
                messages.put(line.strip())
    for stream, target, notify in ((host.stdout, output, True), (host.stderr, errors, False)):
        threading.Thread(target=collect, args=(stream, target, notify), daemon=True).start()
    owner = None
    x11 = display = None
    peer = 0
    def wait(expected, timeout=10):
        deadline = time.monotonic()+timeout
        while time.monotonic()<deadline:
            try:
                line=messages.get(timeout=deadline-time.monotonic())
            except queue.Empty:
                break
            if line==expected:
                return
            if line.startswith('CELL_PASTE_ERROR'):
                raise RuntimeError(line)
        raise RuntimeError(f'Missing {expected}: {"".join(output)} {"".join(errors)}')
    def no_accept(duration=.4):
        deadline=time.monotonic()+duration
        while time.monotonic()<deadline:
            try:
                line=messages.get(timeout=deadline-time.monotonic())
            except queue.Empty:
                return
            if line.startswith('CELL_PASTE_ACCEPTED') or line.startswith('CELL_PASTE_ERROR'):
                raise RuntimeError('Unexpected completion after cancellation: '+line)
    try:
        wait('CELL_PASTE_READY')
        with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows=re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows)!=1:
            raise RuntimeError('Expected one private Weston output')
        subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
        payload='Hello 中\ne\u0301\x1b[31m'.encode('utf-8')
        owner=subprocess.Popen([copy, '--foreground'], env=env, stdin=subprocess.PIPE,
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        owner.stdin.write(payload);owner.stdin.close()
        for _ in range(30):
            result=subprocess.run([paste, '--no-newline'], env=env, capture_output=True, timeout=3)
            if result.returncode==0 and result.stdout==payload:
                break
            if owner.poll() is not None:
                raise RuntimeError('Clipboard owner exited')
            time.sleep(.05)
        else:
            raise RuntimeError('Independent clipboard offer did not become readable')
        # wl-paste closes its temporary surface; let focus and the selection
        # offer return to the host without forcing a second X11 focus cycle.
        time.sleep(.8)
        subprocess.run([xdotool, 'key', 'ctrl+v'], env=env, check=True)
        time.sleep(.3)
        if any(line.startswith('CELL_PASTE_ACCEPTED') for line in output):
            raise RuntimeError('Ctrl+V unexpectedly triggered paste')
        checksum=14695981039346656037
        for byte in payload:
            checksum=((checksum^byte)*1099511628211)&0xffffffffffffffff
        subprocess.run([xdotool, 'key', 'ctrl+shift+v'], env=env, check=True)
        wait(f'CELL_PASTE_ACCEPTED {len(payload)} {checksum:016x}')
        # Freeze only our private clipboard owner so its next read cannot finish
        # before an application model change or native focus loss is observed.
        if owner.poll() is not None:
            raise RuntimeError('Clipboard owner exited before cancellation checks')
        os.kill(owner.pid,signal.SIGSTOP)
        subprocess.run([xdotool, 'key', 'ctrl+shift+v'], env=env, check=True)
        wait('CELL_PASTE_PENDING')
        subprocess.run([xdotool, 'key', 'Right'], env=env, check=True)
        wait('CELL_PASTE_CANCELLED')
        os.kill(owner.pid,signal.SIGCONT)
        no_accept()
        os.kill(owner.pid,signal.SIGSTOP)
        subprocess.run([xdotool, 'key', 'ctrl+shift+v'], env=env, check=True)
        wait('CELL_PASTE_PENDING')
        x11=ctypes.CDLL('libX11.so.6')
        x11.XOpenDisplay.argtypes=[ctypes.c_char_p];x11.XOpenDisplay.restype=ctypes.c_void_p
        x11.XDefaultRootWindow.argtypes=[ctypes.c_void_p];x11.XDefaultRootWindow.restype=ctypes.c_ulong
        x11.XCreateSimpleWindow.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,
            ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.c_ulong,ctypes.c_ulong]
        x11.XCreateSimpleWindow.restype=ctypes.c_ulong
        for name in ('XMapWindow','XDestroyWindow'):
            getattr(x11,name).argtypes=[ctypes.c_void_p,ctypes.c_ulong]
        x11.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int];x11.XCloseDisplay.argtypes=[ctypes.c_void_p]
        display=x11.XOpenDisplay(None)
        if not display:
            raise RuntimeError('Cannot open private Xvfb display')
        peer=x11.XCreateSimpleWindow(display,x11.XDefaultRootWindow(display),0,0,32,32,0,0,0)
        if not peer:
            raise RuntimeError('Cannot create private focus peer')
        x11.XMapWindow(display,peer);x11.XSync(display,0)
        subprocess.run([xdotool,'windowfocus',str(peer)],env=env,check=True)
        wait('CELL_PASTE_CANCELLED')
        os.kill(owner.pid,signal.SIGCONT)
        no_accept()
        subprocess.run([xdotool,'windowfocus',windows[0]],env=env,check=True)
        subprocess.run([xdotool, 'key', 'Escape'], env=env, check=True)
        if host.wait(timeout=10)!=0:
            raise RuntimeError(f'Host failed: {"".join(errors)}')
        print('Independent paste reached the app buffer; pending reads were cancelled on model change and focus loss; host closed cleanly.')
        return 0
    finally:
        if display:
            if peer:x11.XDestroyWindow(display,peer)
            x11.XCloseDisplay(display)
        if owner is not None and owner.poll() is None:
            try:
                os.kill(owner.pid,signal.SIGCONT)
            except ProcessLookupError:
                pass
        for process in (host, owner):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill();process.wait()


if __name__=='__main__':
    raise SystemExit(main())
