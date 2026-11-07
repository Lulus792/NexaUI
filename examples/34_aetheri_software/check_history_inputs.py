"""Pin the original historical-process fixture and independent frozen vector."""
import hashlib,json,sys
from pathlib import Path
source=Path(sys.argv[1]);record=json.loads(Path(__file__).with_name('history-inputs.json').read_text())
for name,expected in record['files'].items():
    if hashlib.sha256((source/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=expected:raise SystemExit('Original history input mismatch: '+name)
print('Original history inputs '+record['revision']+': '+str(len(record['files']))+' match')
