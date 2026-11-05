"""Own an Xvfb/Weston seat and run the terminal simple-IME PTY probe."""
import os
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) not in (6, 7) or (len(sys.argv) == 7 and sys.argv[1] != '--inside'):
        return 2
    inside = len(sys.argv) == 7
    args = sys.argv[2:] if inside else sys.argv[1:]
    app, font, shell, xdotool, work = args
    if not inside:
        env = dict(os.environ, NEXAUI_TEST_PRIVATE_X11='1')
        return subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24',
                               sys.executable, __file__, '--inside', *args], env=env,
                              check=False, timeout=90).returncode
    if os.environ.get('NEXAUI_TEST_PRIVATE_X11') != '1' or not os.environ.get('DISPLAY'):
        return 77
    setxkbmap = os.environ.get('NEXAUI_SETXKBMAP', 'setxkbmap')
    subprocess.run([setxkbmap, '-layout', 'us', '-option', '', '-option', 'compose:ralt'],
                   check=True, timeout=10)
    env = dict(os.environ, NEXAUI_WESTON_INPUT_METHOD='weston-simple-im',
               NEXAUI_TEST_SIMPLE_IME='1', XKB_DEFAULT_OPTIONS='compose:ralt')
    root = Path(__file__).resolve().parents[2]
    command = [sys.executable, str(root/'tests/run_weston_native.py'), '--work-dir', work,
               '--', sys.executable, str(Path(__file__).with_name('run_native_ime_pty.py')),
               app, font, shell, xdotool]
    return subprocess.run(command, env=env, check=False, timeout=75).returncode


if __name__ == '__main__':
    raise SystemExit(main())
