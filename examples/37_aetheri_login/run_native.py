"""Exercise the login fixture on its own Xvfb, Weston and AT-SPI session."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--ime', action='store_true')
    parser.add_argument('--cancel-preedit', action='store_true')
    parser.add_argument('--connection', action='store_true')
    parser.add_argument('--connection-loop', action='store_true')
    parser.add_argument('--connection-loss', choices=('eof', 'admission', 'timeout'))
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.cancel_preedit and not args.ime:
        parser.error('--cancel-preedit requires --ime')
    if args.connection_loop:
        args.connection = True
    if args.connection_loss and (not args.connection_loop or not args.ime or args.cancel_preedit):
        parser.error('--connection-loss requires --connection-loop --ime without --cancel-preedit')
    for name in ('build_dir', 'font', 'registry'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    args.work_dir = args.work_dir.resolve()
    if args.work_dir.exists():
        parser.error('Use a new work directory')
    here = Path(__file__).resolve()
    repo = here.parents[2]
    if not args.inside:
        env = dict(os.environ, NEXAUI_TEST_PRIVATE_X11='1')
        command = ['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24',
                   sys.executable, str(here), *sys.argv[1:], '--inside']
        return subprocess.run(command, env=env, timeout=90).returncode
    if os.environ.get('NEXAUI_TEST_PRIVATE_X11') != '1' or not os.environ.get('DISPLAY'):
        parser.error('The inside invocation requires the owned Xvfb fixture')
    xdotool = shutil.which('xdotool')
    if not xdotool:
        parser.error('xdotool is required')
    env = dict(os.environ, NEXAUI_PLATFORM='wayland',
               NEXAUI_LOGIN_TEST_FONT=str(args.font), NEXAUI_LOGIN_TEST_XDOTOOL=xdotool)
    for key in ('NEXAUI_LOGIN_TEST_SIMPLE_IME', 'NEXAUI_LOGIN_TEST_IME_CANCEL',
                'NEXAUI_LOGIN_TEST_CONNECTION', 'NEXAUI_LOGIN_TEST_CONNECTION_LOOP',
                'NEXAUI_LOGIN_TEST_CONNECTION_LOSS', 'NEXAUI_LOGIN_TEST_CONTROL_FD'):
        env.pop(key, None)
    if args.connection:
        env['NEXAUI_LOGIN_TEST_CONNECTION'] = '1'
    if args.connection_loop:
        env['NEXAUI_LOGIN_TEST_CONNECTION_LOOP'] = '1'
    if args.connection_loss:
        env['NEXAUI_LOGIN_TEST_CONNECTION_LOSS'] = args.connection_loss
    if args.ime:
        libexec = env.get('NEXAUI_WESTON_LIBEXEC')
        if not libexec or not (Path(libexec) / 'weston-simple-im').is_file():
            parser.error('NEXAUI_WESTON_LIBEXEC must contain weston-simple-im')
        mapper = env.get('NEXAUI_SETXKBMAP') or shutil.which('setxkbmap')
        if mapper:
            subprocess.run([mapper, '-layout', 'us', '-option', '', '-option', 'compose:ralt'],
                           check=True, timeout=10)
        else:
            compiler = shutil.which('xkbcomp')
            if not compiler:
                parser.error('The private keymap requires setxkbmap or xkbcomp')
            keymap = '''xkb_keymap {
                xkb_keycodes { include "evdev+aliases(qwerty)" };
                xkb_types { include "complete" };
                xkb_compatibility { include "complete" };
                xkb_symbols { include "pc+us+inet(evdev)+compose(ralt)" };
                xkb_geometry { include "pc(pc105)" };
            };'''
            subprocess.run([compiler, '-w', '0', '-', env['DISPLAY']], input=keymap,
                           text=True, check=True, timeout=10)
        env.update(NEXAUI_WESTON_INPUT_METHOD='weston-simple-im',
                   NEXAUI_LOGIN_TEST_SIMPLE_IME='1', XKB_DEFAULT_OPTIONS='compose:ralt')
        if args.cancel_preedit:
            env['NEXAUI_LOGIN_TEST_IME_CANCEL'] = '1'
    command = [sys.executable, str(repo / 'tests/run_weston_native.py'),
               '--work-dir', str(args.work_dir), '--timeout', '60', '--',
               sys.executable, str(repo / 'tests/run_atspi_native.py'), str(args.registry),
               str(args.build_dir / 'aetheri_login_atspi_peer'),
               str(args.build_dir / 'aetheri_login_native_host')]
    return subprocess.run(command, env=env, timeout=80).returncode


if __name__ == '__main__':
    raise SystemExit(main())
