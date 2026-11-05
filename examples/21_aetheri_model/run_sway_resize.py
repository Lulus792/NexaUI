"""Run an installed Settings consumer on a new, private headless Sway session."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


def nodes(node):
    yield node
    for child in node.get('nodes', []) + node.get('floating_nodes', []):
        yield from nodes(child)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sway', 'swaymsg', 'host', 'font', 'work-dir'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    sway, swaymsg, host, font = (p.resolve(strict=True) for p in
                                 (args.sway, args.swaymsg, args.host, args.font))
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    config = work / 'sway.conf'
    config.write_text('output * resolution 1280x720\nseat seat0 fallback true\n'
                      'default_border none\nxwayland disable\n', encoding='utf-8')
    with tempfile.TemporaryDirectory(prefix='nexaui-settings-sway-') as tmp, \
            (work / 'sway.log').open('w') as sway_log, \
            (work / 'host.log').open('w') as host_log:
        runtime = Path(tmp)
        env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WLR_BACKENDS='headless',
                   WLR_RENDERER='pixman', WLR_LIBINPUT_NO_DEVICES='1',
                   NEXAUI_PLATFORM='wayland', NO_AT_BRIDGE='1')
        for key in ('WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'SWAYSOCK', 'DISPLAY'):
            env.pop(key, None)
        server = subprocess.Popen([str(sway), '--config', str(config)], env=env,
                                  stdout=sway_log, stderr=subprocess.STDOUT)
        app = None
        try:
            def wait(predicate, label, timeout=12):
                end = time.monotonic() + timeout
                while time.monotonic() < end:
                    if server.poll() is not None or (app and app.poll() is not None):
                        raise RuntimeError('Process exited while waiting for ' + label)
                    result = predicate()
                    if result:
                        return result
                    time.sleep(.02)
                raise RuntimeError('Timed out waiting for ' + label)

            def sockets():
                ipc = list(runtime.glob('sway-ipc.*.sock'))
                displays = [p for p in runtime.glob('wayland-*') if not p.name.endswith('.lock')]
                return (ipc[0], displays[0]) if len(ipc) == len(displays) == 1 else None

            ipc, display = wait(sockets, 'private compositor sockets')
            env['SWAYSOCK'] = str(ipc)
            env['WAYLAND_DISPLAY'] = display.name
            app = subprocess.Popen([str(host), str(font), '--diagnostics', '--fixture-displays'],
                                   env=env, stdout=host_log, stderr=subprocess.STDOUT)
            def command(*arguments):
                result = subprocess.run([str(swaymsg), '-r', *arguments], env=env,
                                        capture_output=True, text=True, check=True, timeout=5)
                return json.loads(result.stdout)

            def window():
                matches = [n for n in nodes(command('-t', 'get_tree'))
                           if n.get('app_id') == 'org.aetheri.Settings']
                if len(matches) > 1:
                    raise RuntimeError('More than one matching Settings window')
                return matches[0] if matches else None

            def lines():
                return (work / 'host.log').read_text(encoding='utf-8', errors='replace').splitlines()

            def ready(width, height, after):
                log = lines()[after:]
                frame = f'FRAME {width} {height} 1.00'
                if frame not in log:
                    return False
                i = log.index(frame)
                return 'MODEL theme=0 german=0 text=100' in log[i+1:] and \
                       'FIXTURE_DISPLAYS count=2 selected=42 stale=0' in log[i+1:]

            wait(window, 'Settings window')
            wait(lambda: ready(1280, 720, 0), 'initial tiled frame')
            target = '[app_id="^org.aetheri.Settings$"]'
            before = len(lines())
            result = command(f'{target} floating enable')
            if not result or not all(item.get('success') for item in result):
                raise RuntimeError(f'Sway rejected floating mode: {result}')
            wait(lambda: window() and window().get('floating') == 'user_on', 'floating window')
            wait(lambda: ready(800, 480, before), 'initial floating frame')
            for width, height in ((320, 240), (800, 480)):
                before = len(lines())
                result = command(f'{target} resize set width {width} px height {height} px')
                if not result or not all(item.get('success') for item in result):
                    raise RuntimeError(f'Sway rejected resize: {result}')
                (work / f'tree-{width}x{height}.json').write_text(
                    json.dumps(command('-t', 'get_tree'), indent=2), encoding='utf-8')
                wait(lambda: ready(width, height, before), f'{width}x{height} frame')
                current = window()
                if not current or current['rect']['width'] != width or current['rect']['height'] != height:
                    raise RuntimeError(f'Unexpected Sway geometry: {current}')
            app.send_signal(signal.SIGTERM)
            if app.wait(timeout=5) != 0:
                raise RuntimeError('Settings did not close cleanly')
            app = None
            print('Private Sway: Settings rendered at 320x240 and 800x480, retained model and closed.')
            return 0
        finally:
            if app and app.poll() is None:
                app.terminate()
                try:
                    app.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    app.kill(); app.wait()
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill(); server.wait()


if __name__ == '__main__':
    raise SystemExit(main())
