#include <nexaui/cell_grid.h>
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { COLS=16, ROWS=3 };
typedef struct app_cell { uint32_t scalar, mark; unsigned span; } app_cell;
typedef struct app_model { app_cell cells[ROWS][COLS]; unsigned cursor, anchor; } app_model;

static unsigned char *read_font(const char *path,size_t *out_size) {
    FILE *f=fopen(path,"rb"); if(!f)return NULL;
    unsigned char *bytes=NULL; long size=-1;
    if(fseek(f,0,SEEK_END)==0 && (size=ftell(f))>0 && size<64*1024*1024 &&
       fseek(f,0,SEEK_SET)==0 && (bytes=malloc((size_t)size)) &&
       fread(bytes,1,(size_t)size,f)==(size_t)size) *out_size=(size_t)size;
    else {free(bytes);bytes=NULL;}
    if(fclose(f)!=0){free(bytes);return NULL;} return bytes;
}
static void init_model(app_model *m) {
    memset(m,0,sizeof(*m));
    for(unsigned r=0;r<ROWS;++r)for(unsigned c=0;c<COLS;++c)
        m->cells[r][c]=(app_cell){' ',0,1};
    m->cells[0][0].scalar='A';
    m->cells[0][1]=(app_cell){0x4e2d,0,2};
    m->cells[0][2]=(app_cell){0,0,0};
    m->cells[0][3].scalar='B';
    m->cells[1][0]=(app_cell){'e',0x301,1};
    m->cells[1][2].scalar='X';
}
static int snapshot(const app_model *model,const nx_font *font,nx_cell_view *view) {
    for(unsigned r=0;r<ROWS;++r)for(unsigned c=0;c<COLS;++c) {
        const app_cell *source=&model->cells[r][c]; nx_cell_view *cell=&view[r*COLS+c];
        *cell=(nx_cell_view){0}; cell->span=(uint8_t)source->span;
        cell->foreground=(nx_color){.9f,.92f,1,1};
        unsigned index=r*COLS+c;
        unsigned first=model->anchor<model->cursor?model->anchor:model->cursor;
        unsigned end=model->anchor<model->cursor?model->cursor:model->anchor;
        cell->background=index>=first && index<end?(nx_color){.15f,.3f,.6f,1}:(nx_color){.04f,.05f,.08f,1};
        if(index==model->cursor)cell->flags|=NX_CELL_CURSOR_OUTLINE;
        if(r==1 && c==0)cell->flags|=NX_CELL_UNDERLINE;
        if(source->span && source->scalar && source->scalar!=' ') {
            uint16_t glyph=0;
            if(nx_font_glyph_index(font,source->scalar,&glyph)!=NX_OK)return 0;
            cell->glyphs[cell->glyph_count++]=(nx_cell_glyph){font,glyph,0,0};
            if(source->mark) {
                if(nx_font_glyph_index(font,source->mark,&glyph)!=NX_OK)return 0;
                cell->glyphs[cell->glyph_count++]=(nx_cell_glyph){font,glyph,0,0};
            }
        }
    }
    return 1;
}
/* This mapping belongs to the application, which knows cell spans and marks.
   A wide cell contributes one scalar and a combining mark contributes another. */
static int append_utf8(char *text,size_t capacity,size_t *length,uint32_t scalar) {
    unsigned n=scalar<0x80?1:scalar<0x800?2:scalar<0x10000?3:4;
    if(scalar>0x10ffff || (scalar>=0xd800 && scalar<=0xdfff) || capacity-*length<n)return 0;
    if(n==1)text[(*length)++]=(char)scalar;
    else {
        for(unsigned i=n-1;i>0;--i)text[*length+i]=(char)(0x80|(scalar&0x3f)),scalar>>=6;
        text[*length]=(char)((n==2?0xc0:n==3?0xe0:0xf0)|scalar); *length+=n;
    }
    return 1;
}
static int text_snapshot(const app_model *model,char *text,size_t capacity,size_t *length,size_t *anchor,size_t *cursor) {
    *length=0; *anchor=0; *cursor=0;
    for(unsigned r=0;r<ROWS;++r) {
        for(unsigned c=0;c<COLS;++c) {
            const app_cell *cell=&model->cells[r][c];
            if(r*COLS+c==model->cursor)*cursor=*length;
            if(r*COLS+c==model->anchor)*anchor=*length;
            if(!cell->span)continue;
            if(!append_utf8(text,capacity,length,cell->scalar) ||
               (cell->mark && !append_utf8(text,capacity,length,cell->mark)))return 0;
        }
        if(r+1<ROWS && !append_utf8(text,capacity,length,'\n'))return 0;
    }
    return 1;
}
static nx_result copy_selection(nx_window *window,const app_model *model) {
    if(model->anchor==model->cursor)return NX_OK; /* Leave an existing clipboard alone. */
    char text[ROWS*COLS*8+ROWS]; size_t length=0,anchor=0,cursor=0;
    if(!text_snapshot(model,text,sizeof(text),&length,&anchor,&cursor))return NX_ERROR_ARGUMENT;
    size_t start=anchor<cursor?anchor:cursor, end=anchor<cursor?cursor:anchor;
    return nx_window_clipboard_set_text(window,text+start,end-start);
}
int main(int argc,char **argv) {
    int verify=argc==3 && !strcmp(argv[2],"--verify");
    if(argc!=2 && !verify){fputs("usage: nexaui_cell_grid_example FONT [--verify]\n",stderr);return 2;}
    size_t font_size=0; unsigned char *bytes=read_font(argv[1],&font_size);
    nx_font *font=NULL; nx_context *ctx=NULL; nx_window *window=NULL; nx_software_view *renderer=NULL;
    int okay=0,dirty=1,waiting=0,ready=0,reported_text=0,reported_one=0,reported_three=0;
    app_model model; init_model(&model);
    nx_window_desc window_desc={"Cell grid example",320,90,0,{0}};
    if(!bytes || nx_font_open(bytes,font_size,NULL,&font)!=NX_OK || nx_context_create(NULL,&ctx)!=NX_OK ||
       nx_window_create(&window_desc,&window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK)goto cleanup;
    okay=1;
    while(okay) {
        if(nx_window_poll(window,dirty&&!waiting?16u:UINT32_MAX)!=NX_OK){okay=0;break;}
        nx_window_info info; if(nx_window_get_info(window,&info)!=NX_OK){okay=0;break;}
        if(info.should_close)break;
        int available=0; nx_event event;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(!available)break;
            if(event.type==NX_EVENT_RESIZE)dirty=1;
            if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE)goto finished;
            if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_C && !event.repeat &&
               (event.modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==(NX_MOD_CONTROL|NX_MOD_SHIFT)) {
                nx_result result=copy_selection(window,&model);
                if(result!=NX_OK)fprintf(stderr,"Clipboard copy unavailable: %d\n",(int)result);
            }
            if(event.type==NX_EVENT_KEY_DOWN && (event.key==NX_KEY_RIGHT || event.key==NX_KEY_LEFT)) {
                unsigned next=model.cursor;
                if(event.key==NX_KEY_RIGHT && next+1<ROWS*COLS) {
                    ++next;
                    if(!model.cells[next/COLS][next%COLS].span && next+1<ROWS*COLS)++next;
                } else if(event.key==NX_KEY_LEFT && next>0) {
                    --next;
                    if(!model.cells[next/COLS][next%COLS].span && next>0)--next;
                }
                if(next!=model.cursor || (!(event.modifiers&NX_MOD_SHIFT) && model.anchor!=next)) {
                    model.cursor=next;
                    if(!(event.modifiers&NX_MOD_SHIFT))model.anchor=next;
                    dirty=1;
                }
            }
            if(event.type==NX_EVENT_TEXT && model.cursor==0 && event.scalar>=32) {
                model.cells[0][0].scalar=event.scalar; model.anchor=model.cursor; dirty=1;
            }
        } while(available);
        if(!okay || !dirty)continue;
        nx_cell_view cells[ROWS*COLS]; if(!snapshot(&model,font,cells)){okay=0;break;}
        char visible_text[ROWS*COLS*8+ROWS]; size_t text_length=0,text_anchor=0,text_cursor=0;
        if(!text_snapshot(&model,visible_text,sizeof(visible_text),&text_length,&text_anchor,&text_cursor)){okay=0;break;}
        nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,info.width,info.height},
            .name={"Cell grid example",17}};
        nx_semantic_desc text={.id=2,.parent_id=1,.role=NX_ROLE_TEXT,.bounds={0,0,info.width,info.height},
            .name={"Visible terminal cells",22},.value={visible_text,text_length},.state=NX_SEM_READONLY};
        nx_cell_grid_desc desc={.bounds={0,0,info.width,info.height},.cell_width=20,.cell_height=30,
            .baseline=23,.pixels_per_em=20,.cells=cells,.rows=ROWS,.columns=COLS,.row_stride=COLS,
            .cell_capacity=ROWS*COLS,
            .glyph_cache_bytes=65536,.mask_budget_bytes=1024*1024,.cursor_color={1,.75f,.15f,1}};
        const nx_command *commands=NULL; size_t count=0; nx_software_view_outcome outcome;
        if(nx_context_set_text_raster_scale(ctx,info.scale)!=NX_OK || nx_begin_frame(ctx,info.width,info.height,0)!=NX_OK ||
           nx_draw_cell_grid(ctx,&desc)!=NX_OK || nx_semantics_add(ctx,&root,NULL)!=NX_OK ||
           nx_semantics_add_readonly_text_snapshot(ctx,&text,1,text_anchor,text_cursor,NULL)!=NX_OK ||
           nx_end_frame(ctx)!=NX_OK ||
           nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){0,0,0,1},&outcome)!=NX_OK) {okay=0;break;}
        nx_result ax=nx_window_publish_semantics(window,ctx);
        if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED){okay=0;break;}
        waiting=outcome==NX_SOFTWARE_VIEW_BUSY; dirty=outcome!=NX_SOFTWARE_VIEW_READY;
        if(outcome!=NX_SOFTWARE_VIEW_READY)continue;
        if(verify && !ready){puts("CELL_GRID_READY");fflush(stdout);ready=1;}
        if(verify && model.cells[0][0].scalar=='Z' && !reported_text){puts("CELL_GRID_TEXT 90");fflush(stdout);reported_text=1;}
        if(verify && model.cursor==1 && !reported_one){puts("CELL_GRID_CURSOR 1");fflush(stdout);reported_one=1;}
        if(verify && model.cursor==3 && !reported_three){puts("CELL_GRID_CURSOR 3");fflush(stdout);reported_three=1;}
        if(verify && reported_text && reported_one && reported_three)break;
    }
finished:
    if(verify && !(reported_text && reported_one && reported_three))okay=0;
cleanup:
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    nx_window_destroy(window); nx_context_destroy(ctx); nx_font_close(font); free(bytes);
    if(!okay)fputs("Cell-grid example failed.\n",stderr);
    return okay?0:1;
}
