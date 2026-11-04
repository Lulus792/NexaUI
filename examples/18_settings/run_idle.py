"""Check that an unchanged Settings view waits beyond a periodic one-second tick."""
import os
import re
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    result = subprocess.run([sys.argv[1], sys.argv[2], '--idle-probe'],
                            capture_output=True, text=True, timeout=15, check=False)
    print(result.stdout, end='')
    if result.returncode:
        print(result.stderr, file=sys.stderr, end='')
        return result.returncode
    match = re.search(r'idle elapsed=([\d.]+) cpu=([\d.]+) polls=(\d+) presents=(\d+) long_waits=(\d+)', result.stdout)
    if not match or float(match[1]) < 2.8 or int(match[5]) < 1:
        print('Settings did not enter a longer event-driven idle wait.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
