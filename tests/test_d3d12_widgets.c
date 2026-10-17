#include "nexaui/nexaui.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "search_font_fixture.h"
#include "paint_scene.h"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

static int compare_pixels(const uint8_t *gpu, const uint8_t *headless,
                          uint32_t width, uint32_t height) {
    size_t stride = (size_t)width * 4u;
    for (size_t i = 0; i < stride * height; ++i) {
        if (gpu[i] != headless[i]) {
            fprintf(stderr, "widget GPU/headless mismatch at (%zu,%zu) channel %zu: %u != %u\n",
                    (i / 4u) % width, i / stride, i % 4u,
                    gpu[i], headless[i]);
            return 0;
        }
    }
    return 1;
}
#include "d3d12_paint_diagnostic.h"

static int run_theme(nx_d3d12_renderer *renderer, uint32_t width,
                     uint32_t height, nx_theme_kind kind,
                     const nx_text_layout *text, uint8_t *gpu,
                     uint8_t *headless) {
    nx_layout *layout = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_layout_desc column = nx_layout_default_desc(NX_LAYOUT_COLUMN);
    column.padding = (nx_insets){12, 12, 12, 12};
    column.gap = 8;
    nx_layout_desc leaf = nx_layout_default_desc(NX_LAYOUT_LEAF);
    leaf.width.preferred = 138;
    leaf.width.has_preferred = 1;
    leaf.height.has_preferred = 1;
    uint32_t root, nodes[4];
    nx_rect bounds[4];
    nx_size desired;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &column, &root) == NX_OK);
    for (unsigned i = 0; i < 4; ++i) {
        nx_layout_desc item = leaf;
        item.height.preferred = i == 0 ? 24.0f : i == 3 ? 12.0f : 20.0f;
        CHECK(nx_layout_add(layout, root, &item, &nodes[i]) == NX_OK);
    }
    CHECK(nx_layout_measure(layout, root, 164, 124, &desired) == NX_OK);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){8, 8, 164, 124}) == NX_OK);
    for (unsigned i = 0; i < 4; ++i)
        CHECK(nx_layout_get_rect(layout, nodes[i], &bounds[i]) == NX_OK);

    nx_theme theme;
    CHECK(nx_theme_preset(kind, &theme) == NX_OK);
    CHECK(nx_context_set_theme(ctx, &theme) == NX_OK);
    nx_control_desc button = {0}, checkbox = {0}, toggle = {0};
    button.id = 2; button.parent_id = 1; button.bounds = bounds[0];
    button.name = (nx_utf8_span){"01", 2};
    button.label_layout = text;
    button.label_origin = (nx_point){bounds[0].x + 8, bounds[0].y + 2};
    button.glyph_cache_bytes = 1024 * 1024;
    checkbox.id = 3; checkbox.parent_id = 1; checkbox.bounds = bounds[1];
    checkbox.name = (nx_utf8_span){"Confirm", 7};
    toggle.id = 4; toggle.parent_id = 1; toggle.bounds = bounds[2];
    toggle.name = (nx_utf8_span){"Enabled", 7};
    nx_value_control_desc slider = {0};
    slider.control.id = 5; slider.control.parent_id = 1;
    slider.control.bounds = bounds[3];
    slider.control.name = (nx_utf8_span){"Level", 5};
    slider.minimum = 0; slider.maximum = 100; slider.step = 5;
    int checked = 0, on = 0;
    double value = 35;
    for (unsigned frame = 0; frame < 2; ++frame) {
        if (frame) {
            nx_event event = {0};
            event.button = NX_BUTTON_LEFT;
            event.x = bounds[1].x + bounds[1].width * 0.5f;
            event.y = bounds[1].y + bounds[1].height * 0.5f;
            event.type = NX_EVENT_POINTER_DOWN;
            CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
            event.type = NX_EVENT_POINTER_UP;
            CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 180, 140, 0) == NX_OK);
        nx_color background;
        CHECK(nx_theme_color(ctx, NX_COLOR_BACKGROUND, &background) == NX_OK);
        CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, 180, 140}, background) == NX_OK);
        nx_container_desc panel = {0};
        panel.id = 1; panel.bounds = (nx_rect){8, 8, 164, 124};
        panel.name = (nx_utf8_span){"Controls", 8};
        CHECK(nx_panel(ctx, &panel) == NX_OK);
        nx_interaction_state state;
        CHECK(nx_button(ctx, &button, &state) == NX_OK);
        CHECK(nx_checkbox(ctx, &checkbox, &checked, &state) == NX_OK);
        CHECK(nx_toggle(ctx, &toggle, &on, &state) == NX_OK);
        CHECK(nx_slider(ctx, &slider, &value, &state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(checked == (int)frame);
        const nx_command *commands = NULL;
        size_t count = 0;
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count > 8);
        size_t glyphs = 0;
        for (size_t i = 0; i < count; ++i)
            if (commands[i].type == NX_COMMAND_BITMAP &&
                commands[i].bitmap.format == NX_BITMAP_A8) ++glyphs;
        CHECK(glyphs > 0);
        CHECK(nx_headless_render(commands, count, headless, width, height,
                                 (size_t)width * 4u, background) == NX_OK);
        CHECK(nx_d3d12_test_render_readback(renderer, commands, count, 1.0,
                                             background, gpu,
                                             (size_t)width * 4u) == NX_OK);
        CHECK(compare_pixels(gpu, headless, width, height));
    }
    nx_context_destroy(ctx);
    nx_layout_destroy(layout);
    return 0;
}

static int test_paints(nx_d3d12_renderer *renderer,uint32_t width,uint32_t height,uint8_t *gpu,uint8_t *headless) {
    const double scales[]={1,1.25,1.5,2};
    for(unsigned density=0;density<4;++density) {
        double scale=scales[density];
        /* An explicit diagnostic retains 150%-resource -> 100%-display
           downsampling; ordinary checks scale the entire command stream. */
        if(getenv("NEXAUI_TEST_PAINT_DOWNSAMPLE"))scale=1;
        nx_context *ui=NULL;nx_paint_image *images[3]={0};
        CHECK(nx_context_create(NULL,&ui)==NX_OK);
        CHECK(!paint_scene(ui,images,(float)scales[density]));
        const nx_command *commands;size_t count;
        CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
        nx_color clear={.02f,.04f,.08f,1};
        nx_command *scaled=malloc(count*sizeof(*scaled));CHECK(scaled);
        for(size_t i=0;i<count;++i){
            scaled[i]=commands[i];nx_command *c=&scaled[i];
            c->rect.x=(float)(c->rect.x*scale);c->rect.y=(float)(c->rect.y*scale);
            c->rect.width=(float)(c->rect.width*scale);c->rect.height=(float)(c->rect.height*scale);
            c->radius=(float)(c->radius*scale);c->thickness=(float)(c->thickness*scale);
            for(unsigned j=0;j<3;++j){c->points[j].x=(float)(c->points[j].x*scale);c->points[j].y=(float)(c->points[j].y*scale);}
        }
        CHECK(nx_headless_render(scaled,count,headless,width,height,(size_t)width*4,clear)==NX_OK);
        CHECK(nx_d3d12_test_render_readback(renderer,commands,count,scale,clear,gpu,(size_t)width*4)==NX_OK);
        unsigned maximum=0;size_t differing=0;
        for(size_t i=0;i<(size_t)width*height*4;++i){
            unsigned delta=gpu[i]>headless[i]?gpu[i]-headless[i]:headless[i]-gpu[i];
            if(delta>maximum)maximum=delta;if(delta)++differing;
        }
        printf("paint scale %.2f density %.2f: max channel delta %u; %zu differing bytes\n",scale,scales[density],maximum,differing);
        CHECK(maximum==0); /* Accepted cases remain byte-exact. */
        free(scaled);
        paint_scene_free(images);nx_context_destroy(ui);
    }
    return 0;
}
int main(void) {
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI D3D12 widget regression";
    desc.width = 180; desc.height = 140; desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK);
    nx_d3d12_renderer *renderer = NULL;
    CHECK(nx_d3d12_renderer_create(window, &renderer) == NX_OK);
    nx_d3d12_renderer_info info;
    CHECK(nx_d3d12_renderer_get_info(renderer, &info) == NX_OK);
    CHECK(info.pixel_width >= 180 && info.pixel_height >= 140);
    size_t bytes = (size_t)info.pixel_width * info.pixel_height * 4u;
    uint8_t *gpu = malloc(bytes), *headless = malloc(bytes);
    CHECK(gpu && headless);
    if(getenv("NEXAUI_TEST_PAINT_DIAGNOSE")){
        int status=diagnose_paints(renderer,info.pixel_width,info.pixel_height,gpu,headless);
        free(headless);free(gpu);nx_d3d12_renderer_destroy(renderer);nx_window_destroy(window);return status;
    }
    CHECK(test_identity_texels(renderer,info.pixel_width,info.pixel_height,gpu,headless)==0);
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_text_layout *text = NULL;
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_layout_create(fonts, "01", 2, NX_BIDI_LTR,
                                12.0, 120.0, &text) == NX_OK);
    CHECK(nx_text_layout_glyph_count(text) == 2);
    CHECK(run_theme(renderer, info.pixel_width, info.pixel_height,
                    NX_THEME_LIGHT, text, gpu, headless) == 0);
    CHECK(run_theme(renderer, info.pixel_width, info.pixel_height,
                    NX_THEME_DARK, text, gpu, headless) == 0);
    CHECK(run_theme(renderer, info.pixel_width, info.pixel_height,
                    NX_THEME_HIGH_CONTRAST, text, gpu, headless) == 0);
    CHECK(test_paints(renderer,info.pixel_width,info.pixel_height,gpu,headless)==0);
    nx_text_layout_destroy(text);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    free(headless); free(gpu);
    nx_d3d12_renderer_destroy(renderer);
    nx_window_destroy(window);
    puts("D3D12 widget and glyph frames match headless pixels");
    return 0;
}
