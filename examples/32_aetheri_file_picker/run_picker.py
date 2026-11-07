"""Real original-model input and explicit Release-active presentation checks."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

p=argparse.ArgumentParser(description=__doc__)
for name in ('executable','font','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.work.mkdir(exist_ok=True)
case=Path(tempfile.mkdtemp(prefix='view-',dir=a.work));root=case/'files';root.mkdir()
(root/'folder').mkdir()
for i in range(40):(root/f'snapshot-{i:04d}.txt').write_text(f'original {i}\n')
with (case/'test.log').open('x') as log:
 r=subprocess.run([str(a.executable),str(a.font),str(root)],stdout=log,stderr=subprocess.STDOUT,timeout=25)
(case/'result.json').write_text(json.dumps(dict(exit=r.returncode),indent=2))
print((case/'test.log').read_text(),end='');raise SystemExit(r.returncode)
