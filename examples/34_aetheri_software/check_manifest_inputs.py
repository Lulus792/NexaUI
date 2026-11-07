"""Additional original encoder/vector/spec inputs; core is pinned by example33."""
import hashlib,json,sys
from pathlib import Path
source=Path(sys.argv[1]);record=json.loads(Path(__file__).with_name('manifest-inputs.json').read_text())
for name,expected in record['files'].items():
    if hashlib.sha256((source/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=expected:raise SystemExit('Original manifest input mismatch: '+name)
print('Original Aetheri '+record['revision']+': '+str(len(record['files']))+' additional manifest inputs match')
