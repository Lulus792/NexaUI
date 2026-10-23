from pathlib import Path
import subprocess,os,sys,json,time,hashlib,signal,xml.etree.ElementTree as ET
source=Path(sys.argv[1]).resolve();payload=Path(__file__).resolve().parent;w=Path(sys.argv[2]).resolve();w.mkdir(parents=True,exist_ok=True)
manifest=json.loads((payload/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'terminal':False,'steps':[],'patch_sha256':manifest['patch_sha256'],'source_manifest_sha256':sha(payload/'manifest.json'),'scope':'New legacy attribute-query policy requires native and OS AX regressions; user Mac untouched.'}
env=os.environ.copy();env.update(ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,command,expected=(0,),timeout=180):
 row={'name':name,'command':[str(c) for c in command],'started':time.time()};report['steps'].append(row);save()
 with (w/(name+'.stdout')).open('x') as out,(w/(name+'.stderr')).open('x') as err:
  p=subprocess.Popen(row['command'],env=env,cwd=source,stdin=subprocess.DEVNULL,stdout=out,stderr=err,start_new_session=True)
  try:row['exit']=p.wait(timeout=timeout)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();row['exit']=p.returncode;row['timeout']=True
 row['seconds']=time.time()-row['started'];save();print(name,row['exit'],flush=True);assert row['exit'] in expected and not row.get('timeout'),name;return row['exit']
def binary(p):return {'path':str(p),'sha256':sha(p),'uuid':subprocess.check_output(['dwarfdump','--uuid',p],text=True),'linked':subprocess.check_output(['otool','-L',p],text=True)}
try:
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==manifest['base']
 assert sha(payload/'candidate.patch')==manifest['patch_sha256'];run('apply',['git','apply',payload/'candidate.patch'])
 for f in manifest['sources']:assert sha(source/f['path'])==f['git_lf_sha256'],f['path']
 clang=subprocess.check_output(['xcrun','--find','clang'],text=True).strip();sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
 report['compiler']=subprocess.check_output([clang,'--version'],text=True);report['system']=subprocess.check_output(['sw_vers'],text=True)
 programs=['nexaui_macos_text_value_tests','nexaui_macos_text_value_host','nexaui_macos_text_value_client','nexaui_macos_multiselection_tests','nexaui_macos_multiselection_host','nexaui_macos_multiselection_client','nexaui_macos_accessibility_tests','nexaui_macos_ax_client']
 for mode,shared,san in [('shared-release','ON',False),('static-asan','OFF',True)]:
  b=w/mode;flags=['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_OBJC_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'] if san else []
  run(mode+'-configure',['cmake','-S',source,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE='+('Debug' if san else 'Release'),'-DBUILD_SHARED_LIBS='+shared,'-DNEXAUI_BUILD_EXAMPLES=OFF','-DNEXAUI_TEST_MACOS_AX_VISIBLE=ON','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',*flags])
  run(mode+'-build',['cmake','--build',b,'--parallel','3','--target',*programs],timeout=360)
  for p in programs:report.setdefault('programs',[]).append(binary(b/p))
  if not san:report['libraries']=[binary(b/'libnexaui.0.dylib')]
  code=run(mode+'-test',['ctest','--test-dir',b,'-R','^nexaui_macos_(text_value(_external)?|multiselection(_external)?|accessibility|ax_external)$','--no-tests=error','--verbose','--output-junit',b/'focused.xml'],expected=(0,8),timeout=300)
  cases=list(ET.parse(b/'focused.xml').getroot().iter('testcase'));assert len(cases)==6
  report.setdefault('suites',[]).append({'mode':mode,'exit':code,'cases':len(cases),'skips':sum(c.find('skipped') is not None for c in cases)});save()
 assert all(s['exit']==0 and not s['skips'] for s in report['suites']),'native/OS AX case failure; raw diagnostics preserved'
 report.update(terminal=True,success=True);save()
except BaseException as error:report.update(terminal=True,success=False,error=str(error));save();raise
