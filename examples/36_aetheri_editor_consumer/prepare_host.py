"""Adapt native ownership and explicit C17 predicates in a separate original Editor."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

original=Path(sys.argv[1]).resolve(strict=True)
output=Path(sys.argv[2]).resolve()
here=Path(__file__).resolve().parent
subprocess.run([sys.executable,str(here/'../35_aetheri_files_consumer/check_source.py'),str(original),str(here/'source-inputs.json')],check=True)
if original==output or original in output.parents or output in original.parents:
    raise SystemExit('Separate generated output required')
output.mkdir(parents=True,exist_ok=True)
path=original/'src/editor/graphical_main.c'
data=path.read_bytes();text=data.decode();changes=[]
def replace(before,after,expected=1):
    global text
    if text.count(before)!=expected:raise SystemExit('Changed original Editor boundary: '+repr(before))
    text=text.replace(before,after);changes.append(dict(before=before,after=after))
# Preserve the original zero/nonzero decisions while keeping Clang's strict
# floating-to-bool diagnostics enabled for the copied host and paint test.
replace('if(a->review_max_scroll &&', 'if(a->review_max_scroll!=0.0f &&')
replace('(nh?nh:1)', '(nh!=0.0f?nh:1)')
replace('if(side) {', 'if(side!=0.0f) {')
replace('    bool overlay_shown,close_seen;uint64_t clicked_row;double clicked_at;',
        '    bool overlay_shown,focus_paint_pending;uint64_t clicked_row;double clicked_at;\n    nx_window_close_request native_close;bool chrome_release;uint64_t wait_serial;')
# These controls register and paint before their initial modal focus is set.
# Focus changes do not rewrite commands already recorded in this frame. Keep
# one followup dirty, without forcing continuous redraw once focus is painted.
replace('static bool paint_review(struct app *a) {', '''static nx_result editor_focus_after_paint(struct app *a,uint64_t id) {
    uint64_t previous=nx_focus_id(a->context);
    nx_result result=nx_focus_set(a->context,id);
    if(result==NX_OK && previous!=nx_focus_id(a->context)) a->focus_paint_pending=true;
    return result;
}
static bool paint_review(struct app *a) {''')
replace('nx_focus_set(a->context,a->modal_id+5)',
        'editor_focus_after_paint(a,a->modal_id+5)',expected=2)
replace('nx_focus_set(a->context,a->field_id)',
        'editor_focus_after_paint(a,a->field_id)')
replace('static bool frame(struct app *a,bool *busy) {',
        'static bool frame(struct app *a,bool *busy) {\n    a->focus_paint_pending=false;')
replace('    *busy=outcome==NX_SOFTWARE_VIEW_BUSY;a->dirty=*busy;',
        '    *busy=outcome==NX_SOFTWARE_VIEW_BUSY;a->dirty=*busy || a->focus_paint_pending;')
replace('closing=%u pending=%llu', 'closing=%u focus_pending=%u pending=%llu')
replace('a->close_all?1u:0u,(unsigned long long)a->pending_close',
        'a->close_all?1u:0u,a->focus_paint_pending?1u:0u,(unsigned long long)a->pending_close')
replace('static volatile sig_atomic_t stopping,signal_fd=-1;', '#include "window_input.h"\nstatic volatile sig_atomic_t stopping,signal_fd=-1;')
replace('''        nx_window_info info;if(nx_window_get_info(a.window,&info)!=NX_OK) goto done;''',
'''        nx_window_close_request native={0};
        if(nx_window_take_close_request(a.window,&native)!=NX_OK) goto done;
        if(native.id) {
            a.native_close=native;
            if(a.diagnostics) {printf("NATIVE_CLOSE taken=%llu\\n",(unsigned long long)native.id);fflush(stdout);}
            if(!a.close_all && !apply(&a,CLOSE_WINDOW,0)) goto done;
            a.dirty=true;
        }
        if(a.native_close.id && !a.close_all) {
            if(nx_window_resolve_close(a.window,&a.native_close,NX_WINDOW_CLOSE_CANCEL)!=NX_OK) goto done;
            if(a.diagnostics) {printf("NATIVE_CLOSE cancelled=%llu\\n",(unsigned long long)a.native_close.id);fflush(stdout);}
            a.native_close=(nx_window_close_request){0};
        }
        nx_window_info info;if(nx_window_get_info(a.window,&info)!=NX_OK || info.should_close) goto done;''')
replace('''        /* The pinned SDK exposes a sticky observation, not a consumable close
         * request. Never interpret it again after the user cancels review. */
        if(info.should_close && !a.close_seen) {a.close_seen=true;if(!apply(&a,CLOSE_WINDOW,0)) goto done;a.dirty=true;}
''','')
replace('''            bool handled=false;if(!event(&a,&incoming,&handled)) goto done;''',
'''            bool handled=false;if(!editor_window_event(&a,&incoming,&handled)) goto done;
            if(!handled && !event(&a,&incoming,&handled)) goto done;''')
replace('''        if(available) {
''','''        if(available) {
            if(a.diagnostics && incoming.type==NX_EVENT_KEY_UP && incoming.key==NX_KEY_ESCAPE) {puts("EDITOR_ESCAPE_RELEASE");fflush(stdout);}
''')
replace('''        if(nx_window_poll_with_fd(a.window,busy?16:UINT32_MAX,fds[0],&signals)!=NX_OK) goto done;''',
'''        if(a.diagnostics) {if(a.wait_serial==UINT64_MAX) goto done;printf("EDITOR_WAIT begin=%llu timeout=%u\\n",(unsigned long long)++a.wait_serial,busy?16u:UINT32_MAX);fflush(stdout);}
        if(nx_window_poll_with_fd(a.window,busy?16:UINT32_MAX,fds[0],&signals)!=NX_OK) goto done;
        if(a.diagnostics) {printf("EDITOR_WAIT end=%llu\\n",(unsigned long long)a.wait_serial);fflush(stdout);}''')
replace('''    result=0;
done:''','''    if(a.native_close.id) {
        if(nx_window_resolve_close(a.window,&a.native_close,NX_WINDOW_CLOSE_ACCEPT)!=NX_OK) goto done;
        if(a.diagnostics) {printf("NATIVE_CLOSE accepted=%llu\\n",(unsigned long long)a.native_close.id);fflush(stdout);}
        a.native_close=(nx_window_close_request){0};
    }
    result=0;
done:''')
generated=text.encode();(output/'editor.c').write_bytes(generated)
(output/'source-delta.json').write_text(json.dumps(dict(original=str(path),original_sha256=hashlib.sha256(data).hexdigest(),generated_sha256=hashlib.sha256(generated).hexdigest(),changes=changes,scope='Separate installed consumer: original document/workspace/dialog/save/input/rendering owners retained. Native close consumption, caption/edge gestures, equivalent explicit floating predicates and one followup frame after late modal focus are the adapter delta.'),indent=2)+'\n')
