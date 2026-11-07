"""Keep actual original-model filesystem/lifetime test artifacts in a fresh case."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--executable', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
parser.add_argument('--group', choices=['lifetime'])
args = parser.parse_args()
args.work.mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='case-', dir=args.work))
with (work / 'test.log').open('x') as log:
    run = subprocess.run([str(args.executable), str(work)] + ([args.group] if args.group else []),
                         stdout=log, stderr=subprocess.STDOUT, timeout=25)
(work / 'result.json').write_text(json.dumps(dict(exit=run.returncode,
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    scope='Original Files model on a single worker; actual bounded filesystem and FD lifetimes, not native picker UI or authority adoption'), indent=2) + '\n')
print((work / 'test.log').read_text(), end='')
raise SystemExit(run.returncode)
