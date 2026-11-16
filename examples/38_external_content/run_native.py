"""Run the content fixture on its own Xvfb, Weston and AT-SPI bus."""
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
    parser.add_argument('--case', choices=('full', 'input', 'pending-close', 'rejected-close'), default='full')
    args = parser.parse_args()
    for key in ('build_dir', 'font', 'registry'):
        setattr(args, key, getattr(args, key).resolve(strict=True))
    args.work_dir = args.work_dir.resolve()
    if args.work_dir.exists():
        parser.error('Use a new private work directory')
    xdotool = shutil.which('xdotool')
    if not xdotool:
        parser.error('xdotool is required for the private Xvfb seat')
    repo = Path(__file__).resolve().parents[2]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_TEST_PRIVATE_X11='1',
               NEXAUI_CONTENT_TEST_FONT=str(args.font), NEXAUI_CONTENT_TEST_CASE=args.case,
               NEXAUI_CONTENT_TEST_XDOTOOL=xdotool,
               NEXAUI_CONTENT_CAPTURE_PREFIX=str(args.work_dir / 'content-'))
    command = ['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24', sys.executable,
               str(repo / 'tests/run_weston_native.py'), '--work-dir', str(args.work_dir),
               '--timeout', '60', '--', sys.executable, str(repo / 'tests/run_atspi_native.py'),
               str(args.registry), str(args.build_dir / 'content_atspi_peer'),
               str(args.build_dir / 'content_native_host')]
    return subprocess.run(command, env=env, timeout=90).returncode


if __name__ == '__main__':
    raise SystemExit(main())
