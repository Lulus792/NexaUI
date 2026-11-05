"""Exercise a NexaUI model view with Aetheri's real development supervisor.

Uses Aetheri's existing headless supervisor fixture and isolated child binaries;
does not start or change the carrier's development session or app installation.
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
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture', 'owner', 'server', 'desktop', 'view', 'work-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    fixture, owner_source, server, desktop, view = [p.resolve(strict=True) for p in
        (args.fixture, args.owner, args.server, args.desktop, args.view)]
    root = args.work_dir.resolve(); root.mkdir(mode=0o700)
    build = root / 'build'; build.mkdir(mode=0o700)
    for name, target in (('test_compositor_server', server), ('aetheri-desktop', desktop),
                         ('aetheri-settings', view)):
        (build / name).symlink_to(target)
    for name in ('captures', 'sessions', 'sessions/fixture', 'sessions/fixture/runtime', 'sessions/fixture/cache'):
        (root / name).mkdir(mode=0o700)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    result = {'status': 'failed', 'owner_sha256': digest(owner_source), 'fixture_sha256': digest(fixture),
              'server_sha256': digest(server), 'desktop_sha256': digest(desktop), 'view_sha256': digest(view),
              'scope': 'original supervisor with its headless test fixture and isolated binaries; no app admission or physical acceptance'}
    child = None
    try:
        with (root / 'owner.log').open('wb') as stream:
            child = subprocess.Popen([sys.executable, str(fixture), str(build), str(root), str(issuer.fileno()),
                                      str(owner_source)], cwd=root, pass_fds=(issuer.fileno(),),
                                     stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT)
            issuer.close()
            def log(): return (root / 'owner.log').read_text(errors='replace')
            def wait(predicate, label):
                end = time.monotonic() + 15
                while not predicate():
                    if child.poll() is not None or time.monotonic() >= end:
                        raise RuntimeError('waiting for ' + label + ': ' + log()[-2000:])
                    time.sleep(.02)
            def event(value):
                controller.sendall(value.encode('ascii'))
                if controller.recv(32) != b'OK': raise RuntimeError('rejected input ' + value)
            def key(code, down): event(f'K {code} {int(down)}')
            def tap(code): key(code, True); key(code, False)
            codes = dict(zip('settings', (31, 18, 20, 20, 23, 49, 34, 31)))
            def launch():
                count = log().count('desktop: FOCUS 0\n')
                event('M 100 100'); key(125, True); tap(57); key(125, False)
                wait(lambda: log().count('desktop: FOCUS 0\n') > count, 'search focus')
                for char in 'settings': tap(codes[char])
                wait(lambda: bool(re.search(r'desktop: VIEW .*query=settings ', log())), 'search query')
                tap(28)
            wait(lambda: 'FIXTURE_NOTIFY READY=1' in log() and 'desktop: STABLE 2 3840x84' in log(), 'desktop')
            launch()
            wait(lambda: len(re.findall(r'^SPAWN settings \d+$', log(), re.M)) == 1 and
                 'settings: MODEL theme=0 german=0 text=100' in log(), 'owner-launched Settings')
            first = int(re.findall(r'^SPAWN settings (\d+)$', log(), re.M)[0])
            wait(lambda: bool(re.search(r'desktop: VIEW .*output=0 .*windows=1 ', log())), 'desktop observes Settings')
            # Clicking the Settings dock entry again must activate the observed
            # original window, not merely receive a successful launch reply.
            rows=re.findall(r'desktop: VIEW (\d+) output=0 search=([01]) size=(\d+)x(\d+).*windows=1 ',log())
            frame,expanded,_,height=map(int,rows[-1])
            match=re.search(r'desktop: CONTROL '+str(frame)+r' 0 2 0 ([\d.]+) ([\d.]+)',log())
            if not match: raise RuntimeError('Settings dock control missing')
            x,y=map(float,match.groups()); activated=len(re.findall(r'desktop: ACTIVATED \d+ 1',log()))
            event(f'M {x} {y if expanded else 1080-height+y}'); event('B 272 1'); event('B 272 0')
            wait(lambda: len(re.findall(r'desktop: ACTIVATED \d+ 1',log()))>activated,'existing Settings activation')
            if len(re.findall(r'^SPAWN settings \d+$',log(),re.M))!=1:
                raise RuntimeError('existing Settings was duplicated')
            key(29, True); tap(16); key(29, False)
            wait(lambda: 'desktop: LAUNCH_REPLY X S' in log(), 'first child and pipe drained')
            launch()
            def reopened():
                text=log(); spawns=list(re.finditer(r'^SPAWN settings \d+$', text, re.M))
                return len(spawns)==2 and 'settings: MODEL theme=0 german=0 text=100' in text[spawns[-1].end():]
            wait(reopened, 'Settings reopened')
            second = int(re.findall(r'^SPAWN settings (\d+)$', log(), re.M)[1])
            if first == second: raise RuntimeError('Settings was not reopened')
            wait(lambda: bool(re.search(r'desktop: VIEW .*windows=1 ', log().split('SPAWN settings ' + str(second),1)[1])),
                 'desktop observes reopened window')
            child.send_signal(signal.SIGTERM)
            if child.wait(timeout=15) != 0: raise RuntimeError('supervisor shutdown failed')
            ending=re.search(r'^SESSION_END (.*)$',log(),re.M)
            if 'settings: SHUTDOWN signal\n' not in log() or not ending:
                raise RuntimeError('ordered signal cleanup not observed')
            summary=json.loads(ending.group(1))
            if summary['children'] != {'compositor': 0, 'desktop': 0, 'settings': 0} or not any(
                    item['role']=='settings' and item['pid']==first and item['status']==0 for item in summary['app_history']):
                raise RuntimeError('original children or first Settings did not exit cleanly')
            result.update(status='passed', first_pid=first, reopened_pid=second)
    finally:
        if child and child.poll() is None:
            child.terminate()
            try: child.wait(timeout=10)
            except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
        issuer.close(); controller.close()
        result['owner_exit'] = child.returncode if child else None
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'passed': raise RuntimeError('integration failed; see ' + str(root))
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
