"""Probe installed NexaUI consumers against Aetheri's SDL desktop.

All three binaries are caller-supplied. The compositor and its input and shell
connections are started here and never attach to an existing desktop session.
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
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'desktop', 'consumer', 'settings', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    server_binary, desktop_binary, consumer_binary, settings_binary, font = [p.resolve(strict=True) for p in
        (args.server, args.desktop, args.consumer, args.settings, args.font)]
    root = args.work_dir.resolve()
    root.mkdir(mode=0o700)
    runtime, captures, cache = root / 'runtime', root / 'captures', root / 'cache'
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700); cache.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-dock',
               NEXAUI_PLATFORM='wayland', LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe', LP_NUM_THREADS='2')
    for key in ('DISPLAY', 'WAYLAND_SOCKET', 'SDL_VIDEO_DRIVER', 'SDL_RENDER_DRIVER'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    shell_server, shell_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    launch_owner, launch_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    processes, streams = [], []
    result = {'status': 'failed', 'server_sha256': digest(server_binary),
              'desktop_sha256': digest(desktop_binary), 'consumer_sha256': digest(consumer_binary),
              'settings_sha256': digest(settings_binary), 'font_sha256': digest(font),
              'scope': 'Aetheri headless test compositor and SDL desktop; no physical or human acceptance'}
    try:
        def start(binary, name, arguments=(), inherited=(), extra_env=None):
            stream = (root / (name + '.log')).open('wb'); streams.append(stream)
            child = subprocess.Popen([str(binary), *map(str, arguments)], env=extra_env or env,
                                     stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT,
                                     pass_fds=inherited)
            processes.append(child)
            return child

        def log(name):
            return (root / (name + '.log')).read_text(errors='replace')

        def wait(predicate, label, live):
            deadline = time.monotonic() + 15
            while not predicate():
                if any(p.poll() is not None for p in live):
                    raise RuntimeError('process exited before ' + label)
                if time.monotonic() >= deadline:
                    raise RuntimeError('timed out waiting for ' + label)
                time.sleep(.02)

        def command(child, value):
            child.stdin.write(value.encode('ascii')); child.stdin.flush()

        def event(value):
            controller.sendall(value.encode('ascii'))
            if controller.recv(32) != b'OK':
                raise RuntimeError('input driver rejected ' + value)

        def key(code):
            event(f'K {code} 1'); event(f'K {code} 0')

        server = start(server_binary, 'server', ('--headless', env['WAYLAND_DISPLAY'], captures,
                       issuer.fileno(), '--shell-fd', shell_server.fileno()),
                       (issuer.fileno(), shell_server.fileno()))
        issuer.close(); shell_server.close()
        wait(lambda: 'READY ' in log('server'), 'server', (server,))
        desktop = start(desktop_binary, 'desktop',
                        ('--font', font, '--cache', cache, '--launcher-fd', launch_client.fileno(), '--diagnostics'),
                        inherited=(shell_client.fileno(), launch_client.fileno()),
                        extra_env=dict(env, WAYLAND_SOCKET=str(shell_client.fileno())))
        shell_client.close(); launch_client.close()
        wait(lambda: all(f'STABLE {i} {width}x84\n' in log('desktop') for i, width in enumerate((1920, 2560, 3840))),
             'three SDL dock surfaces', (server, desktop))
        consumer = start(consumer_binary, 'consumer')
        live = (server, desktop, consumer)

        def frame(width, height, prior=0):
            line = f'FRAME {width} {height} {width} {height}\n'
            wait(lambda: log('consumer').count(line) > prior, line.strip(), live)
            return log('consumer').count(line)

        def capture(number, app_y, app_expected):
            server.send_signal(signal.SIGUSR1)
            wait(lambda: all(f'CAPTURE {number} {i}\n' in log('server') for i in range(3)),
                 'composed output', live)
            image = (captures / f'capture-{number}-output-0.ppm').read_bytes()
            magic, dimensions, maximum, pixels = image.split(b'\n', 3)
            width, height = map(int, dimensions.split())
            if magic != b'P6' or maximum != b'255' or (width, height) != (1920, 1080) or len(pixels) != width * height * 3:
                raise RuntimeError('unexpected image format')
            def pixel(x, y):
                offset = (y * width + x) * 3
                return tuple(pixels[offset:offset + 3])
            if pixel(760, 1020) not in ((38, 46, 63), (44, 72, 108), (0, 0, 0)) or pixel(50, 50) != (51, 102, 204):
                raise RuntimeError('dock or application pixels missing')
            if pixel(100, app_y) != app_expected:
                raise RuntimeError('incorrect work-area pixel')
            result[f'capture_{number}_sha256'] = hashlib.sha256(image).hexdigest()

        normal_count = frame(1840, 916)
        capture(1, 960, (16, 20, 28))
        command(consumer, 'm'); frame(1920, 996)
        capture(2, 990, (51, 102, 204))
        command(consumer, 'r'); frame(1840, 916, normal_count)
        capture(3, 960, (16, 20, 28))
        zero_views = len(re.findall(r'^VIEW .* output=0 .* windows=0 ', log('desktop'), re.M))
        command(consumer, 'q')
        if consumer.wait(timeout=10) != 0:
            raise RuntimeError('consumer cleanup failed')
        wait(lambda: len(re.findall(r'^VIEW .* output=0 .* windows=0 ', log('desktop'), re.M)) > zero_views,
             'desktop observes probe close', (server, desktop))
        one_views = len(re.findall(r'^VIEW .* output=0 .* windows=1 ', log('desktop'), re.M))
        settings = start(settings_binary, 'settings', (font, '--verify-keyboard-controls'))
        live_settings = (server, desktop, settings)
        wait(lambda: 'SETTINGS_KEYBOARD_READY\n' in log('settings') and 'MAP 1\n' in log('server') and
             len(re.findall(r'^VIEW .* output=0 .* windows=1 ', log('desktop'), re.M)) > one_views,
             'Settings ready on second output', live_settings)
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f'CAPTURE 4 {i}\n' in log('server') for i in range(3)),
             'composed Settings output', live_settings)
        image = (captures / 'capture-4-output-1.ppm').read_bytes()
        magic, dimensions, maximum, pixels = image.split(b'\n', 3)
        width, height = map(int, dimensions.split())
        if magic != b'P6' or maximum != b'255' or (width, height) != (2560, 1440) or len(pixels) != width * height * 3:
            raise RuntimeError('unexpected Settings output format')
        def settings_pixel(x, y):
            offset = (y * width + x) * 3
            return pixels[offset:offset + 3]
        background = bytes((16, 20, 28))
        changed = sum(settings_pixel(x, y) != background for y in range(50, 390, 4)
                      for x in range(50, 670, 4))
        if settings_pixel(10, 10) != background or changed < 2000:
            raise RuntimeError('Settings pixels missing on second output')
        result['settings_capture_sha256'] = hashlib.sha256(image).hexdigest()
        for code in (15, 15, 57, 15, 57):
            key(code); time.sleep(.15)
        if settings.wait(timeout=15) != 0:
            raise RuntimeError('Settings keyboard flow failed')
        trace = log('settings')
        for focus in (3, 4, 5):
            if not re.search(r'^KEYBOARD_TRACE .*focus=' + str(focus) + r'\b', trace, re.M):
                raise RuntimeError('Settings focus was not observed at control ' + str(focus))
        if not re.search(r'^KEYBOARD_TRACE .*applied_bytes=7 motion=1$', trace, re.M):
            raise RuntimeError('Settings applied state missing')
        wait(lambda: len(re.findall(r'^VIEW .* output=0 .* windows=0 ', log('desktop'), re.M)) > zero_views + 1,
             'desktop observes Settings close', (server, desktop))
        desktop.send_signal(signal.SIGTERM)
        if desktop.wait(timeout=10) != 0 or server.wait(timeout=10) != 0:
            raise RuntimeError('desktop retirement failed')
        if (runtime / env['WAYLAND_DISPLAY']).exists():
            raise RuntimeError('shell retirement or socket cleanup missing')
        result['status'] = 'passed'
    finally:
        for child in reversed(processes):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill(); child.wait(timeout=5)
            if child.stdin:
                child.stdin.close()
        for stream in streams:
            stream.close()
        for sock in (controller, issuer, shell_server, shell_client, launch_owner, launch_client):
            sock.close()
        result['exit_codes'] = [p.returncode for p in processes]
        if any(code != 0 for code in result['exit_codes']):
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
