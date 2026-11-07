"""Use original canonical vectors/encoder, then run the installed C17 consumer."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def seed(source, work):
    spec = importlib.util.spec_from_file_location('original_runtime', source / 'tools/runtime_contracts.py')
    original = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    try:
        sys.dont_write_bytecode = True
        spec.loader.exec_module(original)
    finally:
        sys.dont_write_bytecode = previous
    frozen = json.loads((source / 'spec/vectors/runtime-v1.json').read_text())[0]
    data = bytes.fromhex(frozen['hex'])
    if hashlib.sha256(data).hexdigest() != frozen['sha256'] or original.fixture(frozen['input']) != data:
        raise ValueError('Original canonical vector mismatch')
    large = copy.deepcopy(frozen['input'])
    # The final full path reaches the original 256-byte limit, each segment <=64.
    last = '/'.join(('z' * 64, 'p' * 64, 'q' * 64, 'r' * 61))
    large['files'] = [dict(path=f'entry-{n:02d}', mode=0o444, size=0,
                           sha256=hashlib.sha256(b'').hexdigest()) for n in range(31)]
    large['files'].append(dict(path=last, mode=0o444, size=0, sha256=hashlib.sha256(b'').hexdigest()))
    large['actions'] = []
    records = dict(frozen=data, maximum=original.fixture(large), malformed=b'X' + data[1:],
                   oversize=b'\0' * 131073)
    for name, payload in records.items():
        (work / (name + '.bin')).write_bytes(payload)
        (work / (name + '.sha256')).write_text(hashlib.sha256(payload).hexdigest())
    paths = sorted(item['path'] for item in large['files'])
    (work / 'paths.json').write_text(json.dumps(paths))
    (work / 'paths.txt').write_text('\n'.join(paths) + '\n')
    os.symlink('frozen.bin', work / 'link')
    os.mkfifo(work / 'fifo', 0o600)
    return records, paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--test', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--font', type=Path)
    args = parser.parse_args()
    args.work.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='case-', dir=args.work))
    records, _ = seed(args.source, work)
    run = subprocess.run([str(args.test), str(work)] + ([str(args.font)] if args.font else []),
                         capture_output=True, text=True, timeout=15)
    (work / 'test.log').write_text(run.stdout + run.stderr)
    result = dict(exit=run.returncode, inputs={name: hashlib.sha256(data).hexdigest() for name, data in records.items()},
                  executable_sha256=hashlib.sha256(args.test.read_bytes()).hexdigest())
    (work / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(run.stdout + run.stderr, end='')
    raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
