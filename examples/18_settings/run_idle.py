"""Check idle waits with either an available or deliberately unavailable AT-SPI bus."""
import os
import re
import subprocess
import sys


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ('--available', '--unavailable', '--hidden-unavailable'):
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    hidden = sys.argv[3] == '--hidden-unavailable'
    result = subprocess.run([sys.argv[1], sys.argv[2], '--idle-probe-hidden' if hidden else '--idle-probe'],
                            capture_output=True, text=True, timeout=15, check=False)
    print(result.stdout, end='')
    if result.returncode:
        print(result.stderr, file=sys.stderr, end='')
        return result.returncode
    match = re.search(r'idle elapsed=([\d.]+) cpu=([\d.]+) polls=(\d+) presents=(\d+) long_waits=(\d+) ax_unavailable=(\d+)', result.stdout)
    if not match or float(match[1]) < 2.8 or (not hidden and int(match[4]) < 1):
        print('Settings did not remain idle for the probe interval.', file=sys.stderr)
        return 1
    if sys.argv[3] == '--available' and (int(match[6]) != 0 or int(match[5]) < 1):
        print('Available AT-SPI did not enter a longer event-driven idle wait.', file=sys.stderr)
        return 1
    if sys.argv[3] != '--available' and (int(match[6]) != 1 or not 2 <= int(match[3]) <= 8):
        print('Unavailable AT-SPI did not use the bounded retry wait.', file=sys.stderr)
        return 1
    if hidden:
        attempts = re.search(r' renders=(\d+) suspended=(\d+)', result.stdout)
        if not attempts or int(match[4]) != 0 or not 1 <= int(attempts[1]) == int(attempts[2]) <= 3:
            print('Hidden software window did not suspend between native events.', file=sys.stderr)
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
