"""Run the original native Editor task plus source-reviewed repeated-close work."""
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser()
    for name in ('original','server','app','font','mono','output','sdk_library','sdk_receipt','sdk_source_binding'):
        p.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    p.add_argument('--ime',action='store_true',help='Enable the private native v3 producer and test the document IME variant')
    p.add_argument('--accessible-client',type=Path,help='Independent OS reader for the optional native document owner')
    args=p.parse_args();here=Path(__file__).resolve().parent;original=args.original.resolve(strict=True)
    subprocess.run([sys.executable,str(here/'../35_aetheri_files_consumer/check_source.py'),str(original),str(here/'source-inputs.json')],check=True)
    receipt=json.loads(args.sdk_receipt.read_text());binding=json.loads(args.sdk_source_binding.read_text())
    if receipt['sdk_version'] not in ('0.1.27','0.1.28','0.1.29') or not re.fullmatch('[0-9a-f]{40}',binding['source_revision']):raise SystemExit('Wrong SDK receipt/source binding')
    library=args.sdk_library.resolve(strict=True)
    if not any((x.get('path')==str(library) or (x.get('name') and str(library).endswith('/'+x['name']))) and x['sha256']==digest(library) for x in receipt['artifact_files']):raise SystemExit('Changed or unrecorded installed SDK library')
    if library.name not in ('libnexaui.a','libnexaui.so.0.1.27','libnexaui.so.0.1.28','libnexaui.so.0.1.29'):raise SystemExit('Wrong installed SDK library kind')
    source=(original/'tools/test_editor_graphical.py').read_text();changes=[]
    def replace(before,after):
        nonlocal source
        if source.count(before)!=1:raise SystemExit('Changed original native driver boundary: '+repr(before))
        source=source.replace(before,after);changes.append(dict(before=before,after=after))
    replace('''        loaded = {Path(line.split()[-1]).resolve() for line in maps.splitlines() if '/libnexaui.so' in line}; assert len(loaded) == 1
        library = loaded.pop(); metadata = json.loads((library.parents[2]/'build.json').read_text())
        pin = json.loads((ROOT/'spec/nexaui-inputs.json').read_text())
        assert metadata['revision'] == pin['revision'] and metadata['version'] == pin['version']
        assert digest(library) == metadata['artifacts'][library.relative_to(library.parents[2]).as_posix()]''',
'''        loaded = {Path(line.split()[-1]).resolve() for line in maps.splitlines() if '/libnexaui.so' in line}
        library = _candidate_library; metadata = _candidate_metadata
        assert loaded == ({library} if library.name.startswith('libnexaui.so.') else set())
        assert digest(library) == metadata['sha256']''')
    before='''        before = log().count('FRAME '); stat = Path(f'/proc/{app.pid}/stat')
        def ticks():
            fields = stat.read_text().rsplit(')',1)[1].split(); return int(fields[11])+int(fields[12])
        initial = ticks(); time.sleep(1.2); delta = ticks()-initial
        assert delta <= 2 and log().count('FRAME ') == before, (delta, before, log().count('FRAME '))
        key(16, ctrl=True); assert app.wait(timeout=8) == 0; children.remove(app)
'''
    flow=here/'native_flow.py.inc';native_flow=flow.read_text();ime_flow=here/'native_ime_flow.py.inc'
    accessible_flow=here/'native_accessible_flow.py.inc'
    if args.accessible_client:
        args.accessible_client=args.accessible_client.resolve(strict=True)
        if receipt['sdk_version'] not in ('0.1.28','0.1.29'):raise SystemExit('Native document accessibility requires a checked0.1.28/0.1.29 SDK')
        replace("'DBUS_SESSION_BUS_ADDRESS': 'unix:path=' + str(root / 'absent-bus')", "'DBUS_SESSION_BUS_ADDRESS': os.environ['NEXAUI_EDITOR_TEST_ATSPI_ADDRESS']")
        replace("    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS'):\n        env.pop(name, None)",
            "    for name in ('DISPLAY', 'WAYLAND_SOCKET'):\n        env.pop(name, None)\n    env['AT_SPI_BUS_ADDRESS']=os.environ['NEXAUI_EDITOR_TEST_ATSPI_ADDRESS']")
        replace("            capture(name+' selected document')", "            capture(name+' selected document')\n            ax_style(name)")
        replace("remaining='native editable accessibility, external IME, asynchronous I/O, normal app/session admission'",
            "remaining='broader native document interfaces, delayed clipboard/IME, external IME, human/physical/performance and normal app/session admission'")
        native_flow=accessible_flow.read_text()+native_flow
    if args.ime:
        replace("        wait(lambda: 'READY ' in log('server'), 'private compositor')", "        wait(lambda: 'READY ' in log('server'), 'private compositor')\n        event('I on')")
        replace("            capture(name+' selected document')", "            capture(name+' selected document')\n            ime_style(name)")
        native_flow=ime_flow.read_text()+native_flow
    replace(before,native_flow)
    args.output.mkdir(parents=True,exist_ok=True)
    adapted=args.output/'candidate-editor-native.py';adapted.write_text(source)
    (args.output/'driver-adaptation.json').write_text(json.dumps(dict(original_driver_sha256=digest(original/'tools/test_editor_graphical.py'),adapted_sha256=digest(adapted),flow_sha256=digest(flow),ime=args.ime,ime_flow_sha256=digest(ime_flow) if args.ime else None,accessible_flow_sha256=digest(accessible_flow) if args.accessible_client else None,accessible_client_sha256=digest(args.accessible_client) if args.accessible_client else None,changes=changes,sdk_library=str(library),sdk_library_sha256=digest(library),sdk_receipt_sha256=digest(args.sdk_receipt),sdk_source_binding_sha256=digest(args.sdk_source_binding),source_revision=binding['source_revision'],scope='Original native workflows/assertions/deadlines retained; original empty-view idle/keyboard quit replaced by stronger retained dirty-document repeated-native-close task. Optional IME and independent document-reader tasks compose with original ownership/history/save and private native input. SDK declaration is externally bound.'),indent=2)+'\n')
    sys.argv=[str(original/'tools/test_editor_graphical.py')]+[str(getattr(args,k).resolve(strict=True)) for k in ('server','app','font','mono','output')]
    namespace=dict(__name__='__main__',__file__=str(original/'tools/test_editor_graphical.py'),_candidate_library=library,_candidate_accessible_client=args.accessible_client,_candidate_accessible_ime=args.ime,_candidate_metadata=dict(version=receipt['sdk_version'],revision=binding['source_revision'],sha256=digest(library)))
    exec(compile(source,str(adapted),'exec'),namespace)
if __name__=='__main__':main()
