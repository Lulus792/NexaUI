"""Private Weston test with separate wl-copy and a NexaUI review consumer."""
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) not in (6, 7):
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    app, font, xdotool, copy, paste = sys.argv[1:6]
    shell = sys.argv[6] if len(sys.argv) == 7 else None
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    payload = (b"printf 'ok\\n'\nprintf '%s%s\\n' 'CLIP_' 'PTY_42'\n" if shell
               else 'Hello 界\ne\u0301'.encode('utf-8'))
    checksum = 14695981039346656037
    for value in payload:
        checksum = ((checksum ^ value) * 1099511628211) & 0xffffffffffffffff
    with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as stream:
        windows = re.findall(r'x11 output .*?window id (\d+)', stream.read())
    if len(windows) != 1:
        raise RuntimeError('Expected one isolated Weston output')
    for decision in ('cancel_pending', 'cancel', 'accept', 'invalid'):
        offered = payload if decision != 'invalid' else payload + b'\x1b[31m'
        host = subprocess.Popen([app, font] + ([shell] if shell else []), env=env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        owner = None
        messages, output, errors = queue.Queue(), [], []
        def collect(stream, target, publish=False):
            for line in stream:
                target.append(line)
                if publish:
                    messages.put(line.strip())
        for stream, target, publish in ((host.stdout, output, True), (host.stderr, errors, False)):
            threading.Thread(target=collect, args=(stream, target, publish), daemon=True).start()
        def wait(expected):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                try:
                    line = messages.get(timeout=deadline-time.monotonic())
                except queue.Empty:
                    break
                if line == expected:
                    return
                if line.startswith('NATIVE_PASTE_FOCUS_CANCEL'):
                    raise RuntimeError('Consumer lost focus during transfer')
            raise RuntimeError(f'Missing {expected}: {"".join(output)} {"".join(errors)}')
        try:
            wait('NATIVE_PASTE_READY')
            subprocess.run([xdotool, 'windowfocus', windows[0]], env=env, check=True)
            owner = subprocess.Popen([copy, '--foreground'], env=env, stdin=subprocess.PIPE,
                                     stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            owner.stdin.write(offered); owner.stdin.close()
            for _ in range(30):
                result = subprocess.run([paste, '--no-newline'], env=env, capture_output=True, timeout=3)
                if result.returncode == 0 and result.stdout == offered:
                    break
                if owner.poll() is not None:
                    raise RuntimeError('Independent clipboard owner exited')
                time.sleep(.05)
            else:
                raise RuntimeError('Independent clipboard offer not observed')
            time.sleep(.8)
            if decision == 'cancel_pending':
                os.kill(owner.pid, signal.SIGSTOP)
            subprocess.run([xdotool, 'key', 'ctrl+shift+v'], env=env, check=True)
            wait('NATIVE_PASTE_PENDING')
            if decision == 'cancel_pending':
                subprocess.run([xdotool, 'key', 'Escape'], env=env, check=True)
                wait('NATIVE_PASTE_RECEIVE_CANCELLED')
            elif decision == 'invalid':
                wait('NATIVE_PASTE_REJECTED_CONTROL')
                if any(line.startswith('NATIVE_PASTE_ACCEPTED') or line.startswith('NATIVE_PASTE_REVIEW_READY') for line in output):
                    raise RuntimeError('Rejected control content reached review or delivery')
            elif decision == 'cancel':
                wait('NATIVE_PASTE_REVIEW_READY')
                subprocess.run([xdotool, 'key', 'Escape'], env=env, check=True)
                wait('NATIVE_PASTE_CANCELLED')
            else:
                wait('NATIVE_PASTE_REVIEW_READY')
                subprocess.run([xdotool, 'key', 'Tab'], env=env, check=True)
                subprocess.run([xdotool, 'key', 'Return'], env=env, check=True)
                wait(f'NATIVE_PASTE_ACCEPTED {len(payload)} {checksum:016x}')
                if shell:
                    wait('NATIVE_PASTE_PTY_OUTPUT')
            if shell and decision != 'accept':
                wait('NATIVE_PASTE_PTY_UNCHANGED')
            if host.wait(timeout=10) != 0:
                raise RuntimeError(f'Consumer exited with failure: {"".join(errors)}')
            if shell and decision != 'accept' and any(line.startswith('NATIVE_PASTE_PTY_OUTPUT') for line in output):
                raise RuntimeError('Cancelled or rejected paste reached the test PTY')
        finally:
            if owner is not None and owner.poll() is None:
                try:
                    os.kill(owner.pid, signal.SIGCONT)
                except ProcessLookupError:
                    pass
            for process in (host, owner):
                if process is not None and process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait()
    print('Independent Wayland clipboard: pending cancel, review cancel, accept and control rejection passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
