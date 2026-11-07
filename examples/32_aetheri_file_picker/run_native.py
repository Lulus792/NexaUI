"""Ordinary native picker, original filesystem model and independent AT-SPI peer."""
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


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','registry','host','client','font','work-dir'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('german','large','small','contrast','light','capture','cancel','maximum','pending-cancel','no-ime'):p.add_argument('--'+name,action='store_true')
    a=p.parse_args()
    if a.pending_cancel:a.cancel=True
    if os.getuid()==0:p.error('Run as an unprivileged private test owner')
    paths={name:getattr(a,name).resolve(strict=True) for name in ('server','registry','host','client','font')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700)
    captures=work/'captures';captures.mkdir(mode=0o700);files=work/'files';files.mkdir();folder=files/'folder';folder.mkdir()
    file_count=4095 if a.maximum else 40
    for i in range(file_count):(files/f'snapshot-{i:04d}.txt').write_text(f'exact original {i}\n')
    (folder/'é.txt').write_text('selected Unicode\n');(folder/'other.txt').write_text('other\n')
    address='unix:path='+str(runtime/'bus')
    env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='picker',NEXAUI_PLATFORM='wayland',
             DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for name in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE'):env.pop(name,None)
    for name in ('german','large','small','contrast','light'):
        env.pop('FP_'+name.upper(),None)
        if getattr(a,name):env['FP_'+name.upper()]='1'
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    children=[];streams=[];expected=set();checks=[];images=[];gate_read=gate_write=None
    if a.pending_cancel:
        gate_read,gate_write=os.pipe2(os.O_CLOEXEC);env['FP_TEST_REFRESH_GATE']=str(gate_read)
    result=dict(status='failed',inputs={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items()},checks=checks,images=images,
                configuration={name:getattr(a,name) for name in ('german','large','small','contrast','light','capture','cancel','maximum','pending_cancel','no_ime')})
    def log(name):return (work/(name+'.log')).read_text(errors='replace')
    def start(command,name,pass_fds=(),interactive=False):
        stream=(work/(name+'.log')).open('xb');streams.append(stream)
        child=subprocess.Popen(list(map(str,command)),stdin=subprocess.PIPE if interactive else subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,env=env,pass_fds=pass_fds,cwd=work,start_new_session=True)
        children.append((name,child));return child
    def wait(predicate,label,allowed=None):
        deadline=time.monotonic()+7
        while not predicate():
            if any(c not in expected and c is not allowed and c.poll() is not None for _,c in children):raise RuntimeError(label+': child ended')
            if time.monotonic()>=deadline:raise RuntimeError(label+': timeout')
            time.sleep(.01)
    def event(command):
        left.sendall(command.encode('ascii'))
        if left.recv(64)!=b'OK':raise RuntimeError('Rejected compositor command: '+command)
    def key(code):event(f'K {code} 1');event(f'K {code} 0')
    def capture(label):
        if not a.capture:return
        sequence=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)),'capture '+label)
        for i in range(3):
            path=captures/f'capture-{sequence}-output-{i}.ppm';data=path.read_bytes();magic,extent,maximum,pixels=data.split(b'\n',3);w,h=map(int,extent.split())
            if magic!=b'P6' or maximum!=b'255' or len(pixels)!=w*h*3:raise RuntimeError('Incomplete PPM')
            images.append(dict(label=label,file=path.name,sha256=hashlib.sha256(data).hexdigest()))
    try:
        server=start([paths['server'],'--headless','picker',captures if a.capture else '-',right.fileno()],'server',(right.fileno(),));right.close()
        wait(lambda:'READY ' in log('server'),'compositor')
        if not a.no_ime:event('I on')
        start(['dbus-daemon','--session','--nofork','--address='+address],'bus');wait(lambda:(runtime/'bus').exists(),'bus')
        start([paths['registry']],'registry')
        def registered():
            r=subprocess.run(['dbus-send','--bus='+address,'--print-reply','--dest=org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','string:org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2)
            return r.returncode==0 and 'boolean true' in r.stdout
        wait(registered,'registry')
        host=start([paths['host'],paths['font'],files],'host',(gate_read,) if a.pending_cancel else ())
        if gate_read is not None:os.close(gate_read);gate_read=None
        wait(lambda:re.search(rf'^VIEW busy=0 state=1 depth=0 count={file_count+1} ',log('host'),re.M),'initial complete observation')
        result['maps']=Path(f'/proc/{host.pid}/maps').read_text()
        if any(s in result['maps'] for s in ('libSDL','libX11','libEGL','libvulkan')):raise RuntimeError('Unexpected native presentation dependency')
        peer=start([paths['client'],host.pid]+(['--german'] if a.german else [])+(['--cancel'] if a.cancel else [])+(['--maximum'] if a.maximum else [])+(['--pending-cancel'] if a.pending_cancel else [])+(['--no-ime'] if a.no_ime else []),'client',interactive=True)
        for step in range(9):
            wait(lambda:f'STEP {step}\n' in log('client'),f'peer step {step}')
            if step in (0,4,5,6):key(28)
            if step==1 and not a.no_ime:event('I 0');wait(lambda:'PREEDIT bytes=7\n' in log('host'),'native preedit')
            if step==2:
                key(15)
                if not a.no_ime:wait(lambda:'PREEDIT bytes=0\n' in log('host'),'focus-loss preedit cancellation')
            if step==3 and not a.no_ime:event('I 1')
            if step==7:key(107)
            peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush()
            wait(lambda:f'ACK {step}\n' in log('client'),f'native check {step}',allowed=peer)
            checks.append(dict(step=step))
            if step in (0,4,7):capture('step-'+str(step))
        expected.add(peer);wait(lambda:peer.poll() is not None,'peer exit',allowed=peer)
        if peer.returncode or 'PICKER_NATIVE_KEYS_IME_FILTER_PAGING_AND_RETIREMENT_VERIFIED' not in log('client'):raise RuntimeError('Independent peer failed')
        if a.pending_cancel:wait(lambda:'ORIGINAL_REFRESH_HELD\n' in log('host'),'held original refresh')
        key(28)
        if a.pending_cancel:
            wait(lambda:'GUI_GONE\n' in log('host'),'GUI retirement before original I/O completion')
            if host.poll() is not None:raise RuntimeError('Worker ended before its original read was released')
            os.write(gate_write,b'R');os.close(gate_write);gate_write=None
        wait(lambda:host.poll() is not None,'selected or cancelled close',allowed=host);expected.add(host)
        if host.returncode or f'PICKER_CLOSED picked={0 if a.cancel else 1} requests={6 if a.pending_cancel or not a.cancel else 5} failed=0' not in log('host'):raise RuntimeError('Picker did not close normally')
        if a.pending_cancel and not log('host').index('GUI_GONE\n')<log('host').index('ORIGINAL_REFRESH_RELEASED\n'):raise RuntimeError('Original I/O escaped the held lifetime check')
        if not a.cancel:
            leaf=f'snapshot-{file_count-1:04d}.txt';selected=(files/leaf).stat()
            wanted=f'PICKED leaf_hex={leaf.encode().hex()} device={selected.st_dev} inode={selected.st_ino}\n'
            if wanted not in log('host'):raise RuntimeError('Wrong ordinary-file witness')
        for path in work.glob('*.log'):
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',path.read_text(errors='replace')):raise RuntimeError('Sanitizer diagnostic')
        result.update(status='passed',host_exit=host.returncode,peer_exit=peer.returncode,cancel=a.cancel)
    finally:
        cleanup=[]
        for name,child in reversed(children):
            forced=False
            if child.poll() is None:
                os.killpg(child.pid,signal.SIGTERM)
                try:child.wait(timeout=3)
                except subprocess.TimeoutExpired:forced=True;os.killpg(child.pid,signal.SIGKILL);child.wait(timeout=3)
            cleanup.append(dict(name=name,exit=child.returncode,forced=forced))
        left.close();right.close()
        if gate_read is not None:os.close(gate_read)
        if gate_write is not None:os.close(gate_write)
        for stream in streams:stream.close()
        result['cleanup']=cleanup;(work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
