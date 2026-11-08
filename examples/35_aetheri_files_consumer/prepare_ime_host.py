"""Apply narrowly recorded input integration to a checked exported Files host.

The original export is never written. These generated files are a distinct
candidate, not unchanged-original or normal Aetheri adoption evidence.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: prepare_ime_host.py ORIGINAL_EXPORT OWNED_BUILD_OUTPUT")
    original = Path(sys.argv[1]).resolve(strict=True)
    output = Path(sys.argv[2]).resolve()
    here = Path(__file__).resolve().parent
    subprocess.run([sys.executable, str(here / "check_source.py"), str(original),
                    str(here / "native-inputs.json")], check=True)
    if output == original or original in output.parents or output in original.parents:
        raise SystemExit("IME build output must be separate from the original export")
    output.mkdir(parents=True, exist_ok=True)
    records = []

    def transform(name, changes):
        data = (original / "src/files" / name).read_bytes().replace(b"\r\n", b"\n")
        text = data.decode()
        for before, after in changes:
            if text.count(before) != 1:
                raise SystemExit("Original input does not match reviewed IME change: "
                                 + name + " count=" + str(text.count(before)) + " " + repr(before))
            text = text.replace(before, after)
        generated = text.encode()
        (output / name).write_bytes(generated)
        records.append({"original": "src/files/" + name,
                        "original_sha256": hashlib.sha256(data).hexdigest(),
                        "generated": name, "sha256": hashlib.sha256(generated).hexdigest(),
                        "changes": [{"before": a, "after": b} for a, b in changes]})

    transform("nexaui_view.h", [(
        "nx_context *ae_files_view_context(struct ae_files_view *);",
        "nx_context *ae_files_view_context(struct ae_files_view *);\n"
        "/* Bind once before the first frame. The native window outlives the view. */\n"
        "bool ae_files_view_set_ime_window(struct ae_files_view *,nx_window *);")])
    transform("nexaui_view.c", [(
        "struct edit edits[2];bool unfocused,rebuilding;char filter[AE_FILES_QUERY+1];",
        "struct edit edits[2];bool unfocused,rebuilding;char filter[AE_FILES_QUERY+1];\n"
        """    nx_window *ime_window;bool ime_cancel,ime_enter_down,ime_escape_down;
    nx_event native_begin,native_events[AE_FILES_NAME+8];size_t native_count,native_depth;
    bool native_bad,native_ignore,native_reject_pending;uint64_t native_scope,native_reject_scope;
    nx_text_buffer *native_preview;uint64_t native_preview_target,native_preview_revision,native_preview_scope;"""), (
        "nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }",
        "nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }\n"
        "bool ae_files_view_set_ime_window(struct ae_files_view *v,nx_window *window) {\n"
        "    if(!v || !window || v->ime_window || v->width || v->height) return false;\n"
        "    if(nx_window_set_ime_transaction_events(window,1)!=NX_OK) return false;\n"
        "    v->ime_window=window;return true;\n}"
    ), (
        "nx_text_field_create(value,n,0,&e->field)",
        "nx_text_field_create(value,n,32768,&e->field)"
    ), (
        "nx_context_destroy(v->ui);\n    for(unsigned i=0;i<2;++i)",
        "nx_context_destroy(v->ui);nx_text_buffer_destroy(v->native_preview);\n    for(unsigned i=0;i<2;++i)"
    ), (
        "static bool apply_fields(struct ae_files_view *,bool *);",
        "static bool apply_fields(struct ae_files_view *,bool *);\nstatic void reject_text(struct ae_files_view *);"
    ), (
        "*changed=false;struct ae_files_session *s=v->s;\n    if(!isfinite(s->scale)",
        "*changed=false;struct ae_files_session *s=v->s;\n"
        "    nx_text_buffer_destroy(v->native_preview);v->native_preview=NULL;\n    if(!isfinite(s->scale)"
    ), (
        "bool edited=false;if(!apply_fields(v,&edited)) return false;",
        """bool edited=false;if(!apply_fields(v,&edited)) return false;
    if(v->native_reject_pending) {
        v->native_reject_pending=false;
        if(v->native_reject_scope==v->base) {reject_text(v);edited=true;*changed=true;}
    }"""
    ), (
        "nx_text_field_set_read_only(e->field,invalid || v->s->composing)",
        "nx_text_field_set_read_only(e->field,invalid || v->s->composing || v->ime_cancel)"
    ), (
        ".validate=field_valid,.validate_user=&validation};",
        ".validate=field_valid,.validate_user=&validation,.ime_window=v->ime_window};"
    ), (
        "if(!draft.length) {",
        """size_t native_preedit=0;int32_t native_begin=0,native_end=0;
    if(v->ime_window && nx_focus_id(v->ui)==id && !disabled && !invalid && !v->ime_cancel && !v->s->composing) {
        nx_result current=nx_window_get_ime_preedit(v->ime_window,NULL,0,&native_preedit,&native_begin,&native_end);
        if(current!=NX_OK && current!=NX_ERROR_UNSUPPORTED) return false;
    }
    if(!draft.length && !native_preedit) {"""
    ), (
        "if(nx_end_frame(v->ui)!=NX_OK) return false;\n    bool edited=false;",
        "if(nx_end_frame(v->ui)!=NX_OK) return false;\n"
        "    if(v->ime_cancel) {v->ime_cancel=false;s->dirty=true;*changed=true;}\n"
        "    bool edited=false;"
    ), (
        "struct ae_files_session *s=v->s;s->dirty=true;\n    if(e->type==NX_EVENT_FOCUS)",
        """struct ae_files_session *s=v->s;s->dirty=true;
    if(v->ime_window) {
        bool handled=false;if(!native_transaction(v,e,&handled)) return false;if(handled) return true;
        if(e->type==NX_EVENT_IME_PREEDIT_CHANGED)
            return nx_context_queue_event(v->ui,e)==NX_OK;
        if(e->type==NX_EVENT_KEY_UP && e->key==NX_KEY_ENTER && v->ime_enter_down) {v->ime_enter_down=false;return true;}
        if(e->type==NX_EVENT_KEY_UP && e->key==NX_KEY_ESCAPE && v->ime_escape_down) {v->ime_escape_down=false;return true;}
        if(e->type==NX_EVENT_KEY_DOWN && ((e->key==NX_KEY_ENTER && v->ime_enter_down) ||
           (e->key==NX_KEY_ESCAPE && v->ime_escape_down))) return true;
        size_t bytes=0;int32_t begin=0,end=0;
        nx_result ime=nx_window_get_ime_preedit(v->ime_window,NULL,0,&bytes,&begin,&end);
        if(ime!=NX_OK && ime!=NX_ERROR_UNSUPPORTED) return false;
        if(e->type==NX_EVENT_TEXT_COMMAND &&
           (e->text_command==NX_TEXT_COMMAND_UNDO || e->text_command==NX_TEXT_COMMAND_REDO))
            return bytes || !editing(v) || nx_context_queue_event(v->ui,e)==NX_OK;
        if(bytes && e->type==NX_EVENT_TEXT && !e->ime_target && !e->ime_revision) return true;
        if(bytes && e->type==NX_EVENT_KEY_DOWN) {
            if(e->key==NX_KEY_ESCAPE) {v->ime_escape_down=true;v->ime_cancel=true;return true;}
            if(e->key==NX_KEY_ENTER) {v->ime_enter_down=true;return true;}
            if(e->key!=NX_KEY_TAB && !((e->modifiers&NX_MOD_CONTROL) && e->key==NX_KEY_Q)) return true;
        }
        if(e->type==NX_EVENT_FOCUS && !e->focused) v->ime_enter_down=v->ime_escape_down=false;
    }
    if(e->type==NX_EVENT_FOCUS)"""
    ), (
        "bool ae_files_view_event(struct ae_files_view *v,const nx_event *e) {",
        (here / "ime_transactions.inc").read_text() + "\nbool ae_files_view_event(struct ae_files_view *v,const nx_event *e) {"
    )])
    transform("nexaui_main.c", [(
        "!ae_files_view_create(&h.s,font,font_size,&h.view) || nx_software_view_open",
        "!ae_files_view_create(&h.s,font,font_size,&h.view) || !ae_files_view_set_ime_window(h.view,h.window) || nx_software_view_open"), (
        "int main(int argc,char **argv) {",
        """/* Defer native payload and neutral key releases until the queue is drained.
   Flush completed edits before an ordinary command observes the app model. */
static bool ime_deferred(const nx_event *e) {
    return e->type==NX_EVENT_IME_TRANSACTION_BEGIN || e->type==NX_EVENT_IME_TRANSACTION_END ||
        e->type==NX_EVENT_IME_PREEDIT_CHANGED || e->type==NX_EVENT_IME_DELETE_RANGE ||
        e->type==NX_EVENT_IME_DELETE_SURROUNDING || e->type==NX_EVENT_KEY_UP ||
        e->type==NX_EVENT_KEY_DOWN_CONSUMED ||
        ((e->type==NX_EVENT_TEXT || e->type==NX_EVENT_TEXT_COMMAND) && (e->ime_target || e->ime_revision));
}
int main(int argc,char **argv) {"""
    ), (
        "if(nx_window_get_status(h.window,&status)!=NX_OK || !chrome(&h,&event,&status,&handled) ||\n"
        "                   (!handled && !ae_files_view_event(h.view,&event)) || !draw(&h))",
        "if((!ime_deferred(&event) && !draw(&h)) || nx_window_get_status(h.window,&status)!=NX_OK ||\n"
        "                   !chrome(&h,&event,&status,&handled) || (!handled && !ae_files_view_event(h.view,&event)) ||\n"
        "                   (!ime_deferred(&event) && !draw(&h)))"
    )])
    (output / "source-delta.json").write_text(json.dumps({"files": records,
        "scope": "Distinct consumer IME integration; original model/controller/host source export unchanged"}, indent=2) + "\n")
    print("GENERATED three explicitly changed IME consumer files from checked original inputs")


if __name__ == "__main__":
    main()
