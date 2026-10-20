"""Check private Wayland fixture isolation, failure propagation and cleanup.

Run under xvfb-run with Weston available. --runner permits a negative-control
copy without changing the production fixture. All case output is retained.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def dead(pid):
    if pid is None:
        return True
    try:
        state = Path('/proc', str(pid), 'stat').read_text().rsplit(')', 1)[1].split()[0]
        return state == 'Z'
    except FileNotFoundError:
        return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--runner', type=Path, default=Path(__file__).with_name('run_weston_native.py'))
    args = parser.parse_args()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    runner = args.runner.resolve()
    summaries = []

    def case(name, script, expected, flags=()):
        folder = work/name
        command = [sys.executable, str(runner), '--work-dir', str(folder), *flags,
                   '--', sys.executable, '-c', script]
        with (work/(name+'.stdout')).open('wb') as out, (work/(name+'.stderr')).open('wb') as err:
            result = subprocess.run(command, stdout=out, stderr=err, timeout=30)
        assert result.returncode == expected, f'{name}: expected exit {expected}, got {result.returncode}'
        report = json.loads((folder/'result.json').read_text())
        assert dead(report.get('weston_pid')) and dead(report.get('child_pid')), report
        summaries.append(dict(name=name, command=command, exit_code=result.returncode, report=report))
        (work/'results.json').write_text(json.dumps(summaries, indent=2)+'\n')
        return report

    snapshot = work/'child-environment.json'
    script = ('import json,os,stat; from pathlib import Path; '
              'r=Path(os.environ["XDG_RUNTIME_DIR"]); '
              'assert stat.S_IMODE(r.stat().st_mode)==0o700; '
              'assert (r/os.environ["WAYLAND_DISPLAY"]).is_socket(); '
              'assert not os.environ.get("WAYLAND_SOCKET"); '
              'assert not os.environ.get("DBUS_SESSION_BUS_ADDRESS"); '
              'assert not os.environ.get("AT_SPI_BUS_ADDRESS"); '
              f'Path({str(snapshot)!r}).write_text(json.dumps(dict(runtime=str(r),display=os.environ["WAYLAND_DISPLAY"],x11=os.environ["DISPLAY"]))); ')
    report = case('success', script, 0)
    observed = json.loads(snapshot.read_text())
    assert report['status'] == 'passed' and observed['display'] == 'nexaui-test'
    assert observed['x11'] == os.environ['DISPLAY'] and not Path(observed['runtime']).exists()

    descendant = work/'descendant.pid'
    script = ('import subprocess,sys; from pathlib import Path; '
              'p=subprocess.Popen([sys.executable,"-c","import time; time.sleep(60)"]); '
              f'Path({str(descendant)!r}).write_text(str(p.pid)); raise SystemExit(13)')
    report = case('failed-child', script, 13)
    assert report['status'] == 'test_failed' and report['child_exit'] == 13
    assert dead(int(descendant.read_text())), 'owned descendant survived fixture cleanup'

    started = work/'timeout.started'
    script = f'from pathlib import Path; import time; Path({str(started)!r}).touch(); time.sleep(60)'
    report = case('timeout', script, 1, ('--timeout', '.2'))
    assert started.exists() and report['status'] == 'test_timeout' and report['child_exit'] != 0

    forbidden = work/'must-not-run'
    script = f'from pathlib import Path; Path({str(forbidden)!r}).touch()'
    report = case('bad-server', script, 1, ('--weston', '/bin/false'))
    assert not forbidden.exists() and report['status'] == 'setup_failed' and report['child_exit'] is None
    print('Private Weston fixture: isolation, exit13, startup/timeout failure and owned process-group cleanup passed.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
