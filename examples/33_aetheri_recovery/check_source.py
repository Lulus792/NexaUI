"""Verify immutable original inputs, not the independent live Aetheri checkout."""
import hashlib
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
profile = sys.argv[2] if len(sys.argv) == 3 else 'legacy-v10'
profiles = {'legacy-v10': 'source-inputs.json', 'root-v11': 'source-inputs-root-v11.json'}
if profile not in profiles:
    raise SystemExit('Unknown original source profile: ' + profile)
record = json.loads(Path(__file__).with_name(profiles[profile]).read_text())
for name, expected in record['files'].items():
    actual = hashlib.sha256((source / name).read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    if actual != expected:
        raise SystemExit('Original source mismatch: ' + name)
print('Original Aetheri ' + record['revision'] + ': ' + str(len(record['files'])) + ' inputs match')
