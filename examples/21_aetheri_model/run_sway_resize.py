"""Run an installed Settings consumer on a new, private headless Sway session."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
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
    parser.add_argument('--atspi-client', type=Path)
    parser.add_argument('--atspi-registry', type=Path)
    parser.add_argument('--wtype', type=Path, help='Exercise a private virtual keyboard')
    args = parser.parse_args()
    if bool(args.atspi_client) != bool(args.atspi_registry):
        parser.error('--atspi-client and --atspi-registry must be supplied together')
    if args.wtype and args.atspi_client:
        parser.error('--wtype and --atspi-client are separate test modes')
    wtype = args.wtype.resolve(strict=True) if args.wtype else None
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
            if args.atspi_client:
                env.update(NEXAUI_SETTINGS_FONT=str(font), NEXAUI_MODEL_DISPLAY_FIXTURE='1',
                           NEXAUI_MODEL_RESIZE_READY=str(work/'client-ready'),
                           NEXAUI_MODEL_RESIZE_SMALL=str(work/'resize-small'),
                           NEXAUI_MODEL_RESIZE_CHANGED=str(work/'client-changed'),
                           NEXAUI_MODEL_RESIZE_LARGE=str(work/'resize-large'))
                env.pop('NO_AT_BRIDGE', None)
                runner = Path(__file__).resolve().parents[2]/'tests/run_atspi_native.py'
                app_command = [sys.executable, str(runner), str(args.atspi_registry.resolve(strict=True)),
                               str(args.atspi_client.resolve(strict=True)), str(host)]
            else:
                app_command = [str(host), str(font), '--diagnostics', '--fixture-displays']
            app = subprocess.Popen(app_command,
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

            def ready(width, height, after, model='MODEL theme=0 german=0 text=100'):
                log = lines()[after:]
                frame = f'FRAME {width} {height} 1.00'
                if frame not in log:
                    return False
                i = log.index(frame)
                return model in log[i+1:] and \
                       'FIXTURE_DISPLAYS count=2 selected=42 stale=0' in log[i+1:]

            wait(window, 'Settings window')
            if args.atspi_client:
                wait(lambda: (work/'client-ready').exists(), 'AT-SPI model changes')
                model='MODEL theme=2 german=1 text=200'
            else:
                model='MODEL theme=0 german=0 text=100'
            wait(lambda: ready(1280, 720, 0, model), 'initial tiled frame')
            target = '[app_id="^org.aetheri.Settings$"]'
            before = len(lines())
            result = command(f'{target} floating enable')
            if not result or not all(item.get('success') for item in result):
                raise RuntimeError(f'Sway rejected floating mode: {result}')
            wait(lambda: window() and window().get('floating') == 'user_on', 'floating window')
            wait(lambda: ready(800, 480, before, model), 'initial floating frame')
            for width, height in ((320, 240), (800, 480)):
                before = len(lines())
                result = command(f'{target} resize set width {width} px height {height} px')
                if not result or not all(item.get('success') for item in result):
                    raise RuntimeError(f'Sway rejected resize: {result}')
                (work / f'tree-{width}x{height}.json').write_text(
                    json.dumps(command('-t', 'get_tree'), indent=2), encoding='utf-8')
                wait(lambda: ready(width, height, before, model), f'{width}x{height} frame')
                current = window()
                if not current or current['rect']['width'] != width or current['rect']['height'] != height:
                    raise RuntimeError(f'Unexpected Sway geometry: {current}')
                if args.atspi_client and width==320:
                    (work/'resize-small').write_text('ready\n',encoding='ascii')
                    wait(lambda: (work/'client-changed').exists(), 'AT-SPI action at small size')
                    model='MODEL theme=2 german=1 text=150'
                elif args.atspi_client:
                    (work/'resize-large').write_text('ready\n',encoding='ascii')
                elif wtype and width==320:
                    for focus in (10, 11):
                        before_key = len(lines())
                        subprocess.run([str(wtype), '-s', '100', '-k', 'Tab'], env=env, check=True, timeout=5)
                        wait(lambda: f'FOCUS {focus}' in lines()[before_key:], f'keyboard focus {focus}')
                    before_key = len(lines())
                    subprocess.run([str(wtype), '-s', '100', '-k', 'space'], env=env, check=True, timeout=5)
                    model='MODEL theme=1 german=0 text=100'
                    wait(lambda: ready(320, 240, before_key, model), 'keyboard model change at 320x240')
                elif wtype:
                    if 'FOCUS 11' not in lines()[before:]:
                        raise RuntimeError('Focus was not retained through resize')
                    before_key = len(lines())
                    subprocess.run([str(wtype), '-s', '100', '-k', 'Tab'], env=env, check=True, timeout=5)
                    wait(lambda: 'FOCUS 12' in lines()[before_key:], 'keyboard focus after restore')
                    before_key = len(lines())
                    subprocess.run([str(wtype), '-s', '100', '-k', 'space'], env=env, check=True, timeout=5)
                    wait(lambda: ready(800, 480, before_key, 'MODEL theme=2 german=0 text=100'),
                         'keyboard model change after restore')
                    subprocess.run([str(wtype), '-s', '100', '-k', 'Escape'], env=env, check=True, timeout=5)
            if not args.atspi_client and not wtype:
                app.send_signal(signal.SIGTERM)
            if app.wait(timeout=5) != 0:
                raise RuntimeError('Settings did not close cleanly')
            app = None
            if args.atspi_client:
                print('Private Sway: AT-SPI changed Settings at 320x240, verified the restored size and closed.')
            elif wtype:
                print('Private Sway: virtual keyboard changed Settings before and after resize, retained focus and closed.')
            else:
                print('Private Sway: Settings rendered at 320x240 and 800x480, retained model and closed.')
            return 0
        finally:
            if app and app.poll() is None:
                # The AT-SPI wrapper owns separately-sessioned bus/client
                # processes; SIGINT lets its finally block stop them.
                app.send_signal(signal.SIGINT if args.atspi_client else signal.SIGTERM)
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
