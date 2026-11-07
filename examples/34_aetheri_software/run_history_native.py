"""Private Wayland historical-profile UI and independent OS AT-SPI peer."""
import argparse,hashlib,json,os,re,signal,socket,sqlite3,subprocess,sys,time
from contextlib import closing
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','registry','host','client','font','source','model','fixture','reader','work-dir'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=('history','replacement','unsupported','corrupt','none','unit','unit-replacement','custody','custody-max','custody-replacement'),default='history')
    p.add_argument('--unit-links',action='store_true',help='Inspect original allocation/custody details without unit effects')
    for name in ('german','large','small','contrast','light'):p.add_argument('--'+name,action='store_true')
    a=p.parse_args()
    if os.getuid()==0:p.error('Use an unprivileged private owner')
    paths={n:getattr(a,n).resolve(strict=True) for n in ('server','registry','host','client','font','source','model','fixture','reader')}
    work=a.work_dir.absolute();work.mkdir(mode=0o700);runtime=work/'r';runtime.mkdir(mode=0o700)
    prepare=[sys.executable,str(Path(__file__).with_name('run_history.py')),'--source',paths['source'],'--fixture',paths['fixture'],'--model',paths['model'],'--test',paths['reader'],'--work',work/'fixture','--prepare-only','history' if a.mode=='none' else a.mode]
    prepared=subprocess.run(list(map(str,prepare)),capture_output=True,text=True,timeout=45);(work/'prepare.log').write_text(prepared.stdout+prepared.stderr);assert prepared.returncode==0,prepared.stderr
    fixture=Path(next(x.removeprefix('PREPARED ') for x in prepared.stdout.splitlines() if x.startswith('PREPARED ')));data=json.loads(fixture.read_text());databases=[Path(data[k])/'catalogue.sqlite3' for k in ('system','user')]
    sys.dont_write_bytecode=True;sys.path.insert(0,str(paths['source']/'tools'));from test_contracts import rows
    before_sha={str(d):hashlib.sha256(d.read_bytes()).hexdigest() for d in databases};before_rows=[]
    for d in databases:
        with closing(sqlite3.connect(d)) as db:before_rows.append(rows(db))
    raw=bytes.fromhex(data['profile_hex']);assert len(raw)==384 and raw[:8]==b'AETHEXE1'
    digest=hashlib.sha256(raw).hexdigest();ids=[raw[8+i*16:24+i*16].hex() for i in range(14)];hashes=[raw[256+i*32:288+i*32].hex() for i in range(4)]
    expected=work/'expected.txt';expected.write_text('\n'.join([digest,*ids,*hashes])+'\n')
    first_name=('aetheri.probe · Aktive App beobachtet' if a.german else 'aetheri.probe · Active app observed')+'\n'+ids[6]
    history_name=('Historischer Launch' if a.german else 'Historical launch')+'\n'+ids[10]+'\n'+digest
    address='unix:path='+str(runtime/'bus');env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='history',NEXAUI_PLATFORM='wayland',DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',PYTHONDONTWRITEBYTECODE='1')
    for n in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE'):env.pop(n,None)
    for n in ('german','large','small','contrast','light'):
        env.pop('SWG_'+n.upper(),None)
        if getattr(a,n):env['SWG_'+n.upper()]='1'
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4);children=[];streams=[]
    result=dict(status='failed',inputs={n:hashlib.sha256(v.read_bytes()).hexdigest() for n,v in paths.items() if v.is_file()},configuration={n:getattr(a,n) for n in ('mode','german','large','small','contrast','light','unit_links')},commands=[],scope='Original synthetic historical metadata, native virtual Wayland and OS AT-SPI transport. No current process/resource/ownership, human reader or physical hardware acceptance.')
    def start(command,name,fds=(),pipe=False):
        stream=(work/(name+'.log')).open('wb');streams.append(stream);child=subprocess.Popen(list(map(str,command)),env=env,stdin=subprocess.PIPE if pipe else subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=fds,start_new_session=True);children.append((name,child));return child
    def log(name):return (work/(name+'.log')).read_text(errors='replace')
    def wait(test,label,allowed=()):
        until=time.monotonic()+10
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
    try:
        start([paths['server'],'--headless','history','-',right.fileno()],'server',(right.fileno(),));right.close();wait(lambda:'READY ' in log('server'),'compositor')
        start(['dbus-daemon','--session','--nofork','--address='+address],'bus');wait(lambda:(runtime/'bus').exists(),'private bus');start([paths['registry']],'registry')
        def registered():
            probe=subprocess.run(['dbus-send','--bus='+address,'--print-reply','--dest=org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','string:org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2)
            return probe.returncode==0 and 'boolean true' in probe.stdout
        wait(registered,'registry')
        command=[paths['host'],paths['font'],data['system'],ids[0],ids[5],'1000',ids[6]]+([] if a.mode=='none' else ['--history',data['profile_hex']]);result['host_command']=list(map(str,command));host=start(command,'host')
        wait(lambda:view().get('state')==2 and view().get('count')==1 and 'SOFTWARE_READY\n' in log('host') and 'FOCUS active=1\n' in log('host'),'initial observation and native focus')
        result['maps']=Path(f'/proc/{host.pid}/maps').read_text();assert not any(s in result['maps'] for s in ('libSDL','libX11','libEGL','libvulkan'))
        peer=start([paths['client'],host.pid,first_name,history_name,expected,a.mode,'de' if a.german else 'en']+(['--unit-links'] if a.unit_links else []),'client',pipe=True)
        for sequence in range(1024):
            def commands():return re.findall(r'^HISTORY (\d+) (KEY|SYNC|REFRESH|DONE) (\d+) (\d+)$',log('client'),re.M)
            wait(lambda:len(commands())>sequence,'history peer command',(peer,));actual,kind,first,second=commands()[sequence];first=int(first);second=int(second);assert int(actual)==sequence
            if kind=='KEY':
                before=view().get('keys',0);key(first);wait(lambda:view().get('keys',0)>=before+2,'keyboard frame')
            elif kind=='SYNC':wait(lambda:view().get('mode')==first and view().get('focus')==second,'mode/focus')
            elif kind=='REFRESH':wait(lambda:view().get('epoch')==first and view().get('state')==2,'new original observation')
            result['commands'].append(dict(sequence=sequence,kind=kind,a=first,b=second,view=view()));peer.stdin.write(f'HISTORY_ACK {sequence}\n'.encode());peer.stdin.flush()
            if kind=='DONE':break
        else:raise RuntimeError('History command bound')
        peer.wait(timeout=10);assert peer.returncode==0 and 'HISTORY_NATIVE_ORIGINAL_READONLY_FULL_IDENTITIES_RETIRED_FOCUS_PAGES_REFRESH_UNKNOWN_CURRENT_OK' in log('client')
        assert view().get('request')==2,'Local inspection posted backend work';key(28);host.wait(timeout=10);assert host.returncode==0 and 'SOFTWARE_GUI_GONE\n' in log('host') and 'failed=0\n' in log('host')
        for i,d in enumerate(databases):
            assert hashlib.sha256(d.read_bytes()).hexdigest()==before_sha[str(d)]
            with closing(sqlite3.connect(d)) as db:assert rows(db)==before_rows[i]
        for name in ('host','client'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',log(name))
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
            cleanup.append(dict(name=name,exit=child.returncode,forced=forced))
        left.close();right.close()
        for stream in streams:stream.close()
        result['cleanup']=cleanup;(work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
