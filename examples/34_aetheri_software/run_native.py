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
import subprocess
import sys
import time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','registry','host','client','font','source','model','work-dir'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('german','large','small','contrast','light','ime'):p.add_argument('--'+name,action='store_true')
    a=p.parse_args()
    if os.getuid()==0:p.error('Use an unprivileged private owner')
    paths={name:getattr(a,name).resolve(strict=True) for name in ('server','registry','host','client','font','source','model')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700)
    prepared=subprocess.run([sys.executable,str(Path(__file__).with_name('run_model.py')),'--source',str(paths['source']),'--test',str(paths['model']),'--work',str(work/'fixture'),'--prepare-only'],capture_output=True,text=True,timeout=45)
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
    address='unix:path='+str(runtime/'bus')
    env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='software',NEXAUI_PLATFORM='wayland',DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',PYTHONDONTWRITEBYTECODE='1')
    for name in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE'):env.pop(name,None)
    for name in ('german','large','small','contrast','light'):
        env.pop('SWG_'+name.upper(),None)
        if getattr(a,name):env['SWG_'+name.upper()]='1'
    if a.ime:env['WAYLAND_DEBUG']='client'
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4);children=[];streams=[]
    result={'status':'failed','inputs':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items() if path.is_file()},'configuration':{name:getattr(a,name) for name in ('german','large','small','contrast','light','ime')},'checks':[],'selected_targets':5,'eligible':2,'unavailable':3}
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
        host=start([paths['host'],paths['font'],databases[0].parent,uid(1),uid(2),'1000',*[uid(n) for n in (12,99,10,13,11)]],'host')
        wait(lambda:view().get('state')==2 and view().get('count')==5 and 'SOFTWARE_READY\n' in log('host') and 'FOCUS active=1\n' in log('host'),'initial selected-app observation and native focus')
        result['maps']=Path(f'/proc/{host.pid}/maps').read_text()
        if any(s in result['maps'] for s in ('libSDL','libX11','libEGL','libvulkan')):raise RuntimeError('Unexpected presentation stack')
        peer=start([paths['client'],host.pid,first_name,last_name]+(['--german'] if a.german else [])+(['--ime'] if a.ime else []),'client',input_pipe=True)
        def peer_line(text):wait(lambda:text+'\n' in log('client'),text,(peer,))
        for step in range(10):
            peer_line('STEP '+str(step))
            if step==1:key(107);wait(lambda:view().get('focus')==1004,'End focuses last selected target')
            if step==2:wait(lambda:view().get('mode')==2 and view().get('focus')==43,'selected target detail')
            if step==3:key(1);wait(lambda:view().get('mode')==0 and view().get('focus')==1004,'Escape returns to selected target')
            if step==4:wait(lambda:view().get('epoch')==2 and view().get('state')==2,'explicit Read observes original source again')
            if step==5:
                wait(lambda:view().get('mode')==1 and view().get('focus')==45,'filter focus');before=view().get('keys',0);key(45);wait(lambda:view().get('keys',0)>=before+2,'native text input frame')
            if step==6 and a.ime:event('I 0');ime_barrier(7)
            if step==7 and a.ime:event('I 1');ime_barrier(0)
            if step==8:key(28);wait(lambda:view().get('mode')==0 and view().get('count')==0,'apply local no-match filter')
            if step==9:key(28);wait(lambda:view().get('mode')==0 and view().get('count')==5,'clear local filter');key(107);wait(lambda:view().get('focus')==1004,'End restores selected target focus')
            peer.stdin.write(f'CHECK {step}\n'.encode());peer.stdin.flush()
            if step==2:
                wait(lambda:re.search(r'^READ_PAGES (\d+)$',log('client'),re.M) is not None,'detail page count',(peer,))
                pages=int(re.search(r'^READ_PAGES (\d+)$',log('client'),re.M)[1]);result['pages_each_direction']=pages
                for direction,code in (('D',109),('U',104)):
                    for i in range(pages):
                        before=view().get('frames',0);key(code);wait(lambda:view().get('frames',0)>before,'detail page frame')
                        peer.stdin.write(f'PAGE {direction} {i}\n'.encode());peer.stdin.flush();peer_line(f'PAGE_ACK {direction} {i}')
            peer_line('ACK '+str(step));result['checks'].append({'step':step,'view':view()})
        peer.wait(timeout=7)
        if peer.returncode!=0 or 'SOFTWARE_NATIVE_SELECTED_ROWS_READONLY_DETAILS_RETIRED_REFERENCES_KEYBOARD_FILTER_SELECTION_OK' not in log('client'):raise RuntimeError('Independent peer failed')
        if view().get('request')!=2:raise RuntimeError('Local inspection/filtering posted backend work')
        key(28);host.wait(timeout=7)
        if host.returncode!=0 or 'SOFTWARE_GUI_GONE\n' not in log('host') or 'failed=0\n' not in log('host'):raise RuntimeError('Host did not close normally')
        for index,path in enumerate(databases):
            if hashlib.sha256(path.read_bytes()).hexdigest()!=before_sha[str(path)]:raise RuntimeError('Original catalogue bytes changed')
            with closing(sqlite3.connect(path)) as db:
                if rows(db)!=before_rows[index]:raise RuntimeError('Original catalogue rows changed')
        for name in ('host','client'):
            if re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',log(name)):raise RuntimeError('Sanitizer diagnostic')
        result.update(status='passed',host_exit=host.returncode,peer_exit=peer.returncode,sha256_before_after=before_sha,database_rows_unchanged=True)
    finally:
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
