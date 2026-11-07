#include "picker.h"
#include "../common/text_wrap.h"
#include <stdio.h>
#include <string.h>

static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
static int valid_query(nx_utf8_span text,void *user) { (void)user;return text.length<=AE_FILES_QUERY; }
static const char *word(const fp_picker *p, const char *en, const char *de) { return p->german ? de : en; }
const fp_reply *fp_picker_snapshot(const fp_picker *p) { return p ? fp_reply_get(&p->observation) : NULL; }
static void clear_layouts(fp_picker *p) {
    for (size_t i = 0; i < 16; ++i) { nx_text_layout_destroy(p->labels[i]); p->labels[i] = NULL; }
    for (size_t i = 0; i <= AE_FILES_DEPTH; ++i) { nx_text_layout_destroy(p->crumbs[i]); p->crumbs[i] = NULL; }
    for (size_t i = 0; i < FP_PAGE_ROWS; ++i) { nx_text_layout_destroy(p->rows[i]); p->rows[i] = NULL; }
    p->layout_width = 0;
}
nx_result fp_picker_init(fp_picker *p, fp_worker *worker, nx_font_collection *fonts, uint64_t view) {
    if (!p || !worker || !fonts || !view) return NX_ERROR_ARGUMENT;
    *p = (fp_picker){0}; p->worker = worker; p->fonts = fonts; p->view = view;
    p->text_size = 16; p->layout_page = SIZE_MAX;
    return nx_text_field_create("", 0, 4096, &p->query);
}
void fp_picker_destroy(fp_picker *p) {
    if (!p) return;
    clear_layouts(p); nx_sim_message_dispose(&p->observation); nx_text_field_destroy(p->query); *p = (fp_picker){0};
}
nx_result fp_picker_request(fp_picker *p, const fp_command *source) {
    if (!p || !source || p->busy) return NX_ERROR_STATE;
    if (p->request == UINT64_MAX) { p->error = NX_ERROR_LIMIT; return p->error; }
    fp_command c = *source;
    c.key = (fp_key){p->view, ++p->request};
    const fp_reply *old = fp_picker_snapshot(p); c.expected_state = old ? old->state : 0;
    uint64_t ticket = 0; nx_result result = fp_worker_submit(p->worker, &c, &ticket);
    p->error = result; p->operation = c.operation;
    if (result == NX_OK) { p->ticket = ticket; p->busy = 1; p->restore = FP_CANCEL; }
    clear_layouts(p); return result;
}
int fp_picker_observe(fp_picker *p, nx_sim_message *m) {
    if (!p || !m || !p->busy || m->ticket != p->ticket) return 0;
    fp_reply *r = fp_reply_get(m);
    /* An unrelated well-formed key is never allowed to supersede this request. */
    if (r && (r->key.view != p->view || r->key.request != p->request)) return 0;
    p->busy = 0; p->ticket = 0;
    if (!r) p->error = m->status != NX_OK ? m->status : NX_ERROR_ARGUMENT;
    else {
        nx_sim_message_dispose(&p->observation); p->observation = *m; *m = (nx_sim_message){0};
        p->error = NX_OK; p->selected = 0; p->page = 0; p->scroll_y = 0;
        if (r->result == AE_FILES_OK && p->operation != FP_PICK)
            p->error = nx_text_field_set_text(p->query, r->query, strlen(r->query));
    }
    p->restore = p->operation == FP_FILTER ? FP_QUERY : FP_REFRESH_BUTTON;
    clear_layouts(p); return 1;
}
static const fp_row *selected_row(const fp_picker *p) {
    const fp_reply *r = fp_picker_snapshot(p);
    if (r && p->selected >= FP_ROWS) for (size_t i = 0; i < r->count; ++i)
        if (FP_ROWS + r->rows[i].ordinal == p->selected) return &r->rows[i];
    return NULL;
}
static int command(fp_intent *intent, fp_operation op) {
    if (!intent->requested && !intent->cancel) { intent->requested = 1; intent->command.operation = op;return 1; }
    return 0;
}
static void open_selected(fp_picker *p, fp_intent *intent) {
    const fp_row *row = selected_row(p);
    if (!p->busy && row && (row->entry.kind == AE_FILES_DIRECTORY || row->entry.kind == AE_FILES_REGULAR)) {
        if(command(intent, row->entry.kind == AE_FILES_DIRECTORY ? FP_ENTER : FP_PICK))intent->command.ordinal = row->ordinal;
    }
}
int fp_picker_key(fp_picker *p, uint64_t focus, const nx_event *e, fp_intent *intent) {
    const fp_reply *r = fp_picker_snapshot(p);
    if (!r || p->busy || e->type != NX_EVENT_KEY_DOWN || e->modifiers || focus < FP_ROWS) return 0;
    size_t i = 0; while (i < r->count && FP_ROWS + r->rows[i].ordinal != focus) ++i;
    if (i == r->count) return 0;
    if (e->key == NX_KEY_ENTER && !e->repeat) { p->selected = focus; open_selected(p, intent); return 1; }
    size_t next = i;
    if (e->key == NX_KEY_HOME) next = 0;
    else if (e->key == NX_KEY_END) next = r->count - 1;
    else if (e->key == NX_KEY_UP) { if (next) --next; }
    else if (e->key == NX_KEY_DOWN) { if (next + 1 < r->count) ++next; }
    else return 0;
    if (p->page != next / FP_PAGE_ROWS) { p->page = next / FP_PAGE_ROWS; p->rebind = 1; clear_layouts(p); }
    p->selected = FP_ROWS + r->rows[next].ordinal; p->restore = p->selected; return 1;
}
static const char *status(const fp_picker *p) {
    if (p->busy) return word(p, "Reading folder…", "Ordner wird gelesen…");
    if (p->error != NX_OK) return word(p, "Could not obtain a current result. Retry.", "Kein aktuelles Ergebnis verfügbar. Erneut versuchen.");
    const fp_reply *r = fp_picker_snapshot(p);
    if (!r) return word(p, "Folder not read", "Ordner noch nicht gelesen");
    switch (r->result) {
    case AE_FILES_OK: return r->count ? word(p, "Select a file or open a folder", "Datei auswählen oder Ordner öffnen") :
        word(p, "No matching entries", "Keine passenden Einträge");
    case AE_FILES_CHANGED: return word(p, "Folder or entry changed. Refresh before retrying.", "Ordner oder Eintrag geändert. Vor erneutem Versuch aktualisieren.");
    case AE_FILES_LIMIT: return word(p, "Folder exceeds the original model limit; previous folder retained.", "Ordner überschreitet die Modellgrenze; vorheriger Ordner bleibt erhalten.");
    case AE_FILES_INVALID: return word(p, "Invalid folder or filter; previous observation retained.", "Ungültiger Ordner oder Filter; vorheriges Ergebnis bleibt erhalten.");
    default: return word(p, "Could not complete the request; previous observation retained.", "Anfrage fehlgeschlagen; vorheriges Ergebnis bleibt erhalten.");
    }
}
static int make_layout(fp_picker *p, const char *s, double width, nx_text_layout **layout) {
    return nx_example_small_text_layout(p->fonts, s, strlen(s), p->text_size, width, layout);
}
static nx_result layouts(fp_picker *p, double width) {
    const fp_reply *r = fp_picker_snapshot(p); uint64_t state = r ? r->state : 0;
    if (p->layout_width == width && p->layout_size == p->text_size && p->layout_page == p->page && p->layout_state == state) return NX_OK;
    clear_layouts(p);
    char counts[96]; size_t first = p->page * FP_PAGE_ROWS;
    size_t end = r && first + FP_PAGE_ROWS < r->count ? first + FP_PAGE_ROWS : r ? r->count : 0;
    (void)snprintf(counts, sizeof counts, p->german ? "%zu–%zu von %zu Einträgen (%zu insgesamt)" : "%zu–%zu of %zu entries (%zu total)",
        r && r->count ? first + 1 : 0, end, r ? r->count : 0, r ? r->total : 0);
    const char *text[16] = {
        word(p,"Root","Wurzelordner"), word(p,"Up","Übergeordneter Ordner"),
        word(p,"Refresh / Retry","Aktualisieren / Erneut versuchen"), word(p,"Show hidden","Verborgene anzeigen"),
        word(p,"Apply filter","Filter anwenden"), word(p,"Folder path","Ordnerpfad"),
        status(p), word(p,"Previous page","Vorherige Seite"), word(p,"Next page","Nächste Seite"), counts,
        word(p,"Open","Öffnen"), word(p,"Cancel","Abbrechen"),
        r && r->state ? r->ancestors[r->depth] : word(p,"No current folder","Kein aktueller Ordner"),
        "", "", ""
    };
    for (size_t i = 0; i < 13; ++i) {
        double w = i < 4 || i == 7 || i == 8 || ((i == 10 || i == 11) && width>=432) ? (width - 8) / 2 : width;
        if (!make_layout(p, text[i], w - (i==3||i==5?40:24), &p->labels[i])) { clear_layouts(p); return NX_ERROR_STATE; }
    }
    if (r) {
        for (unsigned d = 0; d <= r->depth; ++d)
            if (!make_layout(p, r->ancestors[d], width - 24, &p->crumbs[d])) { clear_layouts(p); return NX_ERROR_STATE; }
        for (size_t i = first; i < end; ++i)
            if (!make_layout(p, r->rows[i].entry.label, width - (r->rows[i].entry.kind==AE_FILES_DIRECTORY?48:24), &p->rows[i-first])) { clear_layouts(p); return NX_ERROR_STATE; }
    }
    p->layout_width = width; p->layout_size = p->text_size; p->layout_page = p->page; p->layout_state = state; return NX_OK;
}
static float height(const fp_picker *p, const nx_text_layout *l) {
    float h = l ? (float)nx_text_layout_height(l) + 16 : p->text_size + 16;
    return h < p->text_size + 16 ? p->text_size + 16 : h;
}
static float footer_height(const fp_picker *p) {
    float a=height(p,p->labels[10]),b=height(p,p->labels[11]);
    return p->layout_width<432?a+b+8:a>b?a:b;
}
static nx_result button(fp_picker *p, nx_context *ctx, uint64_t id, uint64_t parent, nx_rect b,
    nx_text_layout *label, const char *name, int disabled, int *clicked) {
    (void)p;
    nx_control_desc d = {0}; d.id = id; d.parent_id = parent; d.bounds = b; d.name = span(name);
    d.disabled = disabled; d.label_layout = label; d.label_origin = (nx_point){b.x + 12,b.y + 8}; d.glyph_cache_bytes = 1024u*1024u;
    nx_action_desc action={0};action.control=d;action.variant=id==FP_OPEN?NX_ACTION_PRIMARY:NX_ACTION_SECONDARY;
    nx_interaction_state s; nx_result result = nx_action(ctx,&action,&s); *clicked = result == NX_OK && s.clicked; return result;
}
static nx_result label(nx_context *ctx, uint64_t id, uint64_t parent, nx_rect b, const nx_text_layout *l,
    const char *name, uint32_t state, nx_semantic_role role) {
    nx_semantic_desc d = {0}; d.id=id; d.parent_id=parent; d.bounds=b; d.name=span(name); d.role=role; d.state=state;
    nx_result result=nx_semantics_add(ctx,&d,NULL); nx_color color;
    if(result==NX_OK)result=nx_theme_color(ctx,NX_COLOR_TEXT,&color);
    if(result==NX_OK)result=nx_draw_text_layout(ctx,l,(nx_point){b.x+12,b.y+8},color,1024u*1024u);
    return result;
}
/* Reveal from last frame before registering this one, including a row taller
   than the viewport. PageUp/Down then permits reading its remaining lines. */
int fp_picker_reveal(fp_picker *p,const nx_context *ctx,float window_height) {
    float footer=footer_height(p);
    float viewport=window_height-footer-32,top=8,bottom=top+viewport;
    int redraw=0;
    uint64_t focus=nx_focus_id(ctx);uint32_t index;nx_semantic_view node;
    if(focus!=FP_OPEN && focus!=FP_CANCEL && nx_semantics_find(ctx,focus,&index)==NX_OK && nx_semantics_get(ctx,index,&node)==NX_OK) {
        if(node.bounds.y<top && (node.bounds.height<=viewport || p->revealed_focus!=focus)) {p->scroll_y+=node.bounds.y-top;redraw=1;}
        else if(node.bounds.height<=viewport && node.bounds.y+node.bounds.height>bottom) {p->scroll_y+=node.bounds.y+node.bounds.height-bottom;redraw=1;}
    }
    p->revealed_focus=focus;return redraw;
}
#define TRY(call) do { nx_result fp_result=(call); if(fp_result!=NX_OK)return fp_result; } while(0)
nx_result fp_picker_build(fp_picker *p,nx_context *ctx,nx_window *window,float width,float window_height,fp_intent *intent,int *redraw) {
    if(!p||!ctx||!intent||!redraw||width<180||window_height<160)return NX_ERROR_ARGUMENT;
    *redraw=0;
    const fp_reply *r=fp_picker_snapshot(p);float content_width=width-48;
    TRY(layouts(p,content_width));
    float footer=footer_height(p);
    float viewport=window_height-footer-32;if(viewport<32)return NX_ERROR_LIMIT;
    /* All long content belongs to one scroll area. Footer actions remain fixed. */
    float row_a=height(p,p->labels[0]);if(height(p,p->labels[1])>row_a)row_a=height(p,p->labels[1]);
    float row_b=height(p,p->labels[2]);if(height(p,p->labels[3])>row_b)row_b=height(p,p->labels[3]);
    float query_h=p->text_size+20, apply_h=height(p,p->labels[4]);
    float path_h=height(p,p->labels[5]), current_h=height(p,p->labels[12]), status_h=height(p,p->labels[6]);
    float page_h=height(p,p->labels[7]);if(height(p,p->labels[8])>page_h)page_h=height(p,p->labels[8]);
    float rows_height=0,crumb_height=0;size_t first=p->page*FP_PAGE_ROWS,end=r&&first+FP_PAGE_ROWS<r->count?first+FP_PAGE_ROWS:r?r->count:0;
    for(size_t i=first;i<end;++i)rows_height+=height(p,p->rows[i-first])+4;
    if(r&&p->expanded)for(unsigned d=0;d<=r->depth;++d)crumb_height+=height(p,p->crumbs[d])+4;
    float list_top=row_a+row_b+query_h+apply_h+path_h+current_h+crumb_height+status_h+page_h+height(p,p->labels[9])+60;
    float total=list_top+rows_height;
    /* A navigation target can be on a newly selected page with no old geometry. */
    if(p->restore>=FP_ROWS&&r) {
        float target=list_top;
        for(size_t i=first;i<end;++i) {
            float h=height(p,p->rows[i-first]);
            if(FP_ROWS+r->rows[i].ordinal==p->restore) {
                if(target<p->scroll_y)p->scroll_y=target;
                else if(target+h>p->scroll_y+viewport)p->scroll_y=h>viewport?target:target+h-viewport;
                break;
            }
            target+=h+4;
        }
    }
    nx_scroll_area_desc scroll={0};scroll.id=FP_SCROLL;scroll.parent_id=1;scroll.thumb_y_id=FP_THUMB;
    scroll.name=span(word(p,"Folder contents","Ordnerinhalt"));scroll.axes=NX_SCROLL_Y;
    scroll.viewport=(nx_rect){16,8,width-32,viewport};scroll.content_size=(nx_size){content_width,total};scroll.wheel_step=p->text_size*3;
    float x_offset=0;nx_scroll_info info;TRY(nx_scroll_area_begin(ctx,&scroll,&x_offset,&p->scroll_y,&info));
    float x=info.viewport.x,y=info.viewport.y-p->scroll_y,w=info.viewport.width,half=(w-8)/2;int clicked;
    const char *names[4]={word(p,"Root","Wurzelordner"),word(p,"Up","Übergeordneter Ordner"),word(p,"Refresh / Retry","Aktualisieren / Erneut versuchen"),word(p,"Show hidden","Verborgene anzeigen")};
    for(size_t i=0;i<4;++i) {
        float h=i<2?row_a:row_b;
        if(i==3) {
            nx_control_desc hidden={0};hidden.id=FP_HIDDEN_BUTTON;hidden.parent_id=FP_SCROLL;hidden.bounds=(nx_rect){x+half+8,y,half,h};hidden.name=span(names[i]);
            hidden.label_layout=p->labels[i];hidden.label_origin=(nx_point){hidden.bounds.x+28,y+8};hidden.glyph_cache_bytes=1024u*1024u;hidden.disabled=p->busy||!r||!r->state;
            int checked=r?r->show_hidden:0,old=checked;nx_interaction_state toggled;
            TRY(nx_checkbox(ctx,&hidden,&checked,&toggled));clicked=old!=checked;
        } else TRY(button(p,ctx,FP_ROOT+i,FP_SCROLL,(nx_rect){x+(i%2?half+8:0),y,half,h},p->labels[i],names[i],p->busy||(i<2&&(!r||!r->depth)),&clicked));
        if(clicked) {
            int accepted=command(intent,i==0?FP_ANCESTOR:i==1?FP_UP:i==2?(r&&r->state?FP_REFRESH:FP_READ):FP_HIDDEN);
            if(accepted&&i==3)intent->command.show_hidden=!r->show_hidden;
        }
        if(i%2)y+=h+8;
    }
    nx_text_field_desc field={0};field.id=FP_QUERY;field.parent_id=FP_SCROLL;field.bounds=(nx_rect){x,y,w,query_h};
    field.name=span(word(p,"Name filter","Namensfilter"));field.fonts=p->fonts;field.pixels_per_em=p->text_size;field.glyph_cache_bytes=1024u*1024u;
    field.disabled=p->busy||!r||!r->state;field.ime_window=window;field.validate=valid_query;
    if(window)TRY(nx_text_clipboard_for_window(window,&field.clipboard));
    nx_interaction_state edited;int valid,submitted;
    TRY(nx_text_field_widget_described(ctx,&field,p->query,span(word(p,"Enter applies the filter. Esc cancels this picker.","Enter wendet den Filter an. Esc bricht die Dateiauswahl ab.")),&edited,&valid,&submitted));
    y+=query_h+8;
    TRY(button(p,ctx,FP_APPLY,FP_SCROLL,(nx_rect){x,y,w,apply_h},p->labels[4],word(p,"Apply filter","Filter anwenden"),field.disabled||!valid,&clicked));
    if((clicked||submitted)&&!field.disabled&&valid) {
        nx_utf8_span query;TRY(nx_text_field_text(p->query,&query));
        if(query.length<=AE_FILES_QUERY) {if(command(intent,FP_FILTER)){intent->command.query_bytes=query.length;memcpy(intent->command.query,query.data,query.length);}}
        else {p->error=NX_ERROR_LIMIT;p->layout_width=0;*redraw=1;}
    }
    y+=apply_h+8;
    nx_control_desc path={0};path.id=FP_PATH;path.parent_id=FP_SCROLL;path.bounds=(nx_rect){x,y,w,path_h};path.name=span(word(p,"Folder path","Ordnerpfad"));
    path.label_layout=p->labels[5];path.label_origin=(nx_point){x+28,y+8};path.glyph_cache_bytes=1024u*1024u;
    nx_collapsible_desc section={0};section.container.id=91;section.container.parent_id=FP_SCROLL;
    section.container.name=path.name;section.container.bounds=path.bounds;section.container.bounds.height+=current_h+8+crumb_height;section.header_id=path.id;section.header_bounds=path.bounds;
    section.container.label_layout=path.label_layout;section.container.label_origin=path.label_origin;section.container.glyph_cache_bytes=path.glyph_cache_bytes;
    int old_expanded=p->expanded;nx_interaction_state path_state;TRY(nx_collapsible_section(ctx,&section,&p->expanded,&path_state));
    if(old_expanded!=p->expanded)*redraw=1;
    y+=path_h+4;
    TRY(label(ctx,90,91,(nx_rect){x,y,w,current_h},p->labels[12],r&&r->state?r->ancestors[r->depth]:word(p,"No current folder","Kein aktueller Ordner"),0,NX_ROLE_TEXT));y+=current_h+4;
    if(r&&old_expanded)for(unsigned d=0;d<=r->depth;++d) {
        char ancestor[AE_FILES_LABEL+32];(void)snprintf(ancestor,sizeof ancestor,p->german?"Vorfahre: %s":"Ancestor: %s",r->ancestors[d]);
        float h=height(p,p->crumbs[d]);TRY(button(p,ctx,FP_ANCESTORS+d,91,(nx_rect){x,y,w,h},p->crumbs[d],ancestor,p->busy||d==r->depth,&clicked));
        if(clicked&&command(intent,FP_ANCESTOR))intent->command.depth=d;
        y+=h+4;
    }
    TRY(label(ctx,FP_STATUS,FP_SCROLL,(nx_rect){x,y,w,status_h},p->labels[6],status(p),p->busy?NX_SEM_BUSY:p->error!=NX_OK||(r&&r->result!=AE_FILES_OK)?NX_SEM_INVALID:0,NX_ROLE_STATUS));y+=status_h+8;
    for(size_t i=0;i<2;++i) {
        TRY(button(p,ctx,i?FP_NEXT:FP_PREVIOUS,FP_SCROLL,(nx_rect){x+(i?half+8:0),y,half,page_h},p->labels[7+i],i?word(p,"Next page","Nächste Seite"):word(p,"Previous page","Vorherige Seite"),p->busy||!r||(i?end==r->count:!p->page),&clicked));
        if(clicked&&!p->rebind) {
            p->page=i?p->page+1:p->page-1;p->selected=0;
            p->restore=!p->page?FP_NEXT:(p->page+1)*FP_PAGE_ROWS>=r->count?FP_PREVIOUS:i?FP_NEXT:FP_PREVIOUS;
            p->rebind=1;*redraw=1;
        }
    }
    y+=page_h+8;float counts_h=height(p,p->labels[9]);
    char counts[96];(void)snprintf(counts,sizeof counts,p->german?"%zu–%zu von %zu Einträgen (%zu insgesamt)":"%zu–%zu of %zu entries (%zu total)",r&&r->count?first+1:0,end,r?r->count:0,r?r->total:0);
    TRY(label(ctx,FP_COUNTS,FP_SCROLL,(nx_rect){x,y,w,counts_h},p->labels[9],counts,0,NX_ROLE_TEXT));y+=counts_h+4;
    nx_semantic_desc list={0};list.id=FP_LIST;list.parent_id=FP_SCROLL;list.role=NX_ROLE_LIST;list.name=span(word(p,"Entries on this page","Einträge dieser Seite"));list.bounds=(nx_rect){x,y,w,rows_height};TRY(nx_semantics_add(ctx,&list,NULL));
    for(size_t i=first;i<end;++i) {
        const fp_row *row=&r->rows[i];float h=height(p,p->rows[i-first]);nx_action_desc a={0};a.variant=NX_ACTION_TILE;a.selected=p->selected==FP_ROWS+row->ordinal;
        a.control.id=FP_ROWS+row->ordinal;a.control.parent_id=FP_LIST;a.control.name=span(row->entry.label);a.control.bounds=(nx_rect){x,y,w,h};
        a.control.disabled=p->busy;a.control.label_layout=p->rows[i-first];a.control.label_origin=(nx_point){x+(row->entry.kind==AE_FILES_DIRECTORY?36:12),y+8};a.control.glyph_cache_bytes=1024u*1024u;
        if(row->entry.kind==AE_FILES_DIRECTORY){a.icon=NX_ICON_FOLDER;a.icon_bounds=(nx_rect){x+8,y+10,20,20};}
        nx_interaction_state s;TRY(nx_action(ctx,&a,&s));if(s.clicked){p->selected=a.control.id;*redraw=1;}y+=h+4;
    }
    TRY(nx_scroll_area_end(ctx));
    int open=selected_row(p)!=NULL;
    const fp_row *chosen=selected_row(p);if(chosen&&chosen->entry.kind!=AE_FILES_DIRECTORY&&chosen->entry.kind!=AE_FILES_REGULAR)open=0;
    int stacked=width<480;float open_height=stacked?height(p,p->labels[10]):footer;
    TRY(button(p,ctx,FP_OPEN,1,(nx_rect){16,window_height-footer-12,stacked?width-32:(width-40)/2,open_height},p->labels[10],word(p,"Open","Öffnen"),p->busy||!open,&clicked));if(clicked)open_selected(p,intent);
    TRY(button(p,ctx,FP_CANCEL,1,(nx_rect){stacked?16:24+(width-40)/2,stacked?window_height-footer-12+open_height+8:window_height-footer-12,stacked?width-32:(width-40)/2,stacked?height(p,p->labels[11]):footer},p->labels[11],word(p,"Cancel","Abbrechen"),0,&clicked));if(clicked)intent->cancel=1;
    if(p->restore) {if(nx_focus_set(ctx,p->restore)==NX_OK){p->restore=0;*redraw=1;}}
    return NX_OK;
}
