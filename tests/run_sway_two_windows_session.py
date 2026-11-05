"""Start an isolated headless Sway session and run the two-window close test."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sway', required=True)
    parser.add_argument('--swaymsg', required=True)
    parser.add_argument('--host', required=True)
    parser.add_argument('--ctest-dir', help='Run the registered CTest case from this build')
    parser.add_argument('--poll-many', action='store_true')
    parser.add_argument('--work-dir', required=True, type=Path)
    args=parser.parse_args()
    work=args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    config=work/'sway.conf'
    config.write_text('output * resolution 1280x720\nseat seat0 fallback true\n',encoding='utf-8')
    with tempfile.TemporaryDirectory(prefix='nexaui-sway-w02-') as tmp, (work/'sway.log').open('w') as log:
        runtime=Path(tmp)
        env=dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WLR_BACKENDS='headless',
                 WLR_RENDERER='pixman', WLR_LIBINPUT_NO_DEVICES='1', NEXAUI_TEST_PRIVATE_SWAY='1')
        env.pop('WAYLAND_DISPLAY',None)
        env.pop('SWAYSOCK',None)
        compositor=subprocess.Popen([args.sway,'--config',str(config)],env=env,stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+12
            while time.monotonic()<deadline:
                sockets=list(runtime.glob('sway-ipc.*.sock'))
                displays=[p for p in runtime.glob('wayland-*') if not p.name.endswith('.lock')]
                if len(sockets)==1 and len(displays)==1:
                    break
                if compositor.poll() is not None:
                    raise RuntimeError(f'Sway exited {compositor.returncode}; see {work}/sway.log')
                time.sleep(.05)
            else:
                raise RuntimeError(f'Sway did not create its private sockets; see {work}/sway.log')
            env['SWAYSOCK']=str(sockets[0]);env['WAYLAND_DISPLAY']=displays[0].name
            testname='^nexaui_wayland_two_windows_sway_close_many$' if args.poll_many else '^nexaui_wayland_two_windows_sway_close$'
            command=([ 'ctest', '--test-dir', args.ctest_dir, '-R', testname,
                       '--no-tests=error', '--output-on-failure'] if args.ctest_dir else
                     [sys.executable,str(ROOT/'tests/run_sway_two_windows.py'),args.host,args.swaymsg])
            if args.poll_many and not args.ctest_dir: command.append('--poll-many')
            result=subprocess.run(command,
                                  env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=35)
            (work/'client.log').write_text(result.stdout,encoding='utf-8')
            print(result.stdout,end='')
            return result.returncode
        finally:
            compositor.terminate()
            try:
                compositor.wait(timeout=5)
            except subprocess.TimeoutExpired:
                compositor.kill();compositor.wait()

if __name__=='__main__':
    raise SystemExit(main())
