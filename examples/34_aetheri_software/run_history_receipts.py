"""Original receipt reads and worker ownership, with unchanged database bytes/rows."""
import argparse,hashlib,json,sqlite3,subprocess,sys,tempfile
from contextlib import closing
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','fixture','model','reader','test','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work));sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'))
from test_contracts import rows
commands=[];cases=[]
def run(name,command):
    command=list(map(str,command));done=subprocess.run(command,capture_output=True,text=True,timeout=180);log=root/(name+'.log');log.write_text(done.stdout+done.stderr)
    commands.append(dict(name=name,command=command,exit=done.returncode,log=str(log)));(root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    assert done.returncode==0,(name,done.stdout,done.stderr)
    assert not any(s in done.stderr for s in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    return done
for mode in ('history','replacement','unsupported','withdrawn','unit','unit-replacement','custody','custody-replacement','corrupt'):
    prepared=run(mode+'-prepare',[sys.executable,Path(__file__).with_name('run_history.py'),'--source',a.source,'--fixture',a.fixture,'--model',a.model,'--test',a.reader,'--work',root/(mode+'-fixture'),'--prepare-only',mode])
    path=Path(next(line.removeprefix('PREPARED ') for line in prepared.stdout.splitlines() if line.startswith('PREPARED ')));data=json.loads(path.read_text());databases=[Path(data[k])/'catalogue.sqlite3' for k in ('system','user')];before=[hashlib.sha256(d.read_bytes()).hexdigest() for d in databases];before_rows=[]
    for d in databases:
        with closing(sqlite3.connect(d)) as db:before_rows.append(rows(db))
    done=run(mode+'-receipts',[a.test,data['system'],data['profile'],mode]);assert 'ORIGINAL_RESERVATION_RUN_RECEIPTS_PROFILE_CHAIN_SCOPE_ERRORS_WORKER_OWNED_READONLY_OK' in done.stdout
    for i,d in enumerate(databases):
        assert hashlib.sha256(d.read_bytes()).hexdigest()==before[i]
        with closing(sqlite3.connect(d)) as db:assert rows(db)==before_rows[i]
    cases.append(dict(mode=mode,fixture=str(path),database_bytes_unchanged=True,database_rows_unchanged=True,sha256_before_after=before))
(root/'result.json').write_text(json.dumps(dict(cases=cases,commands=commands,scope='Original synthetic metadata-only fixtures and original read APIs. Reservation/request binding, original-runtime-only Root-local receipt chain, no live process, current lease/drain/permission or recovery authority.'),indent=2)+'\n')
print('PASS original reservation and lifetime receipts, profile/chain binding, missing and replaced owners, unchanged bytes/rows, errors after prefix and worker ownership')
