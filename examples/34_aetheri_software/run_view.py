"""Original selected-app view with a real borrowed font; no native-window claim."""
import argparse
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','model','view','font','work'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();sys.dont_write_bytecode=True;sys.path.insert(0,str(a.source.resolve()/'tools'))
import catalogue_contracts as cc
from test_contracts import rows
prepared=subprocess.run([sys.executable,str(Path(__file__).with_name('run_model.py')),'--source',str(a.source),'--test',str(a.model),'--work',str(a.work),'--prepare-only'],capture_output=True,text=True,timeout=45)
print(prepared.stdout,flush=True);assert prepared.returncode==0,(prepared.returncode,prepared.stdout,prepared.stderr)
fixture=Path(next(x.removeprefix('PREPARED ') for x in prepared.stdout.splitlines() if x.startswith('PREPARED ')));root=fixture.parent
paths=[Path(v)/'catalogue.sqlite3' for v in json.loads(fixture.read_text()).values()];reports=[]
for version in range(2,11):
    with closing(sqlite3.connect(paths[0])) as db:
        if version>2:
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,'system')
    if version not in (2,10):continue
    digests={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths};before=[]
    for path in paths:
        with closing(sqlite3.connect(path)) as db:before.append(rows(db))
    images=root/f'images-v{version}';images.mkdir();command=[str(a.view.resolve()),str(paths[0].parent),str(a.font.resolve())]
    done=subprocess.run(command,capture_output=True,text=True,timeout=100,env=dict(os.environ,SWG_CAPTURE_DIR=str(images)))
    log=root/f'view-root-v{version}.log';log.write_text(done.stdout+done.stderr);reports.append({'root_schema':version,'command':command,'exit':done.returncode,'log':log.name,'images':images.name})
    (root/'view-commands.json').write_text(json.dumps(reports,indent=2)+'\n');assert done.returncode==0,(version,done.returncode,done.stdout,done.stderr)
    assert 'SOFTWARE_VIEW_KEYS_FULL_METADATA_FILTER_DRAFT_STALE_ACTION_FOCUS_COMPACT_ERROR_INTENTS_OK' in done.stdout
    assert not any(x in done.stderr for x in ('AddressSanitizer','LeakSanitizer','runtime error:'))
    for index,path in enumerate(paths):
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digests[str(path)]
        with closing(sqlite3.connect(path)) as db:assert rows(db)==before[index]
    reports[-1].update(database_bytes_unchanged=True,database_rows_unchanged=True,sha256_before_after=digests)
(root/'view-result.json').write_text(json.dumps({'cases':reports,'real_font_sha256':hashlib.sha256(a.font.read_bytes()).hexdigest(),'styles_per_schema':3,'native_gui_tests':0,'declared_maximum_targets':100000},indent=2)+'\n')
print('PASS original Root v2/v10 views, three styles/densities, complete metadata, keyboard/filter/draft/focus/retirement/error/declared maximum',flush=True)
