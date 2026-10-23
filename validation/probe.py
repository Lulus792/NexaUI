from pathlib import Path
import subprocess,json,urllib.request,time,hashlib,sys,platform
w=Path(sys.argv[1]);w.mkdir(parents=True,exist_ok=True)
report={'scope':'Read-only bounded byte-identical package transfer comparison; no SDK tests, package installation, mirror configuration, deadline or check changes','platform':platform.platform(),'samples':[],'metadata':[]}
def save():(w/'result.json').write_text(json.dumps(report,indent=2)+'\n')
for package in ['fonts-urw-base35','mesa-vulkan-drivers']:
    text=subprocess.check_output(['apt-cache','show','--no-all-versions',package],text=True)
    fields=dict(line.split(': ',1) for line in text.split('\n\n')[0].splitlines() if ': ' in line and not line.startswith(' '))
    filename=fields['Filename'];assert filename.startswith('pool/') and '..' not in filename.split('/')
    report['metadata'].append({k:fields[k] for k in ['Package','Version','Filename','Size','SHA256']});save()
    for round in range(2):
        hosts=['azure.archive.ubuntu.com','archive.ubuntu.com']
        if round:hosts.reverse()
        for host in hosts:
            url='http://'+host+'/ubuntu/'+filename;row={'package':package,'round':round,'url':url,'requested_bytes':1048576};start=time.monotonic()
            try:
                request=urllib.request.Request(url,headers={'Range':'bytes=0-1048575','User-Agent':'NexaUI-bounded-CI-transfer-diagnosis'})
                with urllib.request.urlopen(request,timeout=30) as response:
                    data=bytearray();row.update(http_status=response.status,final_url=response.url,content_range=response.headers.get('Content-Range'),content_length=response.headers.get('Content-Length'))
                    while len(data)<1048576:
                        if time.monotonic()-start>45:raise TimeoutError('bounded 45s sample budget exceeded')
                        block=response.read(min(65536,1048576-len(data)))
                        if not block:break
                        data.extend(block)
                row.update(bytes=len(data),prefix_sha256=hashlib.sha256(data).hexdigest(),complete_prefix=len(data)==1048576)
            except Exception as error:row['error']=str(error)
            row['seconds']=time.monotonic()-start
            if row.get('bytes'):row['mib_per_second']=row['bytes']/1048576/row['seconds']
            report['samples'].append(row);save();print(json.dumps(row),flush=True)
    okay=[s for s in report['samples'] if s['package']==package and s.get('complete_prefix')]
    assert okay and len({s['prefix_sha256'] for s in okay})==1,'Transfer prefixes differ'
report['terminal']=True;save()
