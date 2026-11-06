"""Run the independent Files AT-SPI client on a private Aetheri compositor.

The caller supplies a test compositor, built Files host, AT-SPI client,
registry and the existing run_atspi_native.py. PATH must contain dbus-daemon
and dbus-send; any private runtime libraries must already be on LD_LIBRARY_PATH.
Only the child compositor and the runner's private bus/registry are controlled.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'host', 'client', 'registry', 'runner', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expect-unavailable', action='store_true',
                        help='expect a mapped host without an AT-SPI provider')
    args = parser.parse_args()
    server, host, client, registry, runner, font = [p.resolve(strict=True) for p in
        (args.server, args.host, args.client, args.registry, args.runner, args.font)]
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-atspi',
               NEXAUI_PLATFORM='wayland', NEXAUI_FILES_FONT=str(font))
    for key in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS', 'DBUS_SESSION_BUS_ADDRESS'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    compositor = test = None
    result = {'status': 'failed', 'server_sha256': digest(server), 'host_sha256': digest(host),
              'client_sha256': digest(client), 'registry_sha256': digest(registry),
              'runner_sha256': digest(runner), 'font_sha256': digest(font),
              'expect_unavailable': args.expect_unavailable,
              'scope': 'Aetheri headless test compositor and independent AT-SPI client; no human screenreader acceptance'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'atspi.log').open('wb') as test_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], '-', str(issuer.fileno())],
                                          env=env, stdin=subprocess.DEVNULL, stdout=server_log,
                                          stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(),))
            issuer.close()
            deadline = time.monotonic() + 15
            while 'READY ' not in (root / 'server.log').read_text(errors='replace'):
                if compositor.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('private compositor did not start')
                time.sleep(.02)
            test = subprocess.Popen([sys.executable, str(runner), str(registry), str(client), str(host)],
                                    env=env, stdin=subprocess.DEVNULL, stdout=test_log,
                                    stderr=subprocess.STDOUT)
            exit_code = test.wait(timeout=50)
            output = (root / 'atspi.log').read_text(errors='replace')
            if args.expect_unavailable:
                if exit_code == 0 or 'first && second && filter && close_button && clear_button' not in output:
                    raise RuntimeError('disabled provider did not fail at Files discovery')
            elif exit_code != 0 or 'Independent AT-SPI Files selection, filtering and close passed' not in output:
                raise RuntimeError('independent AT-SPI client failed; see atspi.log')
            if 'MAP 0\n' not in (root / 'server.log').read_text(errors='replace'):
                raise RuntimeError('Files was not mapped on Aetheri')
            result['status'] = 'passed'
    finally:
        if test and test.poll() is None:
            test.terminate(); test.wait(timeout=5)
        if compositor and compositor.poll() is None:
            compositor.send_signal(signal.SIGTERM); compositor.wait(timeout=10)
        issuer.close(); controller.close()
        result['client_exit'] = test.returncode if test else None
        result['server_exit'] = compositor.returncode if compositor else None
        if (not args.expect_unavailable and result['client_exit'] != 0) or result['server_exit'] != 0:
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
