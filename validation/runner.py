from pathlib import Path
import subprocess,sys,os,json,hashlib,time,signal
source=Path(sys.argv[1]).resolve();w=Path(sys.argv[2]).resolve();w.mkdir(parents=True,exist_ok=True);payload=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((payload/'manifest.json').read_text());report={'terminal':False,'steps':[],'scope':'New hidden native Value regression against unchanged12ce provider; expected precise missing writable setter, no user Mac or permission changes','source_manifest_sha256':sha(payload/'manifest.json')}
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,cmd,expected=0,limit=180):
    row={'name':name,'command':[str(c) for c in cmd],'started':time.time()};report['steps'].append(row);save()
    with (w/(name+'.stdout')).open('x') as out,(w/(name+'.stderr')).open('x') as err:
        p=subprocess.Popen(row['command'],cwd=source,stdout=out,stderr=err,stdin=subprocess.DEVNULL,start_new_session=True)
        try:row['exit']=p.wait(timeout=limit)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();row.update(exit=p.returncode,timeout=True)
    row['seconds']=time.time()-row['started'];save();print(name,row['exit'],flush=True);assert row['exit']==expected and not row.get('timeout'),name
try:
    assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==m['base'];assert sha(payload/'candidate.patch')==m['patch_sha256']
    run('apply',['git','apply',payload/'candidate.patch'])
    for row in m['sources']:assert sha(source/row['path'])==row['git_lf_sha256']
    for path in ['platform/macos/accessibility.m','src/nexaui.c','src/text_value.h']:
        assert sha(source/path)==hashlib.sha256(subprocess.check_output(['git','show','HEAD:'+path],cwd=source)).hexdigest();report.setdefault('unchanged_sdk_sources',[]).append({'path':path,'sha256':sha(source/path)})
    clang=subprocess.check_output(['xcrun','--find','clang'],text=True).strip();sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip();b=w/'shared-release'
    report.update(system=subprocess.check_output(['sw_vers'],text=True),compiler=subprocess.check_output([clang,'--version'],text=True))
    run('configure',['cmake','-S',source,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=ON','-DNEXAUI_BUILD_EXAMPLES=OFF','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
    run('build',['cmake','--build',b,'--parallel','3','--target','nexaui_macos_text_value_tests'],limit=360)
    run('native-negative',['ctest','--test-dir',b,'-R','^nexaui_macos_text_value$','--no-tests=error','--verbose','--output-junit',b/'baseline.xml'],expected=8)
    raw=(w/'native-negative.stdout').read_text()+(w/'native-negative.stderr').read_text()
    assert 'editor && [editor isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]' in raw and '***Failed' in raw
    for p in [b/'nexaui_macos_text_value_tests',b/'libnexaui.0.dylib']:
        report.setdefault('programs',[]).append({'path':str(p),'sha256':sha(p),'uuid':subprocess.check_output(['dwarfdump','--uuid',p],text=True),'loaded_dependencies':subprocess.check_output(['otool','-L',p],text=True)})
    report.update(terminal=True,success=True,actual_sdk_failure='missing writable AXValue setter')
except BaseException as error:report.update(terminal=True,success=False,error=str(error));save();raise
save()
