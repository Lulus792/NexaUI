"""Private original-owner SQL fixtures; presentation must leave bytes/rows intact."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--test', type=Path, required=True)
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
sys.dont_write_bytecode = True
sys.path.insert(0, str(a.source / 'tools'))
import catalogue_contracts as cc
from test_contracts import legacy, rows, plan, accept, uid, insert
from test_recovery_inventory import pending_metadata, expected

a.work.mkdir(exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='run-', dir=a.work))
print('EVIDENCE ' + str(root), flush=True)
reports = []
for kind in ('system', 'user'):
    directory = root / kind
    directory.mkdir(mode=0o700)
    path = directory / 'catalogue.sqlite3'
    with closing(sqlite3.connect(path)) as db, closing(sqlite3.connect(root / (kind + '-backup.sqlite3'))) as backup:
        fixture = legacy(db, kind)
        cc.migrate_v1_to_v2(db, kind, backup=backup)
        pending_metadata(db, fixture, kind)
        for i in range(35):
            operation = accept(db, plan(db, fixture))
            if i < 2:
                db.execute('UPDATE operations SET reason_code=?,revision=revision+1 WHERE operation_id=?',
                           (('é' if i == 0 else '🧪') * 64, operation))
                insert(db, 'operation_steps', operation_id=operation, step_no=0, action='stage',
                       subject_kind='storage', subject_ref=uid(), state='planned' if i == 0 else 'uncertain')
        db.commit()
    path.chmod(0o600)
    versions = (2, 3, 4, 5, 6, 7, 8, 9, 10) if kind == 'system' else (2, 3, 4, 7, 8)
    def check_reference(output, version, reference, final):
        lines = output.splitlines()
        header = lines[0].split()
        assert header[0] == 'CONTEXT' and int(header[1]) == version and int(header[3]) == len(reference)
        assert lines[-1] == final
        actual = []
        for line in lines[1:-1]:
            tag, k, key, related, revision, steps, *texts = line.split()
            assert tag == 'ITEM' and len(texts) == 4
            actual.append((int(k), key, related, int(revision), int(steps),
                           *(bytes.fromhex(s).decode() if s != '-' else '' for s in texts)))
        assert actual == reference, (actual, reference)
        return actual
    for version in versions:
        with closing(sqlite3.connect(path)) as db:
            if version > 2:
                migration = {7: cc.migrate_owner_recovery, 9: cc.migrate_units, 10: cc.migrate_unit_custody}.get(version)
                if migration is None:
                    migration = getattr(cc, f'migrate_v{version-1}_to_v{version}')
                migration(db, kind)
            before = rows(db)
            reference = expected(db, kind, version)
        sha = hashlib.sha256(path.read_bytes()).hexdigest()
        command = [str(a.test.resolve()), str(directory.resolve()), kind, fixture['epoch'].hex(),
                   fixture['account'].hex() if kind == 'user' else '-']
        result = subprocess.run(command, capture_output=True, text=True, timeout=20)
        name = f'{kind}-v{version}'
        (root / (name + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
        actual = check_reference(result.stdout, version, reference, 'MODEL_OK')
        assert hashlib.sha256(path.read_bytes()).hexdigest() == sha, name
        with closing(sqlite3.connect(path)) as db:
            assert rows(db) == before, name
        assert not any(f.name.endswith(('-journal', '-wal', '-shm')) for f in directory.iterdir()), name
        reports.append({'case': name, 'command': command, 'exit': result.returncode,
                        'records': len(actual), 'sha256_before_after': sha, 'rows_unchanged': True})
    # Separate, explicit owner mutation, never counted as a view-only fixture.
    command = [str(a.test.resolve()), str(directory.resolve()), kind, fixture['epoch'].hex(),
               fixture['account'].hex() if kind == 'user' else '-', 'runtime']
    result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    (root / (kind + '-runtime.log')).write_text(result.stdout + result.stderr)
    assert result.returncode == 0, (kind, 'runtime', result.stdout, result.stderr)
    with closing(sqlite3.connect(path)) as db:
        reference = expected(db, kind, versions[-1])
        runtime = db.execute('SELECT runtime_incarnation FROM authority_meta').fetchone()[0]
        assert runtime.hex() == '81000000000040008000000000000033'
    check_reference(result.stdout, versions[-1], reference, 'RUNTIME_OK')
    reports.append({'case': kind + '-runtime', 'command': command, 'exit': result.returncode,
                    'explicit_original_backend_mutation': True, 'old_session_rejected': True})
(root / 'result.json').write_text(json.dumps({'cases': reports}, indent=2) + '\n')
print(f'PASS {len(reports)-2} view-only original owner/schema fixtures and 2 explicit backend runtime replacements')
