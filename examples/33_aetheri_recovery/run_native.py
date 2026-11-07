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
    for name in ('german','large','small','contrast','light','ime','navigation'):p.add_argument('--'+name,action='store_true')
    a=p.parse_args()
    if a.held and a.ime:p.error('The held-transaction workflow has its own lifecycle peer; run IME separately')
    if a.held and a.navigation:p.error('Run ordinary navigation separately from the held-transaction peer')
    if os.getuid()==0:p.error('Use an unprivileged private owner')
    paths={name:getattr(a,name).resolve(strict=True) for name in ('server','registry','host','client','font','source')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700);directory=work/'owner';directory.mkdir(mode=0o700);db_path=directory/'catalogue.sqlite3'
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
    if a.ime:env['WAYLAND_DEBUG']='client'
    gate_read,gate_write=os.pipe2(os.O_CLOEXEC) if a.held else (-1,-1)
    if a.held:env['RVG_TEST_GATE_FD']=str(gate_read)
    else:env.pop('RVG_TEST_GATE_FD',None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4);children=[];streams=[]
    result={'status':'failed','inputs':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items() if path.is_file()},
            'configuration':{name:getattr(a,name) for name in ('kind','german','large','small','contrast','light','ime','held','navigation')},'checks':[],'expected_records':len(reference)}
    def start(command,name,fds=(),input_pipe=False):
        stream=(work/(name+'.log')).open('wb');streams.append(stream)
        child=subprocess.Popen(list(map(str,command)),env=env,stdin=subprocess.PIPE if input_pipe else subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=fds,start_new_session=True)
        children.append((name,child));return child
    def log(name):return (work/(name+'.log')).read_text(errors='replace')
    def wait(test,label,allowed=()):
        until=time.monotonic()+7
        while not test():
            for name,child in children:
                if child not in allowed and child.poll() is not None:raise RuntimeError(label+': '+name+' exited '+str(child.returncode))
            if time.monotonic()>=until:raise RuntimeError(label+': timeout')
            time.sleep(.01)
    def event(command):
        left.sendall(command.encode('ascii'))
        if left.recv(64)!=b'OK':raise RuntimeError('Compositor rejected '+command)
    def key(code):event(f'K {code} 1');event(f'K {code} 0')
    def reverse_tab():event('K 42 1');key(15);event('K 42 0')
    def view():
        lines=re.findall(r'^VIEW (.+)$',log('host'),re.M)
        return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',lines[-1])} if lines else {}
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
        server=start([paths['server'],'--headless','recovery','-',right.fileno()],'server',(right.fileno(),));right.close()
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
        peer=start([paths['client'],host.pid,first_name,last_name]+(['--held',a.held] if a.held else [])+(['--german'] if a.german else [])+(['--ime'] if a.ime else [])+(['--navigation'] if a.navigation else []),'client',input_pipe=True)
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
            for step in range(25 if a.navigation else 10):
                peer_line('STEP '+str(step))
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
        peer.wait(timeout=7)
        marker=('RECOVERY_NATIVE_WHOLE_WINDOW_RETIRED_TEXT_AND_ACTIONS_REFUSED_OK' if a.held=='close' else 'RECOVERY_NATIVE_COMPLETION_REPLACED_ROWS_AND_REENABLED_READ_OK') if a.held else 'RECOVERY_NATIVE_ORIGINAL_ROWS_FULL_READONLY_TEXT_RETIRED_REFERENCES_KEYBOARD_FILTER_AND_SELECTION_OK'
        if peer.returncode!=0 or marker not in log('client'):raise RuntimeError('Independent peer failed')
        if a.navigation and 'RECOVERY_NATIVE_TAB_REVERSE_TAB_READ_BACK_ESCAPE_RETAINED_DRAFT_AND_SELECTION_OK' not in log('client'):raise RuntimeError('Missing independent native navigation checks')
        if view().get('request')!=(3 if a.navigation else 2):raise RuntimeError('Local inspection/filtering posted backend work')
        if a.held=='close':
            if host.poll() is not None or 'HELD_QUERY_DONE' in log('host'):raise RuntimeError('Held worker ended before retirement peer finished')
            release_gate()
        else:key(28)
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
