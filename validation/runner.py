from pathlib import Path
import subprocess,os,json,sys,time,hashlib,signal
source=Path(sys.argv[1]).resolve();payload=Path(__file__).resolve().parent;w=Path(sys.argv[2]).resolve();w.mkdir(parents=True,exist_ok=True)
manifest=json.loads((payload/'manifest.json').read_text());report={'terminal':False,'steps':[],'patch_sha256':manifest['patch_sha256'],'scope':'Independent OS AX client and observer on isolated hosted macOS only; no user Mac or physical input'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,command,accepted=(0,),timeout=120):
    row={'name':name,'command':[str(a) for a in command],'started':time.time()};report['steps'].append(row);save()
    with (w/(name+'.stdout')).open('x') as out,(w/(name+'.stderr')).open('x') as err:
        p=subprocess.Popen(row['command'],cwd=source,stdout=out,stderr=err,start_new_session=True,stdin=subprocess.DEVNULL)
        try:row['exit']=p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();row['exit']=p.returncode;row['timeout']=True
    row['seconds']=time.time()-row['started'];save();print(name,row['exit'],flush=True)
    assert row['exit'] in accepted and not row.get('timeout'),name
    return row['exit']
try:
    clang=subprocess.check_output(['xcrun','--find','clang'],text=True).strip();sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
    report['compiler']=subprocess.check_output([clang,'--version'],text=True);report['system']=subprocess.check_output(['sw_vers'],text=True)
    run('client-build',[clang,'-isysroot',sdk,'-std=c17','-O2','-Wall','-Wextra',payload/'client.c','-framework','ApplicationServices','-framework','CoreFoundation','-o',w/'client'])
    trust=run('trust',[w/'client','--trust'],(0,77));report['trust']=trust==0
    if trust==77:
        report.update(terminal=True,success=False,external_acceptance=False,limitation='Actual independent AXIsProcessTrusted false; no prompt requested');save();sys.exit(0)
    assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==manifest['base']
    assert sha(payload/'candidate.patch')==manifest['patch_sha256'];run('apply',['git','apply',payload/'candidate.patch'])
    for f in manifest['sources']:assert hashlib.sha256((source/f['path']).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==f['git_lf_sha256']
    b=w/'build';run('configure',['cmake','-S',source,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=ON','-DNEXAUI_BUILD_TESTS=OFF','-DNEXAUI_BUILD_EXAMPLES=OFF','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
    run('sdk-build',['cmake','--build',b,'--parallel','3','--target','nexaui'],timeout=300)
    run('host-build',[clang,'-isysroot',sdk,'-std=c17','-O2','-DNDEBUG','-fno-objc-arc','-I'+str(source/'tests'),'-I'+str(source/'include'),'-I'+str(source/'platform/macos'),payload/'host.m','-L'+str(b),'-lnexaui','-Wl,-rpath,'+str(b),'-framework','AppKit','-o',w/'host'])
    for name in ['host','client']:
        p=w/name;report.setdefault('programs',[]).append({'name':name,'sha256':sha(p),'uuid':subprocess.check_output(['dwarfdump','--uuid',p],text=True),'linked':subprocess.check_output(['otool','-L',p],text=True)})
    hidden=run('hidden-client',[w/'host',w/'client','hidden'],(0,78),150);report['hidden_hierarchy_reachable']=hidden==0
    if hidden==78:run('hosted-visible-client',[w/'host',w/'client','visible'],timeout=150)
    report.update(terminal=True,success=True,external_acceptance=True);save()
except BaseException as error:
    if isinstance(error,SystemExit) and error.code==0:raise
    report.update(terminal=True,success=False,error=str(error));save();raise
