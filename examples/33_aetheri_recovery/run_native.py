"""Private original-owner native Wayland UI and independent OS accessibility peer."""
import argparse
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import sqlite3
import struct
import subprocess
import sys
import time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','registry','host','client','font','source','work-dir'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--kind',choices=('system','user'),default='system')
    p.add_argument('--held',choices=('wake','close'),help='Test-only held host and lifecycle OS peer')
    p.add_argument('--clipboard',choices=('normal','close'),help='Independent wl-clipboard transfer/focus or pending-close workflow')
    p.add_argument('--clipboard-tools',type=Path,help='Private bin/wl-copy, bin/wl-paste and lib/loader+runtime directory')
    for name in ('german','large','small','contrast','light','ime','navigation','resize','capture'):p.add_argument('--'+name,action='store_true')
    a=p.parse_args()
    if a.resize:a.navigation=True
    if a.capture and not a.resize:p.error('Capture is attached to the native resize workflow')
    if a.held and a.ime:p.error('The held-transaction workflow has its own lifecycle peer; run IME separately')
    if a.held and a.navigation:p.error('Run ordinary navigation separately from the held-transaction peer')
    if a.clipboard and (a.held or a.navigation):p.error('Clipboard has a separate ordinary-peer sequence')
    if bool(a.clipboard)!=bool(a.clipboard_tools):p.error('Clipboard requires its explicitly selected private tools')
    if os.getuid()==0:p.error('Use an unprivileged private owner')
    paths={name:getattr(a,name).resolve(strict=True) for name in ('server','registry','host','client','font','source')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700);directory=work/'owner';directory.mkdir(mode=0o700);db_path=directory/'catalogue.sqlite3'
    captures=work/'captures';images=[]
    if a.capture:captures.mkdir(mode=0o700)
    sys.dont_write_bytecode=True;sys.path.insert(0,str(paths['source']/'tools'))
    import catalogue_contracts as cc
    from test_contracts import legacy,rows,plan,accept
    from test_recovery_inventory import pending_metadata,expected
    with closing(sqlite3.connect(db_path)) as db,closing(sqlite3.connect(work/'backup.sqlite3')) as backup:
        fixture=legacy(db,a.kind);cc.migrate_v1_to_v2(db,a.kind,backup=backup);pending_metadata(db,fixture,a.kind)
        for i in range(35):accept(db,plan(db,fixture))
        db.commit()
        versions=(3,4,5,6,7,8,9,10) if a.kind=='system' else (3,4,7,8)
        for version in versions:
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,a.kind)
        before_rows=rows(db);reference=expected(db,a.kind,versions[-1])
    db_path.chmod(0o600);before_sha=hashlib.sha256(db_path.read_bytes()).hexdigest()
    kinds={1:('Operation','Vorgang'),5:('Code lease','Code-Lease'),11:('Data epoch','Datenepoche')}
    def row_name(record):return kinds[record[0]][bool(a.german)]+' · '+record[5]+'\n'+record[1]
    first_name,last_name=row_name(reference[0]),row_name(reference[-1])
    address='unix:path='+str(runtime/'bus')
    env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='recovery',NEXAUI_PLATFORM='wayland',DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,
             ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',PYTHONDONTWRITEBYTECODE='1')
    for name in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE'):env.pop(name,None)
    for name in ('german','large','small','contrast','light'):
        env.pop('RVG_'+name.upper(),None)
        if getattr(a,name):env['RVG_'+name.upper()]='1'
    if a.ime or a.resize or a.clipboard:env['WAYLAND_DEBUG']='client'
    gate_read,gate_write=os.pipe2(os.O_CLOEXEC) if a.held else (-1,-1)
    if a.held:env['RVG_TEST_GATE_FD']=str(gate_read)
    else:env.pop('RVG_TEST_GATE_FD',None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4);children=[];streams=[];background=set();stopped=set()
    result={'status':'failed','inputs':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items() if path.is_file()},
            'configuration':{name:getattr(a,name) for name in ('kind','german','large','small','contrast','light','ime','held','navigation','resize','capture','clipboard')},'checks':[],'expected_records':len(reference),'images':images}
    def start(command,name,fds=(),input_pipe=False,may_exit=False):
        stream=(work/(name+'.log')).open('wb');streams.append(stream)
        child=subprocess.Popen(list(map(str,command)),env=env,stdin=subprocess.PIPE if input_pipe else subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=fds,start_new_session=True)
        children.append((name,child))
        if may_exit:background.add(child)
        return child
    def log(name):return (work/(name+'.log')).read_text(errors='replace')
    def wait(test,label,allowed=()):
        until=time.monotonic()+7
        while not test():
            for name,child in children:
                if child not in allowed and child not in background and child.poll() is not None:raise RuntimeError(label+': '+name+' exited '+str(child.returncode))
            if time.monotonic()>=until:raise RuntimeError(label+': timeout')
            time.sleep(.01)
    def event(command):
        left.sendall(command.encode('ascii'))
        if left.recv(64)!=b'OK':raise RuntimeError('Compositor rejected '+command)
    def key(code):event(f'K {code} 1');event(f'K {code} 0')
    def control(code):event('K 29 1');key(code);event('K 29 0')
    def reverse_tab():event('K 42 1');key(15);event('K 42 0')
    def view():
        lines=re.findall(r'^VIEW (.+)$',log('host'),re.M)
        return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',lines[-1])} if lines else {}
    def resize(width,height):
        before=view();count=log('host').count('RESIZE_REQUEST status=0\n')
        def configures(size):return list(re.finditer(r'xdg_toplevel#\d+\.configure\([^\n]*array\['+str(size)+r'\]\)',log('host')))
        admitted=len(configures(8))
        # Pinned original compositor: client origin (40,40), active+resizing
        # states during its real grab, active alone after pointer release.
        event(f'M {40+before["width"]-6} {40+before["height"]-6}');event('B 272 1')
        wait(lambda:log('host').count('RESIZE_REQUEST status=0\n')>count,'native resize grip request')
        wait(lambda:len(configures(8))>admitted,'compositor admitted native resize')
        until=time.monotonic()+7
        while view().get('width')!=width or view().get('height')!=height:
            if host.poll() is not None or time.monotonic()>=until:raise RuntimeError('Native resize did not reach requested geometry')
            event(f'M {40+width-6} {40+height-6}');time.sleep(.01)
        released=len(configures(4));event('B 272 0');wait(lambda:len(configures(4))>released,'compositor released resize')
        boundary=configures(4)[-1].end()
        wait(lambda:re.search(r'^VIEW [^\n]*width='+str(width)+r' height='+str(height)+r' [^\n]*\nRECOVERY_READY\n',log('host')[boundary:],re.M) is not None,'submitted frame after released configure')
        after=view()
        if any(after.get(k)!=before.get(k) for k in ('state','epoch','count','mode','request')):raise RuntimeError('Resize changed owner/filter state')
        result.setdefault('resizes',[]).append({'before':before,'after':after,'requested':[width,height]})
    def capture(label):
        if not a.capture:return
        sequence=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)),'native capture '+label)
        for i in range(3):
            path=captures/f'capture-{sequence}-output-{i}.ppm';data=path.read_bytes();magic,extent,maximum,pixels=data.split(b'\n',3);width,height=map(int,extent.split())
            if magic!=b'P6' or maximum!=b'255' or len(pixels)!=width*height*3:raise RuntimeError('Incomplete native PPM')
            images.append({'label':label,'file':path.name,'width':width,'height':height,'sha256':hashlib.sha256(data).hexdigest(),'view':view()})
    def foreign(tool):
        root=a.clipboard_tools.resolve(strict=True);loader=root/'lib/ld-musl-x86_64.so.1';binary=root/'bin'/tool
        for path in (loader,binary):
            path.resolve(strict=True);result.setdefault('clipboard_tool_inputs',{})[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
        return [loader,'--library-path',root/'lib',binary]
    def foreign_read(expected,owner=None):
        until=time.monotonic()+7
        while True:
            if owner is not None and owner.poll() is not None:raise RuntimeError('Independent clipboard owner exited before publication')
            done=subprocess.run([*foreign('wl-paste'),'--no-newline'],env=env,capture_output=True,timeout=3)
            result.setdefault('clipboard_probes',[]).append({'exit':done.returncode,'length':len(done.stdout),'sha256':hashlib.sha256(done.stdout).hexdigest(),'stderr':done.stderr.decode(errors='replace')})
            if done.returncode==0 and done.stdout==expected:return
            if time.monotonic()>=until:raise RuntimeError('Independent clipboard probe did not receive exact bytes')
            time.sleep(.01)
    def foreign_copy(payload,ordinal):
        # Fixed public test text: wl-copy's argument path avoids its stdin spool.
        child=start([*foreign('wl-copy'),'--foreground','--type','text/plain;charset=utf-8','--',payload.decode('utf-8')],'clipboard-source'+str(ordinal),may_exit=True)
        name='clipboard-source'+str(ordinal)
        wait(lambda:re.search(r'->\s*wl_data_device#\d+\.set_selection\(',log(name)) is not None and re.search(r'->\s*wl_surface#\d+\.destroy\(\)',log(name)) is not None,'foreign selection published and popup retired')
        foreign_read(payload,child)
        if child.poll() is not None:raise RuntimeError('Independent clipboard owner exited before transfer')
        return child
    def source_returned_to_socket_wait(child,send_before):
        # Parent-owned Linux x86-64 child: its send callback must have returned
        # before it can block on the next Wayland socket read/poll. No extra
        # wl-paste popup may steal the consumer's focus during this barrier.
        if sys.platform!='linux' or os.uname().machine!='x86_64':raise RuntimeError('Clipboard send witness requires Linux x86-64')
        sends=len(re.findall(r'wl_data_source#\d+\.send\(',log('clipboard-source2')))
        if sends<=send_before:return False
        try:
            raw=Path(f'/proc/{child.pid}/syscall').read_text().split();descriptors=[]
            if raw[0] in ('0','47'):descriptors=[int(raw[1],0)]
            elif raw[0] in ('7','271'):
                address,nfds=int(raw[1],0),int(raw[2],0)
                if not 1<=nfds<=16:return False
                with Path(f'/proc/{child.pid}/mem').open('rb',buffering=0) as memory:data=os.pread(memory.fileno(),nfds*8,address)
                if len(data)!=nfds*8:return False
                descriptors=[fd for fd,events,_ in struct.iter_unpack('<ihh',data) if events&1]
            else:return False
            sockets={fd:os.readlink(f'/proc/{child.pid}/fd/{fd}') for fd in descriptors}
            if not sockets or not all(name.startswith('socket:[') for name in sockets.values()):return False
            result['clipboard_send_return_witness']={'syscall':raw,'sockets':sockets,'source_sends_before':send_before,'source_sends_after':sends};return True
        except (FileNotFoundError,ProcessLookupError,IndexError):return False
    def ime_barrier(bytes_count):
        marker=f'PREEDIT bytes={bytes_count}\n';wait(lambda:marker in log('host'),'native preedit change')
        wait(lambda:re.search(r'^VIEW ',log('host')[log('host').rfind(marker)+len(marker):],re.M) is not None,'field frame after preedit')
        def settled():
            sent=len(re.findall(r'->\s*zwp_text_input_v3#\d+\.commit\(\)',log('host')))
            served=re.findall(r'^IME commit enabled=1 serial=(\d+)',log('server'),re.M)
            return sent>0 and served and int(served[-1])==sent
        wait(settled,'producer processed text-input commits')
    def release_gate():
        nonlocal gate_write
        if gate_write>=0:
            try:os.write(gate_write,b'R')
            finally:os.close(gate_write);gate_write=-1
    def sleeping_with_worker_fd():
        # The selected carrier is Linux x86-64. Observe the parent's actual child
        # in poll, then read its bounded pollfd array; no guessed sleep interval.
        if sys.platform!='linux' or os.uname().machine!='x86_64':raise RuntimeError('Native wait witness requires the selected Linux x86-64 carrier')
        matches=re.findall(r'^HELD_WAIT_ENTER fd=(\d+) timeout=(\d+)$',log('host'),re.M)
        if not matches:return False
        try:
            raw=Path(f'/proc/{host.pid}/syscall').read_text().split()
            if len(raw)<4 or raw[0]!='7':return False
            address,nfds=int(raw[1],0),int(raw[2],0)
            if not 1<=nfds<=32:return False
            with Path(f'/proc/{host.pid}/mem').open('rb',buffering=0) as memory:data=os.pread(memory.fileno(),nfds*8,address)
            if len(data)!=nfds*8:return False
            descriptors=[{'fd':fd,'events':events,'revents':revents} for fd,events,revents in struct.iter_unpack('<ihh',data)]
            wake_fd=int(matches[-1][0])
            if not any(d['fd']==wake_fd and d['events']&1 for d in descriptors):return False
            result['native_wait_witness']={'syscall':raw,'descriptors':descriptors,'worker_fd':wake_fd,'view':view()};return True
        except (FileNotFoundError,ProcessLookupError):return False
    try:
        server=start([paths['server'],'--headless','recovery',captures if a.capture else '-',right.fileno()],'server',(right.fileno(),));right.close()
        wait(lambda:'READY ' in log('server'),'compositor')
        if a.ime:event('I on')
        start(['dbus-daemon','--session','--nofork','--address='+address],'bus');wait(lambda:(runtime/'bus').exists(),'private bus')
        start([paths['registry']],'registry')
        def registered():
            probe=subprocess.run(['dbus-send','--bus='+address,'--print-reply','--dest=org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','string:org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2)
            return probe.returncode==0 and 'boolean true' in probe.stdout
        wait(registered,'registry')
        host=start([paths['host'],paths['font'],directory,a.kind,fixture['epoch'].hex(),fixture['account'].hex() if a.kind=='user' else '-'],'host',(gate_read,) if a.held else ())
        if gate_read>=0:os.close(gate_read);gate_read=-1
        wait(lambda:view().get('state')==2 and view().get('count')==len(reference) and 'RECOVERY_READY\n' in log('host') and 'FOCUS active=1\n' in log('host'),'initial original observation and native focus')
        result['maps']=Path(f'/proc/{host.pid}/maps').read_text()
        if any(s in result['maps'] for s in ('libSDL','libX11','libEGL','libvulkan')):raise RuntimeError('Unexpected presentation stack')
        peer=start([paths['client'],host.pid,first_name,last_name]+(['--held',a.held] if a.held else [])+(['--german'] if a.german else [])+(['--ime'] if a.ime else [])+(['--navigation'] if a.navigation else [])+(['--resize'] if a.resize else [])+(['--clipboard-'+a.clipboard] if a.clipboard else []),'client',input_pipe=True)
        def peer_line(text):wait(lambda:text+'\n' in log('client'),text,(peer,))
        if a.held:
            for step in (0,1):
                peer_line('STEP '+str(step))
                if step==1:
                    wait(lambda:'HELD_QUERY_ENTERED ordinal=2 transaction=1\n' in log('host'),'original SQL transaction held')
                    wait(lambda:view().get('state')==1 and view().get('epoch')==1 and view().get('request')==2,'Reading while original query held')
                peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush();peer_line('ACK '+str(step));result['checks'].append({'step':step,'view':view()})
            peer_line('STEP 2')
            if 'HELD_QUERY_DONE' in log('host'):raise RuntimeError('Original query completed before gate release')
            if a.held=='close':
                wait(lambda:view().get('mode')==1 and view().get('focus')==42,'Close focused beside retained field')
                key(28)
                wait(lambda:'RECOVERY_GUI_GONE\n' in log('host') and 'UNMAP 0\n' in log('server'),'native GUI unmaps while original query is held')
                if host.poll() is not None or 'HELD_QUERY_DONE' in log('host'):raise RuntimeError('Host did not retain its held worker after GUI close')
                result['gui_gone_before_gate_release']=True
            else:
                wait(sleeping_with_worker_fd,'native main thread asleep in poll with worker FD')
                before_keys=view().get('keys');offset=len(log('host'));release_gate()
                wait(lambda:'HELD_QUERY_DONE ordinal=2 status=0\n' in log('host') and view().get('epoch')==2 and view().get('state')==2,'completion adopts without further input')
                if view().get('keys')!=before_keys:raise RuntimeError('Unexpected native key while waiting for worker completion')
                returns=re.findall(r'^HELD_WAIT_RETURN ready=(\d+) status=0$',log('host')[offset:],re.M)
                if not any(int(mask)&1 for mask in returns):raise RuntimeError('No readable worker-FD return after gate release')
                result['completion_without_further_input']=True
            peer.stdin.write(b'CHECK 2\n');peer.stdin.flush();peer_line('ACK 2');result['checks'].append({'step':2,'view':view()})
        else:
            for step in range((15 if a.clipboard=='close' else 16) if a.clipboard else 30 if a.resize else 25 if a.navigation else 10):
                peer_line('STEP '+str(step))
                if a.clipboard and step>=10:
                    sample='Grüße 🌍 e\u0301'.encode()
                    if step==10:key(28);wait(lambda:view().get('mode')==1 and view().get('focus')==45,'clipboard filter focus')
                    if step==11:
                        source=foreign_copy(sample,1)
                        bounds=re.search(r'^FIELD_BOUNDS (\d+) (\d+) (\d+) (\d+)$',log('client'),re.M)
                        if not bounds:raise RuntimeError('Missing independent field bounds')
                        x,y,width,height=map(int,bounds.groups());event(f'M {40+x+width//2} {40+y+height//2}');event('B 272 1');event('B 272 0');control(47)
                    if step==12:
                        sets=len(re.findall(r'->\s*wl_data_device#\d+\.set_selection\(',log('host')));keys=view().get('keys',0)
                        control(30);control(46)
                        wait(lambda:len(re.findall(r'->\s*wl_data_device#\d+\.set_selection\(',log('host')))>sets and view().get('keys',0)>=keys+8,'native copy publishes after complete Ctrl-A/C input')
                        source.wait(timeout=7)
                        if source.returncode!=0 or not re.search(r'wl_data_source#\d+\.cancelled\(\)',log('clipboard-source1')):raise RuntimeError('Native copy did not replace the foreign owner')
                        result['clipboard_copy_replaced_foreign_owner']=True;foreign_read(sample)
                    if step==13:
                        pending=foreign_copy('late clipboard 🌙'.encode(),2);pending.send_signal(signal.SIGSTOP);stopped.add(pending)
                        wait(lambda:re.search(r'^State:\s+T',Path(f'/proc/{pending.pid}/status').read_text(),re.M) is not None,'independent source stopped')
                        send_before=len(re.findall(r'wl_data_source#\d+\.send\(',log('clipboard-source2')))
                        offset=len(log('host'));control(47)
                        wait(lambda:re.search(r'->\s*wl_data_offer#\d+\.receive\(',log('host')[offset:]) is not None,'native pending clipboard request')
                        result['clipboard_pending_witness']={'source_pid':pending.pid,'source_status':Path(f'/proc/{pending.pid}/status').read_text(),'view':view()}
                    if step==14:
                        if a.clipboard=='close':
                            wait(lambda:view().get('focus')==42,'Close beside pending clipboard');key(28)
                            wait(lambda:'RECOVERY_GUI_GONE\n' in log('host') and 'UNMAP 0\n' in log('server'),'clipboard-pending GUI retired',(host,))
                            host.wait(timeout=7)
                            if host.returncode!=0:raise RuntimeError('Host failed closing with pending clipboard')
                            background.add(host)
                            result['clipboard_host_closed_while_source_stopped']=True
                        else:
                            key(15);wait(lambda:view().get('focus')!=45,'focus leaves pending clipboard field')
                            pending.send_signal(signal.SIGCONT);stopped.remove(pending)
                            wait(lambda:source_returned_to_socket_wait(pending,send_before),'source returned after late clipboard send')
                            keys=view().get('keys',0);frame=view().get('frames',0);key(42)
                            wait(lambda:view().get('keys',0)>=keys+2 and view().get('frames',0)>frame,'native frame after late clipboard send')
                    if step==15:key(1);wait(lambda:view().get('mode')==0 and view().get('focus')==40,'Cancel after foreign clipboard')
                    peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush();peer_line('ACK '+str(step));result['checks'].append({'step':step,'view':view()});continue
                if step==1:key(107);wait(lambda:view().get('focus')==1000+len(reference)-1,'logical End')
                if step==2:
                    wait(lambda:view().get('mode')==2,'detail mode')
                if step==3:key(1);wait(lambda:view().get('mode')==0,'Back to records')
                if step==4:wait(lambda:view().get('epoch')==2 and view().get('state')==2,'fresh original observation')
                if step==5:
                    wait(lambda:view().get('mode')==1 and view().get('focus')==45,'filter focus');count=view().get('frames',0);key(45)
                    wait(lambda:view().get('frames',0)>count,'literal key frame')
                if step==6 and a.ime:
                    event('I 0');ime_barrier(7);count=view().get('keys',0);key(28);wait(lambda:view().get('keys',0)>=count+2,'Enter during preedit')
                    if view().get('mode')!=1:raise RuntimeError('Enter applied an uncommitted filter prefix')
                if step==7 and a.ime:event('I 1');ime_barrier(0)
                if step==8:key(28);wait(lambda:view().get('mode')==0 and view().get('count')==0,'filtered empty')
                if step==9:
                    key(28);wait(lambda:view().get('mode')==0 and view().get('count')==len(reference),'clear local filter');key(107)
                    wait(lambda:view().get('focus')==1000+len(reference)-1,'restored selected record')
                if step==10:reverse_tab();wait(lambda:view().get('focus')==41,'reverse Tab Close to Read')
                if step==11:
                    key(28);wait(lambda:view().get('epoch')==3 and view().get('state')==2 and view().get('request')==3,'keyboard Read observes original owner');key(107)
                    wait(lambda:view().get('focus')==1000+len(reference)-1,'selected last record after keyboard refresh')
                if step==12:key(15);wait(lambda:view().get('focus')==41,'forward Tab Filter to Read')
                if step==13:reverse_tab();wait(lambda:view().get('focus')==40,'reverse Tab Read to Filter')
                if step in (14,21,23):key(28);wait(lambda:view().get('mode')==1 and view().get('focus')==45,'keyboard opens retained filter')
                if step==15:
                    count=view().get('keys',0);key(45);wait(lambda:view().get('keys',0)>=count+2,'keyboard draft event pair')
                if step==16:key(15);wait(lambda:view().get('focus')==(11 if result.get('filter_scrollbar') else 44),'forward Tab field to next focusable control')
                if step==17:reverse_tab();wait(lambda:view().get('focus')==(11 if result.get('filter_scrollbar') else 45),'reverse Tab Apply to previous focusable control')
                if step==18:reverse_tab();wait(lambda:view().get('focus')==42,'reverse Tab field to Close')
                if step==19:reverse_tab();wait(lambda:view().get('focus')==43,'reverse Tab Close to Back')
                if step in (20,22,24):
                    key(28 if step==20 else 1);wait(lambda:view().get('mode')==0 and view().get('focus')==40 and view().get('count')==len(reference),'Cancel preserves applied results and restores Filter focus')
                if step==25:key(28);wait(lambda:view().get('mode')==1 and view().get('focus')==45,'filter before native resize')
                if step==26:
                    result['resize_origin']=view();resize(64,64);wait(lambda:view().get('focus')==42,'compact Close focus')
                if step==27:resize(240,96);wait(lambda:view().get('focus')==42,'second compact Close focus')
                if step==28:
                    origin=result['resize_origin'];resize(origin['width'],origin['height']);wait(lambda:view().get('focus')==origin['focus'],'native restored field focus')
                if step==29:key(1);wait(lambda:view().get('mode')==0 and view().get('focus')==40 and view().get('count')==len(reference),'Cancel after native restore')
                peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush()
                if step==2:
                    wait(lambda:re.search(r'^READ_PAGES (\d+)$',log('client'),re.M) is not None,'detail geometry',(peer,))
                    pages=int(re.search(r'^READ_PAGES (\d+)$',log('client'),re.M)[1]);result['pages_each_direction']=pages
                    for direction,code in (('D',109),('U',104)):
                        for i in range(pages):
                            before=view().get('frames',0);key(code);wait(lambda:view().get('frames',0)>before,'detail page frame')
                            peer.stdin.write(f'PAGE {direction} {i}\n'.encode());peer.stdin.flush();peer_line(f'PAGE_ACK {direction} {i}')
                if step in (16,17) and a.navigation and result.get('filter_scrollbar'):
                    peer_line('TAB_SCROLL_FORWARD' if step==16 else 'TAB_SCROLL_REVERSE')
                    (key(15) if step==16 else reverse_tab());wait(lambda:view().get('focus')==(44 if step==16 else 45),'Tab continues through help scrollbar')
                    peer.stdin.write(b'NEXT_TAB\n');peer.stdin.flush()
                peer_line('ACK '+str(step));result['checks'].append({'step':step,'view':view()})
                if step==15 and a.navigation:
                    matches=re.findall(r'^FILTER_SCROLLBAR ([01])$',log('client'),re.M)
                    if len(matches)!=1:raise RuntimeError('Missing independent filter-scrollbar inventory')
                    result['filter_scrollbar']=bool(int(matches[0]))
                if step in (25,26,27,28):capture(('filter','tiny','compact','restored')[step-25])
        peer.wait(timeout=7)
        marker=('RECOVERY_NATIVE_WHOLE_WINDOW_RETIRED_TEXT_AND_ACTIONS_REFUSED_OK' if a.held=='close' else 'RECOVERY_NATIVE_COMPLETION_REPLACED_ROWS_AND_REENABLED_READ_OK') if a.held else 'RECOVERY_NATIVE_ORIGINAL_ROWS_FULL_READONLY_TEXT_RETIRED_REFERENCES_KEYBOARD_FILTER_AND_SELECTION_OK'
        if peer.returncode!=0 or marker not in log('client'):raise RuntimeError('Independent peer failed')
        if a.navigation and 'RECOVERY_NATIVE_TAB_REVERSE_TAB_READ_BACK_ESCAPE_RETAINED_DRAFT_AND_SELECTION_OK' not in log('client'):raise RuntimeError('Missing independent native navigation checks')
        if a.resize and 'RECOVERY_NATIVE_POINTER_RESIZE_COMPACT_RETIRED_FIELD_DRAFT_SELECTION_AND_FOCUS_RESTORED_OK' not in log('client'):raise RuntimeError('Missing independent native resize checks')
        if a.clipboard:
            marker='RECOVERY_NATIVE_PENDING_FOREIGN_CLIPBOARD_CLOSE_RETIRED_REFERENCES_OK' if a.clipboard=='close' else 'RECOVERY_NATIVE_FOREIGN_UNICODE_PASTE_COPY_AND_LATE_FOCUS_REPLY_REFUSED_OK'
            if marker not in log('client'):raise RuntimeError('Missing independent clipboard checks')
        if view().get('request')!=(3 if a.navigation else 2):raise RuntimeError('Local inspection/filtering posted backend work')
        if a.held=='close':
            if host.poll() is not None or 'HELD_QUERY_DONE' in log('host'):raise RuntimeError('Held worker ended before retirement peer finished')
            release_gate()
        elif a.clipboard!='close':key(28)
        host.wait(timeout=7)
        if host.returncode!=0 or 'RECOVERY_GUI_GONE\n' not in log('host') or 'failed=0\n' not in log('host'):raise RuntimeError('Host did not close normally')
        if a.held and 'RECOVERY_NATIVE_HELD_ORIGINAL_QUERY_RETIREMENT_DISPOSAL_AND_FDS_OK' not in log('host'):raise RuntimeError('Missing original late-result disposal or descriptor balance')
        if a.held=='close' and 'RECOVERY_CLOSED requests=2 adopted=1 ignored=0 failed=0' not in log('host'):raise RuntimeError('Closed GUI adopted the late observation')
        if hashlib.sha256(db_path.read_bytes()).hexdigest()!=before_sha:raise RuntimeError('Owner database bytes changed')
        with closing(sqlite3.connect(db_path)) as db:
            if rows(db)!=before_rows:raise RuntimeError('Owner rows changed')
        for name in ('host','client'):
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',log(name)):raise RuntimeError('Sanitizer diagnostic')
        result.update(status='passed',host_exit=host.returncode,peer_exit=peer.returncode,owner_sha256_before_after=before_sha,owner_rows_unchanged=True)
    finally:
        for child in stopped:
            if child.poll() is None:child.send_signal(signal.SIGCONT)
        if gate_write>=0:
            try:release_gate()
            except BrokenPipeError:pass
        if gate_read>=0:os.close(gate_read)
        cleanup=[]
        for name,child in reversed(children):
            forced=False
            if child.poll() is None:
                try:os.killpg(child.pid,signal.SIGTERM)
                except ProcessLookupError:pass
                try:child.wait(timeout=3)
                except subprocess.TimeoutExpired:forced=True;os.killpg(child.pid,signal.SIGKILL);child.wait(timeout=3)
            cleanup.append({'name':name,'exit':child.returncode,'forced':forced})
        left.close();right.close()
        for stream in streams:stream.close()
        result['cleanup']=cleanup;(work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
