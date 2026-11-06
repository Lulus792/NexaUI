"""Verify 0.1.22 text-field consumer acceptance and 0.1.21 rejection on Linux."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('old-prefix', 'new-prefix', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--shared', action='store_true', help='also reject the old same-SONAME library at runtime')
    args = parser.parse_args()
    old, new = (p.resolve(strict=True) for p in (args.old_prefix, args.new_prefix))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    source = Path(__file__).resolve().parent / 'text_field_version_consumer'
    result = {'status': 'failed', 'old_prefix': str(old), 'new_prefix': str(new), 'shared': args.shared}

    def run(name, command, env=None):
        completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, env=env, timeout=120)
        (root / (name + '.log')).write_text(completed.stdout)
        result[name + '_exit'] = completed.returncode
        return completed

    try:
        old_config = run('old_configure', ['cmake', '-S', str(source), '-B', str(root / 'old'),
                                          '-DCMAKE_PREFIX_PATH=' + str(old)])
        if old_config.returncode == 0 or '0.1.21' not in old_config.stdout or '0.1.22' not in old_config.stdout:
            raise RuntimeError('0.1.21 package was not rejected for the expected version mismatch')
        configure = run('new_configure', ['cmake', '-S', str(source), '-B', str(root / 'new'),
                                          '-DCMAKE_PREFIX_PATH=' + str(new)])
        if configure.returncode:
            raise RuntimeError('0.1.22 package configuration failed')
        if run('new_build', ['cmake', '--build', str(root / 'new')]).returncode:
            raise RuntimeError('installed consumer build failed')
        executable = root / 'new' / 'text_field_version_consumer'
        env = os.environ.copy()
        env.pop('LD_PRELOAD', None)
        env['LD_LIBRARY_PATH'] = os.pathsep.join(str(new / p) for p in ('lib', 'lib64'))
        if args.shared:
            lookup = run('new_library_lookup', ['ldd', str(executable)], env)
            if lookup.returncode or str(new) not in lookup.stdout:
                raise RuntimeError('new shared library was not loaded from the selected prefix')
        positive = run('new_run', [str(executable)], env)
        if positive.returncode or 'Installed 0.1.22 text field entry points resolved' not in positive.stdout:
            raise RuntimeError('installed consumer did not resolve the expected library')
        if args.shared:
            env['LD_LIBRARY_PATH'] = os.pathsep.join(str(p / d) for p in (old, new) for d in ('lib', 'lib64'))
            lookup = run('old_library_lookup', ['ldd', str(executable)], env)
            if lookup.returncode or str(old) not in lookup.stdout:
                raise RuntimeError('old shared library was not selected for the negative control')
            stale = run('old_library_run', [str(executable)], env)
            if stale.returncode == 0:
                raise RuntimeError('old shared library was accepted')
        result['status'] = 'passed'
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
