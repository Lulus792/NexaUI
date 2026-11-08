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
                raise SystemExit("Original input does not match reviewed IME change: " + name)
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
        "    nx_window *ime_window;bool ime_cancel,ime_enter_down,ime_escape_down;uint64_t ime_commit_target,ime_commit_revision;"), (
        "nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }",
        "nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }\n"
        "bool ae_files_view_set_ime_window(struct ae_files_view *v,nx_window *window) {\n"
        "    if(!v || !window || v->ime_window || v->width || v->height) return false;\n"
        "    v->ime_window=window;return true;\n}"
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
        if(e->type==NX_EVENT_IME_PREEDIT_CHANGED)
            return nx_context_queue_event(v->ui,e)==NX_OK;
        if(e->type==NX_EVENT_KEY_UP && e->key==NX_KEY_ENTER && v->ime_enter_down) {v->ime_enter_down=false;return true;}
        if(e->type==NX_EVENT_KEY_UP && e->key==NX_KEY_ESCAPE && v->ime_escape_down) {v->ime_escape_down=false;return true;}
        if(e->type==NX_EVENT_KEY_DOWN && ((e->key==NX_KEY_ENTER && v->ime_enter_down) ||
           (e->key==NX_KEY_ESCAPE && v->ime_escape_down))) return true;
        size_t bytes=0;int32_t begin=0,end=0;
        nx_result ime=nx_window_get_ime_preedit(v->ime_window,NULL,0,&bytes,&begin,&end);
        if(ime!=NX_OK && ime!=NX_ERROR_UNSUPPORTED) return false;
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
        "if(e->ime_target || e->ime_revision) { reject_text(v);v->commit_active=false;return true; }",
        """if(e->ime_target || e->ime_revision) {
            if(!v->ime_window) { reject_text(v);v->commit_active=false;return true; }
            size_t length=0;int32_t begin=0,end=0;
            nx_result current=nx_window_get_ime_preedit_for(v->ime_window,e->ime_target,e->ime_revision,NULL,0,&length,&begin,&end);
            if(current==NX_ERROR_STATE || current==NX_ERROR_UNSUPPORTED || !editing(v) || nx_focus_id(v->ui)!=e->ime_target) {
                v->commit_active=false;return true;
            }
            if(current!=NX_OK) return false;
        }"""
    ), (
        "v->commit_bad=false;v->commit_scope=v->base;",
        "v->commit_bad=false;v->commit_scope=v->base;v->ime_commit_target=e->ime_target;v->ime_commit_revision=e->ime_revision;"
    ), (
        "if(!n || n>sizeof(v->commit)-1-v->commit_size || v->commit_scope!=v->base)",
        "if(!n || n>sizeof(v->commit)-1-v->commit_size || v->commit_scope!=v->base || v->ime_commit_target!=e->ime_target || v->ime_commit_revision!=e->ime_revision)"
    ), (
        "else append(v,v->commit,v->commit_size);v->commit_active=false;",
        "else if(v->ime_commit_target) append_native(v,v->commit,v->commit_size,v->ime_commit_target,v->ime_commit_revision);else append(v,v->commit,v->commit_size);v->commit_active=false;"
    ), (
        "static bool edit_key(struct ae_files_view *v,const nx_event *event,bool *handled)",
        """/* Publish a complete, fresh native batch in one widget frame. Partial native
   scalars must not update the original model/scope before END. */
static void append_native(struct ae_files_view *v,const char *bytes,size_t length,uint64_t target,uint64_t revision) {
    if(!editing(v) || (v->s->dialog?v->s->name_bad:v->s->query_bad)) return;
    size_t at=0;
    while(at<length) {
        uint32_t scalar;if(!ae_utf8_next(bytes,length,&at,&scalar)) {reject_text(v);return;}
        nx_event event={.type=NX_EVENT_TEXT,.scalar=scalar,.ime_target=target,.ime_revision=revision};
        if(nx_context_queue_event(v->ui,&event)!=NX_OK) {reject_text(v);return;}
    }
}
static bool edit_key(struct ae_files_view *v,const nx_event *event,bool *handled)"""
    )])
    transform("nexaui_main.c", [(
        "!ae_files_view_create(&h.s,font,font_size,&h.view) || nx_software_view_open",
        "!ae_files_view_create(&h.s,font,font_size,&h.view) || !ae_files_view_set_ime_window(h.view,h.window) || nx_software_view_open")])
    (output / "source-delta.json").write_text(json.dumps({"files": records,
        "scope": "Distinct consumer IME integration; original model/controller/host source export unchanged"}, indent=2) + "\n")
    print("GENERATED three explicitly changed IME consumer files from checked original inputs")


if __name__ == "__main__":
    main()
