#include "scene.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_BYTES (4u * 1024u * 1024u)
#define NAME_CAP 128u
#define TRY(x) do { if ((x) != NX_OK) return 0; } while (0)

typedef struct settings_strings {
    const char *root, *title, *options, *name, *motion, *apply, *apply_name;
    const char *pending, *applied, *invalid, *clean, *applying, *error, *unknown;
} settings_strings;
static const settings_strings languages[]={
    {"Settings example","Settings","Settings options","Display name","Reduce motion","Apply","Apply settings",
     "Unsaved changes","Applied to this example","Enter a name under 128 bytes","No changes",
     "Applying to example...","Example apply failed. Retry.","Outcome unknown. Check the service."},
    {"Einstellungsbeispiel","Einstellungen","Einstellungsoptionen","Anzeigename","Bewegung reduzieren","Anwenden","Einstellungen anwenden",
     "Ungespeicherte \xc3\x84nderungen","Im Beispiel angewendet","Name unter 128 Bytes eingeben","Keine \xc3\x84nderungen",
     "Wird im Beispiel angewendet...","Anwenden fehlgeschlagen. Erneut versuchen.","Ergebnis unbekannt. Dienst pr\xc3\xbc" "fen."}
};
struct settings_scene {
    nx_context *ui;
    nx_window *window;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_field *name, *probe_secret;
    nx_text_layout *title, *name_label, *motion_label, *apply_label;
    nx_text_layout *pending_label, *applied_label, *invalid_label, *clean_label;
    nx_text_layout *applying_label, *error_label, *unknown_label;
    char applied_name[NAME_CAP];
    size_t applied_length;
    char submitted_name[NAME_CAP];
    size_t submitted_length;
    uint64_t next_submission, pending_submission;
    int submitted_motion, apply_error, restore_apply_focus, outcome_unknown, dispatch_pending;
    int motion, applied_motion, applied;
    unsigned language;
    int probe_redraw, probe_step;
    float scroll_x, scroll_y;
    float status_width, status_max_height;
    uint64_t revealed_focus;
    float revealed_viewport_height, revealed_width;
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
/* The text API does not break unbreakable words. Scale only when such a word
   would exceed this example's fixed minimum-width content column. */
static int fitted_label(settings_scene *s, const char *str, double size,
                        float width, nx_text_layout **out) {
    nx_text_layout *layout=NULL;
    if (nx_text_layout_create(s->fonts,str,strlen(str),NX_BIDI_LTR,size,width,&layout)!=NX_OK) return 0;
    if (nx_text_layout_width(layout)>width) {
        size*=0.98*width/nx_text_layout_width(layout);
        nx_text_layout_destroy(layout); layout=NULL;
        if (nx_text_layout_create(s->fonts,str,strlen(str),NX_BIDI_LTR,size,width,&layout)!=NX_OK) return 0;
    }
    if (nx_text_layout_width(layout)>width) { nx_text_layout_destroy(layout); return 0; }
    *out=layout; return 1;
}
static int reflow_status(settings_scene *s, float width) {
    if (s->status_width==width) return 1;
    const settings_strings *strings=&languages[s->language];
    const char *values[]={strings->pending,strings->applied,strings->invalid,
                          strings->clean,strings->applying,strings->error,strings->unknown};
    nx_text_layout **slots[]={&s->pending_label,&s->applied_label,&s->invalid_label,
                              &s->clean_label,&s->applying_label,&s->error_label,&s->unknown_label};
    nx_text_layout *next[7]={0}; float maximum=0;
    nx_text_layout *motion=NULL, *name=NULL, *title=NULL, *apply=NULL;
    for (size_t i=0;i<7;++i) {
        if (!fitted_label(s,values[i],15,width,&next[i])) {
            for (size_t j=0;j<7;++j) nx_text_layout_destroy(next[j]);
            return 0;
        }
        maximum=fmaxf(maximum,(float)nx_text_layout_height(next[i]));
    }
    float motion_width=width<116 ? width : width-38;
    if (!fitted_label(s,strings->motion,width<116?14:16,motion_width,&motion)) {
        for (size_t j=0;j<7;++j) nx_text_layout_destroy(next[j]);
        return 0;
    }
    if (!fitted_label(s,strings->name,16,width,&name) ||
        !fitted_label(s,strings->title,26,width,&title) ||
        !fitted_label(s,strings->apply,16,fminf(110,width)-12,&apply)) {
        for (size_t j=0;j<7;++j) nx_text_layout_destroy(next[j]);
        nx_text_layout_destroy(motion); nx_text_layout_destroy(name); nx_text_layout_destroy(title);
        nx_text_layout_destroy(apply);
        return 0;
    }
    for (size_t i=0;i<7;++i) { nx_text_layout_destroy(*slots[i]); *slots[i]=next[i]; }
    nx_text_layout_destroy(s->motion_label); s->motion_label=motion;
    nx_text_layout_destroy(s->name_label); s->name_label=name;
    nx_text_layout_destroy(s->title); s->title=title;
    nx_text_layout_destroy(s->apply_label); s->apply_label=apply;
    s->status_width=width; s->status_max_height=maximum;
    return 1;
}
void settings_destroy(settings_scene *s) {
    if (!s) return;
    nx_text_layout_destroy(s->unknown_label);
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
        !label(s, "Outcome unknown. Check the service.", 15, &s->unknown_label) ||
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
int settings_set_language(settings_scene *s, const char *language) {
    if (!s || !language) return 0;
    unsigned next;
    if (!strcmp(language,"en")) next=0;
    else if (!strcmp(language,"de")) next=1;
    else return 0;
    if (s->language!=next) {s->language=next;s->status_width=0;s->revealed_width=-1;}
    return 1;
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
uint64_t settings_take_apply_request(settings_scene *s) {
    if (!s || !s->pending_submission || !s->dispatch_pending || s->outcome_unknown) return 0;
    s->dispatch_pending=0; return s->pending_submission;
}
int settings_observe_apply_unknown(settings_scene *s, uint64_t token) {
    if (!s || !token || token!=s->pending_submission) return 0;
    s->outcome_unknown=1; s->dispatch_pending=0; return 1;
}
int settings_complete_apply(settings_scene *s, uint64_t token, int success) {
    if (!s || !token || token != s->pending_submission || (success != 0 && success != 1)) return 0;
    s->pending_submission = 0;
    s->dispatch_pending=0; s->outcome_unknown=0;
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
    if (!reflow_status(s,width-64)) return 0;
    const settings_strings *strings=&languages[s->language];
    TRY(nx_begin_frame(s->ui, width, height, 1.0/60.0));
    nx_color fg, muted;
    TRY(nx_theme_color(s->ui, NX_COLOR_TEXT, &fg));
    TRY(nx_theme_color(s->ui, NX_COLOR_TEXT_MUTED, &muted));
    nx_semantic_desc root = {0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.bounds=(nx_rect){0,0,width,height}; root.name=text(strings->root);
    TRY(nx_semantics_add(s->ui, &root, NULL));
    nx_container_desc panel={0}; panel.id=2; panel.parent_id=1;
    panel.bounds=(nx_rect){16,16,width-32,height-32}; panel.name=text(strings->title);
    panel.label_layout=s->title; panel.label_origin=(nx_point){32,30}; panel.glyph_cache_bytes=CACHE_BYTES;
    TRY(nx_panel(s->ui, &panel));
    nx_scroll_area_desc scroll={0}; scroll.id=7; scroll.parent_id=2;
    scroll.name=text(strings->options); scroll.viewport=(nx_rect){16,70,width-32,height-86};
    int stacked_motion=width<180;
    float field_y=fmaxf(104,85+(float)nx_text_layout_height(s->name_label));
    float motion_y=160+field_y-104;
    float apply_y=210+field_y-104+(stacked_motion ? (float)nx_text_layout_height(s->motion_label)+8 : 0);
    float status_y=apply_y+42;
    scroll.content_size=(nx_size){width-32,status_y-70+s->status_max_height+8}; scroll.axes=NX_SCROLL_Y;
    uint64_t focus=nx_focus_id(s->ui);
    if (focus!=s->revealed_focus || scroll.viewport.height!=s->revealed_viewport_height || width!=s->revealed_width) {
        float top=0, bottom=0;
        if (focus==3) {top=field_y-70;bottom=top+38;}
        else if (focus==4) {
            top=motion_y-70;
            bottom=top+(stacked_motion ? 28+(float)nx_text_layout_height(s->motion_label) : 32);
        }
        else if (focus==5) {top=apply_y-70;bottom=top+38;}
        if (bottom>top) {
            if (top<s->scroll_y) s->scroll_y=top;
            else if (bottom>s->scroll_y+scroll.viewport.height)
                s->scroll_y=bottom-scroll.viewport.height;
        }
        s->revealed_focus=focus;
        s->revealed_viewport_height=scroll.viewport.height;
        s->revealed_width=width;
    }
    /* Page through long status text without stealing text-field editing keys. */
    if (focus==4 || focus==5) {
        const nx_event *events=NULL; size_t count=0;
        TRY(nx_frame_events(s->ui,&events,&count));
        for (size_t i=0;i<count;++i) if (events[i].type==NX_EVENT_KEY_DOWN && !events[i].modifiers) {
            if (events[i].key==NX_KEY_PAGE_DOWN) s->scroll_y+=scroll.viewport.height*0.5f;
            else if (events[i].key==NX_KEY_PAGE_UP) s->scroll_y=fmaxf(0,s->scroll_y-scroll.viewport.height*0.5f);
        }
    }
    nx_scroll_info scroll_info;
    TRY(nx_scroll_area_begin(s->ui,&scroll,&s->scroll_x,&s->scroll_y,&scroll_info));
    float y=s->scroll_y;
    TRY(nx_draw_text_layout(s->ui,s->name_label,(nx_point){32,78-y},fg,CACHE_BYTES));
    nx_text_field_desc field={0}; field.id=3; field.parent_id=7;
    field.bounds=(nx_rect){32,field_y-y,width-64,38}; field.name=text(strings->name);
    field.fonts=s->fonts; field.pixels_per_em=16; field.glyph_cache_bytes=CACHE_BYTES;
    field.validate=valid_name; field.ime_window=s->window;
    if (s->window) TRY(nx_text_clipboard_for_window(s->window,&field.clipboard));
    nx_interaction_state interaction; int valid=0, submitted=0;
    TRY(nx_text_field_widget(s->ui,&field,s->name,&interaction,&valid,&submitted));
    nx_control_desc motion={0}; motion.id=4; motion.parent_id=7;
    motion.bounds=(nx_rect){32,motion_y-y,width-64,stacked_motion?24:32}; motion.name=text(strings->motion);
    motion.label_layout=stacked_motion?NULL:s->motion_label; motion.label_origin=(nx_point){70,motion_y+4-y}; motion.glyph_cache_bytes=CACHE_BYTES;
    TRY(nx_checkbox(s->ui,&motion,&s->motion,&interaction));
    if (stacked_motion)
        TRY(nx_draw_text_layout(s->ui,s->motion_label,(nx_point){32,motion_y+28-y},fg,CACHE_BYTES));
    nx_control_desc apply={0}; apply.id=5; apply.parent_id=7;
    float apply_width=fminf(110,width-64);
    apply.bounds=(nx_rect){32,apply_y-y,apply_width,38}; apply.name=text(strings->apply_name);
    apply.label_layout=s->apply_label;
    apply.label_origin=(nx_point){32+(apply_width-(float)nx_text_layout_width(s->apply_label))*0.5f,apply_y+8-y}; apply.glyph_cache_bytes=CACHE_BYTES;
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
        s->dispatch_pending=1; s->outcome_unknown=0;
    }
    int same = name.length==s->applied_length &&
        memcmp(name.data,s->applied_name,name.length)==0 && s->motion==s->applied_motion;
    const char *message = s->outcome_unknown ? strings->unknown : s->pending_submission ? strings->applying :
                          !valid ? strings->invalid :
                          s->apply_error ? strings->error :
                          !same ? strings->pending : s->applied ? strings->applied : strings->clean;
    nx_text_layout *status = s->outcome_unknown ? s->unknown_label : s->pending_submission ? s->applying_label :
                             !valid ? s->invalid_label : s->apply_error ? s->error_label :
                             !same ? s->pending_label : s->applied ? s->applied_label : s->clean_label;
    nx_semantic_desc state={0}; state.id=6; state.parent_id=7; state.role=NX_ROLE_STATUS;
    state.bounds=(nx_rect){32,status_y-y,width-64,(float)nx_text_layout_height(status)}; state.name=text(s->language?"Einstellungsstatus":"Settings status"); state.value=text(message);
    TRY(nx_semantics_add(s->ui,&state,NULL));
    TRY(nx_draw_text_layout(s->ui,status,(nx_point){32,status_y-y},muted,CACHE_BYTES));
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
