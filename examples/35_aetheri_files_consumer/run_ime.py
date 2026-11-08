"""Private native v3/AT-SPI checks for the explicitly changed Files IME variant.

Uses fresh owned fixtures and real original child programs; not human/physical
acceptance, normal app admission or the unchanged-original host.
"""
import argparse,gzip,hashlib,json,os,re,signal,socket,subprocess,sys,tempfile,time
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
for name in ('binaries','original','atspi-fixture','sdk-input','server','font','mono','work-dir'):
    parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--expected-sdk-revision',required=True)
parser.add_argument('--linkage',choices=('static','shared'),required=True)
parser.add_argument('--transactions',action='store_true',help='Require the separately built private producer and exercise replacement/undo packets')
parser.add_argument('--clipboard-tools',type=Path,help='Private checked wl-clipboard tools and musl loader/runtime')
parser.add_argument('--clipboard-budget',action='store_true',help='Also exercise a 65537-byte foreign paste proposal')
parser.add_argument('--fallback-font',type=Path,action='append',default=[],help='Explicit optional app fallback font; may be repeated up to four times')
parser.add_argument('--font-errors',action='store_true',help='Check private startup refusals for missing, oversized, malformed and excess fallback inputs')
args=parser.parse_args();b=args.binaries.resolve(strict=True);original=args.original.resolve(strict=True);fixture=args.atspi_fixture.resolve(strict=True);src=Path(__file__).resolve().parent;sdk=json.loads(args.sdk_input.read_text());profile=args.linkage;case='native-ime-'+profile
if not re.fullmatch(r'[0-9a-f]{40}',args.expected_sdk_revision) or sdk['revision']!=args.expected_sdk_revision or sdk['version']!='0.1.26' or sdk['atspi_compiled'] is not True:raise SystemExit('Wrong candidate SDK declaration')
library=Path(sdk['library']).resolve(strict=True)
if library.name!='libnexaui.so.'+sdk['version'] or hashlib.sha256(library.read_bytes()).hexdigest()!=sdk['sha256']:raise SystemExit('Changed candidate SDK library')
dbus=Path(sdk['dbus_sdk']).resolve(strict=True)
for item in sdk['dbus_headers']:
    if hashlib.sha256((dbus/item['path']).read_bytes()).hexdigest()!=item['sha256']:raise SystemExit('Changed peer D-Bus header')
for name,value in json.loads((fixture/'inputs.json').read_text())['files'].items():
    if hashlib.sha256((fixture/name.lstrip('/')).read_bytes()).hexdigest()!=value:raise SystemExit('Changed private AT-SPI fixture input')
subprocess.run([sys.executable,str(src/'check_source.py'),str(original),str(src/'native-inputs.json')],check=True)
server_binary=args.server.resolve(strict=True);font=args.font.resolve(strict=True);mono=args.mono.resolve(strict=True);root=args.work_dir.resolve()
producer_delta=None
if args.transactions:
    producer_delta=json.loads((server_binary.parent/'generated/source-delta.json').read_text())
    if {f['original'] for f in producer_delta['files']}!={'tests/compositor_text_input.c','tests/compositor_input_driver.c'}:
        raise SystemExit('Wrong private transaction producer delta')
    for f in producer_delta['files']:
        if hashlib.sha256((server_binary.parent/'generated'/f['generated']).read_bytes()).hexdigest()!=f['sha256']:
            raise SystemExit('Changed private transaction producer source')
if len(str(root/'runtime'/'files-ime').encode())>=108:raise SystemExit('Use a short private Unix socket work directory')
root.mkdir(mode=0o700);final=root
runtime=root/'runtime';runtime.mkdir(mode=0o700);documents=root/'documents';documents.mkdir(mode=0o700);cache=root/'cache';cache.mkdir(mode=0o700);captures=root/'captures';captures.mkdir(mode=0o700);seeds={'alpha':b'first','é':b'second'}
for name,data in seeds.items():(documents/name).write_bytes(data)
address='unix:path='+str(runtime/'bus');conf=root/'bus.conf';conf.write_text('<busconfig><type>session</type><listen>'+address+'</listen><auth>EXTERNAL</auth><policy context="default"><allow user="'+str(os.getuid())+'"/><allow own="*"/><allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>\n');env=dict(os.environ,XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY='files-ime',NEXAUI_PLATFORM='wayland',DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,LD_LIBRARY_PATH=str(fixture/'usr/lib64'),GSETTINGS_BACKEND='memory',LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe',LP_NUM_THREADS='2',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE','DBUS_STARTER_ADDRESS','DBUS_STARTER_BUS_TYPE'):env.pop(name,None)
if args.clipboard_tools:env['WAYLAND_DEBUG']='client'
clipboard_task=None
fallback_fonts=[p.resolve(strict=True) for p in args.fallback_font]
fallback_arguments=[value for p in fallback_fonts for value in ('--fallback-font',str(p))]
peer=root/'peer';dbus=Path(sdk['dbus_sdk']);command=['cc','-std=c17','-Wall','-Wextra','-Wpedantic','-Werror','-O2',str(src/'ime_atspi.c'),'-I'+str(original/'tests'),'-o',str(peer)]+['-I'+str(fixture/p) for p in ('usr/include/at-spi-2.0','usr/include/glib-2.0','usr/lib64/glib-2.0/include')]+['-I'+str(dbus/p) for p in ('include/dbus-1.0','lib64/dbus-1.0/include')]+[str(fixture/'usr/lib64/libatspi.so.0.0.1'),'-l:libgobject-2.0.so.0','-l:libglib-2.0.so.0','-Wl,-rpath,'+str(fixture/'usr/lib64'),'-Wl,-rpath-link,'+str(fixture/'usr/lib64')];compiled=subprocess.run(command,capture_output=True,text=True);(root/'peer-build.log').write_text(compiled.stdout+compiled.stderr);assert compiled.returncode==0,compiled.stderr
left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4);children=[];streams=[];images=[];checks=[];app=None;server=None;result=dict(result='failed',case=case,peer_build_command=command,transactions=args.transactions,sdk=sdk,producer=dict(path=str(server_binary),sha256=hashlib.sha256(server_binary.read_bytes()).hexdigest(),source_delta=producer_delta),scope='Separate IME integration variant with real original controller; private native producer/AT-SPI; no active Aetheri change')
def log(name='app'):return (root/(name+'.log')).read_text(errors='replace') if (root/(name+'.log')).exists() else ''
def start(binary,name,*args,pass_fds=()):
    stream=(root/(name+'.log')).open('xb');streams.append(stream);p=subprocess.Popen([str(binary),*map(str,args)],env=env,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds);children.append(p);return p
def wait(predicate,label):
    deadline=time.monotonic()+12
    while not predicate():
        assert all(p.poll() is None for p in children),(label,log(),log('server')[-3000:]);assert time.monotonic()<deadline,(label,log()[-3000:],log('server')[-3000:]);time.sleep(.01)
    checks.append(label)
def event(value):left.sendall(value.encode());assert left.recv(32)==b'OK',value
def key(code,ctrl=False,shift=False):
    if ctrl:event('K 29 1')
    if shift:event('K 42 1')
    event(f'K {code} 1');event(f'K {code} 0')
    if shift:event('K 42 0')
    if ctrl:event('K 29 0')
def state(**wanted):
    rows=re.findall(r'^VIEW (.+)$',log(),re.M);values=dict(re.findall(r'(\w+)=([^ ]+)',rows[-1])) if rows else {};return all(values.get(k)==str(v) for k,v in wanted.items())
def text(field,value,cursor,selection=None):
    command=[str(peer),str(app.pid),field,value,str(cursor)]
    if selection is not None:command.extend(map(str,selection))
    done=subprocess.run(command,env=env,capture_output=True,text=True,timeout=12);checks.append(dict(native_peer=command,exit=done.returncode,stdout=done.stdout,stderr=done.stderr));assert done.returncode==0,(field,value,cursor,selection,done.stdout,done.stderr)
def native_case(mode):
    command=[str(peer),str(app.pid),mode];done=subprocess.run(command,env=env,capture_output=True,text=True,timeout=12);checks.append(dict(native_peer=command,exit=done.returncode,stdout=done.stdout,stderr=done.stderr));assert done.returncode==0,(mode,done.stdout,done.stderr)
def enabled():return log('server').rfind('IME commit enabled=1 ')>log('server').rfind('IME commit enabled=0 ')
def capture(label):
    server.send_signal(signal.SIGUSR1);sequence=len(images)//3+1;wait(lambda:all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)),label)
    visible=False
    for i in range(3):
        p=captures/f'capture-{sequence}-output-{i}.ppm';raw=p.read_bytes();magic,geometry,depth,pixels=raw.split(b'\n',3);w,h=map(int,geometry.split());assert magic==b'P6' and depth==b'255' and len(pixels)==w*h*3
        nonuniform=pixels!=pixels[:3]*(w*h);visible=visible or nonuniform
        packed=p.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));p.unlink();images.append(dict(case=label,output=i,file=packed.name,sha256=hashlib.sha256(raw).hexdigest(),nonuniform=nonuniform))
    assert visible,('all three captures contain only uniform backgrounds',label)

def snapshot(value,cursor,selection=None,after=0):
    anchor=cursor if selection is None else selection[1] if cursor==selection[0] else selection[0]
    expected=(len(value[:cursor].encode()),len(value[:anchor].encode()),value.encode().hex())
    def matches():
        rows=re.findall(r'^IME state enabled=(\d+) serial=(\d+) cursor=(-?\d+) anchor=(-?\d+) hex=([0-9a-f]*)$',log('server')[after:],re.M)
        return bool(rows) and rows[-1][0]=='1' and (int(rows[-1][2]),int(rows[-1][3]),rows[-1][4])==expected
    wait(matches,'processed native surrounding snapshot '+repr((value,cursor,selection)))

def transaction_cases():
    # Every mutation starts from a processed provider snapshot, rather than a
    # timing delay or an older enable notification for a different field state.
    def seed():
        cut=len(log('server'));key(22,True)
        for code in (30,48,46,32,18):key(code)
        text('Name','abcde',5);snapshot('abcde',5,after=cut)
    def expect(value,cursor,selection=None):
        text('Name',value,cursor,selection);snapshot(value,cursor,selection)
    for mode in (16,18,19,24):
        seed();event('I '+str(mode));value='abcdxé' if mode==16 else 'abcdeé' if mode==24 else 'abcd'
        expect(value,len(value));key(44,True);expect('abcde',5);key(21,True);expect(value,len(value))
        if mode==16:capture('native deletion and Unicode replacement')
    seed();event('I 20');expect('abcdexy',7);key(44,True);expect('abcdex',6);key(44,True);expect('abcde',5)
    key(21,True);expect('abcdex',6);key(21,True);expect('abcdexy',7);capture('two native packets keep separate undo')
    seed();key(30,True);text('Name','abcde',5,(0,5));snapshot('abcde',5,(0,5))
    event('I 21');expect('',0);key(44,True);expect('abcde',5,(0,5))
    seed();key(107);key(102,shift=True);text('Name','abcde',0,(0,5));snapshot('abcde',0,(0,5))
    event('I 1');expect('é',1);key(44,True);expect('abcde',0,(0,5));key(21,True);expect('é',1)
    seed();event('I 21');text('Name','abcde',5);key(44,True);expect('abcd',4)
    for mode in (17,22,23):
        seed();event('I '+str(mode));wait(lambda:state(name_bad=1),'whole rejected native proposal '+str(mode));text('Name','abcde',5)
        native_case('invalid');key(28);assert state(dialog=1,name_bad=1)
        assert {p.name:p.read_bytes() for p in documents.iterdir()}==seeds
        checks.append('rejected packet '+str(mode)+' cannot confirm retained valid prefix')
        if mode==22:capture('invalid whole filename retains prefix and blocks confirmation')
    cut=len(log('server'));key(22,True);text('Name','',0);snapshot('',0,after=cut)
    event('I 26');expect('aX́b',4);key(102);key(106);expect('aX́b',1)
    event('I 25');expect('aq́b',3);key(44,True);expect('aX́b',1);key(21,True);expect('aq́b',3)
    capture('atomic deletion avoids intermediate grapheme caret snapping')
    cut=len(log('server'));key(22,True);text('Name','',0);snapshot('',0,after=cut)
    event('I 26');expect('aX́b',4);key(102);key(106);expect('aX́b',1)
    event('I 27');wait(lambda:state(name_bad=1),'second coqueued invalid UTF-8 range rejected');text('Name','aq́b',3)
    native_case('invalid');key(28);assert state(dialog=1,name_bad=1) and {p.name:p.read_bytes() for p in documents.iterdir()}==seeds
    capture('valid first packet retained with rejected second packet')
    cut=len(log('server'));key(22,True);text('Name','',0);snapshot('',0,after=cut)
    seed();event('I 7');wait(lambda:state(dialog=0),'Escape retires target before queued old commit')
    cut=len(log('server'));key(49,True);wait(lambda:state(dialog=1,name_bad=0),'new naming scope after queued old commit')
    text('Name','',0);snapshot('',0,after=cut)
    event('I 8');text('Name','',0);assert state(dialog=1,name_bad=0)
    cut=len(log('server'));key(15,shift=True);text('Name','',0);snapshot('',0,after=cut)
    assert {p.name:p.read_bytes() for p in documents.iterdir()}==seeds
    checks.append('Escape/new scope and Tab discard queued old native commits')
    checks.append('native replacement/empty/delete/sequence/selection/undo/proposal tests passed')
try:
    bus=start(fixture/'usr/bin/dbus-daemon','bus','--nofork','--config-file='+str(conf));wait(lambda:(runtime/'bus').is_socket(),'private bus')
    registry=start(fixture/'usr/libexec/at-spi2-registryd','registry')
    def registry_ready():
        p=subprocess.run(['busctl','--address='+address,'--timeout=1','call','org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','NameHasOwner','s','org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2);return p.returncode==0 and p.stdout.strip()=='b true'
    wait(registry_ready,'private registry');server=start(server_binary,'server','--headless',env['WAYLAND_DISPLAY'],captures,right.fileno(),pass_fds=(right.fileno(),));right.close();wait(lambda:'READY ' in log('server'),'native server');event('I on')
    result['font_inputs']=[dict(path=str(p),size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in (font,*fallback_fonts)]
    host_arguments=['--root',str(documents),'--font',str(font),'--cache',str(cache),'--terminal',str(b/'aetheri_terminal_candidate'),'--editor',str(b/'aetheri_editor_candidate'),'--terminal-font',str(mono),'--diagnostics']
    if args.font_errors:
        invalid=root/'invalid-font.bin';invalid.write_bytes(b'x');oversized=root/'oversized-font.bin'
        with oversized.open('xb') as stream:stream.truncate(4*1024*1024+1)
        trials=[('missing-option',['--fallback-font'],2,None),
                ('excess',['--fallback-font',str(font)]*5,2,None),
                ('missing-file',['--fallback-font',str(root/'absent-font.ttf')],1,'could not be loaded within its byte limit'),
                ('oversized',['--fallback-font',str(oversized)],1,'could not be loaded within its byte limit'),
                ('malformed',['--fallback-font',str(invalid)],1,'unsupported or invalid')]
        result['font_startup_checks']=[]
        for label,extra,expected,diagnostic in trials:
            command=[str(b/'aetheri_files_ime_candidate'),*host_arguments,*extra]
            done=subprocess.run(command,env=env,capture_output=True,text=True,timeout=12)
            result['font_startup_checks'].append(dict(case=label,command=command,exit=done.returncode,stdout=done.stdout,stderr=done.stderr))
            assert done.returncode==expected and (diagnostic is None or diagnostic in done.stderr),(label,done.returncode,done.stdout,done.stderr)
            if expected==1:assert 'CLOSED files frames=0 failed=1' in done.stdout
            assert {p.name:p.read_bytes() for p in documents.iterdir()}==seeds
    app=start(b/'aetheri_files_ime_candidate','app',*host_arguments,*fallback_arguments);wait(lambda:state(count=2,dialog=0),'original initial view')
    maps=Path(f'/proc/{app.pid}/maps').read_text();assert (sdk['library'] in maps) if profile=='shared' else '/libnexaui.so' not in maps;assert not any(name in maps for name in ('libSDL','libX11','libvulkan','libEGL'));(root/'maps.txt').write_text(maps)
    key(38,True);wait(lambda:enabled() and re.search(r'IME commit enabled=1 .*area=\d+,\d+,[1-9]\d*,[1-9]\d*',log('server')),'query native IME activation/candidate rectangle');text('Search this folder','',0)
    frames=log().count('VIEW ');event('I 0');wait(lambda:log().count('VIEW ')>frames,'preedit repainted');text('Search this folder','',0);native_case('preedit-no-placeholder');capture('native query preedit')
    key(28);text('Search this folder','',0);assert state(count=2,dialog=0) and 'OPEN pid=' not in log();checks.append('composition Enter did not activate retained file')
    start_at=len(log('server'));key(1);wait(lambda:'IME commit enabled=0 ' in log('server')[start_at:] and enabled(),'Escape cancellation and reactivation');text('Search this folder','',0);capture('query after Escape cancellation')
    event('I 1');wait(lambda:state(count=1,dialog=0),'Unicode commit filters original model');text('Search this folder','é',1);capture('native Unicode query')
    for _ in range(3):key(66)
    wait(lambda:state(scale='2.00'),'200-percent native text scale');event('I 0');text('Search this folder','é',1);capture('native preedit at 200 percent');key(1);wait(enabled,'200-percent composition cancelled');text('Search this folder','é',1)
    key(66);wait(lambda:state(scale='1.00'),'native text scale restored');event('I 6');text('Search this folder','é',1);capture('native combining preedit');key(1);wait(enabled,'combining composition cancelled');text('Search this folder','é',1)
    start_at=len(log('server'));key(15);wait(lambda:'IME commit enabled=0 ' in log('server')[start_at:],'Tab withdraws native input');text('Search this folder','é',1)
    key(38,True);wait(enabled,'query reactivated');text('Search this folder','',0);event('I 0');key(49,True);assert state(dialog=0);checks.append('composition Ctrl+N did not open naming');key(1);wait(enabled,'composition cancelled before naming')
    key(49,True);wait(lambda:state(dialog=1),'original naming opened');text('Name','',0);wait(enabled,'name native IME activation')
    if args.clipboard_tools:
        from clipboard_task import ClipboardTask
        clipboard_task=ClipboardTask(root,args.clipboard_tools,env,key,text,snapshot,state,native_case,wait,capture,result,lambda:app)
        clipboard_task.run(args.clipboard_budget)
    if args.transactions:transaction_cases()
    event('I 1');text('Name','é',1);capture('native name commit')
    event('I 5');wait(lambda:state(name_bad=1),'filtered native commit blocks retained prefix');native_case('invalid');text('Name','é',1);capture('filtered native commit retains prefix and disables confirmation');key(28);assert not (documents/'literalcommand').exists() and not (documents/'éliteralcommand').exists();checks.append('filtered commit cannot confirm retained prefix')
    key(22,True);text('Name','',0);event('K 49 1');event('K 49 0');text('Name','n',1);event('I 1');text('Name','né',2);key(28);wait(lambda:(documents/'né').is_file() and state(dialog=0),'real original create after explicit clear');assert (documents/'né').read_bytes()==b''
    for name,data in seeds.items():assert (documents/name).read_bytes()==data
    if args.transactions:
        key(38,True);key(49);text('Search this folder','n',1);wait(lambda:state(count=1),'created file selected by original filter')
        cut=len(log('server'));key(60);wait(lambda:state(dialog=1),'original rename opened for created file')
        text('Name','né',2);snapshot('né',2,after=cut);key(30,True)
        text('Name','né',2,(0,2));snapshot('né',2,(0,2))
        event('I 20');text('Name','xy',2);snapshot('xy',2)
        key(44,True);text('Name','x',1);key(44,True);text('Name','né',2,(0,2))
        key(21,True);text('Name','x',1);key(21,True);text('Name','xy',2)
        capture('original rename with native packets and separate undo')
        key(28);wait(lambda:(documents/'xy').is_file() and not (documents/'né').exists() and state(dialog=0),'real original rename after native edits')
        assert (documents/'xy').read_bytes()==b''
        for name,data in seeds.items():assert (documents/name).read_bytes()==data
        key(38,True);wait(enabled,'native input reactivated before close');event('I 0')
    if clipboard_task:clipboard_task.pending_close()
    key(16,True);assert app.wait(timeout=8)==0
    if clipboard_task:clipboard_task.closed()
    children.remove(app);result['result']='passed'
finally:
    if clipboard_task:clipboard_task.close()
    for p in reversed(children):
        if p.poll() is None:
            p.terminate()
            try:p.wait(timeout=8)
            except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
    for stream in streams:stream.close()
    left.close();right.close();result.update(checks=checks,images=images,binaries={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (b/'aetheri_files_ime_candidate',peer)},source_delta=json.loads((b/'files-ime-generated/source-delta.json').read_text()));(root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(result=result['result'],artifacts=str(final),checks=len(checks),images=len(images))))
