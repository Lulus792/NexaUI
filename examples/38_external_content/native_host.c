#define _GNU_SOURCE
#include "native_worker.h"
#include <nexaui/software.h>
#include <limits.h>
#include <stdio.h>
#include "native_input.h"
#ifdef CONTENT_NATIVE_GUEST
#include "native_guest_input.h"
#endif
#include "pdf_selection.h"
#include "content_camera.h"
#ifdef CONTENT_NATIVE_BROWSER
#include "native_browser.h"
#include "native_browser_ax.h"
#endif

enum input_phase { INPUT_IDLE, INPUT_PENDING, INPUT_ACTIVE, INPUT_RELEASED, INPUT_FOCUS_LOST,
                   INPUT_DISCONNECTED, INPUT_DISCONNECTING };
static const char *input_name(enum input_phase phase) {
    static const char *names[]={"Scaled frame ready.","Release keys to enter content.",
        "Input active: Ctrl+Shift+F12 exits.","Input released.","Input focus lost.","Input disconnected; pixels retained."};
    return names[(unsigned)phase];
}
static const char *guest_input_name(enum input_phase phase) {
    static const char *names[]={"Guest running. Keyboard and three-button mouse.","Release keys and buttons to enter guest.",
        "Guest input active: Ctrl+Shift+F12 exits.","Guest input released.","Guest input focus lost.",
        "Console disconnected. Guest retained.","Disconnecting console. Releasing guest input."};
    return names[(unsigned)phase];
}
enum phase { PREPARING, WAITING, REPLACED, RELEASED, OLD_REJECTED, FRESH_PENDING, READY,
             BAD_PENDING, BAD_REJECTED, RECOVERY_PENDING, RECOVERED, SCALED_WAIT, SCALED_PENDING,
             SCALED_READY, FAILED, PDF_PAGE1_SCALED, PDF_PAGE_PENDING, PDF_HIT_READY, PDF_HIT_FAILED };
static int pdf_hit_page(enum phase phase) {
    return phase==PDF_HIT_READY || phase==PDF_HIT_FAILED;
}
static int pdf_can_navigate(enum phase phase) {
    return phase==READY || phase==BAD_REJECTED || phase==RECOVERED ||
        phase==SCALED_READY || phase==PDF_PAGE1_SCALED || pdf_hit_page(phase);
}

static const char *phase_name(enum phase phase) {
    static const char *names[] = {"Preparing old frame.","Waiting for frame.","Replacement waiting.",
        "Old frame completing.","Old frame rejected.","Fresh frame pending.","Frame ready.",
        "Bad frame pending.","Frame format unavailable.","Recovery frame pending.","Frame recovered.",
        "Waiting for scaled frame.","Scaled frame pending.","Scaled frame ready.","Producer unavailable.",
        "PDF page 1 of 2 at 2x.","Rendering PDF page.","PDF page ready.","PDF rendering failed; page retained."};
    return names[(unsigned)phase];
}
static uint8_t *read_font(const char *path, size_t *bytes) {
    FILE *file = fopen(path, "rb"); if (!file) return NULL;
    uint8_t *data = NULL;
    if (!fseek(file, 0, SEEK_END)) {
        long length = ftell(file);
        if (length > 0 && length <= 16 * 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
            data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
            if (data) *bytes = (size_t)length;
        }
    }
    fclose(file); return data;
}
static nx_result scoped_label(nx_context *context, const nx_font_collection *fonts,
    uint64_t id, uint64_t parent, const char *text, nx_rect bounds, double em,nx_color color) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, text, strlen(text), NX_BIDI_AUTO, em, bounds.width, &layout);
    if (result == NX_OK) {
        nx_point origin = {bounds.x,bounds.y};
        result = id ? nx_label(context,id,parent,bounds,layout,origin,color,65536) :
            nx_draw_text_layout(context,layout,origin,color,65536);
    }
    nx_text_layout_destroy(layout); return result;
}
static nx_result label(nx_context *context,const nx_font_collection *fonts,
    uint64_t id,const char *text,nx_rect bounds,double em) {
    return scoped_label(context,fonts,id,1,text,bounds,em,(nx_color){0.05f,0.07f,0.1f,1});
}
static nx_result scoped_button(nx_context *context, const nx_font_collection *fonts, uint64_t id, uint64_t parent,
    const char *text, const char *caption, float x, float y, float width, int enabled, unsigned *action) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, caption, strlen(caption), NX_BIDI_AUTO, 14, width - 12, &layout);
    if (result == NX_OK) {
        nx_control_desc desc = {.id=id,.parent_id=parent,.bounds={x,y,width,32},.name={text,strlen(text)},
            .label_layout=layout,.label_origin={x+6,y+8},.glyph_cache_bytes=65536,.disabled=!enabled};
        nx_interaction_state state;
        result = nx_button(context, &desc, &state);
        if (result == NX_OK && state.clicked && enabled) *action = (unsigned)id;
    }
    nx_text_layout_destroy(layout); return result;
}
static nx_result button(nx_context *context, const nx_font_collection *fonts, uint64_t id,
    const char *text, float x, float y, float width, int enabled, unsigned *action) {
    return scoped_button(context,fonts,id,1,text,text,x,y,width,enabled,action);
}
static nx_result input_hint(nx_context *context,const nx_font_collection *fonts,int guest_case) {
    const char *hint=guest_case?"Host sharing unavailable: clipboard, files, network, microphone, devices.":
        "Physical input fixture; exit: Ctrl+Shift+F12";
    return label(context,fonts,3,hint,(nx_rect){16,122,448,14},11);
}
enum { GUEST_INSPECT=610, GUEST_INSPECT_PANEL, GUEST_INSPECT_BACKDROP, GUEST_INSPECT_CLOSE,
       GUEST_INSPECT_ROW };
typedef struct guest_inspector {
    uint64_t ticket;
    int enabled, open, pending;
    native_guest_report report;
} guest_inspector;

static nx_result guest_resources_overlay(nx_context *context,const nx_font_collection *fonts,
    nx_window_info info,const guest_inspector *view,unsigned *action) {
    static const char title[]="Guest resources";
    float width=info.width<448?info.width-32:416;
    nx_rect panel={(info.width-width)/2,44,width,232};
    nx_overlay_desc overlay={.panel={.id=GUEST_INSPECT_PANEL,.parent_id=1,.bounds=panel,
        .name={title,sizeof(title)-1u}},.backdrop_id=GUEST_INSPECT_BACKDROP,.dismiss_on_escape=1};
    int dismissed;
    nx_result result=nx_modal_begin(context,&overlay,&dismissed);
    if (result!=NX_OK) return result;
    nx_color ink;
    result=nx_theme_color(context,NX_COLOR_TEXT,&ink);
    float x=panel.x+16, y=panel.y+12;
    if (result==NX_OK) result=scoped_label(context,fonts,0,GUEST_INSPECT_PANEL,title,
        (nx_rect){x,y,width-128,24},17,ink);
    if (result==NX_OK) result=scoped_button(context,fonts,GUEST_INSPECT_CLOSE,GUEST_INSPECT_PANEL,
        "Close resources","Close",panel.x+width-86,y-4,70,1,action);
    char lines[5][96];
    unsigned count=1;
    if (view->report.ticket!=view->ticket) {
        strcpy(lines[0],"Reading guest resources");
    } else if (view->report.result!=NX_OK) {
        strcpy(lines[0],"Guest resources unavailable");
    } else {
        const content_guest_resources *r=&view->report.resources;
        snprintf(lines[0],sizeof(lines[0]),"Guest %s",r->state==GUEST_RESOURCE_RUNNING?"running":"paused");
        snprintf(lines[1],sizeof(lines[1]),"%u vCPU%s, %llu MiB RAM assigned",r->cpus,r->cpus==1?"":"s",
            (unsigned long long)(r->ram/(1024u*1024u)));
        count=3;
        if (r->flags&GUEST_RESOURCE_DISK) {
            snprintf(lines[2],sizeof(lines[2]),"Work disk: %llu MiB, %s",
                (unsigned long long)(r->capacity/(1024u*1024u)),
                r->flags&GUEST_RESOURCE_WRITABLE?"read/write":"read-only");
            snprintf(lines[3],sizeof(lines[3]),"Device read %llu bytes",(unsigned long long)r->read);
            snprintf(lines[4],sizeof(lines[4]),"Device wrote %llu bytes",(unsigned long long)r->written);
            count=5;
        } else strcpy(lines[2],"No persistent disk attached");
    }
    for (unsigned i=0;result==NX_OK&&i<count;++i)
        result=scoped_label(context,fonts,GUEST_INSPECT_ROW+i,GUEST_INSPECT_PANEL,lines[i],
            (nx_rect){x,y+42+24*i,width-32,20},14,ink);
    nx_result ended=nx_overlay_end(context);
    if (dismissed) *action=GUEST_INSPECT_CLOSE;
    return result==NX_OK?ended:result;
}
typedef struct pdf_text_view {
    uint64_t generation, resize, sequence, identity;
    int selectable;
} pdf_text_view;
enum { PDF_SELECT_BUTTON=36, PDF_TEXT_NODE=101 };

static nx_result pdf_text_semantics(nx_context *context, content_surface *surface,
    const nx_viewport_desc *viewport, pdf_text_view *text_view, pdf_selection *selection,
    nx_window *window, int enabled, content_camera_view view, int *redraw) {
    content_surface_status status;
    nx_result result = content_surface_get_status(surface, &status);
    if (result != NX_OK) return result;
    if (text_view->generation != status.request.generation || text_view->resize != status.request.resize ||
        text_view->sequence != status.sequence || text_view->selectable!=selection->active) {
        if (text_view->identity == UINT64_MAX) return NX_ERROR_LIMIT;
        *text_view = (pdf_text_view){status.request.generation,status.request.resize,status.sequence,
            text_view->identity+1,selection->active};
    }
    nx_utf8_span text;
    result = content_surface_get_text(surface, &text);
    if (result != NX_OK || !text.length) return result;
    nx_semantic_desc node = {.id=PDF_TEXT_NODE, .parent_id=viewport->id, .role=NX_ROLE_TEXT, .bounds=viewport->bounds,
        .name={"PDF page text",13}, .value=text, .state=NX_SEM_READONLY};
    const content_frame_word *words;
    size_t word_count;
    result = content_surface_get_words(surface, &words, &word_count);
    if (result != NX_OK) return result;
    nx_text_document_box boxes[CONTENT_FRAME_WORD_LIMIT];
    size_t visible=0;
    for (size_t i=0;i<word_count;++i) {
        nx_rect bounds;
        if (content_camera_visible(view,words[i].bounds,&bounds))
            boxes[visible++]=(nx_text_document_box){words[i].start_byte,words[i].end_byte,bounds};
    }
    if (selection->active) return pdf_selection_record(selection,context,window,node,text_view->identity,
        enabled,text,boxes,visible,redraw);
    result = nx_semantics_add_readonly_text_snapshot_without_caret(context,&node,text_view->identity,NULL);
    if (result != NX_OK || !visible) return result;
    nx_text_document_presentation presentation = {.boxes=boxes, .box_count=visible};
    return nx_text_document_set_presentation(context, node.id, text_view->identity, 0, &presentation);
}

static nx_result pdf_link_widgets(nx_context *context, content_surface *surface,
    const nx_viewport_desc *viewport, uint64_t frame_identity, int enabled, content_camera_view view, unsigned *destination,
    char address[CONTENT_PDF_URI_LIMIT+1u]) {
    const content_frame_link *links;
    size_t count;
    nx_result result = content_surface_get_links(surface,&links,&count);
    if (result != NX_OK || !count) return result;
    if (frame_identity >= (UINT64_MAX-1024u)/CONTENT_PDF_LINK_LIMIT) return NX_ERROR_LIMIT;
    uint64_t first_id=1024u+frame_identity*CONTENT_PDF_LINK_LIMIT;
    for (size_t i=0;i<count;++i) {
        char title[64];
        if (links[i].target_page) snprintf(title,sizeof(title),"PDF link to page %u",links[i].target_page);
        else snprintf(title,sizeof(title),"External PDF link");
        nx_rect bounds;
        if (!content_camera_visible(view,links[i].bounds,&bounds)) continue;
        nx_widget_desc widget = {
            .semantic={.id=first_id+i,.parent_id=viewport->id,.role=NX_ROLE_BUTTON,
                .bounds=bounds,.name={title,strlen(title)},.state=enabled?0:NX_SEM_DISABLED,
                .description={links[i].uri,strlen(links[i].uri)},
                .actions=NX_ACTION_FOCUS|NX_ACTION_PRESS},
            .flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
        nx_interaction_state state;
        result=nx_widget_register(context,&widget,&state);
        if (result != NX_OK) return result;
        if (enabled && state.clicked) {
            *destination=links[i].target_page;
            memcpy(address,links[i].uri,sizeof(links[i].uri));
        }
        if (enabled && (state.focused || state.hovered)) {
            result=nx_stroke_rect(context,bounds,2,(nx_color){0,0,0,1});
            if (result==NX_OK) result=nx_stroke_rect(context,bounds,1,(nx_color){1,1,1,1});
            if (result != NX_OK) return result;
        }
    }
    return NX_OK;
}

enum { PDF_URL_DIALOG=40, PDF_URL_BACKDROP, PDF_URL_ADDRESS, PDF_URL_STATUS,
    PDF_URL_OPEN, PDF_URL_COPY, PDF_URL_CLOSE };
typedef struct pdf_url_view {
    uint64_t identity, ticket;
    char uri[CONTENT_PDF_URI_LIMIT+1u];
    const char *status;
} pdf_url_view;

static nx_result pdf_url_dialog(nx_context *context, const nx_font_collection *fonts,
    float width, float height, const pdf_url_view *preview, int can_open, unsigned *action) {
    if (width<320 || height<128) { *action=PDF_URL_CLOSE; return NX_OK; }
    float w=width<456?width-24:432;
    size_t length=strlen(preview->uri);
    nx_text_layout *url=NULL;
    nx_result result=nx_text_layout_create(fonts,preview->uri,length,NX_BIDI_LTR,11,w-32,&url);
    if (result!=NX_OK) return result;
    if (nx_text_layout_width(url)>w-32) {
        /* Force wraps for unbreakable runs. Keep percent escapes together. */
        char wrapped[CONTENT_PDF_URI_LIMIT+16u];
        size_t columns=(size_t)((w-32)/10), bytes=0, offset=0;
        while (offset<length) {
            size_t end=offset+columns;
            if (end<length) {
                if (preview->uri[end-1u]=='%') --end;
                else if (preview->uri[end-2u]=='%') end-=2u;
            } else end=length;
            if (offset) wrapped[bytes++]='\n';
            memcpy(wrapped+bytes,preview->uri+offset,end-offset);
            bytes+=end-offset;
            offset=end;
        }
        nx_text_layout_destroy(url);
        url=NULL;
        result=nx_text_layout_create(fonts,wrapped,bytes,NX_BIDI_LTR,11,w-32,&url);
        if (result!=NX_OK) return result;
    }
    float address_height=(float)nx_text_layout_height(url);
    float panel_height=address_height+101;
    if (panel_height>height-16 || nx_text_layout_width(url)>w-32) {
        nx_text_layout_destroy(url);
        *action=PDF_URL_CLOSE;
        return NX_OK;
    }
    nx_rect panel={(width-w)/2,(height-panel_height)/2,w,panel_height};
    float x=panel.x+16,y=panel.y;
    static const char title[]="External link address";
    nx_overlay_desc overlay={.panel={.id=PDF_URL_DIALOG,.parent_id=1,.bounds=panel,
        .name={title,sizeof(title)-1u}},.backdrop_id=PDF_URL_BACKDROP,.dismiss_on_escape=1};
    int dismissed=0;
    result=nx_modal_begin(context,&overlay,&dismissed);
    if (result!=NX_OK) { nx_text_layout_destroy(url);return result; }
    result=nx_fill_round_rect(context,panel,8,(nx_color){.97f,.98f,1,1});
    if (result==NX_OK) result=label(context,fonts,0,title,(nx_rect){x,y+12,w-32,22},16);
    nx_rect address={x,y+36,w-32,address_height};
    nx_semantic_desc text={.id=PDF_URL_ADDRESS,.parent_id=PDF_URL_DIALOG,.role=NX_ROLE_TEXT,
        .bounds=address,.name={preview->uri,length},.state=NX_SEM_READONLY};
    if (result==NX_OK) result=nx_semantics_add(context,&text,NULL);
    if (result==NX_OK) result=nx_draw_text_layout(context,url,(nx_point){address.x,address.y},
        (nx_color){.05f,.07f,.1f,1},65536);
    nx_text_layout_destroy(url);
    if (result==NX_OK && preview->status) {
        nx_rect bounds={x,y+address_height+42,w-32,16};
        nx_semantic_desc status={.id=PDF_URL_STATUS,.parent_id=PDF_URL_DIALOG,.role=NX_ROLE_STATUS,
            .bounds=bounds,.name={preview->status,strlen(preview->status)}};
        result=nx_semantics_add(context,&status,NULL);
        if (result==NX_OK) result=label(context,fonts,0,preview->status,bounds,12);
    }
    float step=(w-32)/3, buttons_y=y+address_height+59;
    if (result==NX_OK) result=scoped_button(context,fonts,PDF_URL_COPY,PDF_URL_DIALOG,
        "Copy address","Copy address",x,buttons_y,step-4,1,action);
    if (result==NX_OK) result=scoped_button(context,fonts,PDF_URL_OPEN,PDF_URL_DIALOG,
        "Open link","Open link",x+step,buttons_y,step-4,can_open&&!preview->ticket,action);
    if (result==NX_OK) result=scoped_button(context,fonts,PDF_URL_CLOSE,PDF_URL_DIALOG,
        "Close preview","Close preview",x+2*step,buttons_y,step,1,action);
    nx_result ended=nx_overlay_end(context);
    if (dismissed) *action=PDF_URL_CLOSE;
    return result==NX_OK?ended:result;
}

enum { PDF_FIND_FIELD = 32, PDF_NEXT_HIT = 34 };
enum { PDF_PAGES=53, PDF_PAGES_DIALOG, PDF_PAGES_BACKDROP, PDF_PAGES_CLOSE, PDF_PAGES_PREVIOUS,
    PDF_PAGES_NEXT, PDF_PAGES_STATUS };
enum { PDF_PROPERTIES=60, PDF_PROPERTIES_DIALOG, PDF_PROPERTIES_BACKDROP, PDF_PROPERTIES_CLOSE,
    PDF_PROPERTIES_DISMISSED, PDF_PROPERTIES_SCROLL, PDF_PROPERTIES_SCROLL_X, PDF_PROPERTIES_SCROLL_Y,
    PDF_PROPERTIES_TEXT, PDF_PROPERTIES_STATUS };
enum { PDF_EXPORT=70, PDF_EXPORT_STATUS };
typedef struct pdf_properties_view {
    native_pdf_properties_result available;
    uint64_t session;
    float x, y;
    int enabled, open;
} pdf_properties_view;
typedef struct pdf_document_view {
    content_pdf_index *index;
    png_gallery *gallery;
    content_thumbnails thumbnails;
    pdf_detail detail;
    pdf_properties_view properties;
    const char *export_status;
    uint64_t export_ticket;
    int export_enabled;
    unsigned page, pages, counted_page;
    int index_finished;
    content_pdf_hits hits;
    size_t counted_length;
    char counted_query[CONTENT_PDF_QUERY_LIMIT];
} pdf_document_view;

#ifdef CONTENT_NATIVE_PNG
static nx_result pdf_dialog_text(nx_context *context, const nx_font_collection *fonts,
    const char *text, nx_rect bounds, double em, nx_color color) {
    nx_text_layout *layout=NULL;
    nx_result result=nx_text_layout_create(fonts,text,strlen(text),NX_BIDI_AUTO,em,bounds.width,&layout);
    if (result==NX_OK) result=nx_draw_text_layout(context,layout,(nx_point){bounds.x,bounds.y},color,65536);
    nx_text_layout_destroy(layout);
    return result;
}
#endif

#ifdef CONTENT_NATIVE_PDF
static nx_result pdf_properties_rows(nx_context *context, const nx_font_collection *fonts,
    pdf_properties_view *view, nx_rect bounds, nx_color ink) {
    static const char *labels[]={"Title","Author","Subject","Pages","PDF version"};
    const pdf_properties_packet *packet=&view->available.packet;
    char pages[16], version[16];
    snprintf(pages,sizeof(pages),"%u",packet->page_count);
    snprintf(version,sizeof(version),"%u.%u",packet->major,packet->minor);
    const char *values[]={packet->fields[CONTENT_PDF_TITLE],packet->fields[CONTENT_PDF_AUTHOR],
        packet->fields[CONTENT_PDF_SUBJECT],pages,version};
    enum { rows=sizeof(labels)/sizeof(*labels), label_width=108, padding=8 };
    nx_text_layout *layouts[rows]={0};
    char text[CONTENT_PDF_PROPERTY_COUNT*(CONTENT_PDF_PROPERTY_LIMIT+1u)+128u];
    size_t length=0;
    float width=bounds.width-14, height=2*padding;
    nx_result result=NX_OK;
    for (unsigned i=0;result==NX_OK && i<rows;++i) {
        if (!*values[i]) values[i]=packet->unavailable&(1u<<i)?"Value unavailable":"Not specified";
        int n=snprintf(text+length,sizeof(text)-length,"%s\n%s\n",labels[i],values[i]);
        if (n<0 || (size_t)n>=sizeof(text)-length) { result=NX_ERROR_LIMIT;break; }
        length+=(size_t)n;
        result=nx_text_layout_create(fonts,values[i],strlen(values[i]),NX_BIDI_AUTO,14,
            bounds.width-label_width-16-2*padding,&layouts[i]);
        if (result==NX_OK) {
            float row_width=label_width+(float)nx_text_layout_width(layouts[i])+2*padding;
            if (row_width>width) width=row_width;
            height+=(float)nx_text_layout_height(layouts[i])+9;
        }
    }
    nx_scroll_area_desc scroll={.id=PDF_PROPERTIES_SCROLL,.parent_id=PDF_PROPERTIES_DIALOG,
        .thumb_x_id=PDF_PROPERTIES_SCROLL_X,.thumb_y_id=PDF_PROPERTIES_SCROLL_Y,
        .viewport=bounds,.name={"Document property values",24},.content_size={width,height},
        .axes=NX_SCROLL_X|NX_SCROLL_Y};
    nx_scroll_info info;
    if (result==NX_OK) result=nx_scroll_area_begin(context,&scroll,&view->x,&view->y,&info);
    if (result==NX_OK) {
        nx_semantic_desc semantic={.id=PDF_PROPERTIES_TEXT,.parent_id=PDF_PROPERTIES_SCROLL,.role=NX_ROLE_TEXT,
            .bounds=bounds,.name={"PDF properties text",19},.value={text,length},.state=NX_SEM_READONLY};
        result=nx_semantics_add_readonly_text_snapshot_without_caret(context,&semantic,view->session,NULL);
        float x=bounds.x+padding-view->x, y=bounds.y+padding-view->y;
        for (unsigned i=0;result==NX_OK && i<rows;++i) {
            result=pdf_dialog_text(context,fonts,labels[i],(nx_rect){x,y,label_width,20},13,ink);
            if (result==NX_OK) result=nx_draw_text_layout(context,layouts[i],(nx_point){x+label_width,y},ink,65536);
            y+=(float)nx_text_layout_height(layouts[i])+9;
        }
        nx_result ended=nx_scroll_area_end(context);
        if (result==NX_OK) result=ended;
    }
    for (unsigned i=0;i<rows;++i) nx_text_layout_destroy(layouts[i]);
    return result;
}

static nx_result pdf_properties_dialog(nx_context *context, const nx_font_collection *fonts,
    pdf_properties_view *view, nx_rect panel, unsigned *action) {
    static const char title[]="Document properties";
    nx_overlay_desc overlay={.panel={.id=PDF_PROPERTIES_DIALOG,.parent_id=PDF_PAGES_DIALOG,.bounds=panel,
        .name={title,sizeof(title)-1u}},.backdrop_id=PDF_PROPERTIES_BACKDROP,.dismiss_on_escape=1};
    int dismissed;
    nx_result result=nx_modal_begin(context,&overlay,&dismissed);
    if (result!=NX_OK) return result;
    nx_color ink;
    result=nx_theme_color(context,NX_COLOR_TEXT,&ink);
    float x=panel.x+16, y=panel.y;
    if (result==NX_OK) result=pdf_dialog_text(context,fonts,title,(nx_rect){x,y+12,300,26},17,ink);
    if (result==NX_OK) result=scoped_button(context,fonts,PDF_PROPERTIES_CLOSE,PDF_PROPERTIES_DIALOG,
        "Close properties","Close",x+314,y+8,70,1,action);
    nx_rect bounds={x,y+54,384,146};
    if (result==NX_OK && view->available.generation && view->available.result==NX_OK)
        result=pdf_properties_rows(context,fonts,view,bounds,ink);
    else if (result==NX_OK) {
        const char *status=view->available.generation?"Document properties unavailable":"Reading document properties";
        nx_semantic_desc semantic={.id=PDF_PROPERTIES_STATUS,.parent_id=PDF_PROPERTIES_DIALOG,.role=NX_ROLE_STATUS,
            .bounds=bounds,.name={status,strlen(status)}};
        result=nx_semantics_add(context,&semantic,NULL);
        if (result==NX_OK) result=pdf_dialog_text(context,fonts,status,bounds,14,ink);
    }
    nx_result ended=nx_overlay_end(context);
    if (dismissed) *action=PDF_PROPERTIES_DISMISSED;
    return result==NX_OK?ended:result;
}
#endif

#ifdef CONTENT_NATIVE_PNG
static nx_result thumbnail_card(nx_context *context, const nx_font_collection *fonts,
    const content_thumbnails *cache, const png_gallery *gallery, unsigned i, unsigned current,
    nx_rect bounds, unsigned *linked_page) {
    const content_thumbnail *cell=&cache->cells[i];
    unsigned page=cache->first+i;
    char page_name[24];
    const char *name=png_gallery_name(gallery,page);
    if (!name) { snprintf(page_name,sizeof(page_name),"Page %u",page);name=page_name; }
    const char *status=cell->frame?"Preview ready":cell->page?"Preview unavailable":"Loading preview";
    nx_widget_desc widget={.semantic={.id=(UINT64_C(1)<<48)+cache->session*CONTENT_PDF_PAGE_LIMIT+page,
        .parent_id=PDF_PAGES_DIALOG,.role=NX_ROLE_BUTTON,.bounds=bounds,.name={name,strlen(name)},
        .description={status,strlen(status)},.state=page==current?NX_SEM_SELECTED:0,
        .actions=NX_ACTION_FOCUS|NX_ACTION_PRESS},.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
    nx_interaction_state state;
    nx_result result=nx_widget_register(context,&widget,&state);
    if (result!=NX_OK) return result;
    if (state.clicked) *linked_page=page;
    nx_color fill, border, text;
    result=nx_theme_color(context,state.hovered?NX_COLOR_HOVER:NX_COLOR_SURFACE_RAISED,&fill);
    if (result==NX_OK) result=nx_theme_color(context,state.focused?NX_COLOR_FOCUS:
        page==current?NX_COLOR_ACCENT:NX_COLOR_BORDER,&border);
    if (result==NX_OK) result=nx_theme_color(context,NX_COLOR_TEXT,&text);
    if (result==NX_OK) result=nx_fill_round_rect(context,bounds,5,fill);
    nx_rect image={bounds.x+(bounds.width-CONTENT_THUMBNAIL_WIDTH)/2,bounds.y+8,CONTENT_THUMBNAIL_WIDTH,CONTENT_THUMBNAIL_HEIGHT};
    if (result==NX_OK && cell->frame)
        result=nx_draw_bitmap(context,image,content_thumbnail_pixels(cell),(nx_color){1,1,1,1});
    if (result==NX_OK && !cell->frame)
        result=pdf_dialog_text(context,fonts,status,(nx_rect){image.x,image.y+30,image.width,18},11,text);
    if (result==NX_OK) {
        nx_rect caption={image.x,bounds.y+bounds.height-26,image.width,20};
        result=nx_push_clip(context,caption);
        if (result==NX_OK) {
            result=pdf_dialog_text(context,fonts,name,caption,14,text);
            nx_result popped=nx_pop_clip(context);
            if (result==NX_OK) result=popped;
        }
    }
    if (result==NX_OK) result=nx_stroke_rect(context,bounds,state.focused||page==current?2:1,border);
    return result;
}

static nx_result content_pages_dialog(nx_context *context, const nx_font_collection *fonts,
    nx_window_info info, pdf_document_view *document, unsigned *action, unsigned *linked_page) {
    if (info.width<480 || info.height<248) { *action=PDF_PAGES_CLOSE;return NX_OK; }
    const content_thumbnails *cache=&document->thumbnails;
    unsigned total=document->pages;
    nx_rect panel={(info.width-416)/2,(info.height-216)/2,416,216};
    const char *title=document->gallery?"Image gallery":"PDF pages";
    nx_overlay_desc overlay={.panel={.id=PDF_PAGES_DIALOG,.parent_id=1,.bounds=panel,
        .name={title,strlen(title)}},.backdrop_id=PDF_PAGES_BACKDROP,.dismiss_on_escape=1};
    int dismissed;
    nx_result result=nx_modal_begin(context,&overlay,&dismissed);
    if (result!=NX_OK) return result;
    nx_color text;
    result=nx_theme_color(context,NX_COLOR_TEXT,&text);
    float x=panel.x+16, y=panel.y;
    if (result==NX_OK) result=pdf_dialog_text(context,fonts,title,(nx_rect){x,y+12,document->export_enabled?90:190,24},17,text);
    if (result==NX_OK && document->export_enabled) result=scoped_button(context,fonts,PDF_EXPORT,PDF_PAGES_DIALOG,
        "Export PNG","Export PNG",x+96,y+8,98,!document->export_ticket,action);
    if (result==NX_OK && document->properties.enabled) result=scoped_button(context,fonts,PDF_PROPERTIES,PDF_PAGES_DIALOG,
        "Properties","Properties",x+200,y+8,104,1,action);
    if (result==NX_OK) result=scoped_button(context,fonts,PDF_PAGES_CLOSE,PDF_PAGES_DIALOG,
        document->gallery?"Close gallery":"Close pages","Close",x+314,y+8,70,1,action);
    float step=392.0f/CONTENT_THUMBNAIL_COUNT;
    for (unsigned i=0;result==NX_OK && i<CONTENT_THUMBNAIL_COUNT && cache->first+i<=total;++i)
        result=thumbnail_card(context,fonts,cache,document->gallery,i,document->page,
            (nx_rect){x+i*step,y+48,step-8,120},linked_page);
    if (result==NX_OK && !total) {
        const char *message=document->index_finished?"Page previews unavailable":"Indexing document";
        nx_rect area={x,y+64,384,24};
        nx_semantic_desc semantic={.id=PDF_PAGES_STATUS,.parent_id=PDF_PAGES_DIALOG,.role=NX_ROLE_STATUS,.bounds=area,
            .name={message,strlen(message)}};
        result=nx_semantics_add(context,&semantic,NULL);
        if (result==NX_OK) result=pdf_dialog_text(context,fonts,message,area,14,text);
    }
    if (result==NX_OK && total) result=scoped_button(context,fonts,PDF_PAGES_PREVIOUS,PDF_PAGES_DIALOG,
        "Previous previews","Previous",x,y+174,94,cache->first>1,action);
    if (result==NX_OK && total) result=scoped_button(context,fonts,PDF_PAGES_NEXT,PDF_PAGES_DIALOG,
        "Next previews","Next",x+290,y+174,94,cache->first+CONTENT_THUMBNAIL_COUNT<=total,action);
    char range[32];
    unsigned last=cache->first+CONTENT_THUMBNAIL_COUNT-1u;
    if (last>total) last=total;
    snprintf(range,sizeof(range),"%u–%u of %u",cache->first,last,total);
    if (result==NX_OK && document->export_status) {
        const char *message=document->export_status;
        nx_rect area={x+106,y+181,178,24};
        nx_semantic_desc semantic={.id=PDF_EXPORT_STATUS,.parent_id=PDF_PAGES_DIALOG,.role=NX_ROLE_STATUS,.bounds=area,
            .name={message,strlen(message)}};
        result=nx_semantics_add(context,&semantic,NULL);
        if (result==NX_OK) result=pdf_dialog_text(context,fonts,message,area,14,text);
    } else if (result==NX_OK && total) result=pdf_dialog_text(context,fonts,range,(nx_rect){x+151,y+181,138,24},14,text);
#ifdef CONTENT_NATIVE_PDF
    if (result==NX_OK && !dismissed && document->properties.open)
        result=pdf_properties_dialog(context,fonts,&document->properties,panel,action);
#endif
    nx_result ended=nx_overlay_end(context);
    if (dismissed) { *action=PDF_PAGES_CLOSE; *linked_page=0; }
    return result==NX_OK?ended:result;
}
#endif

static int find_visible(const nx_text_field *field, enum phase phase, float width) {
    return field && phase >= READY && phase != FAILED && width >= 256;
}

static int find_query_valid(nx_utf8_span text, void *user) {
    (void)user;
    return text.length <= CONTENT_PDF_QUERY_LIMIT;
}

static nx_result pdf_find_shortcut(nx_context *context, int leased, int *find_pressed, int *redraw) {
    const nx_event *events;
    size_t count;
    nx_result result = nx_frame_events(context,&events,&count);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        const nx_event *event = &events[i];
        if (event->type == NX_EVENT_FOCUS && !event->focused) *find_pressed = 0;
        /* Activate the field after F is released. Activating a v1 input
           method mid-chord can deliver the old release to the new editor. */
        if (event->type == NX_EVENT_KEY_DOWN && event->key == NX_KEY_F &&
            event->modifiers == NX_MOD_CONTROL && nx_focus_id(context)!=PDF_FIND_FIELD)
            *find_pressed = 1;
        if (event->type == NX_EVENT_KEY_UP && event->key == NX_KEY_F && *find_pressed) {
            *find_pressed = 0;
            *redraw = 1;
            return nx_focus_set(context,PDF_FIND_FIELD);
        }
        if (event->type == NX_EVENT_KEY_DOWN && event->key == NX_KEY_ESCAPE &&
            nx_focus_id(context)==PDF_FIND_FIELD) {
            *redraw = 1;
            return nx_focus_set(context,leased?100u:15u);
        }
    }
    return NX_OK;
}

static nx_result pdf_find_field(nx_context *context, nx_window *window,
    const nx_font_collection *fonts, nx_text_field *field) {
    nx_text_field_desc desc = {.id=PDF_FIND_FIELD, .parent_id=1, .bounds={104,48,144,32},
        .name={"Find in PDF",11}, .fonts=fonts, .pixels_per_em=14, .glyph_cache_bytes=65536,
        .validate=find_query_valid, .ime_window=window};
    nx_result result = nx_text_clipboard_for_window(window,&desc.clipboard);
    if (result != NX_OK) return result;
    result = label(context,fonts,0,"Find in PDF",(nx_rect){16,57,88,18},13);
    if (result != NX_OK) return result;
    nx_interaction_state interaction;
    int valid, submitted;
    static const char description[] = "Case-sensitive search. Next hit moves to another matching page.";
    return nx_text_field_widget_described(context,&desc,field,
        (nx_utf8_span){description,sizeof(description)-1u},&interaction,&valid,&submitted);
}

static nx_result pdf_status_label(nx_context *context, const nx_font_collection *fonts,
    uint64_t id, nx_rect bounds, const char *message) {
    nx_semantic_desc status = {.id=id,.parent_id=1,.role=NX_ROLE_STATUS,.bounds=bounds,
        .name={message,strlen(message)}};
    nx_result result = nx_semantics_add(context,&status,NULL);
    return result == NX_OK ? label(context,fonts,0,message,bounds,11) : result;
}

#ifdef CONTENT_NATIVE_PDF
static nx_result pdf_document_find(nx_context *context, const nx_font_collection *fonts,
    const nx_text_field *field, pdf_document_view *document, float width) {
    nx_utf8_span query;
    nx_result result = nx_text_field_text(field,&query);
    if (result != NX_OK) return result;
    if (!query.length || query.length > CONTENT_PDF_QUERY_LIMIT) {
        document->counted_length = 0;
        document->hits = (content_pdf_hits){0};
        return NX_OK;
    }
    const char *message = document->index_finished ? "Document search unavailable" : "Indexing document";
    char summary[64];
    if (document->index && document->page && document->page <= document->pages) {
        if (document->counted_page != document->page || document->counted_length != query.length ||
            memcmp(document->counted_query,query.data,query.length)) {
            result = content_pdf_index_find(document->index,query,document->page,&document->hits);
            if (result != NX_OK) return result;
            document->counted_page = document->page;
            document->counted_length = query.length;
            memcpy(document->counted_query,query.data,query.length);
        }
        snprintf(summary,sizeof(summary),"%zu %s in document",document->hits.total,
            document->hits.total==1?"match":"matches");
        message = summary;
    } else document->hits = (content_pdf_hits){0};
    float split = width * .58f;
    return pdf_status_label(context,fonts,35,(nx_rect){16+split,122,width-split,14},message);
}
#endif

static nx_result pdf_find_results(nx_context *context, const nx_font_collection *fonts,
    nx_text_field *field, const content_surface *surface, content_camera_view view) {
    nx_utf8_span query, text = {0};
    nx_result result = nx_text_field_text(field,&query);
    if (result != NX_OK || !query.length) return result;
    const content_frame_word *words = NULL;
    size_t word_count = 0;
    if (surface) {
        result = content_surface_get_text(surface,&text);
        if (result != NX_OK) return result;
        result = content_surface_get_words(surface,&words,&word_count);
        if (result != NX_OK) return result;
    }
    const char *message = "Page text unavailable";
    char summary[96];
    if (query.length > CONTENT_PDF_QUERY_LIMIT) {
        message = "Search text exceeds 128 bytes";
    } else if (text.length) {
        size_t matches = 0, next_word = 0, highlighted = 0;
        /* Both inputs are validated UTF-8. A literal match begins and ends
           at scalar boundaries. Use non-overlapping occurrences. */
        for (size_t start = 0; query.length <= text.length - start;) {
            if (memcmp(text.data + start,query.data,query.length)) {
                ++start;
                continue;
            }
            ++matches;
            size_t end = start + query.length;
            while (next_word < word_count && words[next_word].start_byte < end) {
                const content_frame_word *word = &words[next_word++];
                if (word->end_byte <= start) continue;
                ++highlighted;
                nx_rect bounds;
                if (content_camera_visible(view,word->bounds,&bounds)) {
                    result=nx_fill_rect(context,bounds,(nx_color){1,0.73f,0.14f,0.42f});
                    if (result!=NX_OK) return result;
                }
            }
            start = end;
        }
        snprintf(summary,sizeof(summary),"%zu %s on page%s",matches,matches==1?"match":"matches",
            matches && !highlighted ? " (highlight unavailable)" : "");
        message = summary;
    }
    return pdf_status_label(context,fonts,33,(nx_rect){16,122,view.viewport.width*.58f,14},message);
}

static int frame(nx_context *context, nx_window_info info, const nx_font_collection *fonts,
    content_surface *surface, double scale, enum phase phase, int focus, unsigned *action,
    nx_result *content_result, int *leased, int input_case, enum input_phase input_phase, int png_case, int pdf_case, int browser_case,
    int guest_case, const guest_inspector *inspector,
    pdf_text_view *text_view, nx_window *window, nx_text_field *query, pdf_document_view *document,
    unsigned *linked_page, pdf_selection *selection, int *redraw,
    char link_address[CONTENT_PDF_URI_LIMIT+1u], pdf_url_view *preview, int can_open_url,
    const content_camera *camera, content_camera_view *page_view,const char *browser_status,int browser_enter,
    int browser_ax_ready,content_ax_action *browser_action) {
    *action = 0; *leased = 0; *linked_page=0;
    *browser_action=(content_ax_action){0};
#ifndef CONTENT_NATIVE_BROWSER
    (void)browser_ax_ready;
#endif
    link_address[0]=0;
    if (nx_begin_frame(context, info.width, info.height, 0) != NX_OK) return 0;
    nx_semantic_desc root = {.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,info.width,info.height},
        .name={"External CPU content fixture",28}};
    int input_controls=(input_case||guest_case)&&phase==SCALED_READY;
    int gallery=document->gallery!=NULL;
    const char *status=png_case&&phase==BAD_REJECTED?"PNG decoding failed; image retained.":phase_name(phase);
    if (browser_case && phase==BAD_REJECTED) status="Browser engine unavailable; view retained.";
    if (guest_case && phase==BAD_REJECTED) status="Guest paused. Last display retained.";
    if (browser_status) status=browser_status;
    char pdf_status[64];
    if (pdf_case) {
        if (phase==BAD_REJECTED) status="PDF rendering failed; page retained.";
        if (phase==READY || phase==RECOVERED || phase==SCALED_READY || phase==PDF_PAGE1_SCALED || phase==PDF_HIT_READY) {
            const char *backing=scale==2?" at 2x":"";
            if (document->pages)
                snprintf(pdf_status,sizeof(pdf_status),"PDF page %u of %u%s.",document->page,document->pages,backing);
            else snprintf(pdf_status,sizeof(pdf_status),"PDF page %u%s.",document->page,backing);
            status=pdf_status;
        }
    }
    if (gallery) {
        if (phase==PDF_PAGE_PENDING||phase==SCALED_WAIT) status="Loading image.";
        else if (phase==PDF_HIT_FAILED) status=document->page?"Image decoding failed; image retained.":"Image unavailable.";
        else {
            snprintf(pdf_status,sizeof(pdf_status),"Image %u of %u.",document->page,document->pages);
            status=pdf_status;
        }
    }
    int search = find_visible(query,phase,info.width);
    int hit_page = pdf_case&&pdf_hit_page(phase);
    int zoom_visible=(pdf_case||png_case)&&info.width>=480&&pdf_can_navigate(phase)&&!selection->active;
    int pages_visible=(pdf_case||gallery)&&pdf_can_navigate(phase)&&(document->pages||document->properties.enabled||document->export_enabled)&&
        info.width>=480&&info.height>=248&&
        !selection->active;
    if (nx_semantics_add(context, &root, NULL) != NX_OK || label(context, fonts, 2,
        input_controls?(guest_case?guest_input_name(input_phase):input_name(input_phase)):status,
        (nx_rect){16,12,info.width-(pages_visible?262:zoom_visible?201:32),24},
        (pages_visible||png_case)&&phase==BAD_REJECTED?13:15) != NX_OK ||
        (search && pdf_find_field(context,window,fonts,query)!=NX_OK) ||
        (!gallery&&!search && (button(context,fonts,input_controls||browser_status?16u:10u,
            input_controls?(guest_case?"Enter guest":"Enter content"):browser_status?"Enter page":"Replace tab",16,48,112,
            input_controls?input_phase!=INPUT_DISCONNECTED:browser_status?browser_enter:phase==WAITING,action)!=NX_OK ||
        button(context,fonts,input_controls||browser_status?17u:11u,
            input_controls?"Release input":browser_status?"Release page":"Release old",136,48,112,
            input_controls?input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING:browser_status?!browser_enter:phase==REPLACED,action)!=NX_OK)) ||
        (!gallery&&(button(context,fonts,input_controls?18u:12u,input_controls?"Disconnect":
            pdf_case&&(phase==BAD_REJECTED||phase==PDF_PAGE1_SCALED||hit_page)?"Next page":
                guest_case&&phase==BAD_REJECTED?"Resume guest":"Publish frame",256,48,128,
            input_controls?input_phase!=INPUT_DISCONNECTED:phase==OLD_REJECTED||phase==BAD_REJECTED||
            phase==SCALED_WAIT||phase==PDF_PAGE1_SCALED||(hit_page&&document->page<document->pages),action)!=NX_OK ||
        button(context,fonts,13,pdf_case&&(phase==SCALED_READY||hit_page)?"Previous page":
            browser_case?"Crash browser":guest_case?"Pause guest":"Bad frame",16,88,112,
            phase==READY||(pdf_case&&phase==SCALED_READY)||(hit_page&&document->page>1u),action)!=NX_OK)) ||
        ((!gallery||scale==1)&&button(context,fonts,14,"Scale 2x",136,88,112,
            phase==RECOVERED||(hit_page&&scale==1)||(gallery&&document->page&&pdf_hit_page(phase)),action)!=NX_OK) ||
        button(context,fonts,15,"Close",256,88,(pdf_case||inspector->enabled)&&info.width>=480?112:128,1,action)!=NX_OK) return 0;
    if (inspector->enabled&&phase==SCALED_READY&&info.width>=480&&
        button(context,fonts,GUEST_INSPECT,"Resources",376,88,88,1,action)!=NX_OK) return 0;
    if (pdf_case&&info.width>=480&&pdf_can_navigate(phase)) {
        nx_utf8_span text;
        if (content_surface_get_text(surface,&text)!=NX_OK ||
            button(context,fonts,PDF_SELECT_BUTTON,selection->active?"Done":"Select text",376,88,88,
                text.length!=0,action)!=NX_OK) return 0;
    }
    if(input_controls&&input_hint(context,fonts,guest_case)!=NX_OK) return 0;
    if (pages_visible&&button(context,fonts,PDF_PAGES,gallery?"Gallery":"Pages",info.width-234,8,58,1,action)!=NX_OK) return 0;
    if (zoom_visible) {
        char zoom[16];
        snprintf(zoom,sizeof(zoom),"%u%%",camera->percent);
        float x=info.width-169;
        if (scoped_button(context,fonts,CONTENT_ZOOM_OUT,1,"Zoom out","−",x,8,44,camera->percent>100,action)!=NX_OK ||
            scoped_button(context,fonts,CONTENT_ZOOM_FIT,1,png_case?"Fit image":"Fit page",zoom,x+50,8,61,camera->percent!=100,action)!=NX_OK ||
            scoped_button(context,fonts,CONTENT_ZOOM_IN,1,"Zoom in","+",x+117,8,36,camera->percent<CONTENT_ZOOM_MAX,action)!=NX_OK)
            return 0;
    }
#ifdef CONTENT_NATIVE_PDF
    if (search && pdf_document_find(context,fonts,query,document,info.width-32)!=NX_OK) return 0;
    if (search && document->hits.next_page && info.width>=480) {
        if (button(context,fonts,PDF_NEXT_HIT,"Next hit",392,48,72,pdf_can_navigate(phase),action)!=NX_OK) return 0;
    }
#endif
    const char *name=pdf_case?"PDF page image":gallery?"Image view":browser_case?"Browser content":
        guest_case?"Guest display":png_case?"PNG image":"Fixture frame";
    nx_viewport_desc desc = {.id=100,.parent_id=1,.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
        .name={name,strlen(name)},.scale=scale};
    nx_interaction_state interaction;
    *content_result = content_surface_draw(surface, context, &desc, &interaction);
    *leased = *content_result == NX_OK && desc.bounds.width>0 && desc.bounds.height>0;
    nx_rect fitted={0,0,1,1};
    if (*leased && (png_case||pdf_case||guest_case)) {
        content_surface_status accepted;
        if (content_surface_get_status(surface,&accepted)!=NX_OK) return 0;
        fitted=content_image_destination(accepted,fitted);
        *page_view=content_camera_place(camera,desc.bounds,fitted);
        if ((png_case||guest_case) && accepted.image.source_width) {
            char description[64];
            if (accepted.jpeg.width)
                snprintf(description,sizeof(description),"JPEG image, %u × %u pixels",accepted.jpeg.width,accepted.jpeg.height);
            else
                snprintf(description,sizeof(description),guest_case?"Guest video, %u × %u pixels":"PNG image, %u × %u pixels",
                    accepted.image.source_width,accepted.image.source_height);
            if (!guest_case&&label(context,fonts,0,description,(nx_rect){16,122,desc.bounds.width,14},11)!=NX_OK) return 0;
            nx_rect bounds=guest_case?content_image_destination(accepted,desc.bounds):(nx_rect){0};
            if (guest_case||content_camera_visible(*page_view,fitted,&bounds)) {
                nx_semantic_desc image={.id=72,.parent_id=desc.id,.role=NX_ROLE_IMAGE,.bounds=bounds,
                    .name={description,strlen(description)}};
                if (nx_semantics_add(context,&image,NULL)!=NX_OK) return 0;
            }
        }
    } else *page_view=content_camera_place(camera,desc.bounds,fitted);
#ifdef CONTENT_NATIVE_BROWSER
    if (browser_case && *leased && browser_ax_semantics(context,surface,&desc,browser_ax_ready,browser_action)!=NX_OK)
        return 0;
#endif
    if (pdf_case && *leased && pdf_text_semantics(context,surface,&desc,text_view,selection,window,
        pdf_can_navigate(phase),*page_view,redraw)!=NX_OK) return 0;
    if (pdf_case&&*leased&&selection->active&&selection->copy_status!=PDF_COPY_IDLE&&info.width>=480 &&
        pdf_status_label(context,fonts,37,(nx_rect){296,15,info.width-312,16},
            selection->copy_status==PDF_COPY_SUCCEEDED?"Selection copied":"Copy unavailable")!=NX_OK) return 0;
    if (*content_result != NX_OK && nx_fill_rect(context, desc.bounds,
        (nx_color){227.0f/255.0f,231.0f/255.0f,239.0f/255.0f,1}) != NX_OK) return 0;
    if (search && pdf_find_results(context,fonts,query,*leased?surface:NULL,*page_view)!=NX_OK) return 0;
    if (pdf_case && *leased && pdf_link_widgets(context,surface,&desc,text_view->identity,
        pdf_can_navigate(phase)&&!selection->active,*page_view,linked_page,link_address)!=NX_OK) return 0;
    if (focus) {
        uint64_t id = phase==WAITING?10u:phase==REPLACED?11u:
            phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT?12u:
            phase==READY?13u:phase==RECOVERED?14u:15u;
        if (pdf_case&&phase==SCALED_READY) id=13u;
        if (phase==PDF_PAGE1_SCALED) id=12u;
        if (hit_page&&*leased) id=100u;
        if (gallery) id=pages_visible?PDF_PAGES:15u;
        if (nx_focus_id(context)!=PDF_FIND_FIELD && nx_focus_set(context,id)!=NX_OK) return 0;
    }
    if (preview->uri[0] && (!*leased || preview->identity!=text_view->identity))
        *preview=(pdf_url_view){0};
    if (preview->uri[0] && pdf_url_dialog(context,fonts,info.width,info.height,preview,can_open_url,action)!=NX_OK)
        return 0;
#ifdef CONTENT_NATIVE_PNG
    if (document->thumbnails.open && content_pages_dialog(context,fonts,info,document,action,linked_page)!=NX_OK) return 0;
#endif
    if (inspector->open&&guest_resources_overlay(context,fonts,info,inspector,action)!=NX_OK) return 0;
    return nx_end_frame(context)==NX_OK;
}

int main(void) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *path=getenv("NEXAUI_CONTENT_TEST_FONT"),*test_case=getenv("NEXAUI_CONTENT_TEST_CASE");
    if (!isolated||strcmp(isolated,"1")||!display||strcmp(display,"nexaui-test")||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!path||!test_case||
        (strcmp(test_case,"full")&&strcmp(test_case,"png")&&strcmp(test_case,"pdf")&&strcmp(test_case,"input")&&
        strcmp(test_case,"browser")&&strcmp(test_case,"guest")&&strcmp(test_case,"guest-desktop")&&strcmp(test_case,"guest-storage")&&
        strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close"))) return 2;
    int browser_case=!strcmp(test_case,"browser");
    int guest_desktop=!strcmp(test_case,"guest-desktop")||!strcmp(test_case,"guest-storage");
    int guest_case=guest_desktop||!strcmp(test_case,"guest");
    const char *guest_socket=guest_case?getenv("NEXAUI_CONTENT_TEST_GUEST_SOCKET"):NULL;
#ifndef CONTENT_NATIVE_GUEST
    if (guest_case) return 2;
#endif
    if (guest_case&&(!guest_socket||guest_socket[0]!='/')) return 2;
    int png_case=browser_case||!strcmp(test_case,"png");
    const char *browser_socket=browser_case?getenv("NEXAUI_CONTENT_TEST_BROWSER_SOCKET"):NULL;
#ifndef CONTENT_NATIVE_BROWSER
    if (browser_case) return 2;
#endif
    int browser_pointer_case=browser_case&&getenv("NEXAUI_CONTENT_TEST_BROWSER_POINTER");
    int browser_text_case=browser_pointer_case||(browser_case&&getenv("NEXAUI_CONTENT_TEST_BROWSER_INPUT"));
    int browser_ax_case=browser_case&&getenv("NEXAUI_CONTENT_TEST_BROWSER_AX");
    int gallery_case=png_case&&getenv("NEXAUI_CONTENT_TEST_PNG_GALLERY");
    int pdf_case=!strcmp(test_case,"pdf");
    int pdf_select_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_SELECT");
    int pdf_zoom_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_ZOOM");
    int pdf_detail_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_DETAIL");
    const char *export_path=pdf_case?getenv("NEXAUI_CONTENT_TEST_EXPORT_PATH"):NULL;
    int pdf_export_case=export_path&&export_path[0]=='/';
    int content_thumbnails_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_THUMBNAILS");
    int pdf_properties_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_PROPERTIES");
    int pdf_compose=pdf_case&&getenv("NEXAUI_CONTENT_PDF_COMPOSE");
    const char *encoded_dir=(png_case||pdf_case)?getenv("NEXAUI_CONTENT_TEST_ENCODED"):NULL;
    const char *pdf_renderer=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_RENDERER"):NULL;
    const char *pdf_reader=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_READER"):NULL;
    const char *pdf_link_reader=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_LINK_READER"):NULL;
    const char *uri_launcher=pdf_case?getenv("NEXAUI_CONTENT_TEST_URI_LAUNCHER"):NULL;
    int pdf_url_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_URL");
#ifndef CONTENT_NATIVE_PNG
    if (png_case) return 2;
#endif
#ifndef CONTENT_NATIVE_PDF
    if (pdf_case) return 2;
#endif
    if (((png_case||pdf_case)&&!browser_case&&!encoded_dir)||(pdf_case&&(!pdf_renderer||!pdf_reader))||
        (browser_case&&(gallery_case||!browser_socket||browser_socket[0]!='/'))) return 2;
    int result=1,leased=0,worker_ready=0;
    nx_snapshot_exchange *exchange=NULL; content_surface *surface=NULL;
    nx_window *window=NULL; nx_software_view *software=NULL; nx_context *context=NULL;
    nx_font *font=NULL; nx_font_collection *fonts=NULL;
    content_input *input=NULL; native_input_receiver receiver={.generation=1};
#ifdef CONTENT_NATIVE_GUEST
    native_guest_input guest={.ledger.generation=1};
#endif
    int input_case=!strcmp(test_case,"input"); enum input_phase input_phase=INPUT_IDLE;
    guest_inspector inspector={.enabled=guest_desktop};
    native_worker worker={0}; worker.wake[0]=worker.wake[1]=-1;
#ifdef CONTENT_NATIVE_BROWSER
    browser_editor editor={.pointer_enabled=browser_pointer_case};
#endif
#ifdef CONTENT_NATIVE_PNG
    native_png_source png_source={0};
#endif
    size_t bytes=0; uint8_t *data=read_font(path,&bytes);
    enum phase phase=gallery_case?PDF_PAGE_PENDING:PREPARING,last_phase=FAILED;
    unsigned frames=0,action=0,linked_page=0; int need_frame=1,initial_job=0; double scale=1;
    unsigned nav_completion=0,pending_page=0,scaled_search_completion=0; enum phase nav_target=FAILED;
    uint64_t nav_sequence=0;
    pdf_document_view document={.properties.enabled=pdf_link_reader!=NULL,.export_enabled=pdf_export_case};
    pdf_text_view text_view={0};
    pdf_selection selection={0};
    content_camera camera;
    content_camera_fit(&camera);
    pdf_url_view preview={0};
    uint64_t uri_serial=0;
#ifdef CONTENT_NATIVE_PDF
    uint64_t export_serial=0;
#endif
    char link_address[CONTENT_PDF_URI_LIMIT+1u];
    nx_text_field *query=NULL;
    int logged_preedit=0,find_pressed=0;
    uint64_t logged_generation=0,logged_resize=0,logged_sequence=0;
#ifdef CONTENT_NATIVE_PNG
    if (gallery_case) {
        if (png_gallery_open(encoded_dir,&document.gallery)!=NX_OK) goto done;
        document.pages=png_gallery_count(document.gallery);
    }
#endif
    const nx_window_desc desc={.title_utf8="External CPU content fixture",.width=480,.height=360};
    if (!data||nx_font_open(data,bytes,NULL,&font)!=NX_OK||nx_font_collection_create(NULL,&fonts)!=NX_OK||
        nx_font_collection_add(fonts,font)!=NX_OK||nx_snapshot_exchange_create(NULL,&exchange)!=NX_OK||
        content_surface_create(exchange,1,4u*1024u*1024u,NULL,&surface)!=NX_OK||
        nx_context_create(NULL,&context)!=NX_OK||nx_window_create(&desc,&window)!=NX_OK||
        (pdf_case&&nx_text_field_create("",0,4096,&query)!=NX_OK)||
        nx_software_view_open(window,&software)!=NX_OK||
        native_worker_open(&worker,exchange,encoded_dir,pdf_renderer,pdf_reader,pdf_link_reader,uri_launcher,export_path,
            document.gallery,browser_socket,browser_ax_case,guest_socket)) goto done;
    worker_ready=1;
    if(input_case&&content_input_create(1,NX_KEY_F12,NX_MOD_CONTROL|NX_MOD_SHIFT,native_input_receive,&receiver,&input)!=NX_OK) goto done;
#ifdef CONTENT_NATIVE_GUEST
    if(guest_case&&content_input_create(1,NX_KEY_F12,NX_MOD_CONTROL|NX_MOD_SHIFT,native_guest_receive,&guest,&input)!=NX_OK) goto done;
#endif
    if(browser_text_case&&nx_window_set_ime_transaction_events(window,1)!=NX_OK) goto done;
    for (;;) {
        uint32_t ready=0; size_t routed=0; nx_window_info info;
        int wait_ms=guest_desktop&&phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED?80:200;
        if (nx_window_poll_with_fd_events(window,need_frame?0:wait_ms,worker.wake[0],NX_WINDOW_FD_READABLE,&ready)!=NX_OK||
            nx_window_get_info(window,&info)!=NX_OK) goto done;
        if(input) {
            /* Drain native events first; pending native AX actions still use
               route_events below, in their ordinary native-then-AX order. */
            for(;;) {
                nx_event event,host; int available=0,route=0;
                if(nx_window_next_event(window,&event,&available)!=NX_OK) goto done;
                if(!available) break;
                ++routed;
                if(event.type==NX_EVENT_FOCUS||event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP||event.type==NX_EVENT_POINTER_CANCEL) {
                    printf("INPUT_NATIVE type=%d key=%d focus=%d repeat=%d\n",(int)event.type,(int)event.key,event.focused,event.repeat);fflush(stdout);
                }
                content_input_status before,after;
                if(content_input_get_status(input,&before)!=NX_OK||
                    content_input_event(input,&event,&host,&route)!=NX_OK||
                    content_input_get_status(input,&after)!=NX_OK) goto done;
                if(route&&nx_context_queue_event(context,&host)!=NX_OK) goto done;
                if(before.active&&!after.active) {
                    input_phase=event.type==NX_EVENT_FOCUS&&!event.focused?INPUT_FOCUS_LOST:INPUT_RELEASED;
                    if(nx_focus_set(context,15)!=NX_OK) goto done;
                    need_frame=1;
                }
                /* Some backends deliver pointer cancellation before FocusOut.
                   Preserve the actual focus-loss reason after that release. */
                if(event.type==NX_EVENT_FOCUS&&!event.focused&&phase==SCALED_READY&&
                    input_phase!=INPUT_DISCONNECTED&&input_phase!=INPUT_DISCONNECTING) {
                    input_phase=INPUT_FOCUS_LOST;need_frame=1;
                }
            }
        }
#ifdef CONTENT_NATIVE_BROWSER
        if (browser_text_case) {
            for (;;) {
                nx_event event;
                int available=0,route=0;
                if (nx_window_next_event(window,&event,&available)!=NX_OK) goto done;
                if (!available) break;
                ++routed;
                if (!browser_edit_filter(&editor,window,&event,&route) ||
                    (route && nx_context_queue_event(context,&event)!=NX_OK)) goto done;
            }
        }
#endif
        size_t ax_routed=0;
        if(nx_window_route_events(window,context,&ax_routed)!=NX_OK) goto done;
        routed+=ax_routed;
        int worker_frame=0;
        if (ready) {
            char wake[64]; ssize_t count;
            do {
                count=read(worker.wake[0],wake,sizeof(wake));
                for (ssize_t i=0;i<count;++i) worker_frame|=wake[i]!=NATIVE_PROPERTIES_READY&&wake[i]!=NATIVE_EXPORT_READY;
            } while (count>0||(count<0&&errno==EINTR));
            if (count<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK) goto done;
        }
        native_worker_state state=native_state(&worker);
        inspector.report=state.guest_report;
        if (uri_launcher) {
            native_uri_result released=native_take_uri_result(&worker);
            if (released.ticket && released.ticket==preview.ticket) {
                preview.ticket=0;
                preview.status=released.result==NX_OK?"Link handed off":"Open unavailable";
                need_frame=1;
            }
        }
#ifdef CONTENT_NATIVE_PNG
        if (pdf_case||gallery_case) {
            content_surface_status current;
            if (content_surface_get_status(surface,&current)!=NX_OK) goto done;
            content_thumbnails_configure(&document.thumbnails,current.request.generation,info.scale*scale);
            content_thumbnail thumbnail=native_take_thumbnail(&worker);
            if (thumbnail.page) {
                int accepted=content_thumbnails_adopt(&document.thumbnails,thumbnail);
                printf("%s_PREVIEW page=%u generation=%llu revision=%llu pixels=%ux%u accepted=%d bytes=%zu\n",
                    gallery_case?"PNG":"PDF",thumbnail.page,(unsigned long long)thumbnail.request.generation,
                    (unsigned long long)thumbnail.request.resize,thumbnail.request.width,thumbnail.request.height,
                    accepted,thumbnail.bytes);
                fflush(stdout);
                need_frame|=accepted&&document.thumbnails.open;
            }
        }
#endif
#ifdef CONTENT_NATIVE_PDF
        if (pdf_case) {
            content_surface_status current;
            if (content_surface_get_status(surface,&current)!=NX_OK) goto done;
            native_pdf_index_result available = native_take_pdf_index(&worker);
            if (available.generation) {
                if (available.generation==current.request.generation) {
                    content_pdf_index_destroy(document.index);
                    document.index=available.index;
                    document.index_finished=1;
                    document.pages=content_pdf_index_pages(available.index);
                    document.counted_length=0;
                    need_frame=1;
                } else {
                    if (available.index) {
                        printf("PDF_INDEX stale generation=%llu current=%llu\n",
                            (unsigned long long)available.generation,
                            (unsigned long long)current.request.generation);
                        fflush(stdout);
                    }
                    content_pdf_index_destroy(available.index);
                }
            }
            if (pdf_link_reader&&ready) {
                native_pdf_properties_result properties=native_take_pdf_properties(&worker);
                if (properties.generation) {
                    int accepted=properties.generation==current.request.generation;
                    if (accepted) {
                        document.properties.available=properties;
                        need_frame|=document.properties.open;
                    }
                    printf("PDF_PROPERTIES generation=%llu accepted=%d result=%d\n",
                        (unsigned long long)properties.generation,accepted,(int)properties.result);
                    fflush(stdout);
                }
            }
            if (document.export_enabled&&ready) {
                native_export_result exported=native_take_export(&worker);
                if (exported.ticket && exported.ticket==document.export_ticket &&
                    exported.generation==current.request.generation) {
                    document.export_ticket=0;
                    document.export_status=exported.result==NX_OK?"PNG saved":
                        exported.result==NX_ERROR_STATE?"PNG already exists":"Export unavailable";
                    printf("PDF_EXPORT ticket=%llu generation=%llu page=%u result=%d\n",(unsigned long long)exported.ticket,
                        (unsigned long long)exported.generation,exported.page,(int)exported.result);
                    fflush(stdout);
                    need_frame|=document.thumbnails.open;
                }
            }
        }
#endif
        if (phase==PREPARING&&state.prepared) { phase=WAITING; need_frame=1; }
        int decode_rejected=(png_case||pdf_case||guest_case)&&state.completed==3&&
            (guest_case?state.result==NX_ERROR_STATE:
                state.result==NX_ERROR_ARGUMENT||(browser_case&&state.result!=NX_OK));
#ifdef CONTENT_NATIVE_BROWSER
        if (browser_text_case&&phase==SCALED_READY&&editor.awaiting==state.completed&&editor.awaiting&&state.result!=NX_OK &&
            !browser_edit_stop(&editor,window)) goto done;
#endif
        int navigation_error=pdf_case&&(phase==PDF_PAGE_PENDING||phase==PDF_HIT_FAILED)&&
            state.completed==nav_completion&&state.result!=NX_OK;
        if (!gallery_case&&state.result!=NX_OK&&!decode_rejected&&!navigation_error&&phase!=FAILED) { phase=FAILED; need_frame=1; }
        if (info.should_close) { result=0; break; }
        if (!need_frame&&!routed&&!worker_frame) {
#ifdef CONTENT_NATIVE_GUEST
            if (guest_desktop&&phase==SCALED_READY&&!inspector.open&&input_phase!=INPUT_DISCONNECTED&&
                input_phase!=INPUT_DISCONNECTING&&!state.working&&!state.pending) {
                content_surface_status current;
                if (content_surface_get_status(surface,&current)!=NX_OK||!current.sequence||
                    current.sequence==UINT64_MAX||
                    !native_schedule(&worker,(native_job){.request=current.request,.sequence=current.sequence+1,.kind=6}))
                    goto done;
            }
#endif
            continue;
        }
        need_frame=0;
        int had_preview=preview.uri[0]!=0;
        int had_pages=document.thumbnails.open;
        enum phase drawn=phase; nx_result content_result=NX_ERROR_STATE;
        content_camera_view page_view;
        const char *browser_status=NULL;
        int browser_enter=0;
        content_ax_action browser_action;
#ifdef CONTENT_NATIVE_BROWSER
        if (browser_text_case&&phase==SCALED_READY) {
            browser_enter=editor.phase==BROWSER_HOST;
            browser_status=editor.phase==BROWSER_ENTERING?"Focusing browser page.":
                editor.phase==BROWSER_EDITING?"Page input active: Ctrl+Shift+F12 exits.":
                editor.phase==BROWSER_LEAVING?"Releasing page input.":
                editor.revision?"Page input released.":"Scaled frame ready.";
        }
#endif
        ++frames;
        if ((!guest_desktop&&frames>((browser_pointer_case||guest_case)?220u:150u))||
            !frame(context,info,fonts,surface,scale,phase,phase!=last_phase,&action,
            &content_result,&leased,input_case,input_phase,png_case&&!browser_case,pdf_case,browser_case,guest_case,&inspector,&text_view,window,query,&document,
            &linked_page,&selection,&need_frame,link_address,&preview,uri_launcher!=NULL,&camera,&page_view,
            browser_status,browser_enter,browser_ax_case&&phase==SCALED_READY&&!state.working&&!state.pending,
            &browser_action)) goto done;
        int search_focused=nx_focus_id(context)==PDF_FIND_FIELD;
        if (!had_pages&&!had_preview&&!preview.uri[0]&&find_visible(query,drawn,info.width)) {
            if (pdf_find_shortcut(context,leased,&find_pressed,&need_frame)!=NX_OK) goto done;
        } else find_pressed=0;
        if (phase!=last_phase) need_frame=1;
        last_phase=drawn;
        if (!had_pages&&!had_preview&&!preview.uri[0]&&!inspector.open&&!search_focused&&nx_key_pressed(context,NX_KEY_ESCAPE)) {
            if (!selection.active || nx_focus_id(context)!=PDF_TEXT_NODE) { result=0; break; }
            selection.active=selection.dragging=selection.take_focus=0;
            if (nx_focus_set(context,leased?100u:15u)!=NX_OK) goto done;
            need_frame=1;
        }
        if (pdf_compose&&!logged_preedit&&search_focused) {
            size_t bytes=0;
            int32_t begin=0,end=0;
            if (nx_window_get_ime_preedit(window,NULL,0,&bytes,&begin,&end)==NX_OK&&bytes) {
                puts("PDF_FIND_PREEDIT");
                fflush(stdout);
                logged_preedit=1;
            }
        }
        if (phase==RELEASED&&state.completed==1) {
            phase=content_result==NX_ERROR_STATE?OLD_REJECTED:FAILED; need_frame=1;
        }
        if ((phase==FRESH_PENDING&&state.completed==2)||(phase==RECOVERY_PENDING&&state.completed==4)||
            (phase==SCALED_PENDING&&state.completed==(scaled_search_completion?scaled_search_completion:5u))) {
            if (content_result!=NX_OK) phase=FAILED;
            else {
                if (pdf_case && phase!=SCALED_PENDING) document.page=phase==FRESH_PENDING?1u:2u;
                phase=phase==FRESH_PENDING?READY:phase==RECOVERY_PENDING?RECOVERED:
                    scaled_search_completion?PDF_HIT_READY:SCALED_READY;
            }
            scaled_search_completion=0;
            need_frame=1;
        }
        if (phase==BAD_PENDING&&state.completed==3) {
            int rejected=(png_case||pdf_case||guest_case)?decode_rejected&&content_result==NX_OK:content_result==NX_ERROR_UNSUPPORTED;
            phase=rejected?BAD_REJECTED:FAILED; need_frame=1;
        }
        if (phase==PDF_PAGE_PENDING&&state.completed==nav_completion) {
            content_surface_status accepted;
            if (content_surface_get_status(surface,&accepted)!=NX_OK) goto done;
            if (state.result==NX_OK && content_result==NX_OK && accepted.sequence==nav_sequence) {
                document.page=pending_page;
                phase=nav_target;
            } else phase=(gallery_case||content_result==NX_OK)?PDF_HIT_FAILED:FAILED;
            need_frame=1;
        }
        if (nx_window_publish_semantics(window,context)!=NX_OK) goto done;
        const nx_command *commands=NULL; size_t count=0; nx_software_view_outcome outcome=NX_SOFTWARE_VIEW_READY;
        if (nx_frame_commands(context,&commands,&count)!=NX_OK) goto done;
        content_surface_status status;
        if (content_surface_get_status(surface,&status)!=NX_OK) goto done;
        nx_rect image=page_view.image;
        const nx_bitmap_view *detail_bitmap=NULL;
#ifdef CONTENT_NATIVE_PNG
        if (png_case) {
            native_png_take(&worker,&png_source,status);
            if (leased && camera.percent>100 && png_source.bitmap.pixels && png_source.sequence==status.sequence) {
                image=content_image_destination(status,image);
                detail_bitmap=&png_source.bitmap;
            }
        }
#endif
#ifdef CONTENT_NATIVE_PDF
        if (pdf_case) {
            pdf_detail_select(&document.detail,status,image,leased&&camera.percent>100?document.page:0,info.scale);
            pdf_detail_result detail=native_take_detail(&worker);
            if (detail.key.edge) {
                int accepted=pdf_detail_adopt(&document.detail,detail);
                printf("PDF_DETAIL generation=%llu resize=%llu base=%llu page=%u edge=%u pixels=%ux%u accepted=%d result=%d\n",
                    (unsigned long long)detail.key.generation,(unsigned long long)detail.key.resize,
                    (unsigned long long)detail.key.sequence,detail.key.page,detail.key.edge,
                    detail.bitmap.width,detail.bitmap.height,accepted,(int)detail.result);
                fflush(stdout);
            }
            if (document.detail.visible.bitmap.pixels) {
                image=content_image_destination(status,image);
                detail_bitmap=&document.detail.visible.bitmap;
            }
        }
#endif
        if ((pdf_case||png_case)&&leased&&camera.percent>100 &&
            content_surface_map_commands(surface,commands,count,image,detail_bitmap,&commands)!=NX_OK) goto done;
        if (nx_software_view_render(software,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK) goto done;
        /* The presenter borrows commands only until return. Discard this
           stream after completing its lease; BUSY/SUSPENDED rebuild later. */
        if (leased) { if (content_surface_complete(surface)!=NX_OK) goto done; leased=0; }
        if (outcome!=NX_SOFTWARE_VIEW_READY) need_frame=1;
        if (status.request.generation!=logged_generation||status.request.resize!=logged_resize||status.sequence!=logged_sequence) {
            printf("CONTENT_STATE generation=%llu resize=%llu width=%u height=%u sequence=%llu copied=%llu copy_bytes=%zu\n",
                (unsigned long long)status.request.generation,(unsigned long long)status.request.resize,
                status.request.width,status.request.height,(unsigned long long)status.sequence,
                (unsigned long long)status.copied_frames,status.last_copy_bytes);
            fflush(stdout);
            logged_generation=status.request.generation;logged_resize=status.request.resize;logged_sequence=status.sequence;
        }
        if (gallery_case&&(!initial_job||phase==SCALED_WAIT)&&status.request.width&&status.request.height) {
            pending_page=document.page?document.page:1u;
            nav_sequence=1;
            nav_target=PDF_HIT_READY;
            nav_completion=state.completed+1;
            native_job job={.request=status.request,.sequence=nav_sequence,.page=pending_page};
            if (!native_schedule(&worker,job)) goto done;
            phase=PDF_PAGE_PENDING;
            need_frame=1;
            initial_job=1;
        } else if (!initial_job&&status.request.width&&status.request.height) {
            if (!native_schedule(&worker,(native_job){.request=status.request,.sequence=1,.held=1})) goto done;
            initial_job=1;
        }
        if (action==15) { result=0; break; }
        if (action==GUEST_INSPECT&&inspector.enabled&&!inspector.open) {
            if (inspector.ticket==UINT64_MAX||content_input_release(input)!=NX_OK) goto done;
            if (input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING) input_phase=INPUT_RELEASED;
            ++inspector.ticket;
            inspector.open=inspector.pending=1;
            need_frame=1;
        } else if (action==GUEST_INSPECT_CLOSE&&inspector.open) {
            inspector.open=inspector.pending=0;
            nx_result closed=nx_overlay_close(context);
            if (closed!=NX_OK&&closed!=NX_ERROR_STATE) goto done;
            need_frame=1;
        }
#ifdef CONTENT_NATIVE_BROWSER
        if (browser_action.node && !action && phase==SCALED_READY && content_result==NX_OK) {
            const content_ax_node *nodes;
            size_t count;
            uint32_t epoch;
            if (content_surface_get_ax(surface,&nodes,&count,&epoch)!=NX_OK || !count ||
                epoch!=browser_action.epoch || status.sequence==UINT64_MAX ||
                !native_schedule(&worker,(native_job){.request=status.request,.sequence=status.sequence+1u,
                    .ax_action=browser_action})) goto done;
            need_frame=1;
        }
        if (browser_status) {
            if (browser_enter&&action==16) {
                nx_rect bounds={16,136,info.width-32,info.height-152};
                if (!browser_edit_enter(&editor,context,status,bounds)) goto done;
                need_frame=1;
            }
            if (action==17) {
                if (!browser_edit_leave(&editor,window)) goto done;
                need_frame=1;
            }
            if (!browser_edit_update(&editor,&worker,state,status,window,context,info,&need_frame)) goto done;
        }
#endif
        if ((pdf_case||png_case)&&!had_pages&&!preview.uri[0]&&!had_preview&&content_result==NX_OK&&pdf_can_navigate(drawn)) {
            if (content_camera_update(&camera,context,page_view,100,action,&need_frame)!=NX_OK) goto done;
        } else camera.dragging=0;
#ifdef CONTENT_NATIVE_PNG
        if (action==PDF_PAGES && !had_pages && !had_preview && (gallery_case||(content_result==NX_OK&&document.page))) {
            content_thumbnails_open(&document.thumbnails,document.page?document.page:1u);
            need_frame=1;
            continue;
        }
        if (had_pages) {
#ifdef CONTENT_NATIVE_PDF
            if (action==PDF_EXPORT && !document.export_ticket) {
                int scheduled=0;
                if (content_result==NX_OK && status.sequence && status.image.width && status.image.height && export_serial!=UINT64_MAX) {
                    native_export_job job={.ticket=++export_serial,.generation=status.request.generation,
                        .page=document.page,.view={.viewport=page_view.viewport,.page=content_image_destination(status,page_view.image),
                            .width=641,.height=361,.scale=1.25}};
                    scheduled=native_schedule_export(&worker,job);
                    if (scheduled) document.export_ticket=job.ticket;
                }
                document.export_status=scheduled?"Exporting PNG":"Export unavailable";
                need_frame=1;
                continue;
            }
            if (action==PDF_PROPERTIES) {
                document.properties.open=1;
                document.properties.x=document.properties.y=0;
                ++document.properties.session;
                need_frame=1;
                continue;
            }
            if (action==PDF_PROPERTIES_CLOSE || action==PDF_PROPERTIES_DISMISSED) {
                document.properties.open=0;
                if (action==PDF_PROPERTIES_CLOSE && nx_overlay_close(context)!=NX_OK) goto done;
                need_frame=1;
                continue;
            }
#endif
            if (action==PDF_PAGES_CLOSE || linked_page) {
                document.thumbnails.open=0;
                document.properties.open=0;
                nx_result closed=nx_overlay_close(context);
                if (closed!=NX_OK&&closed!=NX_ERROR_STATE) goto done;
                need_frame=1;
                if (linked_page==document.page) linked_page=0;
                action=0;
            } else {
                if (document.properties.open) continue;
                unsigned first=document.thumbnails.first;
                if (action==PDF_PAGES_PREVIOUS) first-=CONTENT_THUMBNAIL_COUNT;
                if (action==PDF_PAGES_NEXT) first+=CONTENT_THUMBNAIL_COUNT;
                if (first!=document.thumbnails.first) {
                    content_thumbnails_set_group(&document.thumbnails,first);
                    need_frame=1;
                }
                unsigned missing=content_thumbnails_missing(&document.thumbnails,document.pages);
                if (missing) (void)native_schedule_thumbnail(&worker,document.thumbnails.request,missing);
                continue;
            }
        }
#endif
        if (gallery_case&&linked_page) {
            if (state.completed==UINT_MAX || status.sequence==UINT64_MAX) goto done;
            pending_page=linked_page;
            nav_sequence=status.sequence+1;
            nav_target=PDF_HIT_READY;
            native_job job={.request=status.request,.sequence=nav_sequence,.page=pending_page};
            if (!native_schedule(&worker,job)) goto done;
            nav_completion=state.completed+1;
            phase=PDF_PAGE_PENDING;
            need_frame=1;
        }
        if (preview.uri[0]) {
            if (action==PDF_URL_CLOSE) {
                preview=(pdf_url_view){0};
                nx_result closed=nx_overlay_close(context);
                if (closed!=NX_OK&&closed!=NX_ERROR_STATE) goto done;
                need_frame=1;
            } else if (action==PDF_URL_COPY) {
                nx_result copied=nx_window_clipboard_set_text(window,preview.uri,strlen(preview.uri));
                preview.status=copied==NX_OK?"Address copied":"Copy unavailable";
                need_frame=1;
            } else if (action==PDF_URL_OPEN && !preview.ticket && uri_serial!=UINT64_MAX) {
                if (native_schedule_uri(&worker,++uri_serial,preview.uri)) {
                    preview.ticket=uri_serial;
                    preview.status="Opening link";
                } else preview.status="Open unavailable";
                need_frame=1;
            }
            continue;
        }
        if (link_address[0]&&content_result==NX_OK) {
            preview=(pdf_url_view){.identity=text_view.identity};
            memcpy(preview.uri,link_address,sizeof(preview.uri));
            need_frame=1;
        } else if (action==PDF_SELECT_BUTTON) {
            selection.active=!selection.active;
            selection.take_focus=selection.active;
            selection.dragging=0;
            need_frame=1;
        } else if (action==10&&phase==WAITING) {
            if (content_surface_destroy(&surface)!=NX_OK||content_surface_create(exchange,2,4u*1024u*1024u,NULL,&surface)!=NX_OK) goto done;
#ifdef CONTENT_NATIVE_PDF
            content_pdf_index_destroy(document.index);
            content_thumbnails_clear(&document.thumbnails);
            pdf_detail_clear(&document.detail);
            document=(pdf_document_view){.properties.enabled=pdf_link_reader!=NULL,.export_enabled=pdf_export_case};
#endif
            if(input) { if(content_input_rebind(input,2)!=NX_OK) goto done; receiver.generation=2; }
#ifdef CONTENT_NATIVE_GUEST
            if(guest_case) guest.ledger.generation=2;
#endif
            selection=(pdf_selection){0};
            content_camera_fit(&camera);
            phase=REPLACED; need_frame=1;
        } else if (action==11&&phase==REPLACED) { native_release(&worker); phase=RELEASED; need_frame=1; }
        else if (action==12&&(phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT)) {
            native_job job={.request=status.request,.sequence=phase==BAD_REJECTED?3u:1u,
                .kind=phase==BAD_REJECTED?3u:phase==SCALED_WAIT?4u:1u,
                .page=scaled_search_completion?document.page:0};
            if (!native_schedule(&worker,job)) goto done;
            phase=phase==OLD_REJECTED?FRESH_PENDING:phase==BAD_REJECTED?RECOVERY_PENDING:SCALED_PENDING; need_frame=1;
        } else if (action==13&&phase==READY) {
            if (!native_schedule(&worker,(native_job){.request=status.request,.sequence=2,.kind=2})) goto done;
            phase=BAD_PENDING; need_frame=1;
        } else if (action==14&&(phase==RECOVERED||((pdf_case||gallery_case)&&pdf_hit_page(phase)))) {
            if (state.completed==UINT_MAX) goto done;
            if (pdf_case&&pdf_hit_page(phase)) scaled_search_completion=state.completed+1;
            scale=2; phase=SCALED_WAIT; need_frame=1;
        }
        else if (pdf_case&&((action==13&&phase==SCALED_READY)||(action==12&&phase==PDF_PAGE1_SCALED)||
            (pdf_hit_page(phase)&&((action==12&&document.page<document.pages)||(action==13&&document.page>1u)))||
            (action==PDF_NEXT_HIT&&document.hits.next_page&&content_result==NX_OK)||
            (linked_page&&content_result==NX_OK))) {
            if (status.sequence==UINT64_MAX||state.completed==UINT_MAX) goto done;
            int from_hits=pdf_hit_page(phase);
            nav_target=linked_page||action==PDF_NEXT_HIT||from_hits?PDF_HIT_READY:action==13?PDF_PAGE1_SCALED:SCALED_READY;
            pending_page=linked_page?linked_page:action==PDF_NEXT_HIT?document.hits.next_page:
                action==13?document.page-1u:document.page+1u;
            nav_sequence=status.sequence+1;
            native_job job={.request=status.request,.sequence=nav_sequence,.kind=5,.page=pending_page};
            if (!native_schedule(&worker,job)) goto done;
            nav_completion=state.completed+1; phase=PDF_PAGE_PENDING; need_frame=1;
        }
        if(input) {
            if(action==16&&phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED&&
                input_phase!=INPUT_DISCONNECTING&&input_phase!=INPUT_ACTIVE) {
                input_phase=INPUT_PENDING;
                if(nx_focus_set(context,100)!=NX_OK) goto done;
                need_frame=1;
            } else if(action==17&&(input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING)) {
                if(content_input_release(input)!=NX_OK||nx_focus_set(context,15)!=NX_OK) goto done;
                input_phase=INPUT_RELEASED;need_frame=1;
            } else if(action==18&&input_phase!=INPUT_DISCONNECTED&&input_phase!=INPUT_DISCONNECTING) {
                if(content_input_release(input)!=NX_OK||nx_focus_set(context,15)!=NX_OK) goto done;
                input_phase=guest_case?INPUT_DISCONNECTING:INPUT_DISCONNECTED;need_frame=1;
            }
            content_input_target target={.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
                .width=status.request.width,.height=status.request.height,
                .available=phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED&&input_phase!=INPUT_DISCONNECTING,
                .focused=!inspector.open&&nx_focus_id(context)==100};
            if (guest_case) {
                target.bounds=content_image_destination(status,target.bounds);
                target.width=status.image.source_width;
                target.height=status.image.source_height;
                target.available=target.available&&target.width&&target.height;
            }
            content_input_status before,after;
            if(content_input_get_status(input,&before)!=NX_OK||content_input_set_target(input,&target)!=NX_OK||
                content_input_get_status(input,&after)!=NX_OK) goto done;
            if(before.active&&!after.active) { input_phase=INPUT_RELEASED;need_frame=1; }
            if(input_phase==INPUT_PENDING) {
                nx_result armed=content_input_arm(input);
                printf("INPUT_ARM result=%d focus=%llu window=%d keys=%zu buttons=%u draining=%zu/%u\n",(int)armed,
                    (unsigned long long)nx_focus_id(context),after.window_focused,after.physical_keys,after.physical_buttons,
                    after.draining_keys,after.draining_buttons);fflush(stdout);
                if(armed==NX_OK) { input_phase=INPUT_ACTIVE;need_frame=1; }
                else if(armed!=NX_ERROR_STATE) goto done;
            }
        }
#ifdef CONTENT_NATIVE_GUEST
        if(guest_case&&phase==SCALED_READY&&!state.working&&!state.pending) {
            if (guest.pending.count) {
                if(!status.sequence||status.sequence==UINT64_MAX||
                    !native_schedule(&worker,(native_job){.request=status.request,.sequence=status.sequence+1,.kind=5,
                        .guest_input=guest.pending})) goto done;
                guest.pending.count=0;
            } else if (input_phase==INPUT_DISCONNECTING) {
                input_phase=INPUT_DISCONNECTED;
                need_frame=1;
            } else if (inspector.pending) {
                if (!native_schedule(&worker,(native_job){.request=status.request,.sequence=inspector.ticket,
                        .kind=CONTENT_GUEST_RESOURCES})) goto done;
                inspector.pending=0;
            }
        }
#endif
#ifdef CONTENT_NATIVE_PDF
        if (pdf_case&&!need_frame&&pdf_can_navigate(phase)&&pdf_detail_missing(&document.detail))
            native_schedule_detail(&worker,document.detail.wanted);
#endif
    }
done:
#ifdef CONTENT_NATIVE_BROWSER
    if (browser_text_case&&window&&!browser_ime_disable(&editor,window)) result=1;
#endif
    if(content_input_close(&input)!=NX_OK) result=1;
    if (leased&&content_surface_complete(surface)!=NX_OK) result=1;
    if (window&&nx_window_publish_semantics(window,NULL)!=NX_OK) result=1;
    if (software&&nx_software_view_close(&software)!=NX_OK) result=1;
#ifdef CONTENT_NATIVE_PNG
    free((void *)png_source.bitmap.pixels);
#endif
    if (content_surface_destroy(&surface)!=NX_OK) result=1;
    nx_context_destroy(context);
    nx_text_field_destroy(query);
#ifdef CONTENT_NATIVE_PDF
    content_pdf_index_destroy(document.index);
    pdf_detail_clear(&document.detail);
#endif
#ifdef CONTENT_NATIVE_PNG
    content_thumbnails_clear(&document.thumbnails);
#endif
    if (native_worker_close(&worker)) result=1;
#ifdef CONTENT_NATIVE_PNG
    png_gallery_close(document.gallery);
#endif
    if (exchange&&nx_snapshot_exchange_destroy(exchange)!=NX_OK) result=1;
    if (window) nx_window_destroy(window);
    nx_font_collection_destroy(fonts); nx_font_close(font); free(data);
    unsigned expected=(!strcmp(test_case,"full")||input_case||png_case)?5u:!strcmp(test_case,"rejected-close")?1u:0u;
#ifdef CONTENT_NATIVE_GUEST
    if (guest_case) {
        expected=worker.completed;
        if (expected<7u || guest.pending.count || guest.ledger.focused || native_input_keys(&guest.ledger) || guest.ledger.buttons ||
            input_phase!=INPUT_DISCONNECTED || phase!=SCALED_READY) result=1;
    }
#endif
#ifdef CONTENT_NATIVE_BROWSER
    if (browser_text_case) {
        expected=worker.completed;
        if (expected<10u || editor.awaiting || editor.count || editor.phase!=BROWSER_HOST ||
            editor.revision<(browser_pointer_case?1u:3u)) result=1;
    }
    if (browser_ax_case) expected=6u;
#endif
    if (pdf_case) expected=pdf_url_case?2u:(pdf_select_case||pdf_properties_case||pdf_detail_case||pdf_export_case)?4u:
        pdf_zoom_case?5u:content_thumbnails_case?6u:pdf_link_reader?10u:8u;
    unsigned published=expected-(unsigned)(png_case||guest_case||(pdf_case&&!pdf_url_case));
    unsigned discarded=!strcmp(test_case,"pending-close")?1u:0u;
    if (gallery_case) {
        if (!worker_ready||atomic_load(&worker.disposals)!=worker.publications) result=1;
    } else if (!worker_ready||worker.completed!=expected||worker.publications!=published||worker.discarded!=discarded||
        atomic_load(&worker.disposals)!=published||((expected==5u)&&phase!=SCALED_READY)||
        (pdf_case&&(pdf_url_case?(phase!=READY||document.page!=1u):
            (pdf_select_case||pdf_properties_case||pdf_detail_case||pdf_export_case)?(phase!=RECOVERED||document.page!=2u):
            pdf_zoom_case?(phase!=SCALED_READY||document.page!=2u):
            content_thumbnails_case?(phase!=PDF_HIT_READY||document.page!=7u):
            (phase!=PDF_HIT_READY||document.page!=1u)))||
        (!strcmp(test_case,"rejected-close")&&phase!=OLD_REJECTED)||(!strcmp(test_case,"pending-close")&&phase!=WAITING)) result=1;
    if(input_case) {
        if(receiver.downs!=6||receiver.ups!=6||receiver.pointer_downs!=3||receiver.pointer_ups!=3||
            receiver.enters!=3||receiver.exits!=3||receiver.texts||receiver.buttons||native_input_keys(&receiver)||
            receiver.focused||input_phase!=INPUT_DISCONNECTED) result=1;
        printf("INPUT_EXIT result=%d down=%u up=%u pdown=%u pup=%u enter=%u exit=%u text=%u keys=%u buttons=%u\n",
            result,receiver.downs,receiver.ups,receiver.pointer_downs,receiver.pointer_ups,receiver.enters,receiver.exits,
            receiver.texts,native_input_keys(&receiver),receiver.buttons);
    }
    printf("CONTENT_EXIT result=%d publications=%u discarded=%u disposals=%u frames=%u completed=%u\n",
        result,worker.publications,worker.discarded,atomic_load(&worker.disposals),frames,worker.completed);
    return result;
}
