"""Compare two fixed ICD discovery orders; never treat retention as a leak fix.

Runs the independent strict Vulkan controls in fresh processes for A:B and B:A.
Only each diagnostic child's environment changes; no manifest/system file changes.
Every child result, including LSan failure, propagates to the command's exit code.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', type=Path, required=True)
    parser.add_argument('--first', type=Path, required=True)
    parser.add_argument('--second', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('this diagnostic requires Linux')
    control = args.control.resolve()
    if not control.is_file():
        parser.error('control executable is missing')
    manifests = []
    for path in (args.first, args.second):
        try:
            path = path.resolve(strict=True)
            data = path.read_bytes()
            library = json.loads(data)['ICD']['library_path']
            if not isinstance(library, str) or not library or ':' in str(path):
                raise ValueError('invalid ICD library/path')
        except (OSError, ValueError, KeyError, TypeError) as error:
            parser.error('invalid manifest: ' + str(error))
        manifests.append(dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(), library=library))
    if manifests[0]['path'] == manifests[1]['path'] or manifests[0]['library'] == manifests[1]['library']:
        parser.error('two distinct driver manifests/libraries are required')
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    report = dict(scope='Two fixed discovery orders, no suppression or retention workaround. Failures propagate.',
                  control=str(control), control_sha256=hashlib.sha256(control.read_bytes()).hexdigest(),
                  manifests=manifests, runs=[])
    failed = False
    runner = Path(__file__).with_name('trace_vulkan_modules.py').resolve()
    for name, order in (('first-second', manifests), ('second-first', manifests[::-1])):
        env = os.environ.copy()
        for key in ('VK_ICD_FILENAMES', 'VK_DRIVER_FILES', 'VK_ADD_DRIVER_FILES',
                    'VK_LOADER_DRIVERS_SELECT', 'VK_LOADER_DRIVERS_DISABLE'):
            env.pop(key, None)
        env['VK_DRIVER_FILES'] = ':'.join(item['path'] for item in order)
        command = [sys.executable, str(runner), '--control', str(control), '--work-dir', str(work/name)]
        with (work/(name+'.stdout')).open('wb') as out, (work/(name+'.stderr')).open('wb') as err:
            try:
                result = subprocess.run(command, env=env, stdout=out, stderr=err, timeout=740)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = None
        results = work/name/'results.json'
        try:
            controls = json.loads(results.read_text())['runs']
        except (OSError, ValueError, KeyError, TypeError):
            controls = []
        complete = len(controls) == 4
        entry = dict(name=name, command=command, VK_DRIVER_FILES=env['VK_DRIVER_FILES'],
                     exit_code=code, four_controls_recorded=complete, controls=controls)
        report['runs'].append(entry)
        (work/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps({k: v for k, v in entry.items() if k != 'controls'}), flush=True)
        failed |= code != 0 or not complete
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
