"""Original Root observations joined to original verified canonical manifests."""
import argparse
from contextlib import closing
import hashlib,json,os,sqlite3,subprocess,sys,tempfile
from pathlib import Path
from components_fixture import seed
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','model','test','font','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.work.mkdir(exist_ok=True);root=Path(tempfile.mkdtemp(prefix='run-',dir=a.work));manifest=seed(a.source,root)
prepared=subprocess.run([sys.executable,str(Path(__file__).with_name('run_model.py')),'--source',str(a.source),'--test',str(a.model),'--work',str(root/'catalogue'),'--prepare-only','--fixture-manifest',str(manifest)],capture_output=True,text=True,timeout=45)
(root/'prepare.log').write_text(prepared.stdout+prepared.stderr);assert prepared.returncode==0,prepared.stderr
fixture=Path(next(x.removeprefix('PREPARED ') for x in prepared.stdout.splitlines() if x.startswith('PREPARED ')));paths=[Path(v)/'catalogue.sqlite3' for v in json.loads(fixture.read_text()).values()]
sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'));import catalogue_contracts as cc
from test_contracts import rows
reports=[]
for version in range(2,11):
    with closing(sqlite3.connect(paths[0])) as db:
        if version>2:
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,'system')
    if version not in (2,10):continue
    digests={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths};before=[]
    for path in paths:
        with closing(sqlite3.connect(path)) as db:before.append(rows(db))
    images=root/f'images-v{version}';images.mkdir();command=[str(a.test.resolve()),str(paths[0].parent),str(root),str(a.font.resolve())]
    done=subprocess.run(command,capture_output=True,text=True,timeout=90,env=dict(os.environ,SWG_CAPTURE_DIR=str(images)))
    log=root/f'components-v{version}.log';log.write_text(done.stdout+done.stderr);reports.append({'schema':version,'command':command,'exit':done.returncode,'log':log.name,'images':images.name});(root/'commands.json').write_text(json.dumps(reports,indent=2)+'\n')
    assert done.returncode==0,(version,done.returncode,done.stdout,done.stderr)
    assert 'SOFTWARE_COMPONENTS_ORIGINAL_IDENTITIES_OWNED_DOCUMENTS_FULL_DECLARED_DEPENDENCIES_WORKER_VIEW_OK' in done.stdout
    assert not any(x in done.stderr for x in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    for index,path in enumerate(paths):
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digests[str(path)]
        with closing(sqlite3.connect(path)) as db:assert rows(db)==before[index]
    reports[-1].update(database_bytes_unchanged=True,database_rows_unchanged=True,sha256_before_after=digests)
(root/'result.json').write_text(json.dumps({'cases':reports,'inputs':json.loads((root/'inputs.json').read_text()),'font_sha256':hashlib.sha256(a.font.read_bytes()).hexdigest(),'native_gui_tests':0},indent=2)+'\n')
print('PASS original Root v2/v10 exact manifest join, worker/document lifetimes and dependency view',flush=True)
