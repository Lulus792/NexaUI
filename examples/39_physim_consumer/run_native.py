"""Run original Physim on a private Wayland window/seat and assistive bus."""
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
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--case', choices=('full', 'ime-cancel', 'ime-replace', 'ime-close'), default='full')
    args = parser.parse_args()
    for key in ('build_dir', 'font', 'registry'):
        setattr(args, key, getattr(args, key).resolve(strict=True))
    args.work_dir = args.work_dir.resolve()
    if args.work_dir.exists():
        parser.error('Use a new private work directory')
    driver = shutil.which('xdotool')
    if not driver:
        parser.error('xdotool is required for the private virtual seat')
    repo = Path(__file__).resolve().parents[2]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_TEST_PRIVATE_X11='1',
               NEXAUI_PHYSIM_FONT=str(args.font), NEXAUI_PHYSIM_CASE=args.case,
               NEXAUI_PHYSIM_XDOTOOL=driver, NEXAUI_PHYSIM_CAPTURE=str(args.work_dir / 'physim-'),
               NEXAUI_PHYSIM_EXPORT=str(args.work_dir / 'physim-camera-export.ppm'),
               NEXAUI_PHYSIM_TRACE=str(args.work_dir / 'physim-native.stderr'))
    if args.case.startswith('ime-'):
        env.update(NEXAUI_WESTON_INPUT_METHOD='weston-simple-im', XKB_DEFAULT_OPTIONS='compose:ralt',
                   NEXAUI_TEST_SIMPLE_IME='1', WAYLAND_DEBUG='1')
    command = ['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24', sys.executable,
               str(repo / 'tests/run_weston_native.py'), '--work-dir', str(args.work_dir), '--timeout', '60',
               '--', sys.executable, str(repo / 'tests/run_atspi_native.py'), str(args.registry),
               str(args.build_dir / 'physim_atspi_peer'), str(args.build_dir / 'physim_native_host')]
    return subprocess.run(command, env=env, timeout=90).returncode


if __name__ == '__main__':
    raise SystemExit(main())
