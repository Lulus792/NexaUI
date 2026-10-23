from pathlib import Path
import subprocess,os,sys,json,time,hashlib,signal,xml.etree.ElementTree as ET
source=Path(sys.argv[1]).resolve();payload=Path(__file__).resolve().parent;w=Path(sys.argv[2]).resolve();w.mkdir(parents=True,exist_ok=True)
manifest=json.loads((payload/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'terminal':False,'steps':[],'patch_sha256':manifest['patch_sha256'],'source_manifest_sha256':sha(payload/'manifest.json'),'scope':'Permanent CMake native and external AX regression on isolated hosted macOS. No user Mac.'}
env=os.environ.copy();env.update(ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,command,expected=(0,),timeout=180):
    row={'name':name,'command':[str(c) for c in command],'started':time.time()};report['steps'].append(row);save()
    with (w/(name+'.stdout')).open('x') as out,(w/(name+'.stderr')).open('x') as err:
        p=subprocess.Popen(row['command'],env=env,cwd=source,stdin=subprocess.DEVNULL,stdout=out,stderr=err,start_new_session=True)
        try:row['exit']=p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();row['exit']=p.returncode;row['timeout']=True
    row['seconds']=time.time()-row['started'];save();print(name,row['exit'],flush=True)
    assert row['exit'] in expected and not row.get('timeout'),name
    return row['exit']
def binary(p):return {'path':str(p),'sha256':sha(p),'uuid':subprocess.check_output(['dwarfdump','--uuid',p],text=True),'linked':subprocess.check_output(['otool','-L',p],text=True)}
try:
    assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==manifest['base']
    assert sha(payload/'candidate.patch')==manifest['patch_sha256'];run('apply',['git','apply',payload/'candidate.patch'])
    for f in manifest['sources']:assert sha(source/f['path'])==f['git_lf_sha256'],f['path']
    clang=subprocess.check_output(['xcrun','--find','clang'],text=True).strip();sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
    report['compiler']=subprocess.check_output([clang,'--version'],text=True);report['system']=subprocess.check_output(['sw_vers'],text=True)
    for mode,shared,san in [('shared-release','ON',False),('static-asan','OFF',True)]:
        b=w/mode;flags=['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_OBJC_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'] if san else []
        run(mode+'-configure',['cmake','-S',source,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE='+('Debug' if san else 'Release'),'-DBUILD_SHARED_LIBS='+shared,'-DNEXAUI_BUILD_EXAMPLES=OFF','-DNEXAUI_TEST_MACOS_AX_VISIBLE=ON','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',*flags])
        run(mode+'-build',['cmake','--build',b,'--parallel','3','--target','nexaui_macos_multiselection_tests','nexaui_macos_multiselection_host'],timeout=360)
        run(mode+'-test',['ctest','--test-dir',b,'-R','^nexaui_macos_multiselection(_external)?$','--no-tests=error','--verbose','--output-junit',b/'focused.xml'],timeout=300)
        cases=list(ET.parse(b/'focused.xml').getroot().iter('testcase'));assert len(cases)==2 and not any(c.find(t) is not None for c in cases for t in ['failure','error','skipped'])
        for p in ['nexaui_macos_multiselection_tests','nexaui_macos_multiselection_host','nexaui_macos_multiselection_client']:report.setdefault('programs',[]).append(binary(b/p))
        if not san:
            # The default hidden opt-in remains enforceable by the permanent host.
            run('hidden-optout',[b/'nexaui_macos_multiselection_host',b/'nexaui_macos_multiselection_client','hidden'],expected=(77,),timeout=90)
            report['libraries']=[binary(b/'libnexaui.0.dylib')]
    report.update(terminal=True,success=True);save()
except BaseException as error:
    report.update(terminal=True,success=False,error=str(error));save();raise
