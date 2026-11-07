"""Native tree keys on an explicitly supplied private Aetheri test compositor."""
import argparse
import hashlib
import json
import os
import re
import signal
import socket
import subprocess
import time
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'registry', 'host', 'client', 'font', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expect-unavailable', action='store_true')
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--actions', action='store_true', help='use the eight-step independent expansion-action peer')
    args = parser.parse_args()
    if args.actions and args.expect_unavailable:
        parser.error('--actions requires the accessible positive profile')
    paths = {name: getattr(args, name).resolve(strict=True)
             for name in ('server', 'registry', 'host', 'client', 'font')}
    work = args.work_dir.absolute()
    work.mkdir(mode=0o700)
    runtime = work / 'r'
    runtime.mkdir(mode=0o700)
    captures = work / 'captures'
    captures.mkdir(mode=0o700)
    if len(os.fsencode(runtime / 'bus')) > 90:
        raise ValueError('Use a shorter new work directory for private UNIX sockets')
    address = 'unix:path=' + str(runtime / 'bus')
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='tree',
               NEXAUI_PLATFORM='wayland', DBUS_SESSION_BUS_ADDRESS=address,
               AT_SPI_BUS_ADDRESS=address, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'NO_AT_BRIDGE'):
        env.pop(name, None)
    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    left.settimeout(4)
    children, streams, cleanups = [], [], []
    expected_done = set()
    result = dict(status='failed', input_sha256={name: digest(path) for name, path in paths.items()},
                  expect_unavailable=args.expect_unavailable, cleanup=cleanups,
                  actions=args.actions,
                  scope='Native keyboard and independent AT-SPI on a private test seat; no real hierarchy service or physical/human acceptance')

    def start(command, name, pass_fds=(), interactive=False):
        log = (work / (name + '.log')).open('xb')
        streams.append(log)
        child = subprocess.Popen(list(map(str, command)), env=env, cwd=work,
                                 stdin=subprocess.PIPE if interactive else subprocess.DEVNULL,
                                 stdout=log, stderr=subprocess.STDOUT, pass_fds=pass_fds,
                                 start_new_session=True)
        children.append((name, child))
        return child

    def log(name):
        return (work / (name + '.log')).read_text(errors='replace')

    def wait(predicate, label, allowed_exit=None, timeout=7):
        deadline = time.monotonic() + timeout
        while not predicate():
            if any(child is not allowed_exit and child not in expected_done and child.poll() is not None
                   for _, child in children):
                raise RuntimeError(label + ': child ended; inspect ' + str(work))
            if time.monotonic() >= deadline:
                raise RuntimeError(label + ': timeout; inspect ' + str(work))
            time.sleep(.01)

    def event(command):
        left.sendall(command.encode('ascii'))
        if left.recv(64) != b'OK':
            raise RuntimeError('test compositor rejected ' + command)

    def key(code):
        event(f'K {code} 1')
        event(f'K {code} 0')

    def keys_seen():
        values = re.findall(r'^VIEW .* keys=(\d+)$', log('host'), re.M)
        return int(values[-1]) if values else 0

    def capture(label):
        if not args.capture:
            return
        before = set(captures.glob('*.ppm'))
        server.send_signal(signal.SIGUSR1)
        def complete():
            images = set(captures.glob('*.ppm')) - before
            if len(images) != 3:
                return False
            for path in images:
                with path.open('rb') as stream:
                    if stream.readline() != b'P6\n':
                        return False
                    extent = stream.readline().split()
                    if len(extent) != 2 or stream.readline() != b'255\n':
                        return False
                    expected = stream.tell() + 3 * int(extent[0]) * int(extent[1])
                if path.stat().st_size != expected:
                    return False
            return True
        wait(complete, 'three complete output captures')
        result.setdefault('captures', []).append(dict(label=label, files=[str(p) for p in sorted(set(captures.glob('*.ppm')) - before)]))

    try:
        server = start([paths['server'], '--headless', 'tree', captures if args.capture else '-',
                        right.fileno()], 'server', pass_fds=(right.fileno(),))
        right.close()
        wait(lambda: 'READY ' in log('server'), 'compositor startup')
        start(['dbus-daemon', '--session', '--nofork', '--address=' + address], 'bus')
        wait(lambda: (runtime / 'bus').exists(), 'private bus startup')
        start([paths['registry']], 'registry')

        def registered():
            reply = subprocess.run(['dbus-send', '--bus=' + address, '--print-reply',
                                    '--dest=org.freedesktop.DBus', '/org/freedesktop/DBus',
                                    'org.freedesktop.DBus.NameHasOwner', 'string:org.a11y.atspi.Registry'],
                                   env=env, capture_output=True, text=True, timeout=2)
            return reply.returncode == 0 and 'boolean true' in reply.stdout
        wait(registered, 'registry startup')
        host = start([paths['host'], paths['font']], 'host')
        wait(lambda: 'TREE_READY\n' in log('host'), 'mapped software window')
        result['host_pid'] = host.pid
        result['host_maps'] = Path(f'/proc/{host.pid}/maps').read_text()
        client_args = [paths['client'], host.pid]
        if args.expect_unavailable:
            client_args.append('--expect-unavailable')
        peer = start(client_args, 'client', interactive=not args.expect_unavailable)
        if args.expect_unavailable:
            wait(lambda: peer.poll() is not None, 'provider-off discovery negative', allowed_exit=peer, timeout=9)
            if peer.returncode != 77 or 'TREE_UNAVAILABLE' not in log('client'):
                raise RuntimeError('provider-off peer did not reject discovery')
        else:
            sequences = [[108], [106], [105], [105], [106], [106], [102], [107],
                         [108], [108], [102], [28], [105, 108], [106], [106], [108]]
            if args.actions:
                sequences = [[] for _ in range(8)]
            for step, codes in enumerate(sequences):
                wait(lambda: f'STEP {step}\n' in log('client'), f'peer step {step}')
                count = keys_seen() + 2 * len(codes)
                for code in codes:
                    key(code)
                wait(lambda: keys_seen() >= count, f'published key batch {step}')
                peer.stdin.write(f'CHECK {step}\n'.encode('ascii'))
                peer.stdin.flush()
                wait(lambda: f'ACK {step}\n' in log('client'), f'independent native state {step}', allowed_exit=peer)
                if step == len(sequences) - 1:
                    expected_done.add(peer)  # Final capture may outlive the verified peer.
                if args.actions:
                    def published_action():
                        views = re.findall(r'^VIEW (.+)$', log('host'), re.M)
                        current = dict(re.findall(r'(\w+)=(\d+)', views[-1])) if views else {}
                        expected_root = (step & 1) if step >= 4 else 1
                        expected_child = 1 if step >= 4 else step & 1
                        return all(current.get(k) == str(v) for k, v in
                                   dict(root=expected_root, child=expected_child, selected=15, other=31, focus=17, keys=0).items())
                    wait(published_action, f'published expansion action {step}')
                if step in ((0, 7) if args.actions else (3, 13)):
                    capture('collapsed' if step in (0, 3) else 'restored-selection')
            wait(lambda: peer.poll() is not None, 'independent client exit', allowed_exit=peer)
            marker = 'TREE_ACTIONS_VERIFIED ' if args.actions else 'TREE_VERIFIED '
            if peer.returncode or marker not in log('client'):
                raise RuntimeError('native tree client failed')
        expected_done.add(peer)
        key(1)  # The test-only ordinary window closes normally with Escape.
        wait(lambda: host.poll() is not None, 'normal host close', allowed_exit=host)
        if host.returncode or 'TREE_CLOSED ' not in log('host'):
            raise RuntimeError('native host did not close cleanly')
        result.update(status='passed', host_exit=host.returncode, client_exit=peer.returncode)
    finally:
        for name, child in reversed(children):
            forced = False
            if child.poll() is None:
                # Only this driver's retained private process groups are targeted.
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    forced = True
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait(timeout=3)
            cleanups.append(dict(name=name, exit=child.returncode, forced=forced))
            if child.stdin:
                child.stdin.close()
        left.close()
        right.close()
        for stream in streams:
            stream.close()
        (work / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(dict(status=result['status'], work_dir=str(work))))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
