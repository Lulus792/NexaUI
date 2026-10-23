from pathlib import Path
import subprocess,os,sys,json,time,hashlib,signal,shutil
source=Path(sys.argv[1]).resolve();payload=Path(__file__).resolve().parent;w=Path(sys.argv[2]).resolve();w.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();manifest=json.loads((payload/'manifest.json').read_text())
report={'terminal':False,'steps':[],'manifest_sha256':sha(payload/'manifest.json'),'scope':'Unchanged final v7 native/host/client binaries with fixed/old/mutated libraries; no user Mac'};env=os.environ.copy()
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,args,expected=0,timeout=180):
 row={'name':name,'command':[str(a) for a in args]};report['steps'].append(row);save()
 with (w/(name+'.stdout')).open('x') as out,(w/(name+'.stderr')).open('x') as err:
  p=subprocess.Popen(row['command'],cwd=source,env=env,stdin=subprocess.DEVNULL,stdout=out,stderr=err,start_new_session=True)
  try:row['exit']=p.wait(timeout=timeout)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();row['exit']=p.returncode;row['timeout']=True
 save();print(name,row['exit'],flush=True);assert row['exit']==expected and not row.get('timeout'),name
def binary(p):return {'path':str(p),'sha256':sha(p),'uuid':subprocess.check_output(['dwarfdump','--uuid',p],text=True),'linked':subprocess.check_output(['otool','-L',p],text=True)}
try:
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==manifest['base']
 assert sha(payload/'candidate.patch')==manifest['patch_sha256'];run('apply',['git','apply',payload/'candidate.patch'])
 for f in manifest['sources']:assert sha(source/f['path'])==f['git_lf_sha256']
 programs={};libraries={}
 for f in manifest['binaries']:
  original=payload/f['payload'];assert sha(original)==f['sha256']
  folder=w/('programs' if f['kind']=='program' else f['name']);folder.mkdir(exist_ok=True)
  dst=folder/(f['name'] if f['kind']=='program' else 'libnexaui.0.dylib');shutil.copyfile(original,dst)
  if f['kind']=='program':dst.chmod(0o700);programs[f['name']]=dst
  else:libraries[f['name']]=dst
  report.setdefault('binaries',[]).append(binary(dst))
 native=programs['nexaui_macos_text_value_tests'];host=programs['nexaui_macos_text_value_host'];client=programs['nexaui_macos_text_value_client']
 for name,program,args,code,needle in [
  ('fixed-strings',native,['strings'],0,'all five NSString'),
  ('fixed-external',host,[client,'visible'],0,'commands=59'),
  ('old-base',native,['operations'],1,'editor && [editor isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]'),
  ('old-core',native,['stale'],1,'forwarded=1 kind=5 actual=stale'),
  ('old-policy',host,[client,'visible'],1,'settable error=0 actual=1 expected=0')]:
  library=libraries['fixed' if name.startswith('fixed') else name];env.update(DYLD_LIBRARY_PATH=str(library.parent),DYLD_PRINT_LIBRARIES='1')
  run(name,[program,*args],code);raw=(w/(name+'.stdout')).read_text()+(w/(name+'.stderr')).read_text();assert str(library) in raw and needle in raw
  report.setdefault('controls',[]).append({'name':name,'program_sha256':sha(program),'library_sha256':sha(library),'expected_exit':code,'actual_loaded_path_verified':str(library),'exact_diagnostic':needle});save()
 env.pop('DYLD_LIBRARY_PATH',None);env.pop('DYLD_PRINT_LIBRARIES',None)
 p=source/'platform/macos/accessibility.m';original=p.read_text();start=original.index('- (BOOL)requestText:',original.index('@implementation NXMacAXBridge'));end=original.index('- (NSArray *)selected:',start)
 segment=original[start:end];assert segment.count('publication==revision')==5
 mutated=original[:start]+segment.replace('publication==revision','1')+original[end:];p.write_text(mutated)
 report['mutation']={'scope':'Only five monotone publication comparisons in requestText bypassed; guards/limits/copy bounds unchanged','original_sha256':hashlib.sha256(original.encode()).hexdigest(),'mutated_sha256':sha(p)}
 (w/'publication-mutated.m').write_text(mutated);save();clang=subprocess.check_output(['xcrun','--find','clang'],text=True).strip();sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip();b=w/'publication-mutant'
 run('mutation-configure',['cmake','-S',source,'-B',b,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=ON','-DNEXAUI_BUILD_TESTS=OFF','-DNEXAUI_BUILD_EXAMPLES=OFF','-DCMAKE_C_COMPILER='+clang,'-DCMAKE_OBJC_COMPILER='+clang,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
 run('mutation-build',['cmake','--build',b,'--target','nexaui','--parallel','3'],timeout=360)
 library=b/'libnexaui.0.dylib';report['mutated_library']=binary(library);env.update(DYLD_LIBRARY_PATH=str(b),DYLD_PRINT_LIBRARIES='1');save()
 run('publication-mutant',[native,'strings'],1);raw=(w/'publication-mutant.stderr').read_text();assert str(library) in raw and 'stage=0 kind=0 fired=1/1 error=0 accepted=1' in raw
 report['controls'].append({'name':'publication-mutant','program_sha256':sha(native),'library_sha256':sha(library),'expected_exit':1,'actual_loaded_path_verified':str(library),'exact_diagnostic':'stage=0 kind=0 fired=1/1 error=0 accepted=1'})
 for f in manifest['binaries']:
  if f['kind']=='program':assert sha(programs[f['name']])==f['sha256']
 report.update(terminal=True,success=True);save()
except BaseException as error:report.update(terminal=True,success=False,error=str(error));save();raise
