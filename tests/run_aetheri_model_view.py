"""Exercise the external Aetheri-model view on a private test compositor."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','view','font','work-dir'):
        p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args()
    server,view,font=[path.resolve(strict=True) for path in (args.server,args.view,args.font)]
    root=args.work_dir.resolve(); root.mkdir(mode=0o700)
    runtime=root/'runtime'; runtime.mkdir(mode=0o700)
    captures=root/'captures'; captures.mkdir(mode=0o700)
    env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='nexaui-model',NEXAUI_PLATFORM='wayland')
    for key in ('DISPLAY','WAYLAND_SOCKET','AT_SPI_BUS_ADDRESS','DBUS_SESSION_BUS_ADDRESS'):
        env.pop(key,None)
    owner,peer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET); owner.settimeout(5)
    compositor=app=None
    result={'status':'failed','server_sha256':sha(server),'view_sha256':sha(view),'font_sha256':sha(font),
            'scope':'Aetheri headless test compositor with virtual input; no physical or human acceptance'}
    try:
        with (root/'server.log').open('wb') as sl,(root/'view.log').open('wb') as al:
            compositor=subprocess.Popen([str(server),'--headless',env['WAYLAND_DISPLAY'],str(captures),str(peer.fileno())],
                env=env,stdin=subprocess.DEVNULL,stdout=sl,stderr=subprocess.STDOUT,pass_fds=(peer.fileno(),))
            peer.close()
            def log(name): return (root/name).read_text(errors='replace')
            def wait(predicate,label):
                end=time.monotonic()+15
                while not predicate():
                    if compositor.poll() is not None or (app and app.poll() is not None) or time.monotonic()>=end:
                        raise RuntimeError('failed waiting for '+label)
                    time.sleep(.02)
            def event(value):
                owner.sendall(value.encode('ascii'))
                if owner.recv(32)!=b'OK': raise RuntimeError('input driver rejected '+value)
            def key(code):
                event(f'K {code} 1'); event(f'K {code} 0'); time.sleep(.12)
            wait(lambda:'READY ' in log('server.log'),'compositor')
            app=subprocess.Popen([str(view),str(font),'--verify'],env=env,stdin=subprocess.DEVNULL,stdout=al,stderr=subprocess.STDOUT)
            wait(lambda:'MODEL theme=0 german=0 text=100\n' in log('view.log') and 'MAP 0\n' in log('server.log'),'initial model')
            for _ in range(3): key(15)
            key(57); wait(lambda:'MODEL theme=2 german=0 text=100\n' in log('view.log'),'contrast model')
            for _ in range(2): key(15)
            key(57); wait(lambda:'MODEL theme=2 german=1 text=100\n' in log('view.log'),'language model')
            for _ in range(3): key(15)
            key(57); wait(lambda:'MODEL theme=2 german=1 text=200\n' in log('view.log'),'scale model')
            compositor.send_signal(signal.SIGUSR1)
            wait(lambda:all(f'CAPTURE 1 {i}\n' in log('server.log') for i in range(3)),'output capture')
            image=(captures/'capture-1-output-0.ppm').read_bytes()
            magic,dimensions,peak,pixels=image.split(b'\n',3)
            width,height=map(int,dimensions.split())
            if magic!=b'P6' or peak!=b'255' or (width,height)!=(1920,1080) or len(pixels)!=width*height*3:
                raise RuntimeError('bad capture')
            result['capture_sha256']=hashlib.sha256(image).hexdigest()
            key(1)
            if app.wait(timeout=10)!=0: raise RuntimeError('view did not close cleanly')
            result['status']='passed'
    finally:
        if app and app.poll() is None: app.terminate(); app.wait(timeout=5)
        if compositor and compositor.poll() is None: compositor.send_signal(signal.SIGTERM); compositor.wait(timeout=10)
        peer.close(); owner.close()
        result['view_exit']=app.returncode if app else None; result['server_exit']=compositor.returncode if compositor else None
        if result['view_exit']!=0 or result['server_exit']!=0: result['status']='failed'
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    if result['status']!='passed': raise RuntimeError('failed; see '+str(root))
    print(json.dumps(result,indent=2))


if __name__=='__main__': main()
