"""Exercise clipboard ownership between separate processes on a private compositor."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'peer', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    server, peer = (p.resolve(strict=True) for p in (args.server, args.peer))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-peers',
               NEXAUI_PLATFORM='wayland')
    for key in ('DISPLAY', 'WAYLAND_SOCKET'):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    processes, streams = [], []
    result = {'status': 'failed', 'server_sha256': digest(server), 'peer_sha256': digest(peer),
              'scope': 'isolated Aetheri test compositor, separate processes and virtual input'}
    try:
        def start(binary, name, arguments=(), inherited=()):
            stream = (root / (name + '.log')).open('wb'); streams.append(stream)
            child = subprocess.Popen([str(binary), *map(str, arguments)], env=env,
                                     stdin=subprocess.PIPE, stdout=stream,
                                     stderr=subprocess.STDOUT, pass_fds=inherited)
            processes.append(child)
            return child

        def log(name):
            return (root / (name + '.log')).read_text(errors='replace')

        def wait(predicate, label, live):
            deadline = time.monotonic() + 15
            while not predicate():
                if any(process.poll() is not None for process in live):
                    raise RuntimeError('process exited before ' + label)
                if time.monotonic() >= deadline:
                    raise RuntimeError('timed out waiting for ' + label)
                time.sleep(.02)

        def event(command):
            controller.sendall(command.encode('ascii'))
            if controller.recv(32) != b'OK':
                raise RuntimeError('input driver rejected ' + command)

        def key(code):
            event(f'K {code} 1'); event(f'K {code} 0')

        def click(x, y):
            event(f'M {x} {y}'); event('B 272 1'); event('B 272 0')

        def command(process, byte):
            process.stdin.write(byte); process.stdin.flush()

        compositor = start(server, 'server', ('--headless', env['WAYLAND_DISPLAY'], '-', issuer.fileno()),
                           (issuer.fileno(),))
        issuer.close()
        wait(lambda: 'READY ' in log('server'), 'compositor', (compositor,))
        source = start(peer, 'source')
        wait(lambda: 'PEER_READY\n' in log('source'), 'source window', (compositor, source))
        receiver = start(peer, 'receiver')
        live = (compositor, source, receiver)
        wait(lambda: 'PEER_READY\n' in log('receiver'), 'receiver window', live)
        click(1980, 60); key(48)
        wait(lambda: 'TEXT 98\n' in log('receiver'), 'initial receiver focus', live)
        command(receiver, b'L')
        wait(lambda: 'CLIPBOARD_LOST\n' in log('receiver'), 'initial missing selection', live)
        click(60, 60); key(30)
        wait(lambda: 'TEXT 97\n' in log('source'), 'source keyboard serial', live)
        command(source, b'C')
        wait(lambda: 'CLIPBOARD_SET\n' in log('source'), 'clipboard set', live)
        click(1980, 60); key(48)
        wait(lambda: log('receiver').count('TEXT 98\n') == 2, 'receiver focus and input', live)
        command(receiver, b'P')
        wait(lambda: 'CLIPBOARD_READ\n' in log('receiver') and
             'CLIPBOARD_MATCH\n' in log('receiver'), 'cross-process transfer', live)
        command(source, b'Q')
        if source.wait(timeout=10) != 0 or 'PEER_CLOSED\n' not in log('source'):
            raise RuntimeError('source did not close cleanly')
        key(46)
        wait(lambda: 'TEXT 99\n' in log('receiver'), 'receiver after owner exit', (compositor, receiver))
        command(receiver, b'L')
        wait(lambda: log('receiver').count('CLIPBOARD_LOST\n') == 2, 'owner loss', (compositor, receiver))
        command(receiver, b'Q')
        if receiver.wait(timeout=10) != 0 or 'PEER_CLOSED\n' not in log('receiver'):
            raise RuntimeError('receiver did not close cleanly')
        for name in ('source', 'receiver'):
            trace = log(name)
            if any(s in trace for s in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')):
                raise RuntimeError('sanitizer reported an error in ' + name)
        result.update(status='passed', missing_selection='rejected before ownership',
                      clipboard='separate processes transferred Unicode',
                      owner_loss='new read rejected after source exited')
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=5)
            if process.stdin:
                process.stdin.close()
        for stream in streams:
            stream.close()
        issuer.close(); controller.close()
        result['exit_codes'] = [p.returncode for p in processes]
        if result['exit_codes'] != [0, 0, 0]:
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
