"""Exercise a read-only view of an original Aetheri catalogue in a private folder.

The original intent encoder and frozen manifest vector seed the test-only owner.
No payload installation, publication, activation or privileged service is run.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('aetheri-source', 'executable', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    source, executable, font = (p.resolve(strict=True) for p in
                                (args.aetheri_source, args.executable, args.font))
    work = args.work_dir.resolve()
    work.mkdir(mode=0o700)
    spec = importlib.util.spec_from_file_location('aetheri_runtime_reference', source / 'tools/runtime_contracts.py')
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    frozen = json.loads((source / 'spec/vectors/runtime-v1.json').read_text())[0]

    def identity(last):
        return '000000000000400080000000000000' + f'{last:02x}'

    intent = dict(plan=identity(10), authority=identity(1), actor=identity(12), scope=identity(3),
                  kind='install', component=frozen['input']['component'], install=None, generation=None,
                  data=None, artifact=frozen['sha256'], grant=None, retention='none', action=None,
                  boot=identity(2), expires=8_000_000_000_000_000_000,
                  reads=[dict(epoch=identity(1), kind='scope', ref=identity(3), revision=1)],
                  effects=[dict(number=n, kind=kind, component=frozen['input']['component'],
                                ref=identity(target), disposition='create', reason='fixture_prepare')
                           for n, (kind, target) in enumerate((('install', 14), ('generation', 15), ('storage', 16)))])
    manifest, expected, catalogue = work / 'manifest', work / 'intent-digest', work / 'catalogue'
    manifest.write_bytes(bytes.fromhex(frozen['hex']))
    expected.write_bytes(hashlib.sha256(reference.intent(intent)).digest())
    catalogue.mkdir(mode=0o700)
    command = [str(executable), str(manifest), str(expected), str(catalogue), str(font), str(work)]
    result = {'status': 'failed', 'command': command,
              'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
              'font_sha256': hashlib.sha256(font.read_bytes()).hexdigest(),
              'catalogue_source_sha256': hashlib.sha256((source / 'src/catalogue/store.c').read_bytes()).hexdigest(),
              'scope': 'Original private catalogue metadata and headless UI; no actual installation or native acceptance'}
    try:
        with (work / 'test.log').open('w') as output:
            run = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=30)
        result['exit'] = run.returncode
        if run.returncode:
            raise RuntimeError('observer failed; see ' + str(work / 'test.log'))
        result['images'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(work.glob('*.ppm'))}
        result['status'] = 'passed'
    finally:
        (work / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
