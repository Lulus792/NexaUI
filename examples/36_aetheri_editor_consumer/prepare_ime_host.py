"""Connect native IME to the separately generated original Editor owner.

Input is prepare_host.py's checked generated native-close owner, not Aetheri's
active checkout. Every additional native-IME substitution is recorded.
"""
import hashlib,json,sys
from pathlib import Path
source=Path(sys.argv[1]).resolve(strict=True);output=Path(sys.argv[2]).resolve()
if output==source or source in output.parents:raise SystemExit('Separate generated IME host output required')
data=source.read_bytes();text=data.decode();changes=[]
def replace(before,after):
    global text
    if text.count(before)!=1:raise SystemExit('Changed generated Editor IME boundary: '+repr(before))
    text=text.replace(before,after);changes.append(dict(before=before,after=after))
replace('struct app {','#include "ime_host.h"\nstruct app {\n    struct editor_ime_host ime;')
replace('static void cancel_input(struct app *a) {','static void cancel_input(struct app *a) {\n    editor_ime_cancel(a);')
replace('    if(!ae_editor_workspace_review(a->workspace,&a->review)) a->review=(struct ae_editor_close_token){0};',
        '    if(!ae_editor_workspace_review(a->workspace,&a->review)) a->review=(struct ae_editor_close_token){0};\n    if(!editor_ime_frame(a)) return false;')
replace('                    if(nx_fill_rect(a->context,(nx_rect){origin.x+(float)caret.x,origin.y+(float)caret.top,1.5f,(float)caret.height},foreground(a))!=NX_OK) return false;',
        '                    if(!editor_ime_paint(a,row,origin,&caret)) return false;')
replace('static bool pointer_position(struct app *a,float x,float y,bool extend) {',
        '#include "ime_host.inc"\nstatic bool pointer_position(struct app *a,float x,float y,bool extend) {')
replace('    bool busy=false;\n    while(a.running) {','    if(!editor_ime_initialize(&a)) goto done;\n    bool busy=false;\n    while(a.running) {')
replace('            bool handled=false;if(!editor_window_event(&a,&incoming,&handled)) goto done;',
        '            bool handled=false;if(!editor_ime_event(&a,&incoming,&handled)) goto done;\n            if(!handled && !editor_window_event(&a,&incoming,&handled)) goto done;')
replace('        uint32_t signals=0;','        if(!editor_ime_idle(&a)) goto done;\n        uint32_t signals=0;')
replace('done:\n    cancel_paste(&a);','done:\n    editor_ime_cancel(&a);if(a.ime.failed) result=1;\n    cancel_paste(&a);')
replace('pending=%llu dirty=%u uncertain=%u\\n",','pending=%llu dirty=%u uncertain=%u ime=%zu ime_hidden=%u ime_epoch=%llu\\n",')
replace('uncertain?1u:0u);fflush(stdout);','uncertain?1u:0u,a->ime.length,a->ime.length && a->ime.cursor<0?1u:0u,(unsigned long long)a->ime.batch.revision);fflush(stdout);')
output.mkdir(parents=True,exist_ok=True);generated=text.encode();(output/'editor.c').write_bytes(generated)
(output/'ime-host-delta.json').write_text(json.dumps(dict(input=str(source),input_sha256=hashlib.sha256(data).hexdigest(),generated_sha256=hashlib.sha256(generated).hexdigest(),changes=changes,scope='Optional original Editor native IME consumer. Original document/save/rendering owners retained; complete marked batches, transient preedit, bounded surrounding text and logical candidate geometry use installed SDK APIs.'),indent=2)+'\n')
