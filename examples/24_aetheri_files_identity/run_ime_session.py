"""Own a private Xvfb/Weston seat and run the Files simple-IME probe."""
import os
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) not in (5, 6) or (len(sys.argv) == 6 and sys.argv[1] != '--inside'):
        return 2
    inside = len(sys.argv) == 6
    args = sys.argv[2:] if inside else sys.argv[1:]
    app, font, xdotool, work = args
    if not inside:
        env = dict(os.environ, NEXAUI_TEST_PRIVATE_X11='1')
        return subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24',
                               sys.executable, __file__, '--inside', *args], env=env,
                              check=False, timeout=120).returncode
    if os.environ.get('NEXAUI_TEST_PRIVATE_X11') != '1' or not os.environ.get('DISPLAY'):
        return 77
    setxkbmap = os.environ.get('NEXAUI_SETXKBMAP', 'setxkbmap')
    subprocess.run([setxkbmap, '-layout', 'us', '-option', '', '-option', 'compose:ralt'],
                   check=True, timeout=10)
    env = dict(os.environ, NEXAUI_WESTON_INPUT_METHOD='weston-simple-im',
               NEXAUI_FILES_IME_TEST='1', XKB_DEFAULT_OPTIONS='compose:ralt', WAYLAND_DEBUG='1')
    root = Path(__file__).resolve().parents[2]
    command = [sys.executable, str(root/'tests/run_weston_native.py'), '--work-dir', work,
               '--', sys.executable, str(Path(__file__).with_name('run_ime_native.py')), app, font, xdotool]
    return subprocess.run(command, env=env, check=False, timeout=100).returncode


if __name__ == '__main__':
    raise SystemExit(main())
