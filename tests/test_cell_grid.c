#include <nexaui/cell_grid.h>
#include "fixture_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) {fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1;} } while (0)
typedef struct heap {size_t live,calls,fail;} heap;
static void *allocate(void *data,size_t size) {
    heap *h=data; if (++h->calls==h->fail) return NULL;
    void *p=malloc(size); if(p)h->live+=size; return p;
}
static void deallocate(void *data,void *p,size_t size) {
    heap *h=data; if(size>h->live) abort(); h->live-=size; free(p);
}
static nx_cell_view cell(nx_color background) {
    nx_cell_view value={0}; value.span=1; value.foreground=(nx_color){1,1,1,1};
    value.background=background; return value;
}
int main(void) {
    uint8_t bytes[512]; font_fixture(bytes);
    nx_font *font=NULL; CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    nx_cell_view cells[8];
    for (unsigned i=0;i<8;++i) cells[i]=cell((nx_color){0,0,0,1});
    cells[0].background=(nx_color){.2f,.4f,.6f,1};
    cells[0].glyph_count=1; cells[0].glyphs[0]=(nx_cell_glyph){font,2,0,0};
    cells[1].span=2; cells[1].glyph_count=2;
    cells[1].glyphs[0]=(nx_cell_glyph){font,2,0,0};
    cells[1].glyphs[1]=(nx_cell_glyph){font,2,1,-2};
    cells[1].flags=NX_CELL_UNDERLINE;
    cells[2].span=0; cells[3].flags=NX_CELL_CURSOR_OUTLINE;
    nx_cell_grid_desc d={.bounds={4,4,79,36},.cell_width=20,.cell_height=18,
        .baseline=14,.pixels_per_em=16,.cells=cells,.rows=2,.columns=4,.row_stride=4,.cell_capacity=8,
        .glyph_cache_bytes=65536,.mask_budget_bytes=65536,.cursor_color={1,0,0,1}};
    size_t row=99,col=99;
    CHECK(nx_cell_grid_hit_test(&d,25,5,&row,&col)==NX_OK && row==0 && col==1);
    CHECK(nx_cell_grid_hit_test(&d,45,5,&row,&col)==NX_OK && row==0 && col==1);
    CHECK(nx_cell_grid_hit_test(&d,64,23,&row,&col)==NX_OK && row==1 && col==3);
    CHECK(nx_cell_grid_hit_test(&d,83,5,&row,&col)==NX_ERROR_ARGUMENT && row==0 && col==0);
    CHECK(nx_cell_grid_hit_test(&d,5,5,&row,&row)==NX_ERROR_ARGUMENT && row==0);
    heap h={0}; nx_context_desc context_desc={0};
    context_desc.allocator=(nx_allocator){&h,allocate,deallocate};
    nx_context *ctx=NULL; CHECK(nx_context_create(&context_desc,&ctx)==NX_OK);
    nx_font *temporary_font=NULL; CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&temporary_font)==NX_OK);
    cells[0].glyphs[0].font=temporary_font;
    cells[1].glyphs[0].font=temporary_font;
    cells[1].glyphs[1].font=temporary_font;
    CHECK(nx_context_set_text_raster_scale(ctx,1.5f)==NX_OK);
    CHECK(nx_begin_frame(ctx,100,60,0)==NX_OK);
    CHECK(nx_draw_cell_grid(ctx,&d)==NX_OK);
    nx_font_close(temporary_font);
    cells[0].glyphs[0].font=font;
    cells[1].glyphs[0].font=font;
    cells[1].glyphs[1].font=font;
    CHECK(nx_end_frame(ctx)==NX_OK);
    const nx_command *commands=NULL;size_t count=0;
    CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK && count>10);
    unsigned masks=0; int outline=0,underline=0;
    for (size_t i=0;i<count;++i) {
        if (commands[i].type==NX_COMMAND_BITMAP) {
            CHECK(commands[i].bitmap.format==NX_BITMAP_A8 && commands[i].bitmap.pixels);
            ++masks;
        }
        if (commands[i].type==NX_COMMAND_STROKE_RECT && commands[i].rect.x==64) outline=1;
        if (commands[i].type==NX_COMMAND_FILL_RECT && commands[i].rect.x==24 &&
            commands[i].rect.width==40 && commands[i].rect.height==1) underline=1;
    }
    CHECK(masks==3 && outline && underline);
    uint8_t pixels[100*60*4];
    CHECK(nx_headless_render_scaled(commands,count,pixels,100,60,400,(nx_color){0},1)==NX_OK);
    size_t p=(size_t)(5*100+5)*4u;
    CHECK(pixels[p]==51 && pixels[p+1]==102 && pixels[p+2]==153 && pixels[p+3]==255);
    p=(size_t)(5*100+83)*4u;
    CHECK(pixels[p]==0 && pixels[p+1]==0 && pixels[p+2]==0 && pixels[p+3]==0);
    CHECK(nx_begin_frame(ctx,100,60,0)==NX_OK);
    CHECK(nx_fill_rect(ctx,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
    d.mask_budget_bytes=1;
    CHECK(nx_draw_cell_grid(ctx,&d)==NX_ERROR_LIMIT);
    CHECK(nx_end_frame(ctx)==NX_OK && nx_frame_commands(ctx,&commands,&count)==NX_OK && count==1);
    d.mask_budget_bytes=65536;
    CHECK(nx_begin_frame(ctx,100,60,0)==NX_OK);
    CHECK(nx_fill_rect(ctx,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
    h.fail=h.calls+1;
    CHECK(nx_draw_cell_grid(ctx,&d)==NX_ERROR_MEMORY);
    h.fail=0;
    CHECK(nx_end_frame(ctx)==NX_OK && nx_frame_commands(ctx,&commands,&count)==NX_OK && count==1);
    cells[2].span=1;
    CHECK(nx_cell_grid_hit_test(&d,5,5,&row,&col)==NX_ERROR_ARGUMENT && row==0 && col==0);
    cells[2].span=0;
    d.cell_capacity=7;
    CHECK(nx_cell_grid_hit_test(&d,5,5,&row,&col)==NX_ERROR_ARGUMENT);
    d.cell_capacity=8;
    d.row_stride=SIZE_MAX;
    CHECK(nx_cell_grid_hit_test(&d,5,5,&row,&col)==NX_ERROR_LIMIT);
    nx_context_destroy(ctx); CHECK(h.live==0);

    d.row_stride=4;
    unsigned failures=0; int success=0;
    for (size_t fail=1;fail<200 && !success;++fail) {
        heap trial={0}; nx_context_desc trial_desc={0};
        trial_desc.allocator=(nx_allocator){&trial,allocate,deallocate};
        nx_context *candidate=NULL; CHECK(nx_context_create(&trial_desc,&candidate)==NX_OK);
        CHECK(nx_begin_frame(candidate,100,60,0)==NX_OK);
        CHECK(nx_fill_rect(candidate,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
        trial.fail=trial.calls+fail;
        nx_result result=nx_draw_cell_grid(candidate,&d);
        trial.fail=0;
        if (result==NX_OK) success=1;
        else { CHECK(result==NX_ERROR_MEMORY); ++failures; }
        CHECK(nx_end_frame(candidate)==NX_OK && nx_frame_commands(candidate,&commands,&count)==NX_OK);
        if (!success) CHECK(count==1);
        nx_context_destroy(candidate); CHECK(trial.live==0);
    }
    CHECK(success && failures>3);

    nx_font *faces[NX_CELL_GRID_MAX_FONTS+1u]={0};
    nx_cell_view many[NX_CELL_GRID_MAX_FONTS+1u];
    for(size_t i=0;i<NX_CELL_GRID_MAX_FONTS+1u;++i) {
        CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&faces[i])==NX_OK);
        many[i]=cell((nx_color){0,0,0,1}); many[i].glyph_count=1;
        many[i].glyphs[0]=(nx_cell_glyph){faces[i],2,0,0};
    }
    nx_cell_grid_desc many_desc=d;
    many_desc.cells=many; many_desc.rows=1; many_desc.columns=NX_CELL_GRID_MAX_FONTS+1u;
    many_desc.row_stride=many_desc.columns; many_desc.cell_capacity=NX_CELL_GRID_MAX_FONTS+1u;
    many_desc.bounds=(nx_rect){0,0,400,30};
    nx_context *many_ctx=NULL; CHECK(nx_context_create(NULL,&many_ctx)==NX_OK);
    CHECK(nx_begin_frame(many_ctx,400,30,0)==NX_OK);
    CHECK(nx_fill_rect(many_ctx,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
    CHECK(nx_draw_cell_grid(many_ctx,&many_desc)==NX_ERROR_LIMIT);
    CHECK(nx_end_frame(many_ctx)==NX_OK && nx_frame_commands(many_ctx,&commands,&count)==NX_OK && count==1);
    nx_context_destroy(many_ctx);
    for(size_t i=0;i<NX_CELL_GRID_MAX_FONTS+1u;++i)nx_font_close(faces[i]);

    size_t total=240u*100u;
    nx_cell_view *large=calloc(total,sizeof(*large)); CHECK(large);
    for(size_t i=0;i<total;++i)large[i]=cell((nx_color){0,0,0,1});
    nx_cell_grid_desc large_desc=d;
    large_desc.cells=large; large_desc.rows=100; large_desc.columns=240; large_desc.row_stride=240; large_desc.cell_capacity=total;
    large_desc.bounds=(nx_rect){0,0,240,100}; large_desc.cell_width=1; large_desc.cell_height=1; large_desc.baseline=1;
    nx_context *large_ctx=NULL; CHECK(nx_context_create(NULL,&large_ctx)==NX_OK);
    CHECK(nx_begin_frame(large_ctx,240,100,0)==NX_OK && nx_draw_cell_grid(large_ctx,&large_desc)==NX_OK && nx_end_frame(large_ctx)==NX_OK);
    CHECK(nx_frame_commands(large_ctx,&commands,&count)==NX_OK && count>=total && count<total*2u);
    large_desc.rows=137;
    CHECK(nx_cell_grid_hit_test(&large_desc,1,1,&row,&col)==NX_ERROR_LIMIT);
    nx_context_destroy(large_ctx); free(large);
    nx_font_close(font);
    puts("Fixed cells, positioned marks, selection colors, bounded viewport and failure rollback passed.");
    return 0;
}
