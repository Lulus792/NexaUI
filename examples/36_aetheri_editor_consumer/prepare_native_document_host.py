"""Connect the new public SDK seam to a separately generated original owner."""
import hashlib,json,sys
from pathlib import Path
source=Path(sys.argv[1]).resolve(strict=True);output=Path(sys.argv[2]).resolve()
if source==output or source in output.parents or output in source.parents:
    raise SystemExit('Separate native document host output required')
data=source.read_bytes();text=data.decode();changes=[]
def replace(before,after):
    global text
    if text.count(before)!=1:raise SystemExit('Changed original native document boundary: '+repr(before))
    text=text.replace(before,after);changes.append(dict(before=before,after=after))
replace('struct app {','#include "native_document.h"\nstruct app {\n    struct editor_native_document native_document;')
replace('static void close_picker(struct app *a) {','#include "native_document.inc"\nstatic void close_picker(struct app *a) {')
a=text.index('static bool register_document(struct app *a,struct ids *ids) {')
b=text.index('\nstatic bool paint_document(struct app *a) {',a)
replace(text[a:b],'static bool register_document(struct app *a,struct ids *ids) {\n    return editor_native_document_register(a,ids);\n}')
replace('static bool frame(struct app *a,bool *busy) {','static bool frame(struct app *a,bool *busy) {\n    a->native_document.changed=false;')
replace('    *busy=outcome==NX_SOFTWARE_VIEW_BUSY;a->dirty=*busy;',
    '    *busy=outcome==NX_SOFTWARE_VIEW_BUSY;a->dirty=*busy || a->native_document.changed;')
replace('''    if(outcome==NX_SOFTWARE_VIEW_READY) {
        nx_result result=nx_window_publish_semantics(a->window,a->context);if(result!=NX_OK && result!=NX_ERROR_UNSUPPORTED) return false;''',
'''    /* Native replies and queries follow the completed model projection even
       while the software presenter still owns an in-flight pixel buffer. */
    nx_result published=nx_window_publish_semantics(a->window,a->context);
    if(published!=NX_OK && published!=NX_ERROR_UNSUPPORTED) return false;
    if(outcome==NX_SOFTWARE_VIEW_READY) {''')
replace('            if(a.dirty && !busy) continue;',
    '            /* Drain original/native requests before the header followup frame. */')
replace('''        nx_event incoming;int available=0;
        if(nx_window_next_event(a.window,&incoming,&available)!=NX_OK) goto done;
''','')
replace('        if(a.dirty && info.width>=64 && info.height>=64) {',
'''        nx_event incoming;int available=0;
        if(nx_window_next_event(a.window,&incoming,&available)!=NX_OK) goto done;
        /* A presenter retry must not retire the frame of a native request
           received during the preceding wait. Keep the next original event
           locally, and route only after the queue was actually empty. */
        if(!available) {
            size_t native_requests=0;
            if(nx_window_route_events(a.window,a.context,&native_requests)!=NX_OK) goto done;
            if(native_requests) a.dirty=true;
        }
        if(a.dirty && info.width>=64 && info.height>=64) {''')
replace('        if(a.save.work && ae_editor_save_work_done(a.save.work)) {',
'''        /* next_event just proved the owner-thread native event queue empty.
           Route assistive requests here, preserving each original raw-event
           frame and its document/focus provenance. No intervening native poll. */
        size_t native_requests=0;
        if(nx_window_route_events(a.window,a.context,&native_requests)!=NX_OK) goto done;
        if(native_requests) a.dirty=true;
        if(a.dirty && !busy) continue;
        if(a.save.work && ae_editor_save_work_done(a.save.work)) {''')
output.mkdir(parents=True,exist_ok=True);generated=text.encode();(output/'editor.c').write_bytes(generated)
(output/'native-document-delta.json').write_text(json.dumps(dict(input=str(source),input_sha256=hashlib.sha256(data).hexdigest(),generated_sha256=hashlib.sha256(generated).hexdigest(),changes=changes,scope='Optional installed SDK0.1.28 document bridge; original model/history/save/render state retained, ephemeral atomic callbacks and bounded preflight UI capacity. One followup frame refreshes headers built before an accepted native edit. Native IME remains independently selectable.'),indent=2)+'\n')
