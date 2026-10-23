from pathlib import Path
import subprocess,os,json,urllib.request,time,hashlib,sys,signal,platform,re
w=Path(sys.argv[1]).resolve();w.mkdir(parents=True,exist_ok=True)
dump=subprocess.check_output(['apt-config','dump'],text=True)
report={'scope':'Bounded APT-vs-HTTP transfer diagnosis on disposable hosted runner; no installs, source/deadline/check or persistent apt-config changes','platform':platform.platform(),'apt_version':subprocess.check_output(['apt-get','--version'],text=True).splitlines()[0],'config':[line for line in dump.splitlines() if re.search(r'Dl-Limit|Pipeline-Depth|Queue-Mode|Retries|ForceIPv|^Acquire::http::Timeout|^Acquire::https::Timeout',line)],'samples':[]}
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
package='fonts-urw-base35';text=subprocess.check_output(['apt-cache','show','--no-all-versions',package],text=True)
fields=dict(line.split(': ',1) for line in text.split('\n\n')[0].splitlines() if ': ' in line and not line.startswith(' '));filename=fields['Filename']
report['metadata']={k:fields[k] for k in ['Package','Version','Filename','Size','SHA256']};save()
url='http://azure.archive.ubuntu.com/ubuntu/'+filename
for name,headers in [('http-full-prefix',{}),('http-range',{'Range':'bytes=0-1048575'}),('apt-user-agent-prefix',{'User-Agent':report['apt_version']})]:
    row={'name':name,'url':url,'bytes':0};start=time.monotonic()
    try:
        with urllib.request.urlopen(urllib.request.Request(url,headers=headers),timeout=20) as response:
            row.update(status=response.status,content_length=response.headers.get('Content-Length'),content_range=response.headers.get('Content-Range'));data=response.read(1048576);row.update(bytes=len(data),prefix_sha256=hashlib.sha256(data).hexdigest())
    except Exception as error:row['error']=str(error)
    row['seconds']=time.monotonic()-start;report['samples'].append(row);save();print(json.dumps(row),flush=True)
for name,options in [('apt-default',[]),('apt-no-download-limit',['-o','Acquire::http::Dl-Limit=0']),('apt-other-agent',['-o','Acquire::http::User-Agent=NexaUI-bounded-transfer-diagnosis'])]:
    directory=w/name;directory.mkdir();command=['apt-get',*options,'download',package];row={'name':name,'command':command};start=time.monotonic()
    with (w/(name+'.stdout')).open('x') as out,(w/(name+'.stderr')).open('x') as err:
        p=subprocess.Popen(command,cwd=directory,stdout=out,stderr=err,stdin=subprocess.DEVNULL,start_new_session=True)
        try:row['exit']=p.wait(timeout=35)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10);row.update(exit=p.returncode,timeout=True)
    row['seconds']=time.monotonic()-start;row['files']=[]
    for file in directory.rglob('*'):
        if not file.is_file():continue
        row['files'].append({'name':str(file.relative_to(directory)),'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
    if row['exit']==0:assert any(f['sha256']==fields['SHA256'] and f['bytes']==int(fields['Size']) for f in row['files'])
    report['samples'].append(row);save();print(json.dumps(row),flush=True)
report['terminal']=True;save()
