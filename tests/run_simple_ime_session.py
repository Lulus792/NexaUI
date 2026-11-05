"""Run and require the Settings simple-IME cases on an owned Xvfb/Weston seat.

Requires Xvfb, xauth, setxkbmap, dbus-run-session and a Weston installation
with weston-simple-im. NEXAUI_SETXKBMAP can select a private setxkbmap binary;
the normal NEXAUI_WESTON, NEXAUI_WESTON_LIBEXEC and NEXAUI_WESTON_SHELL
fixture overrides are also supported. No existing display is reconfigured.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
CASES = {
    'nexaui_settings_wayland_simple_ime',
    'nexaui_settings_wayland_simple_ime_focus_cancel',
    'nexaui_settings_wayland_simple_ime_control_cancel',
    'nexaui_settings_wayland_simple_ime_replace',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    build, work = args.build_dir.resolve(), args.work_dir.resolve()
    if not args.inside:
        if work.exists():
            parser.error('work directory must not already exist')
        env = dict(os.environ, NEXAUI_TEST_PRIVATE_X11='1')
        return subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24',
                               sys.executable, __file__, '--inside', '--build-dir', str(build),
                               '--work-dir', str(work)], env=env, check=False, timeout=60).returncode
    if os.environ.get('NEXAUI_TEST_PRIVATE_X11') != '1' or not os.environ.get('DISPLAY'):
        parser.error('internal invocation requires the owned Xvfb fixture')
    if not os.environ.get('NEXAUI_WESTON_LIBEXEC'):
        parser.error('NEXAUI_WESTON_LIBEXEC must contain weston-simple-im and weston-desktop-shell')
    subprocess.run([os.environ.get('NEXAUI_SETXKBMAP', 'setxkbmap'), '-layout', 'us',
                    '-option', '', '-option', 'compose:ralt'], check=True, timeout=10)
    env = dict(os.environ, NEXAUI_WESTON_INPUT_METHOD='weston-simple-im',
               NEXAUI_TEST_SIMPLE_IME='1', XKB_DEFAULT_OPTIONS='compose:ralt')
    junit = work / 'simple-ime.xml'
    command = [sys.executable, str(ROOT / 'tests/run_weston_native.py'), '--work-dir', str(work),
               '--', 'dbus-run-session', '--', 'ctest', '--test-dir', str(build),
               '-R', '^nexaui_settings_wayland_simple_ime', '--no-tests=error',
               '--output-on-failure', '--output-junit', str(junit)]
    result = subprocess.run(command, env=env, check=False, timeout=55)
    if result.returncode:
        return result.returncode
    cases = {case.get('name'): case for case in ET.parse(junit).iter('testcase')}
    if set(cases) != CASES or any(case.find('skipped') is not None or
                                   case.find('failure') is not None or
                                   case.find('error') is not None for case in cases.values()):
        print('Expected four executed, passing simple-IME cases.', file=sys.stderr)
        return 1
    print('All four simple-IME cases executed and passed.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
