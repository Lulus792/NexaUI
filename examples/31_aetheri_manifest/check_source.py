"""Check the explicitly selected original snapshot, never its working checkout."""
import hashlib
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
record = json.loads(Path(__file__).with_name('source-inputs.json').read_text())
for name, expected in record['files'].items():
    actual = hashlib.sha256((source / name).read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    if actual != expected:
        raise SystemExit('Original source mismatch: ' + name)
print('Original Aetheri ' + record['revision'] + ': ' + str(len(record['files'])) + ' inputs match')
