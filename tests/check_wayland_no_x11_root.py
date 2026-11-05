#!/usr/bin/env python3
"""Check the Wayland package probe in a prepared, network-isolated Linux root.

The caller prepares and owns ROOT and SOURCE. ROOT must contain bubblewrap's
usual build tools and NexaUI's Wayland development dependencies. No packages
are installed, modified or downloaded by this checker. It screens conventional
X11/XCB headers, libraries and pkg-config files, then mounts ROOT and SOURCE
read-only and runs the existing static/shared installed-package probe offline.
XKB keyboard data is allowed: its name or data path does not imply an X11 link.
This does not prove native compositor, IME, accessibility or hardware behavior.
"""
import argparse
import fnmatch
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    root, source = args.root.resolve(strict=True), args.source.resolve(strict=True)
    if root == Path('/') or not (root / 'usr/bin/python3').exists():
        parser.error('root must be a separate prepared Linux filesystem with Python')
    if any(not (root / name).is_dir() for name in ('source', 'tmp', 'dev', 'proc')):
        parser.error('prepared root must provide /source, /tmp, /dev and /proc mount points')
    if not (source / 'tests/check_wayland_only_package.py').is_file():
        parser.error('source must contain the NexaUI package probe')
    forbidden_dirs = [root / 'usr/include/X11', root / 'usr/include/xcb',
                      root / 'usr/include/xkbcommon/xkbcommon-x11.h']
    patterns = ('libX*.so*', 'libX*.a', 'libxcb*.so*', 'libxcb*.a', 'libxkbcommon-x11.so*',
                'libxkbcommon-x11.a', 'x11.pc', 'xcb.pc', 'xkbcommon-x11.pc')
    forbidden = [str(path) for path in forbidden_dirs if path.exists()]
    for directory in (root / 'lib', root / 'usr/lib', root / 'usr/local/lib'):
        if directory.exists():
            forbidden.extend(str(path) for path in directory.rglob('*')
                             if any(fnmatch.fnmatchcase(path.name, pattern) for pattern in patterns))
    if forbidden:
        print('Forbidden X11/XCB development or runtime files:\n' + '\n'.join(forbidden), file=sys.stderr)
        return 1
    command = ['bwrap', '--ro-bind', str(root), '/', '--ro-bind', str(source), '/source',
               '--tmpfs', '/tmp', '--dev', '/dev', '--proc', '/proc', '--unshare-net',
               '--clearenv', '--setenv', 'PATH', '/usr/sbin:/usr/bin:/sbin:/bin',
               '--setenv', 'HOME', '/tmp', '--chdir', '/source',
               '/usr/bin/python3', '/source/tests/check_wayland_only_package.py']
    print('Checking static and shared installed packages in an offline root without X11/XCB files.', flush=True)
    return subprocess.run(command, check=False, timeout=600).returncode


if __name__ == '__main__':
    raise SystemExit(main())
