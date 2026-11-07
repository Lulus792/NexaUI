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
    for name in ('german','large','small','contrast','light','capture','cancel','maximum','pending-cancel','no-ime','recovery','mixed','directory','owner'):p.add_argument('--'+name,action='store_true')
    a=p.parse_args()
    if a.pending_cancel and not a.owner:a.cancel=True
    if a.recovery and (a.cancel or a.maximum):p.error('Recovery has its own complete initial/child-overflow workflow')
    if a.mixed and (a.recovery or a.cancel):p.error('Mixed input is a separate selection workflow')
    if a.directory and (a.mixed or a.recovery or a.maximum or a.pending_cancel):p.error('Directory selection has its own held-folder workflow')
    if a.owner and (a.directory or a.mixed or a.recovery or a.cancel):p.error('Owner workflow selects a file/folder then cancels a new view')
    if os.getuid()==0:p.error('Run as an unprivileged private test owner')
    paths={name:getattr(a,name).resolve(strict=True) for name in ('server','registry','host','client','font')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700)
    captures=work/'captures';captures.mkdir(mode=0o700);files=work/'files';files.mkdir()
    folder=files/('large' if a.recovery else 'folder');folder.mkdir()
    file_count=4095 if a.maximum else 40
    if a.recovery:
        for i in range(4095):(files/f'root-{i:04d}').write_text(f'root entry {i}\n')
        (files/'document.txt').write_text('retained parent file\n')
        for i in range(4097):(folder/f'item-{i:04d}').write_text(f'original child {i}\n')
    else:
        for i in range(file_count):(files/f'snapshot-{i:04d}.txt').write_text(f'exact original {i}\n')
        (folder/'é.txt').write_text('selected Unicode\n');(folder/'other.txt').write_text('other\n')
    address='unix:path='+str(runtime/'bus')
    env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='picker',NEXAUI_PLATFORM='wayland',
             DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    # The recovery fixture observes actual resize admission/release configures.
    # A flushed client request alone does not prove the compositor began its grab.
    if a.recovery or (a.owner and not a.no_ime):env['WAYLAND_DEBUG']='client'
    for name in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE'):env.pop(name,None)
    for name in ('german','large','small','contrast','light','directory','owner'):
        env.pop('FP_'+name.upper(),None)
        if getattr(a,name):env['FP_'+name.upper()]='1'
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    children=[];streams=[];expected=set();checks=[];images=[];gate_read=gate_write=None
    if a.pending_cancel:
        gate_read,gate_write=os.pipe2(os.O_CLOEXEC);env['FP_TEST_REFRESH_GATE']=str(gate_read)
    result=dict(status='failed',inputs={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items()},checks=checks,images=images,
                configuration={name:getattr(a,name) for name in ('german','large','small','contrast','light','capture','cancel','maximum','pending_cancel','no_ime','recovery','mixed','directory','owner')})
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
    def burst(commands):
        before=view().get('keys',0);os.kill(host.pid,signal.SIGSTOP)
        def stopped():
            state=Path(f'/proc/{host.pid}/status').read_text()
            return re.search(r'^State:\s+T\b',state,re.M) is not None
        wait(stopped,'test-owned host stopped')
        try:
            for command in commands:event(command)
            if not stopped():raise RuntimeError('Host resumed during input burst')
        finally:os.kill(host.pid,signal.SIGCONT)
        expected_keys=sum(command.startswith('K ') for command in commands)
        wait(lambda:view().get('keys',0)>=before+expected_keys,'published complete input burst')
        result.setdefault('bursts',[]).append(dict(commands=commands,keys_before=before,after=view(),host_stopped_during_submission=True))
    def key_commands(*codes):return [f'K {code} {down}' for code in codes for down in (1,0)]
    def view():
        frames=re.findall(r'^VIEW (.+)$',log('host'),re.M)
        return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',frames[-1])} if frames else {}
    def owner_ime_barrier(value):
        marker=f'PREEDIT bytes={value}\n'
        # The host reports the native preedit before rebuilding its field.
        # A completed frame must follow that report, and its v3 commits must
        # actually have reached the pinned producer before the next injection.
        wait(lambda:re.search(r'^VIEW ',log('host')[log('host').rfind(marker)+len(marker):],re.M),'owner IME field frame')
        def settled():
            sent=len(re.findall(r'->\s*zwp_text_input_v3#\d+\.commit\(\)',log('host')))
            received=re.findall(r'^IME commit enabled=1 serial=(\d+)',log('server'),re.M)
            return sent>0 and received and int(received[-1])==sent
        wait(settled,'producer observed owner IME state commits')
        result.setdefault('owner_ime_barriers',[]).append(dict(sent=len(re.findall(r'->\s*zwp_text_input_v3#\d+\.commit\(\)',log('host'))),served=int(re.findall(r'^IME commit enabled=1 serial=(\d+)',log('server'),re.M)[-1])))
    def resize(width,height):
        before=view();count=log('host').count('RESIZE_REQUEST status=0\n')
        def configure_count(bytes_count):return len(re.findall(r'xdg_toplevel#\d+\.configure\([^\n]*array\['+str(bytes_count)+r'\]\)',log('host')))
        admitted=configure_count(8)
        # The pinned test compositor places its first ordinary client at (40,40).
        # Genuine pointer presses drive the production grip and public resize API.
        event(f'M {40+before["width"]-6} {40+before["height"]-6}');event('B 272 1')
        wait(lambda:log('host').count('RESIZE_REQUEST status=0\n')>count,'native resize gesture')
        # The pinned ordinary active window has two states while resizing
        # (activated/resizing), and one after release. No other window/state
        # transition occurs in this fixture; raw protocol traces are retained.
        wait(lambda:configure_count(8)>admitted,'compositor admitted resize')
        until=time.monotonic()+7
        while view().get('width')!=width or view().get('height')!=height:
            if host.poll() is not None or time.monotonic()>=until:raise RuntimeError('Native configure did not reach requested size')
            event(f'M {40+width-6} {40+height-6}');time.sleep(.01)
        released=configure_count(4);event('B 272 0')
        wait(lambda:configure_count(4)>released,'compositor released resize')
        result.setdefault('resizes',[]).append(dict(before=before,after=view(),requested=[width,height]))
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
        initial=r'^OWNER active=0 epoch=0 selections=0 ' if a.owner else r'^VIEW busy=0 state=0 depth=0 count=0 ' if a.recovery else rf'^VIEW busy=0 state=1 depth=0 count={file_count+1} '
        wait(lambda:re.search(initial,log('host'),re.M),'initial observation')
        result['maps']=Path(f'/proc/{host.pid}/maps').read_text()
        if any(s in result['maps'] for s in ('libSDL','libX11','libEGL','libvulkan')):raise RuntimeError('Unexpected native presentation dependency')
        peer_args=[paths['client'],host.pid]+(['--german'] if a.german else [])
        if a.owner:peer_args+=(['--maximum'] if a.maximum else [])+(['--pending-cancel'] if a.pending_cancel else [])+([] if a.no_ime else ['--ime'])
        elif a.directory:peer_args+=(['--cancel'] if a.cancel else [])
        elif a.mixed:peer_args+=(['--maximum'] if a.maximum else [])
        elif not a.recovery:peer_args+=(['--cancel'] if a.cancel else [])+(['--maximum'] if a.maximum else [])+(['--pending-cancel'] if a.pending_cancel else [])+(['--no-ime'] if a.no_ime else [])
        peer=start(peer_args,'client',interactive=True)
        error_pages=0
        for step in range((9 if a.pending_cancel else 8) if a.owner else 3 if a.directory else 5 if a.mixed else 7 if a.recovery else 9):
            wait(lambda:f'STEP {step}\n' in log('client'),f'peer step {step}')
            if a.owner:
                if step==0 and not a.no_ime:
                    seven=log('host').count('PREEDIT bytes=7\n');zero=log('host').count('PREEDIT bytes=0\n')
                    event('I 0');wait(lambda:log('host').count('PREEDIT bytes=7\n')>seven,'owner native preedit')
                    owner_ime_barrier(7)
                    event('I 1');wait(lambda:log('host').count('PREEDIT bytes=0\n')>zero,'owner native commit')
                    owner_ime_barrier(0)
                    seven=log('host').count('PREEDIT bytes=7\n');zero=log('host').count('PREEDIT bytes=0\n')
                    event('I 0');wait(lambda:log('host').count('PREEDIT bytes=7\n')>seven,'owner second native preedit')
                    owner_ime_barrier(7)
                    event('K 42 1');key(15);key(15);event('K 42 0')
                    wait(lambda:log('host').count('PREEDIT bytes=0\n')>zero,'owner focus-loss preedit cancellation')
                    result['owner_ime_preedit_commit_cancel']=True
                key(107 if step==1 else 1 if step==(8 if a.pending_cancel else 7) else 28)
                if a.pending_cancel and step==7:wait(lambda:'ORIGINAL_REFRESH_HELD\n' in log('host'),'owner held original refresh')
            elif a.directory:
                if step in (0,1):key(28)
                else:
                    folder.rename(work/'held-folder');folder.mkdir();(folder/'decoy').write_text('replacement path\n')
            elif a.mixed:
                if step==0:burst(key_commands(15,102))
                if step==1:burst(['K 42 1',*key_commands(15),'K 42 0',*key_commands(107)])
                if step==2:
                    match=re.search(r'^POINTER (\d+) (\d+)$',log('client'),re.M)
                    if not match:raise RuntimeError('Missing peer pointer location')
                    x,y=map(int,match.groups());burst([f'M {40+x} {40+y}','B 272 1','B 272 0',*key_commands(102)])
                if step==3:burst(key_commands(107,102))
                if step==4:
                    start=len(log('host'));burst(key_commands(102,108,107))
                    selected=re.findall(r'^ROW_KEY .*selected=(\d+) page=',log('host')[start:],re.M)
                    if list(map(int,selected))!=[1000,1001,1000+file_count]:raise RuntimeError('Row commands did not preserve their input order')
            elif a.recovery:
                if step==0:capture('initial-error')
                if step==0:(files/'root-4094').unlink()
                if step==2:
                    for _ in range(error_pages):key(109)
                    (folder/'item-4096').unlink()
                if step==4:
                    (folder/'item-4095').rename(work/'retired-item-4095')
                    (folder/'item-4095').write_text('replacement inode\n')
                key(107 if step in (3,6) else 28)
            else:
                if step in (0,4,5,6):key(28)
                if step==1 and not a.no_ime:event('I 0');wait(lambda:'PREEDIT bytes=7\n' in log('host'),'native preedit')
                if step==2:
                    key(15)
                    if not a.no_ime:wait(lambda:'PREEDIT bytes=0\n' in log('host'),'focus-loss preedit cancellation')
                if step==3 and not a.no_ime:event('I 1')
                if step==7:key(107)
            peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush()
            if a.recovery and step==1:
                wait(lambda:re.search(r'^ERROR_READ \d+$',log('client'),re.M),'retained error reading',allowed=peer)
                error_pages=int(re.search(r'^ERROR_READ (\d+)$',log('client'),re.M)[1])
                if not 0<=error_pages<=64:raise RuntimeError('Invalid error reading page count')
                result['error_read_pages']=error_pages;before_keys=view()['keys']
                for _ in range(error_pages):key(104)
                wait(lambda:view().get('keys',0)>=before_keys+2*error_pages,'published error PageUp')
                peer.stdin.write(b'READ_ERROR\n');peer.stdin.flush()
            wait(lambda:f'ACK {step}\n' in log('client'),f'native check {step}',allowed=peer)
            checks.append(dict(step=step))
            if a.owner and step==5:
                previous=files/f'snapshot-{file_count-1:04d}.txt';leaked=[]
                for fd in Path(f'/proc/{host.pid}/fd').iterdir():
                    try:
                        if os.readlink(fd)==str(previous):leaked.append(fd.name)
                    except FileNotFoundError:pass
                if leaked:raise RuntimeError('Owner retained the replaced file witness')
                result['owner_replaced_file_witness_closed']=True
            if step in ((5,) if a.owner else (0,1,4) if a.recovery or a.mixed else (0,4,7)):capture('step-'+str(step))
            if a.recovery and step==3:
                original_size=(view()['width'],view()['height']);original_state=view()
                for phase,size in [('TINY',(64,64)),('SMALL',(240,96)),('RESTORE',original_size)]:
                    wait(lambda:f'RESIZE_{phase}\n' in log('client'),'peer resize phase '+phase)
                    resize(*size)
                    peer.stdin.write(f'SIZE_{phase}\n'.encode());peer.stdin.flush()
                    wait(lambda:f'SIZE_ACK_{phase}\n' in log('client'),'native size check '+phase,allowed=peer)
                    current=view()
                    if any(current.get(k)!=original_state.get(k) for k in ('state','depth','count','page','selected')):raise RuntimeError('Window size changed model or selection')
                    capture('resize-'+phase.lower())
        expected.add(peer);wait(lambda:peer.poll() is not None,'peer exit',allowed=peer)
        marker='PICKER_NATIVE_OWNER_FILE_DIRECTORY_MODAL_RETIREMENT_CANCEL_AND_FOCUS_VERIFIED' if a.owner else 'PICKER_NATIVE_DIRECTORY_FILTERED_EMPTY_AND_HELD_PATH_REPLACEMENT_VERIFIED' if a.directory else 'PICKER_NATIVE_QUEUED_TAB_POINTER_TEXT_AND_ROW_COMMAND_ORDER_VERIFIED' if a.mixed else 'PICKER_NATIVE_INITIAL_RETRY_RETAINED_ENTER_RETRY_CHANGED_REFRESH_AND_RESIZE_VERIFIED' if a.recovery else 'PICKER_NATIVE_KEYS_IME_FILTER_PAGING_AND_RETIREMENT_VERIFIED'
        if peer.returncode or marker not in log('client'):raise RuntimeError('Independent peer failed')
        if a.pending_cancel:wait(lambda:'ORIGINAL_REFRESH_HELD\n' in log('host'),'held original refresh')
        if a.mixed:
            # This burst intentionally closes the host, so it has no final VIEW.
            os.kill(host.pid,signal.SIGSTOP)
            wait(lambda:re.search(r'^State:\s+T\b',Path(f'/proc/{host.pid}/status').read_text(),re.M),'host stopped for final selection')
            commands=key_commands(102,108,28)
            try:
                for command in commands:event(command)
            finally:os.kill(host.pid,signal.SIGCONT)
            result.setdefault('bursts',[]).append(dict(commands=commands,host_stopped_during_submission=True,closes=True))
        else:key(28)
        if a.pending_cancel:
            wait(lambda:'GUI_GONE\n' in log('host'),'GUI retirement before original I/O completion')
            if host.poll() is not None:raise RuntimeError('Worker ended before its original read was released')
            os.write(gate_write,b'R');os.close(gate_write);gate_write=None
        wait(lambda:host.poll() is not None,'selected or cancelled close',allowed=host);expected.add(host)
        requests=(7 if a.pending_cancel else 6) if a.owner else (3 if a.cancel else 4) if a.directory else 2 if a.mixed else 9 if a.recovery else 6 if a.pending_cancel or not a.cancel else 5
        if host.returncode or f'PICKER_CLOSED picked={0 if a.cancel else 1} requests={requests} failed=0' not in log('host'):raise RuntimeError('Picker did not close normally')
        if a.pending_cancel and not log('host').index('GUI_GONE\n')<log('host').index('ORIGINAL_REFRESH_RELEASED\n'):raise RuntimeError('Original I/O escaped the held lifetime check')
        if a.owner:
            selected=(files/f'snapshot-{file_count-1:04d}.txt').stat();chosen_folder=folder.stat()
            if f'OWNER_RESULT count=1 directories=0 device={selected.st_dev} inode={selected.st_ino}\n' not in log('host') or f'OWNER_RESULT count=2 directories=1 device={chosen_folder.st_dev} inode={chosen_folder.st_ino}\n' not in log('host'):raise RuntimeError('Owner received the wrong selection')
            if log('host').count('OWNER_RESULT ')!=2 or 'OWNER_CLOSED selections=2\n' not in log('host'):raise RuntimeError('Cancel or stale replies changed owner result count')
        elif not a.cancel:
            leaf='.' if a.directory else 'snapshot-0000.txt' if a.mixed else 'item-4095' if a.recovery else f'snapshot-{file_count-1:04d}.txt'
            selected=(work/'held-folder').stat() if a.directory else ((folder if a.recovery else files)/leaf).stat()
            if a.directory:
                replacement=folder.stat()
                if (selected.st_dev,selected.st_ino)==(replacement.st_dev,replacement.st_ino):raise RuntimeError('Directory fixture did not replace the held path')
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
                if a.mixed and name=='host':os.kill(child.pid,signal.SIGCONT)
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
