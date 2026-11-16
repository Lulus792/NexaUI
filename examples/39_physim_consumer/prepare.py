"""Export only the pinned original Physim inputs without changing its checkout."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(Path(__file__).with_name('source-inputs.json').read_text())
    source = args.source_repo.resolve(strict=True)
    output = args.output.resolve()
    if output == source or source in output.parents or output in source.parents:
        parser.error('Use a separate new export directory outside the source checkout')
    if output.exists():
        parser.error('Use a new export directory')
    verified = {}
    for entry in manifest['inputs']:
        name = PurePosixPath(entry['path'])
        if name.is_absolute() or any(p in ('.', '..') for p in name.parts) or '\\' in str(name) or ':' in str(name):
            parser.error('Invalid source member')
        data = subprocess.check_output(['git', '-C', str(source), 'show', manifest['revision'] + ':' + str(name)])
        if len(data) > 2 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != entry['sha256']:
            parser.error('Pinned Physim source mismatch: ' + str(name))
        verified[str(name)] = data
    output.mkdir(parents=True)
    for name, data in verified.items():
        p = output / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    print('Exported', len(verified), 'verified original files from', manifest['revision'])


if __name__ == '__main__':
    main()
