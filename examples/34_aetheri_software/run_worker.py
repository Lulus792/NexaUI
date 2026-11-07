"""Real original Root worker lifecycle; no graphical/native presentation claim."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
for name in ('source','model','worker','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'))
import catalogue_contracts as cc
from test_contracts import rows

prepared=subprocess.run([sys.executable,str(Path(__file__).with_name('run_model.py')),'--source',str(a.source),'--test',str(a.model),'--work',str(a.work),'--prepare-only'],capture_output=True,text=True,timeout=45)
print(prepared.stdout,flush=True)
assert prepared.returncode==0,(prepared.returncode,prepared.stdout,prepared.stderr)
fixture=Path(next(x.removeprefix('PREPARED ') for x in prepared.stdout.splitlines() if x.startswith('PREPARED ')))
root=fixture.parent;paths=[Path(v)/'catalogue.sqlite3' for v in json.loads(fixture.read_text()).values()];reports=[]
for version in range(2,11):
    with closing(sqlite3.connect(paths[0])) as db:
        if version>2:
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,'system')
    digests={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    before=[]
    for path in paths:
        with closing(sqlite3.connect(path)) as db:before.append(rows(db))
    command=[str(a.worker.resolve()),str(paths[0].parent)]
    done=subprocess.run(command,capture_output=True,text=True,timeout=45)
    log=root/f'worker-root-v{version}.log';log.write_text(done.stdout+done.stderr)
    reports.append({'root_schema':version,'command':command,'exit':done.returncode,'log':log.name})
    (root/'worker-commands.json').write_text(json.dumps(reports,indent=2)+'\n')
    assert done.returncode==0,(version,done.returncode,done.stdout,done.stderr)
    assert 'SOFTWARE_WORKER_REAL_SQL_TRANSACTION_UI_RETIREMENT_QUEUED_CANCEL_DRAIN_JOIN_FDS_OK' in done.stdout
    assert not any(x in done.stderr for x in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    for index,path in enumerate(paths):
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digests[str(path)]
        with closing(sqlite3.connect(path)) as db:assert rows(db)==before[index]
    reports[-1].update(database_bytes_unchanged=True,database_rows_unchanged=True,sha256_before_after=digests)
(root/'worker-result.json').write_text(json.dumps({'cases':reports,'read_only_root_schemas':9,'held_original_read_transactions_per_schema':2,'gui_context_retirement':True,'native_window_retirement':False},indent=2)+'\n')
print('PASS nine original Root schemas: copied jobs, held SQL reads, UI retirement, cancel, error/OOM, wake, drain, join and FD balance',flush=True)
