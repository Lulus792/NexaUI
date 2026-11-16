"""Run the content fixture on its own Xvfb, Weston and AT-SPI bus."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import struct
import sys
import tempfile
import zlib


def write_png_fixtures(directory):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))

    width, height = 112, 52
    colors = ((211, 79, 31, 255), (23, 83, 197, 255), (127, 127, 127, 255),
              (71, 163, 31, 255), (191, 63, 137, 255))
    for kind, color in enumerate(colors):
        image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
        if kind != 2:
            rows = b''.join(b'\0' + b''.join(
                bytes(color if kind != 3 or (56 <= x < 84 and 26 <= y < 39) else colors[1])
                for x in range(width)) for y in range(height))
            image += chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
        (directory / f'{kind}.png').write_bytes(image)
        print(f'PNG_FIXTURE {kind} sha256={hashlib.sha256(image).hexdigest()}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--case', choices=('full', 'png', 'input', 'pending-close', 'rejected-close'), default='full')
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
    if args.case == 'png':
        with tempfile.TemporaryDirectory(prefix='nexaui-content-png-') as directory:
            write_png_fixtures(Path(directory))
            env['NEXAUI_CONTENT_TEST_PNG'] = directory
            return subprocess.run(command, env=env, timeout=90).returncode
    return subprocess.run(command, env=env, timeout=90).returncode


if __name__ == '__main__':
    raise SystemExit(main())
