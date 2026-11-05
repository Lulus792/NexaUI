#!/usr/bin/env python3
"""Check a new shared package against an installed 0.1.0 predecessor on Linux.

Pass two independently installed prefixes. The predecessor used for the loader
check must already export the version/build/revision queries: this exercises the
same-SONAME wrong-version case without assuming older binaries had those APIs.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


SOURCE = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    print('+', *map(str, command), flush=True)
    return subprocess.run(command, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old-prefix', type=Path, required=True)
    parser.add_argument('--new-prefix', type=Path, required=True)
    args = parser.parse_args()
    old, new = args.old_prefix.resolve(), args.new_prefix.resolve()
    with tempfile.TemporaryDirectory(prefix='nexaui-package-compat-') as tmp:
        root = Path(tmp)
        command = ['cmake', '-S', str(SOURCE / 'tests/package_consumer')]
        old_config = run(command + ['-B', str(root / 'old'), '-DCMAKE_PREFIX_PATH=' + str(old)],
                         text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if old_config.returncode == 0 or 'version: 0.1.0' not in old_config.stdout:
            raise RuntimeError('0.1.0 package was not rejected: ' + old_config.stdout)
        print('Older 0.1.0 package rejected at configuration.')
        build = root / 'new'
        run(command + ['-B', str(build), '-DCMAKE_PREFIX_PATH=' + str(new)], check=True)
        run(['cmake', '--build', str(build)], check=True)
        executable = str(build / 'consumer')
        for prefix, expected in ((new, 0), (old, 16)):
            env = os.environ.copy()
            env['LD_LIBRARY_PATH'] = os.pathsep.join(str(prefix / path) for path in ('lib', 'lib64'))
            resolved = run(['ldd', executable], env=env, text=True, check=True,
                           stdout=subprocess.PIPE).stdout
            entries = [line.split('=>', 1)[1].strip().split()[0]
                       for line in resolved.splitlines()
                       if line.strip().startswith('libnexaui.so') and '=>' in line]
            if len(entries) != 1 or not Path(entries[0]).resolve().is_relative_to(prefix):
                raise RuntimeError('Unexpected resolved NexaUI library: ' + resolved)
            result = run([executable, '--expect-wayland-only'], env=env)
            if result.returncode != expected:
                raise RuntimeError(f'{prefix}: expected {expected}, got {result.returncode}')
        print('Current shared library passed; same-SONAME 0.1.0 library was rejected by the version check.')


if __name__ == '__main__':
    main()
