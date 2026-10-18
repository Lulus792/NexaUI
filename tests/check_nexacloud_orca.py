"""Optional real Orca task in the rendered NexaCloud example (Linux/X11).

Run directly with --app, --font and a new --work directory. Requires Xvfb,
xauth, Openbox, xprop, xdotool, D-Bus, Orca, python-dbus, Speech Dispatcher
with python-speechd/espeak-ng/PulseAudio support, PulseAudio and pactl.
All desktop, bus, settings and audio services are private to this run.
Audio is synthesized to a null sink; this is not a human listening test.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import subprocess
import sys
import tempfile
import threading
import time
import traceback


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--font', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.app, args.font, args.work = args.app.resolve(), args.font.resolve(), args.work.resolve()
    if not args.worker:
        if args.work.exists():
            parser.error('--work must not exist; previous evidence is preserved')
        args.work.mkdir(parents=True)
        # Establish isolation before dbus activation; changing only the client's
        # environment would leave auto-activated services with the parent paths.
        with tempfile.TemporaryDirectory(prefix='nexaui-orca-') as runtime:
            env = dict(os.environ, XDG_RUNTIME_DIR=runtime, XDG_CONFIG_HOME=str(args.work/'config'),
                       XDG_DATA_HOME=str(args.work/'data'), XDG_CACHE_HOME=str(args.work/'cache'),
                       GSETTINGS_BACKEND='memory', NEXAUI_PLATFORM='x11', GDK_BACKEND='x11',
                       XDG_SESSION_TYPE='x11', LIBGL_ALWAYS_SOFTWARE='1')
            for key in ('DBUS_SESSION_BUS_ADDRESS', 'AT_SPI_BUS_ADDRESS', 'WAYLAND_DISPLAY', 'NO_AT_BRIDGE'):
                env.pop(key, None)
            return subprocess.call(['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24',
                                    'dbus-run-session', '--', sys.executable, str(Path(__file__).resolve()),
                                    '--worker', '--app', str(args.app), '--font', str(args.font),
                                    '--work', str(args.work)], env=env)
    return worker(args)


def worker(args):
    import dbus
    import speechd
    env, work = dict(os.environ), args.work
    runtime = Path(env['XDG_RUNTIME_DIR'])
    children, files, capture = [], [], bytearray()
    app = orca = reader = cache = None
    capture_state = dict(eof=False, error=None)
    report = dict(status='failed', scope='Native focus and actual Orca control announcements; no audio audition',
                  app_sha256=hashlib.sha256(args.app.read_bytes()).hexdigest(),
                  font_sha256=hashlib.sha256(args.font.read_bytes()).hexdigest(), checkpoints=[])

    def launch(command, name):
        out, err = (work/(name+'.stdout')).open('wb'), (work/(name+'.stderr')).open('wb')
        files.extend((out, err))
        child = subprocess.Popen(command, env=env, stdout=out, stderr=err)
        children.append(child)
        return child

    def wait(predicate, label, timeout=20):
        deadline, last = time.monotonic()+timeout, None
        while time.monotonic() < deadline:
            if app and app.poll() is not None:
                raise AssertionError('App exited before '+label+': '+str(app.returncode))
            if orca and orca.poll() is not None:
                raise AssertionError('Orca exited before '+label+': '+str(orca.returncode))
            try:
                value = predicate()
                if value:
                    return value
            except Exception as error:
                last = repr(error)
            time.sleep(.05)
        raise AssertionError(label+': '+str(last))

    def xrun(*command):
        return subprocess.check_output(['xdotool', *map(str, command)], env=env, text=True,
                                       stderr=subprocess.DEVNULL, timeout=5).strip()

    def debug():
        return capture.decode('utf-8', errors='replace')

    def rows():
        nonlocal cache
        if cache is None:
            for name in a11y.list_names():
                if name.startswith(':') and int(bus_api.GetConnectionUnixProcessID(name)) == app.pid:
                    cache = dbus.Interface(a11y.get_object(name, '/org/a11y/atspi/cache'), 'org.a11y.atspi.Cache')
                    report['app_bus'] = str(name)
                    break
        return cache.GetItems(timeout=3) if cache else []

    def state():
        result = dict(active=False, focused=[])
        # The existing native bulk cache avoids hundreds of serial property
        # round trips while observing a continuously rendered application.
        for row in rows():
            flags = int(row[9][0]) | (int(row[9][1]) << 32)
            if int(row[7]) == 23:
                result['active'] = bool(flags & (1 << 1))
            if flags & (1 << 12):
                result['focused'].append(dict(name=str(row[6]), role=int(row[7]), path=str(row[0][1])))
        return result

    def checkpoint(chord, name):
        offset, started = len(debug()), time.monotonic()
        xrun('key', '--clearmodifiers', chord)
        def native():
            value = state()
            return value if value['active'] and len(value['focused']) == 1 and value['focused'][0]['name'] == name else None
        value = wait(native, 'native focus '+name)
        pattern = re.compile(r"SPEECH OUTPUT: ['\"]"+re.escape(name)+r"['\"] ")
        spoken = wait(lambda: next((line for line in debug()[offset:].splitlines() if pattern.search(line)), None),
                      'actual Orca announcement '+name, timeout=40)
        entry = dict(keys=chord, expected=name, native=value, speech=spoken,
                     elapsed_seconds=round(time.monotonic()-started, 3))
        report['checkpoints'].append(entry)
        print('Orca:', name, entry['elapsed_seconds'], 's', flush=True)

    try:
        launch(['openbox'], 'openbox')
        def wm_ready():
            text = subprocess.check_output(['xprop', '-root', '_NET_SUPPORTING_WM_CHECK'], env=env, text=True, timeout=2)
            match = re.search(r'window id # (0x[0-9a-fA-F]+)', text)
            if not match or int(match[1], 16) == 0:
                return False
            child = subprocess.check_output(['xprop', '-id', match[1], '_NET_SUPPORTING_WM_CHECK'], env=env, text=True, timeout=2)
            return match[1] in child
        wait(wm_ready, 'live window manager')
        bus = dbus.SessionBus()
        status = bus.get_object('org.a11y.Bus', '/org/a11y/bus')
        dbus.Interface(status, 'org.freedesktop.DBus.Properties').Set('org.a11y.Status', 'IsEnabled', dbus.Boolean(True))
        address = str(dbus.Interface(status, 'org.a11y.Bus').GetAddress())
        env['AT_SPI_BUS_ADDRESS'] = address
        a11y = dbus.bus.BusConnection(address)
        bus_api = dbus.Interface(a11y.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus'), 'org.freedesktop.DBus')
        pulse_socket, pulse_config = runtime/'pulse.sock', work/'pulse.pa'
        pulse_config.write_text('load-module module-null-sink sink_name=nexaui_null\n'
                                f'load-module module-native-protocol-unix socket={pulse_socket} auth-anonymous=1\n'
                                'set-default-sink nexaui_null\n')
        env['PULSE_SERVER'] = 'unix:'+str(pulse_socket)
        launch(['pulseaudio', '-n', '--daemonize=no', '--use-pid-file=no', '--exit-idle-time=-1', '--file='+str(pulse_config)], 'pulse')
        wait(pulse_socket.exists, 'private PulseAudio socket')
        sinks = subprocess.check_output(['pactl', '--server='+env['PULSE_SERVER'], 'list', 'short', 'sinks'], env=env, text=True, timeout=5)
        assert len(sinks.splitlines()) == 1 and sinks.split()[1] == 'nexaui_null', sinks
        report['audio_sink'] = sinks.strip()
        config = work/'speech-config'
        (config/'modules').mkdir(parents=True)
        (config/'modules/espeak-ng.conf').write_bytes(Path('/etc/speech-dispatcher/modules/espeak-ng.conf').read_bytes())
        (config/'speechd.conf').write_text('LogLevel 4\nDefaultLanguage "de"\nAudioOutputMethod "pulse"\n'
                                         'AddModule "espeak-ng" "sd_espeak-ng" "espeak-ng.conf"\nDefaultModule "espeak-ng"\n')
        (work/'speech-log').mkdir()
        socket = runtime/'speechd.sock'
        env['SPEECHD_ADDRESS'] = 'unix_socket:'+str(socket)
        launch(['speech-dispatcher', '-s', '-t', '0', '-C', str(config), '-S', str(socket),
                '-P', str(runtime/'speechd.pid'), '-L', str(work/'speech-log')], 'speechd')
        wait(socket.exists, 'private Speech Dispatcher socket')
        events = []
        client = speechd.SSIPClient('nexaui-audio-proof', address=env['SPEECHD_ADDRESS'], autospawn=False)
        try:
            client.set_output_module('espeak-ng')
            client.set_language('de')
            client.speak('Sprachausgabe bereit.', callback=lambda kind, **kw: events.append(kind),
                         event_types=[speechd.CallbackType.BEGIN, speechd.CallbackType.END])
            wait(lambda: speechd.CallbackType.END in events, 'completed synthesis preflight')
            report['audio_completion_probe'] = events
        finally:
            client.close()
        master, slave = pty.openpty()
        log = (work/'orca.debug').open('wb', buffering=0)
        files.append(log)
        try:
            orca = subprocess.Popen(['orca', '--speech-system', 'speechdispatcherfactory', '--debug-file', '/dev/stderr'],
                                    env=env, stdout=slave, stderr=slave)
        finally:
            os.close(slave)
        children.append(orca)
        def copy():
            try:
                while data := os.read(master, 65536):
                    if log.write(data) != len(data):
                        raise OSError('Incomplete debug capture')
                    capture.extend(data)
                capture_state['eof'] = True
            except OSError as error:
                if error.errno == errno.EIO:
                    capture_state['eof'] = True  # Last terminal slave closed.
                else:
                    capture_state['error'] = repr(error)
            except Exception as error:
                capture_state['error'] = repr(error)
            finally:
                os.close(master)
        reader = threading.Thread(target=copy, daemon=True)
        reader.start()
        wait(lambda: bus.name_has_owner('org.gnome.Orca.Service'), 'Orca initialized')
        app = launch([str(args.app), str(args.font)], 'app')
        wait(rows, 'published native tree from owned process')
        # _NET_WM_PID is not currently published. Only harness children share
        # this display; require one exact title and verify the AT-SPI PID above.
        # Use an ASCII prefix for lookup; require the complete UTF-8 name
        # with getwindowname below.
        xid = wait(lambda: xrun('search', '--all', '--onlyvisible', '--name', '^NexaCloud'), 'unique native window')
        assert len(xid.splitlines()) == 1, xid
        assert xrun('getwindowname', xid) == 'NexaCloud — NexaUI'
        xrun('windowactivate', '--sync', xid)
        xrun('windowfocus', '--sync', xid)
        for name in ('Übersicht', 'Dateien', 'Übertragungen', 'Favoriten', 'Papierkorb', 'Einstellungen', 'Systeminformationen'):
            checkpoint('Tab', name)
            if name == 'Dateien':
                xrun('key', '--clearmodifiers', 'Return')
                wait(lambda: any(str(row[6]) == 'Bergsee.jpg' for row in rows()), 'actual files page')
                report['files_page_opened'] = True
        checkpoint('Return', 'Schließen')
        checkpoint('Escape', 'Systeminformationen')
        report['tasks_completed'] = True
    except BaseException:
        report['error'] = traceback.format_exc()
    finally:
        # Stop only services launched here. dbus-run-session and xvfb-run own
        # their bus/registry/display; the parent owns the private runtime.
        for child in ([orca] if orca else []) + list(reversed(children)):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
        if reader:
            reader.join(timeout=5)
        report['capture_error'] = capture_state['error']
        report['capture_complete'] = reader is not None and not reader.is_alive() and capture_state['eof'] and not capture_state['error']
        for file in files:
            file.close()
        text = debug()
        report['speech_backend_available'] = bool(re.search(r'Speech server is <orca\.speechdispatcherfactory\.SpeechServer', text))
        report['speech_output'] = [line.split('SPEECH OUTPUT: ', 1)[1] for line in text.splitlines() if 'SPEECH OUTPUT: ' in line]
        audio_log = work/'speech-log/speech-dispatcher.log'
        report['audio_errors'] = [line for line in audio_log.read_text(errors='replace').splitlines()
                                  if "Can't initialize audio" in line or 'Audio event but server audio not set up' in line] if audio_log.exists() else ['No audio log']
        if (report.get('tasks_completed') and report['capture_complete'] and report['speech_backend_available']
                and report.get('audio_completion_probe') == ['begin', 'end'] and not report['audio_errors']):
            report['status'] = 'passed'
        (work/'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print('Orca NexaCloud:', report['status'], work, flush=True)
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
