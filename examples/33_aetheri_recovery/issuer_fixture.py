"""Private native fixture/reference adapter; stored metadata grants no authority."""
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys


def prepare(source, fixture, host, work, directory, mode):
    commands = []

    def run(name, command):
        command = list(map(str, command))
        done = subprocess.run(command, capture_output=True, text=True, timeout=180)
        log = work / (name + '.log')
        log.write_text(done.stdout + done.stderr)
        commands.append(dict(command=command, exit=done.returncode, log=log.name))
        (work / 'issuer-commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if done.returncode or any(s in done.stderr for s in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')):
            raise RuntimeError(name + ': original fixture failed; see ' + str(log))
        return done

    run('issuer-source-check', [sys.executable, Path(__file__).with_name('check_source.py'), source, 'root-v11'])
    # The existing prepare-only workflow creates canonical intent/profile inputs.
    # Its required model/test arguments are not executed in this explicit mode.
    prepared = run('issuer-canonical', [sys.executable, Path(__file__).parent.parent / '34_aetheri_software/run_history.py',
                   '--source', source, '--fixture', fixture, '--model', host, '--test', host,
                   '--work', work / 'canonical', '--prepare-only', 'custody'])
    canonical = Path(next(line.removeprefix('PREPARED ') for line in prepared.stdout.splitlines() if line.startswith('PREPARED ')))
    user = work / 'issuer-user'; user.mkdir(mode=0o700)
    run('issuer-original', [fixture, directory, user, canonical.parent / 'expected', 'exec-issuer-' + mode])
    from test_contracts import rows
    from test_recovery_inventory import expected
    path = directory / 'catalogue.sqlite3'; user_path = user / 'catalogue.sqlite3'
    path.chmod(0o600); user_path.chmod(0o600)
    with closing(sqlite3.connect(path)) as db:
        assert db.execute('PRAGMA user_version').fetchone() == (11,)
        tables = {name: [dict(zip(columns, row)) for row in values] for name, (columns, values) in rows(db).items()}
        meta, = tables['authority_meta']; assert meta['kind'] == 'system'
        reference = expected(db, 'system', 11)
        # Independently walk complete original rows. The older original Python
        # reference stops at kind14, so extend its metadata projection here.
        allocations = {row['allocation_id']: row for row in tables['unit_allocations']}
        for key, row in allocations.items():
            witnessed = row['selection_digest'] is not None
            reference.append((15, key.hex(), row['operation_id'].hex(), 2 if witnessed else 1,
                              0 if witnessed else 1, 'witnessed' if witnessed else 'preparing',
                              'unit', 'allocation', 'unit_custody_unsettled'))
        for row in tables['unit_custodies']:
            key = row['allocation_id']; steps = [s for s in tables['unit_custody_steps'] if s['allocation_id'] == key]
            reference.append((16, key.hex(), allocations[key]['operation_id'].hex(), 1 + len(steps),
                              sum(s['confirmed_unix_ms'] is None for s in steps),
                              'unsettled', 'unit', 'custody', 'connection_custody_unsettled'))
        for row in tables['unit_issuers']:
            key = row['allocation_id']
            reference.append((17, key.hex(), allocations[key]['operation_id'].hex(), 1, 0,
                              'observed', 'unit', 'issuer', 'issuer_retirement_unsettled'))
        reference.sort()
        assert reference[-1] == (17, '0000000000004000800000000000005a', '00000000000040008000000000000029',
                                 1, 0, 'observed', 'unit', 'issuer', 'issuer_retirement_unsettled')
        assert sum(row[0] == 17 for row in reference) == 1
        identity = dict(epoch=meta['authority_epoch'], account=b'')
        runtime = meta['runtime_incarnation'].hex()
    with closing(sqlite3.connect(user_path)) as db:
        user_rows = rows(db)
    return identity, reference, runtime, commands, (user_path, hashlib.sha256(user_path.read_bytes()).hexdigest(), user_rows)
