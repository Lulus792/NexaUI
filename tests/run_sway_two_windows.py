"""Request a two-window close on an explicitly private Sway session."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time


def main():
    if len(sys.argv) not in (3,4) or (len(sys.argv)==4 and sys.argv[3]!='--poll-many'):
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_SWAY') != '1' or not os.environ.get('SWAYSOCK'):
        return 77
    host, swaymsg = sys.argv[1:3]
    group=len(sys.argv)==4
    with tempfile.TemporaryDirectory(prefix='nexaui-sway-two-windows-') as tmp:
        done = Path(tmp) / 'done'
        env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_W02_NATIVE_CLOSE='1',
                   NEXAUI_W02_DONE_FILE=str(done))
        env.pop('NEXAUI_W02_CLOSE_FILE', None)
        read_fd,write_fd=os.pipe() if group else (-1,-1)
        if group:
            env['NEXAUI_W02_POLL_MANY']='1';env['NEXAUI_W02_CONTROL_FD']=str(read_fd)
            env.pop('NEXAUI_W02_DONE_FILE')
        process = subprocess.Popen([host], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                   pass_fds=(read_fd,) if group else ())
        if group: os.close(read_fd)
        messages = queue.Queue()
        output = []
        def collect():
            for line in process.stdout:
                output.append(line.rstrip())
                messages.put(line.rstrip())
        thread = threading.Thread(target=collect, daemon=True)
        thread.start()
        def wait(value):
            deadline=time.monotonic()+10
            while time.monotonic()<deadline:
                try:
                    line=messages.get(timeout=max(.01,deadline-time.monotonic()))
                except queue.Empty:
                    break
                if line==value:
                    return
            detail=process.stderr.read() if process.poll() is not None else 'host still running'
            raise RuntimeError(f'Missing {value}: {output}; {detail}')
        def command(*args):
            result=subprocess.run([swaymsg, '-r', *args], env=env, capture_output=True,
                                  text=True, check=True, timeout=5)
            return json.loads(result.stdout)
        def nodes(tree):
            yield tree
            for child in tree.get('nodes', []) + tree.get('floating_nodes', []):
                yield from nodes(child)
        try:
            wait('WINDOWS_OPEN')
            result=command('[title="^NexaUI W02 A$"] kill')
            if not result or not all(item.get('success') for item in result):
                raise RuntimeError(f'Sway rejected targeted close: {result}')
            wait('REQUESTED A')
            wait('CLOSED A')
            wait('SURVIVOR_PRESENTED')
            remaining=[node.get('name') for node in nodes(command('-t','get_tree'))
                       if node.get('name') in ('NexaUI W02 A','NexaUI W02 B')]
            if remaining != ['NexaUI W02 B']:
                raise RuntimeError(f'Unexpected remaining windows: {remaining}')
            result=command('[title="^NexaUI W02 B$"] focus')
            if not result or not all(item.get('success') for item in result):
                raise RuntimeError(f'Surviving window cannot be focused: {result}')
            focused=[node for node in nodes(command('-t','get_tree')) if node.get('name')=='NexaUI W02 B']
            if len(focused)!=1 or not focused[0].get('focused'):
                raise RuntimeError('Sway did not report the survivor as focused')
            if group: os.write(write_fd,b'D')
            else: done.write_text('done',encoding='ascii')
            wait('WINDOWS_CLOSED')
            if process.wait(timeout=5):
                raise RuntimeError('Host failed: '+process.stderr.read())
            print('Sway close request affected A; B survived, presented and accepted a focus request.')
            return 0
        finally:
            if group: os.close(write_fd)
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            thread.join(timeout=2)


if __name__ == '__main__':
    raise SystemExit(main())
