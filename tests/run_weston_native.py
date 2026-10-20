"""Run native tests against Weston on an existing private X11 display.

Example: xvfb-run -a -s '-screen 0 1600x1200x24' python3
  tests/run_weston_native.py --work-dir out/wayland-run -- ctest ...

Weston's X11 backend supplies a virtual input seat as well as actual Wayland
surfaces. This is a software/compositor integration test, not physical input or
display acceptance. Runtime, socket and configuration are private to this run.
No compositor is downloaded or installed. NEXAUI_WESTON selects an executable;
NEXAUI_WESTON_SHELL and NEXAUI_WESTON_LIBEXEC support a private tool prefix.
"""

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time


def stop_group(process):
    if process is None:
        return
    # Each group was created by this invocation; never signal the caller's group.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    # Include descendants remaining after their group leader has exited.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def positive_seconds(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError('must be a positive finite duration')
    return number


def run(args, work):
    report = dict(status='setup_failed', command=args.command,
                  scope='Private Weston/Xvfb Wayland integration; virtual input, no physical/human acceptance')
    server = child = None
    started = time.monotonic()
    code = 1
    with tempfile.TemporaryDirectory(prefix='nexaui-weston-') as runtime:
        runtime = Path(runtime)
        env = os.environ.copy()
        for key in ('WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'DBUS_SESSION_BUS_ADDRESS',
                    'AT_SPI_BUS_ADDRESS', 'NO_AT_BRIDGE'):
            env.pop(key, None)
        env.update(XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(runtime/'config'),
                   XDG_DATA_HOME=str(runtime/'data'), XDG_CACHE_HOME=str(runtime/'cache'))
        config = work/'weston.ini'
        text = '[shell]\nlocking=false\npanel-position=none\n'
        libexec = env.get('NEXAUI_WESTON_LIBEXEC')
        if libexec:
            directory = Path(libexec).resolve()
            for name in ('weston-desktop-shell', 'weston-keyboard'):
                if not (directory/name).is_file():
                    raise RuntimeError('Missing private Weston helper: '+str(directory/name))
            text += 'client='+str(directory/'weston-desktop-shell')+'\n'
            text += '[input-method]\npath='+str(directory/'weston-keyboard')+'\n'
        config.write_text(text, encoding='utf-8')
        command = [args.weston, '--backend=x11-backend.so', '--use-pixman',
                   '--shell='+env.get('NEXAUI_WESTON_SHELL', 'desktop-shell.so'),
                   '--width=1600', '--height=1200', '--idle-time=0',
                   '--socket=nexaui-test', '--config='+str(config)]
        report.update(weston_command=command, display=env.get('DISPLAY'))
        with (work/'weston.stdout').open('wb') as output, (work/'weston.stderr').open('wb') as error:
            try:
                if not env.get('DISPLAY'):
                    raise RuntimeError('DISPLAY is missing; run under xvfb-run')
                server = subprocess.Popen(command, env=env, stdout=output, stderr=error,
                                          start_new_session=True)
                report['weston_pid'] = server.pid
                deadline = time.monotonic()+args.startup_timeout
                address = str(runtime/'nexaui-test')
                while True:
                    if server.poll() is not None:
                        raise RuntimeError('Weston exited during startup: '+str(server.returncode))
                    try:
                        with socket.socket(socket.AF_UNIX) as probe:
                            probe.settimeout(.1)
                            probe.connect(address)
                        break
                    except (FileNotFoundError, ConnectionRefusedError, socket.timeout):
                        if time.monotonic() >= deadline:
                            raise RuntimeError('Weston startup timed out')
                        time.sleep(.02)
                childenv = dict(env, WAYLAND_DISPLAY='nexaui-test')
                child = subprocess.Popen(args.command, env=childenv, start_new_session=True)
                report.update(status='running', child_pid=child.pid,
                              startup_seconds=time.monotonic()-started)
                (work/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
                deadline = time.monotonic()+args.timeout
                while child.poll() is None:
                    if server.poll() is not None:
                        raise RuntimeError('Weston exited while the test was running: '+str(server.returncode))
                    if time.monotonic() >= deadline:
                        report['status'] = 'test_timeout'
                        raise RuntimeError('Test command timed out')
                    time.sleep(.02)
                report['child_exit'] = child.returncode
                if server.poll() is not None:
                    raise RuntimeError('Weston exited before test completion was observed')
                code = child.returncode if child.returncode >= 0 else 128-child.returncode
                report['status'] = 'passed' if code == 0 else 'test_failed'
            except (OSError, RuntimeError) as problem:
                report['error'] = str(problem)
                if report['status'] == 'running':
                    report['status'] = 'infrastructure_failed'
                print('Private Wayland fixture: '+str(problem), file=sys.stderr)
            finally:
                stop_group(child)
                stop_group(server)
                report.update(child_exit=child.returncode if child else None,
                              weston_exit=server.returncode if server else None,
                              elapsed_seconds=time.monotonic()-started, exit_code=code)
                (work/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if code:
        print((work/'weston.stderr').read_text(encoding='utf-8', errors='replace')[-16000:], file=sys.stderr)
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, help='new directory retaining compositor logs and exit metadata')
    parser.add_argument('--weston', default=os.environ.get('NEXAUI_WESTON', 'weston'))
    parser.add_argument('--startup-timeout', type=positive_seconds, default=10)
    parser.add_argument('--timeout', type=positive_seconds, default=600)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ['--']:
        args.command = args.command[1:]
    if not args.command:
        parser.error('a test command is required after --')
    if sys.platform != 'linux':
        parser.error('this fixture requires Linux')
    executable = shutil.which(args.weston)
    if not executable:
        parser.error('Weston is unavailable; install a test dependency or set NEXAUI_WESTON')
    args.weston = executable
    if args.work_dir:
        args.work_dir = args.work_dir.resolve()
        args.work_dir.mkdir(parents=True, exist_ok=False)
        return run(args, args.work_dir)
    with tempfile.TemporaryDirectory(prefix='nexaui-weston-logs-') as work:
        return run(args, Path(work))


if __name__ == '__main__':
    raise SystemExit(main())
