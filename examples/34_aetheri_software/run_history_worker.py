"""Use unchanged original launch fixtures to exercise worker and historical UI."""
import argparse,hashlib,json,os,sqlite3,subprocess,sys,tempfile
from contextlib import closing
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','fixture','model','reader','test','font','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work));sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'))
from test_contracts import rows
commands=[];cases=[]
def run(name,command,env=None):
    done=subprocess.run(list(map(str,command)),capture_output=True,text=True,timeout=180,env=env);log=root/(name+'.log');log.write_text(done.stdout+done.stderr)
    commands.append(dict(name=name,command=list(map(str,command)),exit=done.returncode,log=str(log)));(root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    assert done.returncode==0,(name,done.stdout,done.stderr)
    assert not any(s in done.stderr for s in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    return done
for mode in ('history','replacement','unsupported','corrupt'):
    prepared=run(mode+'-prepare',[sys.executable,Path(__file__).with_name('run_history.py'),'--source',a.source,'--fixture',a.fixture,'--model',a.model,'--test',a.reader,'--work',root/(mode+'-fixture'),'--prepare-only',mode])
    fixture=Path(next(line.removeprefix('PREPARED ') for line in prepared.stdout.splitlines() if line.startswith('PREPARED ')));data=json.loads(fixture.read_text());databases=[Path(data[k])/'catalogue.sqlite3' for k in ('system','user')]
    before=[hashlib.sha256(d.read_bytes()).hexdigest() for d in databases];before_rows=[]
    for d in databases:
        with closing(sqlite3.connect(d)) as db:before_rows.append(rows(db))
    captures=root/(mode+'-captures');captures.mkdir();env=dict(os.environ,SWG_CAPTURE_DIR=str(captures))
    run(mode+'-worker-view',[a.test,data['system'],data['profile'],a.font,mode],env)
    for i,d in enumerate(databases):
        assert hashlib.sha256(d.read_bytes()).hexdigest()==before[i]
        with closing(sqlite3.connect(d)) as db:assert rows(db)==before_rows[i]
    cases.append(dict(mode=mode,fixture=str(fixture),database_bytes_unchanged=True,database_rows_unchanged=True,sha256_before_after=before,captures=[str(p) for p in sorted(captures.glob('*.ppm'))]))
(root/'result.json').write_text(json.dumps(dict(cases=cases,commands=commands,scope='Original synthetic historical enrollment metadata; worker/view checks and explicitly synthetic maximum strings. No current ProcessFD/process/service/resource/ownership or human/physical acceptance.'),indent=2)+'\n')
print('PASS original historical worker/view modes, copied profiles, owned transfer, reserved slots, errors and late stop; databases unchanged')
