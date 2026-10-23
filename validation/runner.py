from pathlib import Path
import subprocess,os,sys,json,time,hashlib,signal,xml.etree.ElementTree as ET,shutil
source=Path(sys.argv[1]).resolve();payload=Path(__file__).resolve().parent;w=Path(sys.argv[2]).resolve();w.mkdir(parents=True,exist_ok=True)
manifest=json.loads((payload/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'terminal':False,'steps':[],'patch_sha256':manifest['patch_sha256'],'source_manifest_sha256':sha(payload/'manifest.json'),'scope':'Owned AppKit Value with copied core ancestry; Release and ASanUBSan native subgroups/portable regressions and identical-program old-library controls; hosted Mac only.'}
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
        run(mode+'-configure',['cmake','-S',source,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE='+('Debug' if san else 'Release'),'-DBUILD_SHARED_LIBS='+shared,'-DNEXAUI_BUILD_EXAMPLES=OFF','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',*flags])
        programs=['nexaui_macos_text_value_tests']
        programs+=['nexaui_macos_multiselection_tests','nexaui_macos_accessibility_tests','nexaui_text_value_tests','nexaui_text_field_tests','nexaui_numeric_field_tests']
        run(mode+'-build',['cmake','--build',b,'--parallel','3','--target',*programs],timeout=360)
        for p in programs:report.setdefault('programs',[]).append(binary(b/p))
        group_results=[]
        for group in ['operations','bounds','worker','strings','stale','slots','core']:
            code=run(mode+'-'+group,[b/'nexaui_macos_text_value_tests',group],expected=(0,1),timeout=180)
            group_results.append({'group':group,'exit':code})
        report.setdefault('groups',{})[mode]=group_results;save()
        if True:
            run(mode+'-regressions',['ctest','--test-dir',b,'-R','^nexaui_(macos_(multiselection|accessibility)|text_value|text_field|numeric_field)$','--no-tests=error','--verbose','--output-junit',b/'focused.xml'],timeout=300)
            cases=list(ET.parse(b/'focused.xml').getroot().iter('testcase'));assert len(cases)==5 and not any(c.find(t) is not None for c in cases for t in ['failure','error','skipped'])
        if not san:
            report['libraries']=[binary(b/'libnexaui.0.dylib')]
            for control in manifest['controls']:
                library=payload/control['path'];assert sha(library)==control['sha256']
                old=b/control['name'];old.mkdir();loaded=old/'libnexaui.0.dylib';shutil.copyfile(library,loaded)
                report.setdefault('controls',[]).append({'name':control['name'],'library':binary(loaded),'program':binary(b/'nexaui_macos_text_value_tests')});save()
                original_env=env.copy();env.update(DYLD_LIBRARY_PATH=str(old),DYLD_PRINT_LIBRARIES='1')
                group='stale' if control['name']=='old-core' else 'operations'
                run('identical-program-'+control['name'],[b/'nexaui_macos_text_value_tests',group],expected=(1,))
                raw=(w/('identical-program-'+control['name']+'.stderr')).read_text();assert str(loaded) in raw
                assert ('actual=stale' in raw and 'text(s.fields[0],"seed")==0' in raw) if control['name']=='old-core' else 'editor && [editor isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]' in raw
                env.clear();env.update(original_env)

    assert all(g['exit']==0 for groups in report['groups'].values() for g in groups),'native subgroup failure; raw results preserved'
    report.update(terminal=True,success=True);save()
except BaseException as error:
    report.update(terminal=True,success=False,error=str(error));save();raise
