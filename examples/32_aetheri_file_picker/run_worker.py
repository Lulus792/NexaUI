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
parser.add_argument('--save-target', action='store_true')
args = parser.parse_args()
if args.save_target and args.group:
    parser.error('--save-target cannot combine with --group')
args.work.mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='case-', dir=args.work))
with (work / 'test.log').open('x') as log:
    run = subprocess.run([str(args.executable), str(work)] + ([args.group] if args.group else []),
                         stdout=log, stderr=subprocess.STDOUT, timeout=25)
(work / 'result.json').write_text(json.dumps(dict(exit=run.returncode,
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    scope=('Worker-owned Save destination observation; original app controller separately tests write/conflict handoff, not native Save UI or authority adoption'
           if args.save_target else 'Original Files model on a single worker; actual bounded filesystem and FD lifetimes, not native picker UI or authority adoption')), indent=2) + '\n')
print((work / 'test.log').read_text(), end='')
raise SystemExit(run.returncode)
