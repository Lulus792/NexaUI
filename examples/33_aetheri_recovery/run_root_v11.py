"""Current pinned Root11 original issuer fixtures; no live service/process effect."""
import argparse,hashlib,json,os,sqlite3,subprocess,sys,tempfile
from contextlib import closing
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','fixture','model','test','font','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work));sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source/'tools'))
from test_contracts import rows
commands=[];cases=[]
def run(name,command,env=None):
    command=list(map(str,command));done=subprocess.run(command,capture_output=True,text=True,timeout=180,env=env);log=root/(name+'.log');log.write_text(done.stdout+done.stderr);commands.append(dict(name=name,command=command,exit=done.returncode,log=str(log)));(root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');assert done.returncode==0,(name,done.stdout,done.stderr);assert not any(s in done.stderr for s in ('AddressSanitizer','LeakSanitizer','runtime error:'));return done
# Reuse the existing canonical launch-vector/intent preparation; only its private
# expected input files are reused, never the already-created catalogue handles.
prepared=run('canonical-launch-inputs',[sys.executable,Path(__file__).parent.parent/'34_aetheri_software/run_history.py','--source',a.source,'--fixture',a.fixture,'--model',a.model,'--test',a.model,'--work',root/'canonical','--prepare-only','custody'])
path=Path(next(x.removeprefix('PREPARED ') for x in prepared.stdout.splitlines() if x.startswith('PREPARED ')));expected=path.parent/'expected'
for mode in ('fresh','lost','replacement'):
    case=root/mode;case.mkdir(mode=0o700);system=case/'system';user=case/'user';system.mkdir(mode=0o700);user.mkdir(mode=0o700)
    run(mode+'-original',[a.fixture,system,user,expected,'exec-issuer-'+mode]);databases=[system/'catalogue.sqlite3',user/'catalogue.sqlite3'];before=[hashlib.sha256(d.read_bytes()).hexdigest() for d in databases];before_rows=[]
    for d in databases:
        with closing(sqlite3.connect(d)) as db:before_rows.append(rows(db))
    captures=case/'captures';captures.mkdir();run(mode+'-read-only-view-worker',[a.test,system,a.font],dict(os.environ,RVG_CAPTURE_DIR=str(captures)))
    for i,d in enumerate(databases):
        assert hashlib.sha256(d.read_bytes()).hexdigest()==before[i]
        with closing(sqlite3.connect(d)) as db:assert rows(db)==before_rows[i]
    cases.append(dict(mode=mode,sha256_before_after=before,database_bytes_unchanged=True,database_rows_unchanged=True,captures=[str(p) for p in sorted(captures.glob('*.ppm'))]))
(root/'result.json').write_text(json.dumps(dict(cases=cases,commands=commands,scope='Original metadata-only Root11 fixtures, current pinned reader/model/view/worker. No actual issuer ProcessFD/image/admission/retirement, native OS peer or physical/human/performance acceptance.'),indent=2)+'\n');print('PASS three Root11 original issuer cases, read-only database equality, full kind/ID keys, view and owner worker')
