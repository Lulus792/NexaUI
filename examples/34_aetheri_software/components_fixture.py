"""Private metadata only, encoded by the immutable original Aetheri encoder."""
import copy,hashlib,importlib.util,json,os,sys
from pathlib import Path
def seed(source,work):
    spec=importlib.util.spec_from_file_location('original_component_fixture',source/'tools/runtime_contracts.py');original=importlib.util.module_from_spec(spec)
    previous=sys.dont_write_bytecode
    try:sys.dont_write_bytecode=True;spec.loader.exec_module(original)
    finally:sys.dont_write_bytecode=previous
    frozen=json.loads((source/'spec/vectors/runtime-v1.json').read_text())[0];data=bytes.fromhex(frozen['hex'])
    assert original.fixture(frozen['input'])==data and hashlib.sha256(data).hexdigest()==frozen['sha256']
    doc=copy.deepcopy(frozen['input']);doc['component']='aetheri.probe';doc['publisher']='00000000000040008000000000000009';doc['version']='Ä'*64
    last='/'.join(('z'*64,'p'*64,'q'*64,'r'*61))
    doc['files']=[dict(path=f'entry-{i:02d}',mode=0o555 if i==0 else 0o444,size=0,sha256=hashlib.sha256(b'').hexdigest()) for i in range(31)]+[dict(path=last,mode=0o444,size=0,sha256=hashlib.sha256(b'').hexdigest())]
    doc['actions']=[dict(id=f'run-{i}',executable='entry-00',arguments=['Ω'*128 for _ in range(8)]) for i in range(8)]
    dependencies=sorted(hashlib.sha256(f'declared-dependency-{i}'.encode()).hexdigest() for i in range(8));doc['dependencies']=dependencies
    records={'maximum':original.fixture(doc),'malformed':b'not a canonical manifest'}
    empty=copy.deepcopy(doc);empty['dependencies']=[];records['empty']=original.fixture(empty)
    component=copy.deepcopy(doc);component['component']='other.component';records['component']=original.fixture(component)
    publisher=copy.deepcopy(doc);publisher['publisher']='00000000000040008000000000000008';records['publisher']=original.fixture(publisher)
    for name,payload in records.items():(work/(name+'.bin')).write_bytes(payload);(work/(name+'.sha256')).write_text(hashlib.sha256(payload).hexdigest())
    (work/'dependencies.txt').write_text('\n'.join(dependencies)+'\n');(work/'maximum-path.txt').write_text(last)
    os.symlink('maximum.bin',work/'link');os.mkfifo(work/'fifo',0o600)
    (work/'inputs.json').write_text(json.dumps({'artifacts':{name:hashlib.sha256(value).hexdigest() for name,value in records.items()},'dependencies':dependencies,'declared_files':32,'declared_actions':8,'declared_dependencies':8,'scope':'Synthetic declarations only; no installed payload, dependency/process state or permission.'},indent=2)+'\n')
    return work/'maximum.bin'
