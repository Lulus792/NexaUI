#include "scene.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_BYTES (4u * 1024u * 1024u)
#define NAME_CAP 128u
#define TRY(x) do { if ((x) != NX_OK) return 0; } while (0)

struct settings_scene {
    nx_context *ui;
    nx_window *window;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_field *name, *probe_secret;
    nx_text_layout *title, *name_label, *motion_label, *apply_label;
    nx_text_layout *pending_label, *applied_label, *invalid_label, *clean_label;
    nx_text_layout *applying_label, *error_label;
    char applied_name[NAME_CAP];
    size_t applied_length;
    char submitted_name[NAME_CAP];
    size_t submitted_length;
    uint64_t next_submission, pending_submission;
    int submitted_motion, apply_error, restore_apply_focus;
    int motion, applied_motion, applied;
    int probe_redraw, probe_step;
    float scroll_x, scroll_y;
    uint64_t revealed_focus;
    float revealed_viewport_height;
};
static nx_utf8_span text(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
static int valid_name(nx_utf8_span s, void *unused) {
    (void)unused;
    return s.length > 0 && s.length < NAME_CAP;
}
static int label(settings_scene *s, const char *str, double size, nx_text_layout **out) {
    return nx_text_layout_create(s->fonts, str, strlen(str), NX_BIDI_LTR,
                                 size, 500, out) == NX_OK;
}
void settings_destroy(settings_scene *s) {
    if (!s) return;
    nx_text_layout_destroy(s->error_label); nx_text_layout_destroy(s->applying_label);
    nx_text_layout_destroy(s->clean_label);
    nx_text_layout_destroy(s->invalid_label); nx_text_layout_destroy(s->applied_label);
    nx_text_layout_destroy(s->pending_label); nx_text_layout_destroy(s->apply_label);
    nx_text_layout_destroy(s->motion_label); nx_text_layout_destroy(s->name_label);
    nx_text_layout_destroy(s->title); nx_text_field_destroy(s->name);
    nx_text_field_destroy(s->probe_secret);
    nx_context_destroy(s->ui); nx_font_collection_destroy(s->fonts);
    nx_font_close(s->font); free(s);
}
int settings_create(const unsigned char *bytes, size_t length, nx_window *window,
                    settings_scene **out) {
    if (!out) return 0;
    *out = NULL;
    if (!bytes || !length) return 0;
    settings_scene *s = calloc(1, sizeof(*s));
    if (!s) return 0;
    s->window = window;
    if (nx_context_create(NULL, &s->ui) != NX_OK ||
        nx_font_open(bytes, length, NULL, &s->font) != NX_OK ||
        nx_font_collection_create(NULL, &s->fonts) != NX_OK ||
        nx_font_collection_add(s->fonts, s->font) != NX_OK ||
        nx_text_field_create("Aetheri", 7, 4096, &s->name) != NX_OK ||
        !label(s, "Settings", 26, &s->title) ||
        !label(s, "Display name", 16, &s->name_label) ||
        !label(s, "Reduce motion", 16, &s->motion_label) ||
        !label(s, "Apply", 16, &s->apply_label) ||
        !label(s, "Unsaved changes", 15, &s->pending_label) ||
        !label(s, "Applied to this example", 15, &s->applied_label) ||
        !label(s, "No changes", 15, &s->clean_label) ||
        !label(s, "Applying to example...", 15, &s->applying_label) ||
        !label(s, "Example apply failed. Retry.", 15, &s->error_label) ||
        !label(s, "Enter a name under 128 bytes", 15, &s->invalid_label)) {
        settings_destroy(s); return 0;
    }
    memcpy(s->applied_name, "Aetheri", 7); s->applied_length = 7;
    *out = s; return 1;
}
nx_context *settings_context(settings_scene *s) { return s ? s->ui : NULL; }
int settings_set_draft_name(settings_scene *s, const char *name, size_t length) {
    return s && nx_text_field_set_text(s->name,name,length)==NX_OK;
}
int settings_enable_accessibility_probe(settings_scene *s) {
    return s && !s->probe_secret && nx_text_field_create_password("private",7,&s->probe_secret)==NX_OK;
}
int settings_take_accessibility_probe_redraw(settings_scene *s) {
    if (!s) return 0;
    int dirty=s->probe_redraw; s->probe_redraw=0; return dirty;
}
int settings_applied(settings_scene *s, nx_utf8_span *name, int *motion) {
    if (!s || !name || !motion) return 0;
    *name = (nx_utf8_span){s->applied_name, s->applied_length};
    *motion = s->applied_motion; return 1;
}
uint64_t settings_pending_apply(const settings_scene *s) {
    return s ? s->pending_submission : 0;
}
int settings_complete_apply(settings_scene *s, uint64_t token, int success) {
    if (!s || !token || token != s->pending_submission || (success != 0 && success != 1)) return 0;
    s->pending_submission = 0;
    s->apply_error = !success;
    if (success) {
        memcpy(s->applied_name,s->submitted_name,s->submitted_length);
        s->applied_length=s->submitted_length;
        s->applied_motion=s->submitted_motion;
        s->applied=1;
    }
    return 1;
}
int settings_frame(settings_scene *s, float width, float height) {
    if (!s || width < 128 || height < 120) return 0;
    TRY(nx_begin_frame(s->ui, width, height, 1.0/60.0));
    nx_color fg, muted;
    TRY(nx_theme_color(s->ui, NX_COLOR_TEXT, &fg));
    TRY(nx_theme_color(s->ui, NX_COLOR_TEXT_MUTED, &muted));
    nx_semantic_desc root = {0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.bounds=(nx_rect){0,0,width,height}; root.name=text("Settings example");
    TRY(nx_semantics_add(s->ui, &root, NULL));
    nx_container_desc panel={0}; panel.id=2; panel.parent_id=1;
    panel.bounds=(nx_rect){16,16,width-32,height-32}; panel.name=text("Settings");
    panel.label_layout=s->title; panel.label_origin=(nx_point){32,30}; panel.glyph_cache_bytes=CACHE_BYTES;
    TRY(nx_panel(s->ui, &panel));
    nx_scroll_area_desc scroll={0}; scroll.id=7; scroll.parent_id=2;
    scroll.name=text("Settings options"); scroll.viewport=(nx_rect){16,70,width-32,height-86};
    scroll.content_size=(nx_size){width-32,202}; scroll.axes=NX_SCROLL_Y;
    uint64_t focus=nx_focus_id(s->ui);
    if (focus!=s->revealed_focus || scroll.viewport.height!=s->revealed_viewport_height) {
        float top=0, bottom=0;
        if (focus==3) {top=34;bottom=72;}
        else if (focus==4) {top=90;bottom=122;}
        else if (focus==5) {top=140;bottom=178;}
        if (bottom>top) {
            if (top<s->scroll_y) s->scroll_y=top;
            else if (bottom>s->scroll_y+scroll.viewport.height)
                s->scroll_y=bottom-scroll.viewport.height;
        }
        s->revealed_focus=focus;
        s->revealed_viewport_height=scroll.viewport.height;
    }
    nx_scroll_info scroll_info;
    TRY(nx_scroll_area_begin(s->ui,&scroll,&s->scroll_x,&s->scroll_y,&scroll_info));
    float y=s->scroll_y;
    TRY(nx_draw_text_layout(s->ui,s->name_label,(nx_point){32,78-y},fg,CACHE_BYTES));
    nx_text_field_desc field={0}; field.id=3; field.parent_id=7;
    field.bounds=(nx_rect){32,104-y,width-64,38}; field.name=text("Display name");
    field.fonts=s->fonts; field.pixels_per_em=16; field.glyph_cache_bytes=CACHE_BYTES;
    field.validate=valid_name; field.ime_window=s->window;
    if (s->window) TRY(nx_text_clipboard_for_window(s->window,&field.clipboard));
    nx_interaction_state interaction; int valid=0, submitted=0;
    TRY(nx_text_field_widget(s->ui,&field,s->name,&interaction,&valid,&submitted));
    nx_control_desc motion={0}; motion.id=4; motion.parent_id=7;
    motion.bounds=(nx_rect){32,160-y,width-64,32}; motion.name=text("Reduce motion");
    motion.label_layout=s->motion_label; motion.label_origin=(nx_point){70,164-y}; motion.glyph_cache_bytes=CACHE_BYTES;
    TRY(nx_checkbox(s->ui,&motion,&s->motion,&interaction));
    nx_control_desc apply={0}; apply.id=5; apply.parent_id=7;
    apply.bounds=(nx_rect){32,210-y,110,38}; apply.name=text("Apply settings");
    apply.label_layout=s->apply_label; apply.label_origin=(nx_point){61,218-y}; apply.glyph_cache_bytes=CACHE_BYTES;
    apply.disabled=!valid || s->pending_submission || s->next_submission==UINT64_MAX;
    TRY(nx_button(s->ui,&apply,&interaction));
    if (!s->pending_submission && s->restore_apply_focus) {
        if (!nx_focus_id(s->ui) && !apply.disabled)
            TRY(nx_focus_set(s->ui,5));
        s->restore_apply_focus=0;
    }
    nx_utf8_span name; TRY(nx_text_field_text(s->name,&name));
    if (interaction.clicked && !apply.disabled) {
        memcpy(s->submitted_name,name.data,name.length); s->submitted_length=name.length;
        s->submitted_motion=s->motion; s->apply_error=0;
        s->restore_apply_focus=nx_focus_id(s->ui)==5;
        s->pending_submission=++s->next_submission;
    }
    int same = name.length==s->applied_length &&
        memcmp(name.data,s->applied_name,name.length)==0 && s->motion==s->applied_motion;
    const char *message = s->pending_submission ? "Applying to example..." :
                          !valid ? "Enter a name under 128 bytes" :
                          s->apply_error ? "Example apply failed. Retry." :
                          !same ? "Unsaved changes" : s->applied ? "Applied to this example" : "No changes";
    nx_text_layout *status = s->pending_submission ? s->applying_label :
                             !valid ? s->invalid_label : s->apply_error ? s->error_label :
                             !same ? s->pending_label : s->applied ? s->applied_label : s->clean_label;
    nx_semantic_desc state={0}; state.id=6; state.parent_id=7; state.role=NX_ROLE_STATUS;
    state.bounds=(nx_rect){32,252-y,width-64,20}; state.name=text("Settings status"); state.value=text(message);
    TRY(nx_semantics_add(s->ui,&state,NULL));
    TRY(nx_draw_text_layout(s->ui,status,(nx_point){32,252-y},muted,CACHE_BYTES));
    TRY(nx_scroll_area_end(s->ui));
    if (s->probe_secret) {
        nx_text_field_desc secret={0}; secret.id=8; secret.parent_id=2;
        secret.bounds=(nx_rect){32,500,150,38}; secret.name=text("Secret probe");
        secret.fonts=s->fonts; secret.pixels_per_em=16; secret.glyph_cache_bytes=CACHE_BYTES; secret.password=1;
        TRY(nx_text_field_widget(s->ui,&secret,s->probe_secret,&interaction,&valid,&submitted));
        nx_semantic_desc custom={0}; custom.id=9; custom.parent_id=2;
        custom.role=NX_ROLE_TEXT_FIELD; custom.bounds=(nx_rect){32,550,150,38};
        custom.name=text("Custom text probe"); custom.value=text("not a field");
        TRY(nx_semantics_add(s->ui,&custom,NULL));
        nx_control_desc change={0}; change.id=10; change.parent_id=2;
        change.bounds=(nx_rect){32,600,150,38}; change.name=text("Change text probe");
        TRY(nx_button(s->ui,&change,&interaction));
        if (interaction.clicked) {
            const char *next=s->probe_step++?"A\xc3\xb6Z":"A\xc3\xb6\xf0\x9f\x8c\x8d";
            TRY(nx_text_field_set_text(s->name,next,strlen(next)));
            s->probe_redraw=1;
        }
    }
    TRY(nx_end_frame(s->ui));
    return 1;
}
