"""Private original Root app observations, not installed-software enumeration."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

p=argparse.ArgumentParser(description=__doc__)
for name in ('source','test','work'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--prepare-only',action='store_true',help='Create private metadata fixtures without running model observations')
a=p.parse_args();sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'))
import catalogue_contracts as cc
from test_contracts import rows

a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work));system=root/'system';user=root/'user'
system.mkdir(mode=0o700);user.mkdir(mode=0o700);reports=[]
print('EVIDENCE '+str(root),flush=True)
def run(name,mode):
    command=[str(a.test.resolve()),mode,str(system.resolve()),str(user.resolve())]
    done=subprocess.run(command,capture_output=True,text=True,timeout=45)
    log=root/(name+'.log');log.write_text(done.stdout+done.stderr)
    reports.append({'name':name,'command':command,'exit':done.returncode,'log':log.name})
    (root/'commands.json').write_text(json.dumps(reports,indent=2)+'\n')
    assert done.returncode==0,(name,done.returncode,done.stdout,done.stderr)
    assert not any(x in done.stderr for x in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    return done
run('bootstrap','create')
# The original test_projection.c uses this metadata-only Root fixture. These
# private SQL writes prepare synthetic eligible/ineligible original API inputs;
# no installation, filesystem payload, launcher, process or permission is created.
with closing(sqlite3.connect(system/'catalogue.sqlite3')) as db:
    db.executescript("""
INSERT INTO scopes VALUES(X'00000000000040008000000000000008','machine',NULL,'active',1);
INSERT INTO components VALUES('aetheri.probe',X'00000000000040008000000000000009','app','Probe',0,1),('aetheri.pending',X'00000000000040008000000000000009','app','Pending',0,1);
INSERT INTO artifacts VALUES(zeroblob(32),'aetheri.probe','test','x86_64-linux-gnu','verified');
INSERT INTO installs(install_id,component_id,scope_id,state) VALUES
(X'0000000000004000800000000000000a','aetheri.probe',X'00000000000040008000000000000002','preparing'),
(X'0000000000004000800000000000000b','aetheri.probe',X'00000000000040008000000000000007','preparing'),
(X'0000000000004000800000000000000c','aetheri.probe',X'00000000000040008000000000000008','preparing'),
(X'0000000000004000800000000000000d','aetheri.pending',X'00000000000040008000000000000002','preparing');
INSERT INTO generations SELECT X'0000000000004000800000000000000e',X'0000000000004000800000000000000a','aetheri.probe',zeroblob(32),'committed',0,1;
INSERT INTO generations SELECT X'0000000000004000800000000000000f',X'0000000000004000800000000000000b','aetheri.probe',zeroblob(32),'committed',0,1;
INSERT INTO generations SELECT X'00000000000040008000000000000010',X'0000000000004000800000000000000c','aetheri.probe',zeroblob(32),'committed',0,1;
UPDATE installs SET state='active',active_generation_id=(SELECT generation_id FROM generations WHERE generations.install_id=installs.install_id),revision=2 WHERE install_id!=X'0000000000004000800000000000000d';
""")
    db.commit()
for path in (system/'catalogue.sqlite3',user/'catalogue.sqlite3'):path.chmod(0o600)
(root/'fixture.json').write_text(json.dumps({'system':str(system.resolve()),'user':str(user.resolve())},indent=2)+'\n')
if a.prepare_only:
    print('PREPARED '+str(root/'fixture.json'),flush=True)
    sys.exit(0)
for version in range(2,11):
    with closing(sqlite3.connect(system/'catalogue.sqlite3')) as db:
        if version>2:
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,'system')
        before=rows(db)
    with closing(sqlite3.connect(user/'catalogue.sqlite3')) as db:user_before=rows(db)
    digests={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in (system/'catalogue.sqlite3',user/'catalogue.sqlite3')}
    result=run(f'root-v{version}','test')
    assert 'ORIGINAL_SELECTED_APP_OBSERVATIONS_READONLY_OK' in result.stdout
    for name,digest in digests.items():assert hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest
    with closing(sqlite3.connect(system/'catalogue.sqlite3')) as db:assert rows(db)==before
    with closing(sqlite3.connect(user/'catalogue.sqlite3')) as db:assert rows(db)==user_before
    reports[-1].update(database_bytes_unchanged=True,database_rows_unchanged=True,sha256_before_after=digests)
run('root-v10-explicit-runtime-replacement','runtime')
assert 'EXPLICIT_ORIGINAL_RUNTIME_REPLACEMENT_OLD_SCOPE_REFUSED_OK' in (root/'root-v10-explicit-runtime-replacement.log').read_text()
reports[-1]['explicit_original_backend_mutation']=True
(root/'result.json').write_text(json.dumps({'cases':reports,'read_only_root_schemas':9,'selected_targets':5,'observed_eligible':2,'not_observed_eligible':3,'runtime_replacements':1},indent=2)+'\n')
print('PASS nine original Root schemas, selected account/machine apps, three unavailable targets and one explicit runtime replacement',flush=True)
