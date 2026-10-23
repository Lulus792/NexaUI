from pathlib import Path
import subprocess, os, sys, json, hashlib, shutil, time, signal, xml.etree.ElementTree as ET

source=Path(sys.argv[1]).resolve(); payload=Path(__file__).resolve().parent
work=Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
manifest=json.loads((payload/'manifest.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(payload/'candidate.patch')==manifest['patch_sha256']
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==manifest['base']
subprocess.run(['git','apply','--check',str(payload/'candidate.patch')],cwd=source,check=True)
subprocess.run(['git','apply',str(payload/'candidate.patch')],cwd=source,check=True)
for row in manifest['sources']:
    assert hashlib.sha256((source/row['path']).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==row['git_lf_sha256'],row['path']
report={'terminal':False,'steps':[],'base':manifest['base'],'patch_sha256':manifest['patch_sha256'],
        'source_manifest_sha256':sha(payload/'manifest.json'),'scope':'Hosted hidden native AppKit server; no user Mac, external AX or physical input'}
env=os.environ.copy();env.update(ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
assert not env.get('DYLD_INSERT_LIBRARIES') and not env.get('DYLD_LIBRARY_PATH')
def save():(work/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,cmd,timeout=180,expected=0,extra=None):
    row={'name':name,'command':[str(x) for x in cmd],'started_unix':time.time(),'expected':expected};report['steps'].append(row);save()
    with (work/(name+'.stdout')).open('x') as out,(work/(name+'.stderr')).open('x') as err:
        child_env=env.copy();child_env.update(extra or {})
        p=subprocess.Popen(row['command'],cwd=source,env=child_env,stdin=subprocess.DEVNULL,stdout=out,stderr=err,start_new_session=True)
        row['pid']=p.pid;save()
        try:row['exit']=p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid,signal.SIGTERM)
            try:p.wait(timeout=5)
            except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
            row['exit']=p.returncode;row['timeout']=True
    row['seconds']=time.time()-row['started_unix'];save();print(name,row['exit'],flush=True)
    assert not row.get('timeout') and (row['exit']==expected if expected is not None else row['exit']!=0),name
    return (work/(name+'.stderr')).read_text(errors='replace')
def configure(name,root,shared,san=False):
    b=work/name
    flags=['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_OBJC_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'] if san else []
    run(name+'-configure',['cmake','-S',root,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE='+('Debug' if san else 'Release'),'-DBUILD_SHARED_LIBS='+shared,'-DNEXAUI_BUILD_EXAMPLES=OFF','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',*flags])
    return b
def binary_record(path):
    return {'path':str(path),'sha256':sha(path),'uuid':subprocess.check_output(['dwarfdump','--uuid',path],text=True),'linked':subprocess.check_output(['otool','-L',path],text=True)}
try:
    clang=subprocess.check_output(['xcrun','--find','clang'],text=True).strip()
    sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip();assert Path(sdk).is_dir()
    report['sdk']=sdk
    report['compiler']=subprocess.check_output([clang,'--version'],text=True);report['system']=subprocess.check_output(['sw_vers'],text=True)
    canary=work/'canary.c';canary.write_text('#include <stdlib.h>\nint main(int argc,char **argv){volatile char *p=malloc(1);p[argc]=1;free((void*)p);return 0;}\n')
    run('canary-build',[clang,'-isysroot',sdk,'-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',canary,'-o',work/'canary'])
    diagnosis=run('canary-run',[work/'canary'],30,None)
    assert 'AddressSanitizer: heap-buffer-overflow' in diagnosis
    for name,shared,san in [('shared-release','ON',False),('static-asan','OFF',True)]:
        b=configure(name,source,shared,san)
        run(name+'-build',['cmake','--build',b,'--parallel','3','--target','nexaui_macos_multiselection_tests'],360)
        run(name+'-test',['ctest','--test-dir',b,'-R','^nexaui_macos_multiselection$','--no-tests=error','--output-on-failure','--output-junit',b/'focused.xml'],120)
        cases=list(ET.parse(b/'focused.xml').getroot().iter('testcase'));assert len(cases)==1 and not any(c.find(t) is not None for c in cases for t in ['failure','error','skipped'])
        report.setdefault('programs',[]).append(binary_record(b/'nexaui_macos_multiselection_tests'))
        commands=json.loads((b/'compile_commands.json').read_text());selected=[c for c in commands if Path(c['file']).name in ['accessibility.m','window.m','test_macos_multiselection.m']]
        assert len(selected)==3
        for c in selected:
            assert '-std=c17' in c['command'] and ('-fsanitize=address,undefined' in c['command'])==san
            if not san:assert '-O3' in c['command'] and '-DNDEBUG' in c['command']
        report.setdefault('compiler_commands',[]).append({'mode':name,'commands':selected})
    # The exact same Release program must reject the old Multi-to-Single SDK.
    old=work/'v1-source';shutil.copytree(source,old,ignore=shutil.ignore_patterns('.git'))
    shutil.copy2(payload/'v1-accessibility.m',old/'platform/macos/accessibility.m')
    b=configure('v1-release',old,'ON');run('v1-release-build',['cmake','--build',b,'--parallel','3','--target','nexaui'],360)
    exe=work/'shared-release/nexaui_macos_multiselection_tests';program_hash=sha(exe)
    for name,lib,expected in [('positive',work/'shared-release/libnexaui.0.dylib',0),('previous',b/'libnexaui.0.dylib',1)]:
        folder=work/'bindings'/name;folder.mkdir(parents=True);dst=folder/'libnexaui.0.dylib';shutil.copy2(lib.resolve(),dst)
        text=run(name+'-demotion',[exe,'demotion'],60,expected,{'DYLD_LIBRARY_PATH':str(folder),'DYLD_PRINT_LIBRARIES':'1'})
        assert str(dst) in text and sha(exe)==program_hash
        if name=='previous':assert 'old-owner=1 old-item=1' in text and 'actual=1001' in text
        report.setdefault('libraries',[]).append({'case':name,**binary_record(dst),'same_program_sha256':program_hash})
    b=configure('v1-asan',old,'OFF',True);run('v1-asan-build',['cmake','--build',b,'--parallel','3','--target','nexaui_macos_multiselection_tests'],360)
    text=run('previous-aba',[b/'nexaui_macos_multiselection_tests','aba'],60,None)
    assert 'AddressSanitizer: heap-buffer-overflow' in text and 'verified=1' in text
    report['terminal']=True;report['success']=True;save()
except BaseException as error:
    report['terminal']=True;report['success']=False;report['error']=str(error);save();raise
