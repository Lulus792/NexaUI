"""Exercise native management using Openbox inside the private Xvfb fixture."""
import os
import json
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import tempfile
import time


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--child':
        result = subprocess.run([sys.argv[2]], timeout=30, check=False)
        target = Path(sys.argv[3])
        temporary = target.with_suffix('.tmp')
        temporary.write_text(json.dumps({'exit_code': result.returncode}), encoding='utf-8')
        temporary.replace(target)
        return result.returncode
    if len(sys.argv) != 3 or os.environ.get('NEXAUI_TEST_PRIVATE_X11') != '1':
        raise SystemExit('Use run_xvfb_native.py with OPENBOX TEST_EXECUTABLE')
    with tempfile.TemporaryDirectory(prefix='nexaui-openbox-') as temporary:
        root = Path(temporary)
        config = root/'rc.xml'
        config.write_text('<openbox_config xmlns="http://openbox.org/3.4/rc">'
                          '<applications/><desktops><number>1</number></desktops>'
                          '</openbox_config>', encoding='utf-8')
        env = os.environ.copy()
        env.update(XDG_CONFIG_HOME=str(root), XDG_CACHE_HOME=str(root/'cache'))
        result_path = root/'result.json'
        startup = shlex.join([sys.executable, str(Path(__file__).resolve()), '--child',
                              str(Path(sys.argv[2]).resolve()), str(result_path)])
        with (root/'openbox.log').open('wb') as log:
            # Openbox starts this command after entering OB_STATE_RUNNING.
            # Publishing root capabilities earlier is not a client-map barrier.
            wm = subprocess.Popen([sys.argv[1], '--sm-disable', '--debug', '--config-file', str(config), '--startup', startup],
                                  env=env, stdout=log, stderr=log, start_new_session=True)
            try:
                deadline = time.monotonic()+35
                while not result_path.exists() and wm.poll() is None and time.monotonic()<deadline:
                    time.sleep(0.05)
                code = json.loads(result_path.read_text())['exit_code'] if result_path.exists() else 1
                print((root/'openbox.log').read_text(encoding='utf-8', errors='replace'), file=sys.stderr if code else sys.stdout)
                return code if wm.poll() is None else 1
            finally:
                try:
                    os.killpg(wm.pid,signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    wm.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(wm.pid,signal.SIGKILL)
                    wm.wait()


if __name__ == '__main__':
    raise SystemExit(main())
