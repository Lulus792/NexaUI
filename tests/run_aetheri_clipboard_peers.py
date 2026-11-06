"""Exercise clipboard ownership between separate processes on a private compositor."""
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
    for name in ('server', 'peer', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--test-inflight-loss', action='store_true',
                        help='stop the source, begin a read, then kill the source')
    parser.add_argument('--foreign-paste', type=Path,
                        help='require this unrelated wl-paste client to read the selection')
    parser.add_argument('--foreign-copy', type=Path,
                        help='also replace the selection using this unrelated wl-copy client')
    parser.add_argument('--foreign-loader', type=Path,
                        help='optional loader used for the unrelated client')
    parser.add_argument('--foreign-library-dir', type=Path,
                        help='library directory passed to the optional loader')
    args = parser.parse_args()
    server, peer = (p.resolve(strict=True) for p in (args.server, args.peer))
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='nexaui-peers',
               NEXAUI_PLATFORM='wayland')
    for key in ('DISPLAY', 'WAYLAND_SOCKET'):
        env.pop(key, None)
    if args.test_inflight_loss:
        env['NEXAUI_TEST_CLIPBOARD_INTERRUPTED'] = '1'
        env['WAYLAND_DEBUG'] = 'server'
    if args.foreign_loader and (not args.foreign_paste or not args.foreign_library_dir):
        parser.error('--foreign-loader requires --foreign-paste and --foreign-library-dir')
    if args.foreign_paste and args.test_inflight_loss:
        parser.error('--foreign-paste and --test-inflight-loss are separate cases')
    if args.foreign_copy and not args.foreign_paste:
        parser.error('--foreign-copy requires --foreign-paste')
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
        if args.foreign_paste:
            foreign = args.foreign_paste.resolve(strict=True)
            argv = [str(foreign), '--no-newline']
            if args.foreign_loader:
                argv = [str(args.foreign_loader.resolve(strict=True)), '--library-path',
                        str(args.foreign_library_dir.resolve(strict=True)), *argv]
            try:
                received = subprocess.run(argv, env=env, capture_output=True, timeout=8)
            except subprocess.TimeoutExpired as exc:
                (root / 'foreign.log').write_text('foreign paste timed out\n')
                raise RuntimeError('foreign paste timed out') from exc
            (root / 'foreign.log').write_bytes(received.stderr)
            if received.returncode != 0 or received.stdout != 'Grüße 🌍'.encode('utf-8'):
                raise RuntimeError('foreign paste did not receive the expected Unicode selection')
            result['foreign_paste_sha256'] = digest(foreign)
            result['foreign_paste'] = 'exact Unicode selection'
            if args.foreign_copy:
                copy = args.foreign_copy.resolve(strict=True)
                copy_argv = [str(copy), '--foreground', '--type', 'text/plain;charset=utf-8']
                if args.foreign_loader:
                    copy_argv = [str(args.foreign_loader.resolve(strict=True)), '--library-path',
                                 str(args.foreign_library_dir.resolve(strict=True)), *copy_argv]
                copy_log = (root / 'foreign-copy.log').open('wb'); streams.append(copy_log)
                copy_process = subprocess.Popen(copy_argv, env=env, stdin=subprocess.PIPE,
                                                stdout=copy_log, stderr=subprocess.STDOUT)
                processes.append(copy_process)
                copy_process.stdin.write('Fremd 🌍'.encode('utf-8'))
                copy_process.stdin.close(); copy_process.stdin = None
                deadline = time.monotonic() + 10
                while True:
                    if copy_process.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError('foreign copy did not publish its selection')
                    probe = subprocess.run(argv, env=env, capture_output=True, timeout=8)
                    if probe.returncode == 0 and probe.stdout == 'Fremd 🌍'.encode('utf-8'):
                        break
                    time.sleep(.02)
                result['foreign_copy_sha256'] = digest(copy)
        if args.test_inflight_loss:
            source.send_signal(signal.SIGSTOP)
            def stopped():
                status = Path(f'/proc/{source.pid}/status').read_text()
                return re.search(r'^State:\s+T', status, re.M) is not None
            wait(stopped, 'source stopped', live)
        click(1980, 60); key(48)
        wait(lambda: log('receiver').count('TEXT 98\n') == 2, 'receiver focus and input', live)
        command(receiver, b'F' if args.foreign_copy else b'P')
        if args.test_inflight_loss:
            wait(lambda: 'CLIPBOARD_PENDING\n' in log('receiver') and
                 re.search(r'wl_data_source[@#]\d+\.send\(', log('server')), 'pending source request', live)
            source.kill()
            if source.wait(timeout=10) != -signal.SIGKILL:
                raise RuntimeError('source was not killed')
            wait(lambda: 'CLIPBOARD_ABORTED\n' in log('receiver') or
                 'CLIPBOARD_EOF_EMPTY\n' in log('receiver'), 'interrupted read outcome', (compositor, receiver))
            result['inflight_outcome'] = ('aborted' if 'CLIPBOARD_ABORTED\n' in log('receiver')
                                          else 'empty EOF; indistinguishable from empty data')
        else:
            wait(lambda: 'CLIPBOARD_READ\n' in log('receiver') and
                 'CLIPBOARD_MATCH\n' in log('receiver'), 'cross-process transfer', live)
            command(source, b'Q')
            if source.wait(timeout=10) != 0 or 'PEER_CLOSED\n' not in log('source'):
                raise RuntimeError('source did not close cleanly')
            if args.foreign_copy:
                copy_process.terminate()
                if copy_process.wait(timeout=5) != -signal.SIGTERM:
                    raise RuntimeError('foreign copy did not terminate as expected')
                result['foreign_copy'] = 'exact Unicode selection read by NexaUI'
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
                      owner_loss=('new read rejected after foreign owner terminated' if args.foreign_copy
                                  else 'new read rejected after source exited'))
        if not args.test_inflight_loss and not args.foreign_copy:
            result['clipboard'] = 'separate processes transferred Unicode'
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
        expected = ([0, -signal.SIGKILL, 0] if args.test_inflight_loss else
                    [0, 0, 0, -signal.SIGTERM] if args.foreign_copy else [0, 0, 0])
        if result['exit_codes'] != expected:
            result['status'] = 'failed'
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed':
        raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
