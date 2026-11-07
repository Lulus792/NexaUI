"""Private original owner fixtures for actual threaded read/teardown checks."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--test',type=Path,required=True)
p.add_argument('--work',type=Path,required=True)
a=p.parse_args();sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source/'tools'))
import catalogue_contracts as cc
from test_contracts import legacy,rows,plan,accept
from test_recovery_inventory import pending_metadata
a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work));print('EVIDENCE '+str(root),flush=True);results=[]
for kind in ('system','user'):
    directory=root/kind;directory.mkdir(mode=0o700);path=directory/'catalogue.sqlite3'
    with closing(sqlite3.connect(path)) as db,closing(sqlite3.connect(root/(kind+'-backup.sqlite3'))) as backup:
        f=legacy(db,kind);cc.migrate_v1_to_v2(db,kind,backup=backup);pending_metadata(db,f,kind)
        for i in range(35):accept(db,plan(db,f))
        db.commit()
        for version in ((3,4,5,6,7,8,9,10) if kind=='system' else (3,4,7,8)):
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,kind)
        before=rows(db)
    path.chmod(0o600);sha=hashlib.sha256(path.read_bytes()).hexdigest()
    command=[str(a.test.resolve()),str(directory.resolve()),kind,f['epoch'].hex(),f['account'].hex() if kind=='user' else '-']
    done=subprocess.run(command,capture_output=True,text=True,timeout=30);(root/(kind+'.log')).write_text(done.stdout+done.stderr)
    assert done.returncode==0,(kind,done.returncode,done.stdout,done.stderr)
    assert hashlib.sha256(path.read_bytes()).hexdigest()==sha
    with closing(sqlite3.connect(path)) as db:assert rows(db)==before
    results.append({'kind':kind,'command':command,'exit':done.returncode,'sha256_before_after':sha,'rows_unchanged':True})
(root/'result.json').write_text(json.dumps({'cases':results},indent=2)+'\n');print('PASS 2 original owners; threaded reads, late disposal and unchanged database bytes/rows')
