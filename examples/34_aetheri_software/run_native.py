"""Private original selected-app Wayland host and independent OS AT-SPI peer."""
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
    for name in ('server','registry','host','client','font','source','model','work-dir'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('german','large','small','contrast','light','ime'):p.add_argument('--'+name,action='store_true')
    p.add_argument('--components',choices=('match','identity-failure'),help='Join private original canonical manifest metadata to the first selected app')
    p.add_argument('--contents',action='store_true',help='Inspect all declared files/actions through native keyboard and OS semantics')
    p.add_argument('--held',choices=('wake','close'),help='Test-only original-query hold with the independent lifecycle OS peer')
    a=p.parse_args()
    if a.held and (a.ime or a.components or a.contents):p.error('Run held lifetime, IME and metadata inspection workflows separately')
    if a.contents and a.components!='match':p.error('--contents requires --components match')
    if os.getuid()==0:p.error('Use an unprivileged private owner')
    paths={name:getattr(a,name).resolve(strict=True) for name in ('server','registry','host','client','font','source','model')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700)
    manifest=None;component_input=None
    if a.components:
        from components_fixture import seed
        component_input=work/'manifests';component_input.mkdir(mode=0o700);seed(paths['source'],component_input);manifest=component_input/('maximum.bin' if a.components=='match' else 'component.bin')
    prepared=subprocess.run([sys.executable,str(Path(__file__).with_name('run_model.py')),'--source',str(paths['source']),'--test',str(paths['model']),'--work',str(work/'fixture'),'--prepare-only']+(['--fixture-manifest',str(manifest)] if manifest else []),capture_output=True,text=True,timeout=45)
    (work/'prepare.log').write_text(prepared.stdout+prepared.stderr);assert prepared.returncode==0,prepared.stderr
    fixture=Path(next(x.removeprefix('PREPARED ') for x in prepared.stdout.splitlines() if x.startswith('PREPARED ')))
    databases=[Path(v)/'catalogue.sqlite3' for v in json.loads(fixture.read_text()).values()]
    sys.dont_write_bytecode=True;sys.path.insert(0,str(paths['source']/'tools'))
    import catalogue_contracts as cc
    from test_contracts import rows
    with closing(sqlite3.connect(databases[0])) as db:
        for version in range(3,11):
            migrate={7:cc.migrate_owner_recovery,9:cc.migrate_units,10:cc.migrate_unit_custody}.get(version)
            (migrate if migrate else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,'system')
    before_sha={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in databases};before_rows=[]
    for path in databases:
        with closing(sqlite3.connect(path)) as db:before_rows.append(rows(db))
    def uid(n):return '000000000000400080000000'+f'{n:08x}'
    first_name=('aetheri.probe · Aktive App beobachtet' if a.german else 'aetheri.probe · Active app observed')+'\n'+uid(10)
    last_name=('Ausgewähltes Ziel · Eignung nicht beobachtet' if a.german else 'Selected target · Eligibility not observed')+'\n'+uid(99)
    if a.components:last_name=first_name
    inspected_row=1000 if a.components else 1004;inspection_key=102 if a.components else 107
    address='unix:path='+str(runtime/'bus')
    env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='software',NEXAUI_PLATFORM='wayland',DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',PYTHONDONTWRITEBYTECODE='1')
    for name in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE'):env.pop(name,None)
    for name in ('german','large','small','contrast','light'):
        env.pop('SWG_'+name.upper(),None)
        if getattr(a,name):env['SWG_'+name.upper()]='1'
    if a.ime:env['WAYLAND_DEBUG']='client'
    gate_read,gate_write=os.pipe2(os.O_CLOEXEC) if a.held else (-1,-1)
    if a.held:env['SWG_TEST_GATE_FD']=str(gate_read)
    else:env.pop('SWG_TEST_GATE_FD',None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4);children=[];streams=[]
    result={'status':'failed','inputs':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items() if path.is_file()},'configuration':{name:getattr(a,name) for name in ('german','large','small','contrast','light','ime','components','held')},'checks':[],'selected_targets':5,'eligible':2,'unavailable':3}
    if a.components:result['component_inputs']=json.loads((component_input/'inputs.json').read_text())
    def start(command,name,fds=(),input_pipe=False):
        stream=(work/(name+'.log')).open('wb');streams.append(stream)
        child=subprocess.Popen(list(map(str,command)),env=env,stdin=subprocess.PIPE if input_pipe else subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=fds,start_new_session=True);children.append((name,child));return child
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
    def view():
        lines=re.findall(r'^VIEW (.+)$',log('host'),re.M)
        return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',lines[-1])} if lines else {}
    def release_gate():
        nonlocal gate_write
        if gate_write>=0:
            try:os.write(gate_write,b'R')
            finally:os.close(gate_write);gate_write=-1
    def sleeping_with_worker_fd():
        # Same parent-owned syscall/pollfd witness as the Recovery consumer.
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
    def ime_barrier(bytes_count):
        marker=f'PREEDIT bytes={bytes_count}\n';wait(lambda:marker in log('host'),'native preedit change')
        wait(lambda:re.search(r'^VIEW ',log('host')[log('host').rfind(marker)+len(marker):],re.M) is not None,'field frame after preedit')
        def settled():
            sent=len(re.findall(r'->\s*zwp_text_input_v3#\d+\.commit\(\)',log('host')));served=re.findall(r'^IME commit enabled=1 serial=(\d+)',log('server'),re.M)
            return sent>0 and served and int(served[-1])==sent
        wait(settled,'producer processed text-input commits')
    try:
        server=start([paths['server'],'--headless','software','-',right.fileno()],'server',(right.fileno(),));right.close();wait(lambda:'READY ' in log('server'),'compositor')
        if a.ime:event('I on')
        start(['dbus-daemon','--session','--nofork','--address='+address],'bus');wait(lambda:(runtime/'bus').exists(),'private bus');start([paths['registry']],'registry')
        def registered():
            probe=subprocess.run(['dbus-send','--bus='+address,'--print-reply','--dest=org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','string:org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2)
            return probe.returncode==0 and 'boolean true' in probe.stdout
        wait(registered,'registry')
        host=start([paths['host'],paths['font'],databases[0].parent,uid(1),uid(2),'1000',*[uid(n) for n in (12,99,10,13,11)]]+(['--manifest',uid(10),manifest,'--manifest',uid(12),component_input/'malformed.bin','--manifest',uid(99),component_input/'fifo'] if manifest else []),'host',(gate_read,) if a.held else ())
        if gate_read>=0:os.close(gate_read);gate_read=-1
        wait(lambda:view().get('state')==2 and view().get('count')==5 and 'SOFTWARE_READY\n' in log('host') and 'FOCUS active=1\n' in log('host'),'initial selected-app observation and native focus')
        result['maps']=Path(f'/proc/{host.pid}/maps').read_text()
        if any(s in result['maps'] for s in ('libSDL','libX11','libEGL','libvulkan')):raise RuntimeError('Unexpected presentation stack')
        peer_flags=(['--components',','.join(result['component_inputs']['dependencies'])] if a.components=='match' else ['--component-failure'] if a.components else [])
        if a.contents:peer_flags+=['--contents',str(component_input/'content-peer.txt')]
        if a.held:peer_flags=['--held',a.held]
        peer=start([paths['client'],host.pid,first_name,last_name]+peer_flags+(['--german'] if a.german else [])+(['--ime'] if a.ime else []),'client',input_pipe=True)
        def peer_line(text):wait(lambda:text+'\n' in log('client'),text,(peer,))
        if a.held:
            # The lifecycle peer takes --held before its optional locale flag.
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
                wait(lambda:'SOFTWARE_GUI_GONE\n' in log('host') and 'UNMAP 0\n' in log('server'),'native GUI unmaps while original query is held')
                if host.poll() is not None or 'HELD_QUERY_DONE' in log('host'):raise RuntimeError('Host did not retain its held worker after GUI close')
                result['gui_gone_before_gate_release']=True
            else:
                wait(sleeping_with_worker_fd,'native main thread asleep in poll with worker FD')
                before_keys=view().get('keys');offset=len(log('host'));release_gate()
                wait(lambda:'HELD_QUERY_DONE ordinal=2 status=0 eligible=2 unknown=3\n' in log('host') and view().get('epoch')==2 and view().get('state')==2,'completion adopts without further input')
                if view().get('keys')!=before_keys:raise RuntimeError('Unexpected native key while waiting for worker completion')
                returns=re.findall(r'^HELD_WAIT_RETURN ready=(\d+) status=0$',log('host')[offset:],re.M)
                if not any(int(mask)&1 for mask in returns):raise RuntimeError('No readable worker-FD return after gate release')
                result['completion_without_further_input']=True
            peer.stdin.write(b'CHECK 2\n');peer.stdin.flush();peer_line('ACK 2');result['checks'].append({'step':2,'view':view()})
        for step in (() if a.held else range(10)):
            peer_line('STEP '+str(step))
            if step==1:key(inspection_key);wait(lambda:view().get('focus')==inspected_row,'logical target focus')
            if step==2:wait(lambda:view().get('mode')==2 and view().get('focus')==43,'selected target detail')
            if step==3:key(1);wait(lambda:view().get('mode')==0 and view().get('focus')==inspected_row,'Escape returns to selected target')
            if step==4:wait(lambda:view().get('epoch')==2 and view().get('state')==2,'explicit Read observes original source again')
            if step==5:
                wait(lambda:view().get('mode')==1 and view().get('focus')==45,'filter focus');before=view().get('keys',0);key(45);wait(lambda:view().get('keys',0)>=before+2,'native text input frame')
            if step==6 and a.ime:event('I 0');ime_barrier(7)
            if step==7 and a.ime:event('I 1');ime_barrier(0)
            if step==8:key(28);wait(lambda:view().get('mode')==0 and view().get('count')==0,'apply local no-match filter')
            if step==9:key(28);wait(lambda:view().get('mode')==0 and view().get('count')==5,'clear local filter');key(inspection_key);wait(lambda:view().get('focus')==inspected_row,'logical selection focus restored')
            peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush()
            if step==2:
                wait(lambda:re.search(r'^READ_PAGES (\d+)$',log('client'),re.M) is not None,'detail page count',(peer,))
                pages=int(re.search(r'^READ_PAGES (\d+)$',log('client'),re.M)[1]);result['pages_each_direction']=pages
                for direction,code in (('D',109),('U',104)):
                    for i in range(pages):
                        before=view().get('frames',0);key(code);wait(lambda:view().get('frames',0)>before,'detail page frame')
                        peer.stdin.write(f'PAGE {direction} {i}\n'.encode());peer.stdin.flush();peer_line(f'PAGE_ACK {direction} {i}')
                if a.contents:
                    commands=[]
                    for sequence in range(1024):
                        def content_commands():return re.findall(r'^CONTENTS (\d+) (KEY|SYNC|DONE) (\d+) (\d+)$',log('client'),re.M)
                        wait(lambda:len(content_commands())>sequence,'manifest content command')
                        actual,kind,first,second=content_commands()[sequence];first=int(first);second=int(second)
                        if int(actual)!=sequence:raise RuntimeError('Manifest content sequence')
                        if kind=='KEY':
                            before=view().get('frames',0);key(first);wait(lambda:view().get('frames',0)>before,'manifest keyboard frame')
                        elif kind=='SYNC':wait(lambda:view().get('mode')==first and view().get('focus')==second,'manifest content focus')
                        commands.append(dict(sequence=sequence,kind=kind,a=first,b=second,view=view()))
                        peer.stdin.write(f'CONTENTS_ACK {sequence}\n'.encode());peer.stdin.flush()
                        if kind=='DONE':break
                    else:raise RuntimeError('Manifest content command bound')
                    result['contents_commands']=commands
            peer_line('ACK '+str(step));result['checks'].append({'step':step,'view':view()})
        peer.wait(timeout=7)
        marker=('SOFTWARE_NATIVE_WHOLE_WINDOW_RETIRED_TEXT_AND_ACTIONS_REFUSED_OK' if a.held=='close' else 'SOFTWARE_NATIVE_COMPLETION_REPLACED_ROWS_AND_REENABLED_READ_OK') if a.held else 'SOFTWARE_NATIVE_SELECTED_ROWS_READONLY_DETAILS_RETIRED_REFERENCES_KEYBOARD_FILTER_SELECTION_OK'
        if peer.returncode!=0 or marker not in log('client'):raise RuntimeError('Independent peer failed')
        if view().get('request')!=2:raise RuntimeError('Local inspection/filtering posted backend work')
        if a.held=='close':
            if host.poll() is not None or 'HELD_QUERY_DONE' in log('host'):raise RuntimeError('Held worker ended before retirement peer finished')
            release_gate()
        else:key(28)
        host.wait(timeout=7)
        if host.returncode!=0 or 'SOFTWARE_GUI_GONE\n' not in log('host') or 'failed=0\n' not in log('host'):raise RuntimeError('Host did not close normally')
        if a.held and 'SOFTWARE_NATIVE_HELD_ORIGINAL_QUERY_RETIREMENT_DISPOSAL_AND_FDS_OK' not in log('host'):raise RuntimeError('Missing late-result disposal or descriptor balance')
        if a.held and 'SOFTWARE_CLOSED requests=2 adopted='+('1' if a.held=='close' else '2')+' ignored=0 failed=0\n' not in log('host'):raise RuntimeError('Unexpected late-result adoption')
        for index,path in enumerate(databases):
            if hashlib.sha256(path.read_bytes()).hexdigest()!=before_sha[str(path)]:raise RuntimeError('Original catalogue bytes changed')
            with closing(sqlite3.connect(path)) as db:
                if rows(db)!=before_rows[index]:raise RuntimeError('Original catalogue rows changed')
        for name in ('host','client'):
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',log(name)):raise RuntimeError('Sanitizer diagnostic')
        result.update(status='passed',host_exit=host.returncode,peer_exit=peer.returncode,sha256_before_after=before_sha,database_rows_unchanged=True)
    finally:
        if gate_read>=0:os.close(gate_read)
        if gate_write>=0:os.close(gate_write)
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
