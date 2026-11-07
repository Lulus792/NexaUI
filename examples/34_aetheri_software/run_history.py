"""Original historical process getters; synthetic metadata, never process admission."""
import argparse,hashlib,json,os,sqlite3,struct,subprocess,sys,tempfile
from contextlib import closing
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','fixture','model','test','work'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--prepare-only',choices=('history','replacement','unsupported','corrupt'))
a=p.parse_args();a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work))
sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'))
import runtime_contracts as reference
import catalogue_contracts as cc
from test_contracts import rows
def identity(n):return '000000000000400080000000000000'+f'{n:02x}'
vector=json.loads((a.source/'spec/vectors/execution-process-v1.json').read_text());profile=bytes.fromhex(vector['profile_hex']);assert len(profile)==384
(root/'profile.bin').write_bytes(profile)
# Reuse the original launch fixture's canonical reference inputs. Its C harness
# independently checks the frozen profile bytes and diagnostic expectations.
plan=dict(plan=identity(60),authority=identity(50),actor=identity(62),scope=identity(2),kind='launch',component='aetheri.probe',install=identity(10),generation=identity(14),data=identity(12),artifact=None,grant=None,retention='none',action='run',boot=identity(4),expires=2**63-1,
    reads=[dict(epoch=identity(epoch),kind=kind,ref=identity(ref),revision=revision) for epoch,kind,ref,revision in ((1,'gate',10,3),(1,'install',10,2),(50,'data',12,1),(50,'gate',12,3))],effects=[dict(number=0,kind='data',component='aetheri.probe',ref=identity(12),disposition='retain',reason='launch_data_reservation')])
expected=root/'expected';expected.write_bytes(hashlib.sha256(reference.intent(plan)).digest());expected.with_suffix('.execution').write_bytes(profile+hashlib.sha256(profile).digest())
diagnostics=bytearray()
for kind,phase,error,reason in ((1,0,0,'exec_channel_unknown'),(2,8,1,'exec_failure_p8_e1'),(3,0,0,'exec_channel_malformed'),(4,0,5,'exec_observer_e5')):
    raw=b'AETHEXD1'+hashlib.sha256(profile).digest()+bytes.fromhex(identity(66))+bytes([2])*32+struct.pack('>III',kind,phase,error);assert len(raw)==100
    diagnostics+=hashlib.sha256(raw).digest()+reason.encode().ljust(64,b'\0')
expected.with_suffix('.diagnostics').write_bytes(diagnostics);commands=[];cases=[]
def run(name,command):
    done=subprocess.run(list(map(str,command)),capture_output=True,text=True,timeout=45);log=root/(name+'.log');log.write_text(done.stdout+done.stderr)
    commands.append(dict(name=name,command=list(map(str,command)),exit=done.returncode,log=log.name));(root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    assert done.returncode==0,(name,done.stdout,done.stderr)
    assert not any(s in done.stderr for s in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    return done
for mode,scenario in (('history','exec-process-fresh'),('replacement','exec-process-root-replace'),('unsupported',None),('corrupt','exec-process-corrupt')):
    if a.prepare_only and a.prepare_only!=mode:continue
    if scenario:
        case=root/mode;case.mkdir(mode=0o700);system=case/'system';user=case/'user';system.mkdir(mode=0o700);user.mkdir(mode=0o700)
        run(mode+'-original-fixture',[a.fixture,system,user,expected,scenario])
    else:
        prepared=run('unsupported-original-fixture',[sys.executable,Path(__file__).with_name('run_model.py'),'--source',a.source,'--test',a.model,'--work',root/'unsupported','--prepare-only'])
        fixture=Path(next(line.removeprefix('PREPARED ') for line in prepared.stdout.splitlines() if line.startswith('PREPARED ')));paths=json.loads(fixture.read_text());system=Path(paths['system']);user=Path(paths['user'])
    if a.prepare_only:
        prepared=root/'prepared.json';prepared.write_text(json.dumps(dict(system=str(system),user=str(user),profile=str(root/'profile.bin'),profile_hex=profile.hex(),commands=commands,mode=mode),indent=2)+'\n')
        print('PREPARED '+str(prepared));sys.exit(0)
    versions=range(4,11) if mode=='history' else (2,3) if mode=='unsupported' else (4,)
    for version in versions:
        if (mode=='history' and version>4) or (mode=='unsupported' and version>2):
            with closing(sqlite3.connect(system/'catalogue.sqlite3')) as db:
                migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
                (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,'system')
        databases=[system/'catalogue.sqlite3',user/'catalogue.sqlite3'];before=[hashlib.sha256(d.read_bytes()).hexdigest() for d in databases];before_rows=[]
        for d in databases:
            with closing(sqlite3.connect(d)) as db:before_rows.append(rows(db))
        read=run(f'{mode}-v{version}-readonly',[a.test,system,root/'profile.bin',mode]);assert 'HISTORICAL_PROCESS_ORIGINAL_READS_COPY_SCOPE_NO_CURRENT_STATE_OK' in read.stdout
        for i,d in enumerate(databases):
            assert hashlib.sha256(d.read_bytes()).hexdigest()==before[i]
            with closing(sqlite3.connect(d)) as db:assert rows(db)==before_rows[i]
        cases.append(dict(mode=mode,schema=version,databases=[str(d) for d in databases],sha256_before_after=before,database_bytes_unchanged=True,database_rows_unchanged=True))
(root/'result.json').write_text(json.dumps(dict(cases=cases,commands=commands,profile_sha256=hashlib.sha256(profile).hexdigest(),scope='Synthetic historical enrollment metadata through original C APIs; no actual ProcessFD, process/service/image/resource/ownership or authority acceptance.'),indent=2)+'\n')
print('PASS original historical Root v4-v10, owner replacement and unavailable Root v2/v3; exact profile/recipient/row bindings, 64-profile bound, failures and owned lifetime')
