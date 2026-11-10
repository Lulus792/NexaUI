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
        "bool ae_files_view_set_ime_window(struct ae_files_view *,nx_window *);\n"
        "/* Before the first frame; borrowed font bytes outlive the view. At most four, 4 MiB each. */\n"
        "bool ae_files_view_add_fallback(struct ae_files_view *,const void *,size_t);")])
    transform("nexaui_view.c", [(
        "struct edit { nx_text_field *field;",
        "struct paste_rule { struct ae_files_view *view;bool name; };\nstruct edit { nx_text_field *field;"
    ), (
        "struct edit edits[2];bool unfocused,rebuilding;char filter[AE_FILES_QUERY+1];",
        "struct edit edits[2];bool unfocused,rebuilding;char filter[AE_FILES_QUERY+1];struct paste_rule paste_rules[2];\n"
        """    nx_font *fallback_fonts[4];size_t fallback_count;
    nx_window *ime_window;nx_text_clipboard clipboard;bool ime_cancel,ime_enter_down,ime_escape_down;
    nx_event native_begin,native_events[AE_FILES_NAME+8];size_t native_count,native_depth;
    bool native_bad,native_ignore,native_reject_pending;uint64_t native_scope,native_reject_scope;
    nx_text_buffer *native_preview;uint64_t native_preview_target,native_preview_revision,native_preview_scope;"""), (
        "nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }",
        "nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }\n"
        "bool ae_files_view_set_ime_window(struct ae_files_view *v,nx_window *window) {\n"
        "    if(!v || !window || v->ime_window || v->width!=0.0f || v->height!=0.0f) return false;\n"
        "    nx_result ime=nx_window_set_ime_transaction_events(window,1);\n"
        "    if(ime!=NX_OK && ime!=NX_ERROR_UNSUPPORTED) return false;\n"
        "    if(nx_text_clipboard_for_window(window,&v->clipboard)!=NX_OK) return false;\n"
        "    v->ime_window=window;return true;\n}"
    ), (
        "nx_text_field_create(value,n,0,&e->field)",
        "nx_text_field_create(value,n,32768,&e->field)"
    ), (
        "nx_context_destroy(v->ui);\n    for(unsigned i=0;i<2;++i)",
        "nx_context_destroy(v->ui);nx_text_buffer_destroy(v->native_preview);\n    for(unsigned i=0;i<2;++i)"
    ), (
        "nx_font_collection_destroy(v->fonts);nx_font_close(v->font);free(v);",
        "nx_font_collection_destroy(v->fonts);\n"
        "    for(size_t i=0;i<v->fallback_count;++i) nx_font_close(v->fallback_fonts[i]);\n"
        "    nx_font_close(v->font);free(v);"
    ), (
        "static bool naming(struct ae_files_view *v,nx_rect r,bool review,struct intent *intent) {",
        (here / "appearance.inc").read_text() + "\nstatic bool naming(struct ae_files_view *v,nx_rect r,bool review,struct intent *intent) {"
    ), (
        """        v->cancel=(nx_rect){r.x+r.width-72*z,r.y+6,32*z,r.height-12};
        v->confirm=(nx_rect){r.x+r.width-36*z,r.y+6,32*z,r.height-12};
        v->name=(nx_rect){r.x+6,r.y+6,r.width-84*z,r.height-12};""",
        """        float cancel_width=0,confirm_width=0,gap=4*z;
        if(!naming_button_width(v,tr(v,"Cancel","Abbrechen"),&cancel_width) ||
           !naming_button_width(v,"OK",&confirm_width)) return false;
        v->confirm=(nx_rect){r.x+r.width-6-confirm_width,r.y+6,confirm_width,r.height-12};
        v->cancel=(nx_rect){v->confirm.x-gap-cancel_width,r.y+6,cancel_width,r.height-12};
        v->name=(nx_rect){r.x+6,r.y+6,v->cancel.x-gap-r.x-6,r.height-12};
        if(v->name.width<1) return false;"""
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
        "static bool field(struct ae_files_view *v,enum ae_files_control c,",
        (here / "paste_admission.inc").read_text() + "\nstatic bool field(struct ae_files_view *v,enum ae_files_control c,"
    ), (
        "struct edit *e=&v->edits[c==AE_FV_NAME?1:0];if(!edit_sync(e,value)) return false;",
        "struct edit *e=&v->edits[c==AE_FV_NAME?1:0];if(!edit_sync(e,value)) return false;\n"
        "    unsigned which=c==AE_FV_NAME?1u:0u;v->paste_rules[which]=(struct paste_rule){v,which!=0};\n"
        "    if(nx_text_field_set_paste_validator(e->field,paste_valid,&v->paste_rules[which])!=NX_OK) return false;"
    ), (
        ".validate=field_valid,.validate_user=&validation};",
        ".validate=field_valid,.validate_user=&validation,.ime_window=v->ime_window,.clipboard=v->clipboard};"
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
           (e->text_command==NX_TEXT_COMMAND_UNDO || e->text_command==NX_TEXT_COMMAND_REDO ||
            e->text_command==NX_TEXT_COMMAND_COPY || e->text_command==NX_TEXT_COMMAND_CUT || e->text_command==NX_TEXT_COMMAND_PASTE))
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
        "static unsigned char *load_font(const char *path,size_t *length) {",
        "static unsigned char *load_font_limit(const char *path,size_t *length,size_t limit) {"
    ), (
        "(size=ftell(f))>0 && size<64*1024*1024",
        "(size=ftell(f))>0 && (uint64_t)size<=(uint64_t)limit"
    ), (
        "static bool accepted(nx_result r)",
        "static unsigned char *load_font(const char *path,size_t *length) { return load_font_limit(path,length,64u*1024u*1024u-1u); }\nstatic bool accepted(nx_result r)"
    ), (
        "const char *root=NULL,*font_path=NULL;",
        "const char *root=NULL,*font_path=NULL,*fallback_paths[4]={0};size_t fallback_count=0,fallback_sizes[4]={0};\n"
        "    unsigned char *fallback_bytes[4]={0};"
    ), (
        "for(int i=1;i<argc;++i) {",
        "for(int i=1;i<argc;++i) {\n"
        "        if(!strcmp(argv[i],\"--fallback-font\")) {\n"
        "            if(fallback_count==4 || i+1==argc) return 2;\n"
        "            fallback_paths[fallback_count++]=argv[++i];continue;\n        }"
    ), (
        "size_t font_size=0;unsigned char *font=load_font(font_path,&font_size);",
        "size_t font_size=0;unsigned char *font=load_font(font_path,&font_size);bool fallback_ok=true;\n"
        "    for(size_t i=0;i<fallback_count;++i) {\n"
        "        fallback_bytes[i]=load_font_limit(fallback_paths[i],&fallback_sizes[i],4u*1024u*1024u);\n"
        "        if(!fallback_bytes[i]) {fputs(\"files: fallback font could not be loaded within its byte limit\\n\",stderr);fallback_ok=false;}\n    }"
    ), (
        "if(!font || pipe2(h.wake",
        "if(!font || !fallback_ok || pipe2(h.wake"
    ), (
        "if(h.s.diagnostics) puts(\"READY files nexaui software\");",
        "for(size_t i=0;i<fallback_count;++i) if(!ae_files_view_add_fallback(h.view,fallback_bytes[i],fallback_sizes[i])) {\n"
        "        fputs(\"files: fallback font is unsupported or invalid\\n\",stderr);h.failed=true;goto done;\n    }\n"
        "    if(h.s.diagnostics) puts(\"READY files nexaui software\");"
    ), (
        "free(font);ae_files_destroy(h.s.files);",
        "for(size_t i=0;i<fallback_count;++i) free(fallback_bytes[i]);\n    free(font);ae_files_destroy(h.s.files);"
    ), (
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
