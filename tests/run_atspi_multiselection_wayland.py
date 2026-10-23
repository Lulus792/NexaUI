"""Run multi-selection only inside the compositor owned by run_weston_native."""
import os
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) != 4:
        return 2
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1'
            or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test'):
        print('AT-SPI multi Wayland requires the private Weston fixture.')
        return 77
    env = dict(os.environ, NEXAUI_PLATFORM='wayland')
    return subprocess.run([sys.executable, str(Path(__file__).with_name('run_atspi_native.py')),
                           *sys.argv[1:]], env=env, timeout=45, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
