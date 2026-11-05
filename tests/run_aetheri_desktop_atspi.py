"""Run independent Settings AT-SPI against an isolated Aetheri SDL desktop.

The caller supplies a test compositor, built Settings host, AT-SPI client,
registry and the existing run_atspi_native.py. PATH must contain dbus-daemon
and dbus-send; any private runtime libraries must already be on LD_LIBRARY_PATH.
Only the child compositor and the runner's private bus/registry are controlled.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
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
    for name in ('server', 'desktop', 'host', 'client', 'registry', 'runner', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    server, desktop_binary, host, client, registry, runner, font = [p.resolve(strict=True) for p in
        (args.server, args.desktop, args.host, args.client, args.registry, args.runner, args.font)]
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    cache = root / 'cache'; cache.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-atspi',
               NEXAUI_PLATFORM='wayland', NEXAUI_SETTINGS_FONT=str(font),
               LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe', LP_NUM_THREADS='2')
    for key in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS', 'DBUS_SESSION_BUS_ADDRESS',
                'SDL_VIDEO_DRIVER', 'SDL_RENDER_DRIVER'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    shell_server, shell_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    launch_owner, launch_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    compositor = desktop = test = None
    result = {'status': 'failed', 'server_sha256': digest(server), 'host_sha256': digest(host),
              'desktop_sha256': digest(desktop_binary), 'client_sha256': digest(client),
              'registry_sha256': digest(registry),
              'scope': 'Aetheri SDL desktop on headless test compositor with independent AT-SPI; no human acceptance'}
    try:
        with (root / 'server.log').open('wb') as server_log, (root / 'atspi.log').open('wb') as test_log, (root / 'desktop.log').open('wb') as desktop_log:
            compositor = subprocess.Popen([str(server), '--headless', env['WAYLAND_DISPLAY'], '-', str(issuer.fileno()),
                                           '--shell-fd', str(shell_server.fileno())],
                                          env=env, stdin=subprocess.DEVNULL, stdout=server_log,
                                          stderr=subprocess.STDOUT, pass_fds=(issuer.fileno(), shell_server.fileno()))
            issuer.close(); shell_server.close()
            deadline = time.monotonic() + 15
            while 'READY ' not in (root / 'server.log').read_text(errors='replace'):
                if compositor.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('private compositor did not start')
                time.sleep(.02)
            desktop = subprocess.Popen([str(desktop_binary), '--font', str(font), '--cache', str(cache),
                                        '--launcher-fd', str(launch_client.fileno()), '--diagnostics'],
                                       env=dict(env, WAYLAND_SOCKET=str(shell_client.fileno())),
                                       stdin=subprocess.DEVNULL, stdout=desktop_log, stderr=subprocess.STDOUT,
                                       pass_fds=(shell_client.fileno(), launch_client.fileno()))
            shell_client.close(); launch_client.close()
            deadline = time.monotonic() + 15
            while 'STABLE 2 3840x84\n' not in (root / 'desktop.log').read_text(errors='replace'):
                if desktop.poll() is not None or compositor.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('private SDL desktop did not start')
                time.sleep(.02)
            test = subprocess.Popen([sys.executable, str(runner), str(registry), str(client), str(host)],
                                    env=env, stdin=subprocess.DEVNULL, stdout=test_log,
                                    stderr=subprocess.STDOUT)
            if test.wait(timeout=50) != 0:
                raise RuntimeError('independent AT-SPI client failed; see atspi.log')
            if 'MAP 0\n' not in (root / 'server.log').read_text(errors='replace'):
                raise RuntimeError('Settings was not mapped on Aetheri')
            deadline = time.monotonic() + 5
            while True:
                desktop_trace = (root / 'desktop.log').read_text(errors='replace')
                server_trace = (root / 'server.log').read_text(errors='replace')
                observed = re.findall(r'^VIEW .*windows=(\d+) ', desktop_trace, re.M)
                if 'UNMAP 0\n' in server_trace and '1' in observed and observed[-1] == '0':
                    break
                if desktop.poll() is not None or compositor.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('desktop did not observe Settings open and close')
                time.sleep(.02)
            result['status'] = 'passed'
    finally:
        if test and test.poll() is None:
            test.terminate(); test.wait(timeout=5)
        if desktop and desktop.poll() is None:
            desktop.send_signal(signal.SIGTERM); desktop.wait(timeout=10)
        if compositor and compositor.poll() is None:
            compositor.send_signal(signal.SIGTERM); compositor.wait(timeout=10)
        for sock in (issuer, controller, shell_server, shell_client, launch_owner, launch_client):
            sock.close()
        result['client_exit'] = test.returncode if test else None
        result['desktop_exit'] = desktop.returncode if desktop else None
        result['server_exit'] = compositor.returncode if compositor else None
        if result['client_exit'] != 0 or result['desktop_exit'] != 0 or result['server_exit'] != 0:
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
