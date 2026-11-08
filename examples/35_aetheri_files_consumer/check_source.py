"""Check exported original Files inputs; this grants no runtime/file authority."""
import hashlib,json,sys
from pathlib import Path

def main():
    if len(sys.argv) not in (2,3):raise SystemExit('usage: check_source.py EXPORTED_AETHERI_SOURCE [INPUT_MANIFEST]')
    root=Path(sys.argv[1]).resolve(strict=True)
    if (root/'.git').exists():raise SystemExit('Use an exported snapshot, not the independently developed Aetheri checkout')
    manifest_path=Path(sys.argv[2]) if len(sys.argv)==3 else Path(__file__).with_name('source-inputs.json')
    manifest=json.loads(manifest_path.read_text())
    for item in manifest['files']:
        path=root/item['path']
        if path.is_symlink() or not path.is_file() or root not in path.resolve().parents:
            raise SystemExit('Missing or indirect original input: '+item['path'])
        data=path.read_bytes().replace(b'\r\n',b'\n')
        if len(data)!=item['size'] or hashlib.sha256(data).hexdigest()!=item['sha256']:
            raise SystemExit('Changed original input: '+item['path'])
    print('CHECKED original Aetheri '+manifest['aetheri_revision']+'; '+str(len(manifest['files']))+' exact inputs')

if __name__=='__main__':main()
