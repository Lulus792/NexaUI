#include "scene.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_BYTES (2u * 1024u * 1024u)
#define THUMB_W 256u
#define THUMB_H 160u
#define TRY(call) do { nx_result r_ = (call); if (r_ != NX_OK) { \
    fprintf(stderr, "NexaCloud %d: %s => %d\n", __LINE__, #call, r_); return 0; } } while (0)
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "NexaCloud %d: %s\n", __LINE__, #x); return 0; } } while (0)

enum { OVERVIEW, FILES, TRANSFERS, FAVORITES, TRASH, SETTINGS };
typedef struct cloud_text {
    char text[192];
    unsigned size;
    unsigned width;
    uint64_t used_frame;
    nx_text_layout *layout;
    nx_text_stamp *stamp;
} cloud_text;
typedef struct cloud_file {
    char name[48];
    const char *metadata;
    int folder, favorite, deleted;
    int parent;
    char description[128];
    nx_text_field *description_field;
} cloud_file;

struct cloud_scene {
    nx_context *ui;
    nx_window *window; /* Borrowed, optional for clipboard and IME. */
    nx_cursor_shape cursor;
    unsigned char *font_bytes;
    nx_font *font;
    nx_font_collection *fonts;
    cloud_text text[512];
    size_t text_count;
    nx_text_field *search;
    uint8_t thumbnails[8][THUMB_W * THUMB_H * 4u];
    cloud_file files[20];
    unsigned file_count;
    int page, selected, details, list_view, dialog, paused, light, reduced_motion;
    int pending_theme, folder_depth;
    float detail_x, detail_y;
    float scroll_x, scroll_y, dashboard_y, dashboard_x;
    float width, height, text_scale, requested_text_scale;
    float nav_x, nav_y;
    uint64_t frame_number;
    uint64_t previous_focus;
    int reflow_changed;
    nx_theme theme;
    uint64_t label_id;
    char notice[160];
    double notice_seconds;
};

static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
static nx_color rgb(unsigned r, unsigned g, unsigned b) {
    return (nx_color){r / 255.0f, g / 255.0f, b / 255.0f, 1};
}
static nx_color color(cloud_scene *s, nx_color_token token) { return s->theme.colors[token]; }
static nx_rect box(float x, float y, float w, float h) { return (nx_rect){x, y, w, h}; }
static void notify(cloud_scene *s, const char *message) {
    snprintf(s->notice, sizeof(s->notice), "%s", message); s->notice_seconds = 5;
}

static nx_text_layout *wrapped_layout(cloud_scene *s, const char *text, unsigned ppem, float max_width) {
    ppem = (unsigned)floorf(ppem * s->text_scale + 0.5f);
    unsigned width = (unsigned)fminf(10000, fmaxf(1, floorf(max_width)));
    for (size_t i = 0; i < s->text_count; ++i)
        if (s->text[i].size == ppem && s->text[i].width == width && strcmp(s->text[i].text, text) == 0) {
            s->text[i].used_frame = s->frame_number;
            return s->text[i].layout;
        }
    if (strlen(text) >= sizeof(s->text[0].text)) return NULL;
    size_t slot = s->text_count;
    if (slot == 512) {
        /* Never invalidate a stamp already borrowed by this frame's commands.
           begin_frame has discarded the previous command stream before eviction. */
        for (size_t i = 0; i < s->text_count; ++i)
            if (s->text[i].used_frame != s->frame_number &&
                (slot == 512 || s->text[i].used_frame < s->text[slot].used_frame)) slot = i;
        if (slot == 512) return NULL;
    }
    nx_text_layout *new_layout = NULL;
    nx_text_stamp *new_stamp = NULL;
    if (nx_text_layout_create(s->fonts, text, strlen(text), NX_BIDI_LTR,
                              ppem, width, &new_layout) != NX_OK) return NULL;
    if (nx_text_stamp_create(new_layout, CACHE_BYTES, NULL, &new_stamp) != NX_OK) {
        nx_text_layout_destroy(new_layout); return NULL;
    }
    cloud_text *t = &s->text[slot];
    nx_text_stamp_destroy(t->stamp); nx_text_layout_destroy(t->layout);
    t->layout = new_layout; t->stamp = new_stamp;
    memcpy(t->text, text, strlen(text) + 1u); t->size = ppem; t->width = width;
    t->used_frame = s->frame_number;
    if (slot == s->text_count) ++s->text_count;
    return t->layout;
}

static nx_text_layout *layout(cloud_scene *s, const char *value, unsigned size) {
    return wrapped_layout(s, value, size, 10000);
}

static int wrapped_text(cloud_scene *s, uint64_t parent, const char *value,
                        float x, float y, unsigned size, nx_color tint, float width, float *height) {
    nx_text_layout *t = wrapped_layout(s, value, size, width);
    REQUIRE(t);
    const nx_text_stamp *stamp = NULL;
    for (size_t i = 0; i < s->text_count; ++i) if (s->text[i].layout == t) stamp = s->text[i].stamp;
    TRY(nx_label_stamp(s->ui, s->label_id++, parent,
        box(x, y, (float)nx_text_layout_width(t), (float)nx_text_layout_height(t)),
        stamp, (nx_point){floorf(x + .5f), floorf(y + .5f)}, tint));
    if (height) *height = (float)nx_text_layout_height(t);
    return 1;
}

static int text(cloud_scene *s, uint64_t parent, const char *value,
                 float x, float y, unsigned size, nx_color tint) {
    return wrapped_text(s, parent, value, x, y, size, tint, 10000, NULL);
}

static int card(cloud_scene *s, uint64_t id, uint64_t parent, nx_rect r, const char *name) {
    nx_container_desc d = {0}; d.id = id; d.parent_id = parent; d.bounds = r; d.name = span(name);
    TRY(nx_card(s->ui, &d)); return 1;
}

static int action(cloud_scene *s, uint64_t id, uint64_t parent, nx_rect r,
                    const char *name, nx_icon icon, nx_action_variant variant,
                    int selected, int disabled, int *clicked) {
    nx_action_desc d = {0};
    d.control.id = id; d.control.parent_id = parent; d.control.bounds = r;
    d.control.name = span(name); d.control.disabled = disabled;
    d.control.glyph_cache_bytes = CACHE_BYTES;
    d.variant = variant; d.selected = selected; d.icon = icon;
    float side = fminf(23, r.height - 12);
    d.icon_bounds = box(r.x + 14, r.y + (r.height - side) / 2, side, side);
    if (r.width < 110) d.icon_bounds.x = r.x + (r.width - side) / 2;
    if (r.width >= 110 || (icon == NX_ICON_NONE && r.width >= 70)) {
        nx_text_layout *label = wrapped_layout(s, name, 15, r.width - (icon == NX_ICON_NONE ? 28 : 63)); REQUIRE(label);
        for (size_t i = 0; i < s->text_count; ++i) if (s->text[i].layout == label) d.label_stamp = s->text[i].stamp;
        d.control.label_origin = (nx_point){floorf(r.x + (icon == NX_ICON_NONE ? 14 : 49) + .5f),
            floorf(r.y + (r.height - (float)nx_text_layout_height(label)) / 2 + .5f)};
    }
    nx_interaction_state state;
    TRY(nx_action(s->ui, &d, &state));
    if (state.hovered) s->cursor = NX_CURSOR_LINK;
    if (clicked) *clicked = state.clicked != 0;
    return 1;
}

static int icon(cloud_scene *s, nx_icon kind, nx_rect r, nx_color tint) {
    TRY(nx_draw_icon(s->ui, kind, r, fmaxf(1.4f, r.width / 15), tint)); return 1;
}
static int progress(cloud_scene *s, uint64_t id, uint64_t parent, nx_rect r,
                      const char *name, double value) {
    nx_progress_desc d = {id, parent, r, {name, strlen(name)}, value};
    TRY(nx_progress(s->ui, &d)); return 1;
}

/* Original, generated-in-code sample artwork. No photos or user files are
   embedded. Each RGBA thumbnail is borrowed unchanged by both renderers. */
static void make_thumbnails(cloud_scene *s) {
    for (unsigned n = 0; n < 8; ++n) {
        for (unsigned y = 0; y < THUMB_H; ++y) for (unsigned x = 0; x < THUMB_W; ++x) {
            float u = (float)x / THUMB_W, v = (float)y / THUMB_H;
            float ridge = 0.38f + 0.22f * sinf(u * 9 + n * 0.7f) * sinf(u * 17 + 1);
            float front = 0.62f + 0.16f * sinf(u * 12 + n * 1.3f);
            nx_color c;
            int night = n == 3 || n == 4;
            if (night) {
                c = rgb(7 + y / 12, 14 + y / 9, 38 + y / 3);
                if (n == 4) {
                    float aurora = expf(-powf((v - 0.32f - 0.18f * sinf(u * 7)) * 12, 2));
                    c.g += aurora * 0.55f; c.b += aurora * 0.16f;
                }
                if (((x * 173u + y * 337u + n * 83u) % 1297u) < 3u && v < ridge) c = rgb(190, 216, 255);
            } else if (n == 7) c = rgb(77 + y / 2, 74 + y / 3, 121 - y / 5);
            else c = rgb(66 + y / 2, 126 + y / 3, 180 + y / 4);
            if (v > ridge) c = night ? rgb(12, 24, 43) : rgb(77, 113, 133);
            if (!night && v > ridge && v < ridge + 0.035f && ridge < 0.4f) c = rgb(210, 225, 233);
            if (v > front) c = night ? rgb(4, 16, 27) : n == 1 ? rgb(24, 67, 49) : rgb(31, 70, 73);
            if ((n == 0 || n == 5 || n == 7) && v > 0.7f) {
                float glint = 0.5f + 0.5f * sinf(x * 0.16f + y * 1.3f);
                c = n == 7 ? rgb(139 + (unsigned)(55 * glint), 103, 100) :
                    rgb(38 + (unsigned)(45 * glint), 104 + (unsigned)(35 * glint), 132 + (unsigned)(25 * glint));
            }
            if (n == 1 || n == 2) {
                for (unsigned k = 0; k < 7; ++k) {
                    float tree_x = 0.04f + k * 0.16f;
                    float tree_y = 0.43f + 0.08f * sinf(k * 3.0f);
                    if (v > tree_y && v < 0.9f && fabsf(u - tree_x) < (v - tree_y) * 0.15f)
                        c = n == 2 ? rgb(53, 79, 86) : rgb(10, 42, 35);
                }
            }
            uint8_t *p = &s->thumbnails[n][(y * THUMB_W + x) * 4u];
            p[0] = (uint8_t)(fminf(c.r, 1) * 255); p[1] = (uint8_t)(fminf(c.g, 1) * 255);
            p[2] = (uint8_t)(fminf(c.b, 1) * 255); p[3] = 255;
        }
    }
}

cloud_scene *cloud_create(const char *font_path) {
    if (!font_path) return NULL;
    cloud_scene *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->text_scale = s->requested_text_scale = 1; s->details = 1; s->selected = 0;
    FILE *f = fopen(font_path, "rb");
    long length = -1;
    if (f && fseek(f, 0, SEEK_END) == 0) length = ftell(f);
    int okay = length > 0 && length <= 64 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0;
    if (okay) s->font_bytes = malloc((size_t)length);
    okay = okay && s->font_bytes && fread(s->font_bytes, 1, (size_t)length, f) == (size_t)length;
    if (f && fclose(f) != 0) okay = 0;
    if (okay) okay = nx_context_create(NULL, &s->ui) == NX_OK &&
        nx_font_open(s->font_bytes, (size_t)length, NULL, &s->font) == NX_OK &&
        nx_font_collection_create(NULL, &s->fonts) == NX_OK &&
        nx_font_collection_add(s->fonts, s->font) == NX_OK &&
        nx_text_field_create("", 0, 16384, &s->search) == NX_OK;
    if (!okay) { cloud_destroy(s); return NULL; }
    nx_theme_preset(NX_THEME_MIDNIGHT, &s->theme);
    nx_context_set_theme(s->ui, &s->theme);
    static const cloud_file initial[] = {
        {"Bergsee.jpg", "JPG · 3.2 MB", 0, 1, 0, 0, "Bergsee in den Alpen. Sommer 2026.", NULL},
        {"Wald.png", "PNG · 2.1 MB", 0, 0, 0, 0, "", NULL},
        {"Nebel.jpg", "JPG · 2.8 MB", 0, 0, 0, 0, "", NULL},
        {"Sternenhimmel.jpg", "JPG · 4.6 MB", 0, 1, 0, 0, "", NULL},
        {"Nordlicht.jpg", "JPG · 4.2 MB", 0, 0, 0, 0, "", NULL},
        {"Fluss.png", "PNG · 2.4 MB", 0, 0, 0, 0, "", NULL},
        {"Landschaft.jpg", "JPG · 3.1 MB", 0, 0, 0, 0, "", NULL},
        {"Sonnenuntergang.jpg", "JPG · 2.9 MB", 0, 0, 0, 0, "", NULL},
        {"Jahreszeiten", "Ordner · 12 Elemente", 1, 0, 0, 0, "", NULL},
        {"Reisen", "Ordner · 28 Elemente", 1, 0, 0, 0, "", NULL},
        {"Projekte", "Ordner · 8 Elemente", 1, 0, 0, 0, "", NULL},
        {"Hintergründe", "Ordner · 15 Elemente", 1, 0, 0, 0, "", NULL}
    };
    memcpy(s->files, initial, sizeof(initial)); s->file_count = 12;
    make_thumbnails(s); return s;
}

void cloud_destroy(cloud_scene *s) {
    if (!s) return;
    nx_context_destroy(s->ui);
    nx_text_field_destroy(s->search);
    for (unsigned i = 0; i < s->file_count; ++i) nx_text_field_destroy(s->files[i].description_field);
    for (size_t i = 0; i < s->text_count; ++i) {
        nx_text_stamp_destroy(s->text[i].stamp); nx_text_layout_destroy(s->text[i].layout);
    }
    nx_font_collection_destroy(s->fonts); nx_font_close(s->font);
    free(s->font_bytes); free(s);
}
nx_context *cloud_context(cloud_scene *s) { return s->ui; }
void cloud_set_window(cloud_scene *s, nx_window *window) { s->window = window; }
nx_cursor_shape cloud_cursor(const cloud_scene *s) { return s->cursor; }
int cloud_set_text_scale(cloud_scene *s, float scale) {
    if (!s || !isfinite(scale) || scale < 1 || scale > 2) return 0;
    s->requested_text_scale = scale; return 1;
}
float cloud_text_scale(const cloud_scene *s) { return s ? s->requested_text_scale : 1; }

static int sidebar(cloud_scene *s, float width) {
    TRY(nx_fill_rect(s->ui, box(0, 0, width, s->height), color(s, NX_COLOR_BACKGROUND)));
    TRY(nx_fill_rect(s->ui, box(width - 1, 0, 1, s->height), color(s, NX_COLOR_BORDER)));
    REQUIRE(icon(s, NX_ICON_CLOUD, box(width > 100 ? 23.0f : 18.0f, 25, 42, 42), color(s, NX_COLOR_ACCENT)));
    if (width > 100) {
        REQUIRE(text(s, 1, "NexaCloud", 78, 25, 26, color(s, NX_COLOR_TEXT)));
        REQUIRE(text(s, 1, "Deine Daten. Dein Zentrum.", 79, 25 + 36 * s->text_scale, 10, color(s, NX_COLOR_TEXT_MUTED)));
    }
    static const char *names[] = {"Übersicht", "Dateien", "Übertragungen", "Favoriten", "Papierkorb", "Einstellungen"};
    static const nx_icon icons[] = {NX_ICON_HOME, NX_ICON_FOLDER, NX_ICON_TRANSFER, NX_ICON_STAR, NX_ICON_TRASH, NX_ICON_SETTINGS};
    int next_page = s->page;
    float nav_scale = width > 100 ? s->text_scale : 1;
    int show_status = width > 200 && s->height > 760 * s->text_scale;
    float nav_top = width > 100 ? 100 * s->text_scale : 100;
    nx_scroll_area_desc nav = {0}; nav.id = 35; nav.parent_id = 1; nav.name = span("Navigation scrollen");
    nav.viewport = box(0, nav_top, width, s->height - nav_top - 16 - (show_status ? 188 * s->text_scale : 0));
    nav.content_size = (nx_size){width, 12 + 6 * 58 * nav_scale}; nav.axes = NX_SCROLL_Y; nav.thumb_y_id = 36;
    nx_scroll_info nav_info; TRY(nx_scroll_area_begin(s->ui, &nav, &s->nav_x, &s->nav_y, &nav_info));
    for (int i = 0; i < 6; ++i) {
        int clicked;
        REQUIRE(action(s, 10u + i, 35, box(12, nav_top + 12 + i * 58.0f * nav_scale - s->nav_y,
            width - 24, 48 * nav_scale), names[i], icons[i], NX_ACTION_NAVIGATION,
            s->page == i, 0, &clicked));
        if (clicked) next_page = i;
        if (i == TRANSFERS && width > 200 && s->text_scale == 1) {
            TRY(nx_fill_round_rect(s->ui, box(width - 41, 128 + i * 58.0f - s->nav_y, 21, 21), 11, color(s, NX_COLOR_ACCENT)));
            REQUIRE(text(s, 35, "2", width - 35, 130 + i * 58.0f - s->nav_y, 13, color(s, NX_COLOR_ACCENT_TEXT)));
        }
    }
    TRY(nx_scroll_area_end(s->ui));
    if (next_page != s->page) {
        s->page = next_page; s->scroll_y = s->dashboard_y = 0; s->folder_depth = 0;
        TRY(nx_text_field_set_text(s->search, "", 0));
    }
    if (show_status) {
        float scale = s->text_scale, y = s->height - 188 * scale;
        REQUIRE(card(s, 30, 1, box(16, y, width - 32, 145 * scale), "Verbindungsstatus"));
        TRY(nx_fill_round_rect(s->ui, box(32, y + 20, 10, 10), 5, color(s, NX_COLOR_SUCCESS)));
        REQUIRE(text(s, 30, "Verbunden · Demo", 51, y + 16, 14, color(s, NX_COLOR_SUCCESS)));
        REQUIRE(text(s, 30, "Raspberry Pi 5", 51, y + 45 * scale, 13, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 30, "192.168.4.1", 51, y + 66 * scale, 13, color(s, NX_COLOR_TEXT_MUTED)));
        int click;
        REQUIRE(action(s, 31, 30, box(28, y + 99 * scale, width - 56, 32 * scale), "Systeminformationen", NX_ICON_NONE, NX_ACTION_SECONDARY, 0, 0, &click));
        if (click) { s->page = SETTINGS; s->dialog = 4; }
    }
    return 1;
}

static int search_field(cloud_scene *s, nx_rect bounds) {
    REQUIRE(icon(s, NX_ICON_SEARCH, box(bounds.x - 28, bounds.y + 11, 20, 20), color(s, NX_COLOR_TEXT_MUTED)));
    nx_text_field_desc d = {0}; d.id = 40; d.parent_id = 1; d.bounds = bounds;
    d.name = span("In NexaCloud suchen"); d.fonts = s->fonts; d.pixels_per_em = floor(15 * s->text_scale);
    d.glyph_cache_bytes = CACHE_BYTES; d.ime_window = s->window;
    if (s->window) TRY(nx_text_clipboard_for_window(s->window, &d.clipboard));
    nx_interaction_state state; int valid, submitted;
    TRY(nx_text_field_widget(s->ui, &d, s->search, &state, &valid, &submitted));
    if (state.hovered) s->cursor = NX_CURSOR_TEXT;
    nx_utf8_span query; TRY(nx_text_field_text(s->search, &query));
    if (!query.length && nx_focus_id(s->ui) != 40)
        REQUIRE(text(s, 1, bounds.width >= 200 * s->text_scale ? "In NexaCloud suchen …" : "Suchen …",
            bounds.x + 12, bounds.y + 10, 15, color(s, NX_COLOR_TEXT_MUTED)));
    if (query.length && s->page == OVERVIEW) s->page = FILES;
    return 1;
}

static int topbar(cloud_scene *s, float x) {
    float available = s->width - x - 32;
    int click;
    if (available > 820 && s->text_scale == 1) {
        REQUIRE(action(s, 43, 1, box(x + 8, 27, 38, 42), s->folder_depth ? "Zurück zu Natur" : "Zur Übersicht", s->folder_depth ? NX_ICON_CHEVRON_LEFT : NX_ICON_HOME, NX_ACTION_QUIET, 0, 0, &click));
        if (click) { if (s->folder_depth) s->folder_depth = 0; else s->page = OVERVIEW; }
        REQUIRE(icon(s, NX_ICON_CHEVRON_RIGHT, box(x + 59, 39, 14, 18), color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 1, s->page == OVERVIEW ? "Übersicht" : s->page == SETTINGS ? "Einstellungen" : "Dateien", x + 88, 38, 15, color(s, NX_COLOR_TEXT_MUTED)));
        if (s->page == FILES) {
            REQUIRE(icon(s, NX_ICON_CHEVRON_RIGHT, box(x + 155, 39, 14, 18), color(s, NX_COLOR_TEXT_MUTED)));
            REQUIRE(text(s, 1, s->folder_depth ? s->files[s->folder_depth - 1].name : "Natur", x + 181, 38, 15, color(s, NX_COLOR_TEXT)));
        }
    } else if (s->folder_depth) {
        REQUIRE(action(s, 43, 1, box(x + 8, 27, 38, 42), "Zurück zu Natur", NX_ICON_CHEVRON_LEFT, NX_ACTION_QUIET, 0, 0, &click));
        if (click) s->folder_depth = 0;
    }
    float search_width = fminf(380 * s->text_scale, available - 143 - (s->folder_depth && (available <= 820 || s->text_scale > 1) ? 54 : 0));
    float search_x = s->width - search_width - 140;
    REQUIRE(search_field(s, box(search_x, 26, search_width, 22 + 22 * s->text_scale)));
    REQUIRE(action(s, 41, 1, box(s->width - 125, 27, 42, 42), "Rasteransicht", NX_ICON_GRID,
        NX_ACTION_QUIET, !s->list_view, 0, &click)); if (click) s->list_view = 0;
    REQUIRE(action(s, 42, 1, box(s->width - 76, 27, 42, 42), "Listenansicht", NX_ICON_LIST,
        NX_ACTION_QUIET, s->list_view, 0, &click)); if (click) s->list_view = 1;
    TRY(nx_fill_rect(s->ui, box(x, 69 + 20 * s->text_scale, s->width - x, 1), color(s, NX_COLOR_BORDER)));
    return 1;
}

static float transfer_height(cloud_scene *s, float width, int upload) {
    const char *caption = upload ? "62 % · 892 MB von 1.4 GB" : "28 % · 122 MB von 436 MB";
    const char *status = s->paused ? "Pausiert" : upload ? "12 MB/s · Noch 42 Sekunden" : "8.4 MB/s · Noch 1 Minute";
    nx_text_layout *a = wrapped_layout(s, caption, 13, width - 80);
    nx_text_layout *b = wrapped_layout(s, status, 12, width - 80);
    if (!a || !b) return -1;
    return 36 * s->text_scale + 44 + (float)nx_text_layout_height(a) + (float)nx_text_layout_height(b);
}

static int transfer_card(cloud_scene *s, uint64_t id, uint64_t parent, nx_rect r, int upload) {
    REQUIRE(card(s, id, parent, r, upload ? "Upload simulation.mp4" : "Download data.zip"));
    nx_color tint = upload ? rgb(174, 86, 255) : color(s, NX_COLOR_SUCCESS);
    REQUIRE(icon(s, upload ? NX_ICON_UPLOAD : NX_ICON_DOWNLOAD, box(r.x + 16, r.y + 24, 28, 28), tint));
    float tx = r.x + 62, y = r.y + 12, line;
    REQUIRE(wrapped_text(s, id, upload ? "simulation.mp4" : "data.zip", tx, y, 15, color(s, NX_COLOR_TEXT), r.width - 80, &line));
    y += line + 12;
    nx_theme override = {0}; TRY(nx_theme_set_color(&override, NX_COLOR_ACCENT, upload ? color(s, NX_COLOR_ACCENT) : tint));
    TRY(nx_push_theme(s->ui, &override));
    REQUIRE(progress(s, id + 1, id, box(tx, y, r.width - 80, 10), "Übertragungsfortschritt", upload ? .62 : .28));
    TRY(nx_pop_theme(s->ui));
    y += 22;
    REQUIRE(wrapped_text(s, id, upload ? "62 % · 892 MB von 1.4 GB" : "28 % · 122 MB von 436 MB", tx, y, 13, color(s, NX_COLOR_TEXT_MUTED), r.width - 80, &line));
    y += line + 6;
    REQUIRE(wrapped_text(s, id, s->paused ? "Pausiert" : upload ? "12 MB/s · Noch 42 Sekunden" : "8.4 MB/s · Noch 1 Minute", tx, y, 12, color(s, NX_COLOR_TEXT_MUTED), r.width - 80, NULL));
    return 1;
}

static int chart(cloud_scene *s, uint64_t parent, nx_rect r) {
    float scale = s->text_scale, text_h, header_y = r.y + 18;
    REQUIRE(wrapped_text(s, parent, "Datenrate (letzte 10 Minuten)", r.x + 20, header_y, 17, color(s, NX_COLOR_TEXT), r.width - 40, &text_h));
    header_y += text_h + 12;
    REQUIRE(wrapped_text(s, parent, "Download 28.4 MB/s", r.x + 20, header_y, 12, color(s, NX_COLOR_ACCENT), r.width - 40, &text_h)); header_y += text_h + 6;
    REQUIRE(wrapped_text(s, parent, "Upload 6.7 MB/s", r.x + 20, header_y, 12, rgb(190, 111, 255), r.width - 40, &text_h)); header_y += text_h + 20;
    nx_rect plot = box(r.x + 58 * scale + 10, header_y, r.width - 58 * scale - 34, r.y + r.height - header_y - 42 * scale);
    for (unsigned i = 0; i < 4; ++i) {
        float y = plot.y + plot.height * i / 3;
        TRY(nx_draw_line(s->ui, (nx_point){plot.x, y}, (nx_point){plot.x + plot.width, y}, 1, color(s, NX_COLOR_BORDER)));
    }
    REQUIRE(text(s, parent, "100 MB/s", r.x + 8, plot.y - 7, 10, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, parent, "50 MB/s", r.x + 8, plot.y + plot.height / 2 - 7, 10, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, parent, "0", r.x + 25 * scale, plot.y + plot.height - 10, 11, color(s, NX_COLOR_TEXT_MUTED)));
    for (unsigned i = 0; i <= 5; ++i) {
        float x = plot.x + plot.width * i / 5;
        TRY(nx_draw_line(s->ui, (nx_point){x, plot.y}, (nx_point){x, plot.y + plot.height}, 1, color(s, NX_COLOR_BORDER)));
        static const char *times[] = {"12:26", "12:28", "12:30", "12:32", "12:34", "12:36"};
        if (scale == 1 || i == 0 || i == 3 || i == 5) {
            nx_text_layout *tick = layout(s, times[i], 11); REQUIRE(tick);
            float width = (float)nx_text_layout_width(tick);
            float left = fmaxf(r.x + 8, fminf(x - width / 2, r.x + r.width - 8 - width));
            REQUIRE(text(s, parent, times[i], left, plot.y + plot.height + 14, 11, color(s, NX_COLOR_TEXT_MUTED)));
        }
    }
    nx_point previous[2] = {{0}};
    for (unsigned i = 0; i < 100; ++i) for (unsigned stream = 0; stream < 2; ++stream) {
        float u = i / 99.0f;
        float wave = .12f + .13f * powf(sinf(u * 28), 2) + .13f * sinf(u * 3);
        wave += .48f * expf(-powf((u - .74f) * 22, 2));
        if (stream) wave = .035f + .17f * powf(sinf(u * 26), 6);
        nx_point p = {plot.x + u * plot.width, plot.y + (1 - wave) * plot.height};
        nx_color line = stream ? rgb(175, 61, 235) : rgb(0, 145, 255);
        nx_color area = line; area.a = .2f;
        if (i) {
            nx_point a = previous[stream], b = {a.x, plot.y + plot.height}, c = {p.x, plot.y + plot.height};
            TRY(nx_fill_triangle(s->ui, a, b, p, area)); TRY(nx_fill_triangle(s->ui, p, b, c, area));
            TRY(nx_draw_line(s->ui, a, p, 1.8f, line));
        }
        previous[stream] = p;
    }
    return 1;
}

static int dashboard(cloud_scene *s, nx_rect viewport) {
    float scale = s->text_scale, w = viewport.width - 32;
    int narrow = w < 1000 * scale;
    unsigned cols = (unsigned)fmaxf(1, fminf(4, floorf(w / (240 * scale))));
    float cw = (w - 12 * (cols - 1)) / cols, ch = 153 * scale;
    nx_text_layout *intro = wrapped_layout(s, "Hier ist deine NexaCloud auf einen Blick.", 16, w); REQUIRE(intro);
    float intro_h = 48 * scale + (float)nx_text_layout_height(intro) + 30;
    float left_w = narrow ? w : (w - 16) * .59f, rw = narrow ? w : w - left_w - 16;
    float chart_h = 290 * scale;
    float up_h = transfer_height(s, rw - 24, 1), down_h = transfer_height(s, rw - 24, 0); REQUIRE(up_h > 0 && down_h > 0);
    float transfer_h = 66 * scale + up_h + down_h + 24;
    int stacked_events = left_w < 560 * scale;
    float row_h = (stacked_events ? 92 : 42) * scale;
    float activity_h = 64 * scale + 5 * row_h;
    unsigned quick_cols = rw >= 600 * scale ? 2 : 1;
    float quick_h = 57 * scale + (4 / quick_cols) * (58 * scale + 12) + 14;
    float content_h = 22 + intro_h + ((4 + cols - 1) / cols) * (ch + 12) + 16 +
        (narrow ? chart_h + transfer_h + activity_h + quick_h + 48 : fmaxf(chart_h, transfer_h) + fmaxf(activity_h, quick_h) + 32);
    nx_scroll_area_desc scroll = {0}; scroll.id = 100; scroll.parent_id = 1; scroll.name = span("Übersicht");
    scroll.viewport = viewport; scroll.content_size = (nx_size){viewport.width, content_h}; scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 101;
    nx_scroll_info info;
    TRY(nx_scroll_area_begin(s->ui, &scroll, &s->dashboard_x, &s->dashboard_y, &info));
    float x = viewport.x + 16, y = viewport.y + 22 - s->dashboard_y;
    REQUIRE(text(s, 100, "Guten Tag!", x, y, 32, color(s, NX_COLOR_TEXT)));
    REQUIRE(wrapped_text(s, 100, "Hier ist deine NexaCloud auf einen Blick.", x, y + 49 * scale, 16, color(s, NX_COLOR_TEXT_MUTED), w, NULL));
    if (!narrow) {
        REQUIRE(card(s, 110, 100, box(x + w - 300 * scale, y - 1, 300 * scale, 79 * scale), "Datum und Wetter · Beispieldaten"));
        REQUIRE(text(s, 110, "Dienstag, 14. Oktober 2026", x + w - 283 * scale, y + 11 * scale, 12, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 110, "12:36", x + w - 283 * scale, y + 35 * scale, 24, color(s, NX_COLOR_TEXT)));
        REQUIRE(icon(s, NX_ICON_SUN, box(x + w - 112 * scale, y + 23 * scale, 32, 32), color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 110, "14 °C", x + w - 65 * scale, y + 29 * scale, 16, color(s, NX_COLOR_TEXT)));
    }
    y += intro_h;
    static const char *titles[] = {"Speicher (NVMe)", "CPU Temperatur", "WLAN-Hotspot", "Systemstatus"};
    static const char *values[] = {"237 GB / 500 GB", "42 °C", "Aktiv", "Online"};
    static const char *captions[] = {"263 GB frei", "Normal", "5 Geräte verbunden", "Alles in Ordnung"};
    static const nx_icon icons[] = {NX_ICON_STORAGE, NX_ICON_THERMOMETER, NX_ICON_WIFI, NX_ICON_ACTIVITY};
    for (unsigned i = 0; i < 4; ++i) {
        nx_rect r = box(x + (i % cols) * (cw + 12), y + (i / cols) * (ch + 12), cw, ch);
        REQUIRE(card(s, 120 + i, 100, r, titles[i]));
        nx_color tint = color(s, i >= 2 ? NX_COLOR_SUCCESS : NX_COLOR_ACCENT);
        float tx = r.x + (cw < 300 * scale ? 16 : 73);
        if (cw >= 300 * scale) REQUIRE(icon(s, icons[i], box(r.x + 16, r.y + 33 * scale, 40, 40), tint));
        REQUIRE(text(s, 120 + i, titles[i], tx, r.y + 19 * scale, 13, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 120 + i, values[i], tx, r.y + 49 * scale, 17, i >= 2 ? tint : color(s, NX_COLOR_TEXT)));
        if (i == 0) REQUIRE(progress(s, 130, 120, box(r.x + 17, r.y + 95 * scale, cw - 34, 12), "Speicherbelegung", .474));
        REQUIRE(text(s, 120 + i, captions[i], r.x + 17, r.y + 122 * scale, 12, color(s, NX_COLOR_TEXT_MUTED)));
    }
    y += (ch + 12) * ((4 + cols - 1) / cols) + 4;
    REQUIRE(card(s, 140, 100, box(x, y, left_w, chart_h), "Datenrate"));
    REQUIRE(chart(s, 140, box(x, y, left_w, chart_h)));
    float rx = narrow ? x : x + left_w + 16;
    float ry = narrow ? y + chart_h + 16 : y;
    REQUIRE(card(s, 150, 100, box(rx, ry, rw, transfer_h), "Aktive Übertragungen"));
    REQUIRE(wrapped_text(s, 150, "Aktive Übertragungen (2)", rx + 18, ry + 17, 17, color(s, NX_COLOR_TEXT), rw - 36, NULL));
    REQUIRE(transfer_card(s, 151, 150, box(rx + 12, ry + 66 * scale, rw - 24, up_h), 1));
    REQUIRE(transfer_card(s, 153, 150, box(rx + 12, ry + 66 * scale + up_h + 12, rw - 24, down_h), 0));
    y += (narrow ? chart_h + transfer_h + 16 : fmaxf(chart_h, transfer_h)) + 16;
    REQUIRE(card(s, 160, 100, box(x, y, left_w, activity_h), "Letzte Aktivitäten"));
    REQUIRE(text(s, 160, "Letzte Aktivitäten", x + 19, y + 17, 17, color(s, NX_COLOR_TEXT)));
    static const char *events[] = {"Datei hochgeladen", "Ordner erstellt", "Datei geändert", "Download abgeschlossen", "Datei gelöscht"};
    static const char *event_files[] = {"Bergsee.jpg", "/Projekte", "Notizen.txt", "Wallpapers.zip", "alt_backup.zip"};
    static const char *event_times[] = {"vor 2 Minuten", "vor 18 Minuten", "vor 1 Stunde", "vor 2 Stunden", "vor 3 Stunden"};
    static const nx_icon event_icons[] = {NX_ICON_UPLOAD, NX_ICON_FOLDER, NX_ICON_EDIT, NX_ICON_DOWNLOAD, NX_ICON_TRASH};
    for (unsigned i = 0; i < 5; ++i) {
        float row_y = y + 61 * scale + i * row_h;
        REQUIRE(icon(s, event_icons[i], box(x + 19, row_y, 22, 22), color(s, i == 4 ? NX_COLOR_ERROR : NX_COLOR_ACCENT)));
        REQUIRE(wrapped_text(s, 160, events[i], x + 53, row_y, 12, color(s, NX_COLOR_TEXT_MUTED), stacked_events ? left_w - 72 : left_w * .5f - 58, NULL));
        REQUIRE(text(s, 160, event_files[i], stacked_events ? x + 53 : x + left_w * .5f, stacked_events ? row_y + 30 * scale : row_y, 12, color(s, NX_COLOR_TEXT)));
        nx_text_layout *time = layout(s, event_times[i], 11); REQUIRE(time);
        REQUIRE(text(s, 160, event_times[i], stacked_events ? x + 53 : x + left_w - 16 - (float)nx_text_layout_width(time), stacked_events ? row_y + 58 * scale : row_y, 11, color(s, NX_COLOR_TEXT_MUTED)));
        if (i < 4) TRY(nx_fill_rect(s->ui, box(x + 16, row_y + row_h - 10, left_w - 32, 1), color(s, NX_COLOR_BORDER)));
    }
    rx = narrow ? x : x + left_w + 16; ry = narrow ? y + activity_h + 16 : y;
    REQUIRE(card(s, 170, 100, box(rx, ry, rw, quick_h), "Schnellaktionen"));
    REQUIRE(text(s, 170, "Schnellaktionen", rx + 19, ry + 17, 17, color(s, NX_COLOR_TEXT)));
    static const char *quick[] = {"Dateien öffnen", "Hochladen", "Link erstellen", "Einstellungen"};
    static const nx_icon quick_icons[] = {NX_ICON_FOLDER, NX_ICON_UPLOAD, NX_ICON_LINK, NX_ICON_SETTINGS};
    for (unsigned i = 0; i < 4; ++i) {
        float qw = (rw - 28 - (quick_cols - 1) * 12) / quick_cols;
        nx_rect r = box(rx + 14 + (i % quick_cols) * (qw + 12), ry + 57 * scale + (i / quick_cols) * (58 * scale + 12), qw, 58 * scale);
        int clicked;
        REQUIRE(action(s, 171 + i, 170, r, quick[i], quick_icons[i], NX_ACTION_SECONDARY, 0, 0, &clicked));
        if (clicked) {
            if (i == 0) s->page = FILES;
            else if (i == 1) s->dialog = 1;
            else if (i == 2) s->dialog = 3;
            else s->page = SETTINGS;
        }
    }
    TRY(nx_scroll_area_end(s->ui)); return 1;
}

static int matches(const char *name, nx_utf8_span query) {
    if (!query.length) return 1;
    size_t n = strlen(name);
    for (size_t i = 0; i + query.length <= n; ++i) {
        size_t k = 0;
        while (k < query.length && tolower((unsigned char)name[i + k]) == tolower((unsigned char)query.data[k])) ++k;
        if (k == query.length) return 1;
    }
    return 0;
}
static nx_bitmap_view thumbnail(cloud_scene *s, unsigned index) {
    return (nx_bitmap_view){s->thumbnails[index % 8], THUMB_W, THUMB_H, THUMB_W * 4u, NX_BITMAP_RGBA8};
}

/* Measured content height is shared by the inspector and modal scroll ranges. */
static float detail_height(cloud_scene *s, float width) {
    float scale = s->text_scale, w = width - 36;
    cloud_file *f = &s->files[s->selected];
    nx_text_layout *title = wrapped_layout(s, f->name, 16, w);
    nx_text_layout *metadata = wrapped_layout(s, f->metadata, 14, w);
    nx_text_layout *tags = wrapped_layout(s, "Berge · Natur · Sommer", 14, w);
    if (!title || !metadata || !tags) return -1;
    float h = 48 + (f->folder ? 116 : fminf(w, 320) * .625f + 16) +
        (float)nx_text_layout_height(title) + (float)nx_text_layout_height(metadata) + 36 + 45 * scale + 24;
    const char *values[] = {f->folder ? "Ordner" : strstr(f->name, ".png") ? "PNG-Bild" : "JPEG-Bild", f->metadata,
        "14.10.2026 11:02", "14.10.2026 11:02", f->folder ? "–" : "256 × 160 (Demo)", "/Bilder/Natur/"};
    int stacked = w < 460 * scale;
    for (unsigned i = 0; i < 6; ++i) {
        nx_text_layout *value = wrapped_layout(s, values[i], 13, stacked ? w : w * .60f);
        if (!value) return -1;
        float line = (float)nx_text_layout_height(value);
        h += stacked ? 25 * scale + line + 14 : fmaxf(line, 20 * scale) + 11;
    }
    return h + 12 + 160 * scale + (float)nx_text_layout_height(tags) + 24;
}

static int detail_panel(cloud_scene *s, nx_rect r, uint64_t parent) {
    cloud_file *f = &s->files[s->selected];
    float scale = s->text_scale;
    REQUIRE(card(s, 300, parent, r, "Dateidetails"));
    int click;
    REQUIRE(action(s, 301, 300, box(r.x + r.width - 43, r.y + 7, 34, 34), "Details schließen", NX_ICON_CLOSE, NX_ACTION_QUIET, 0, 0, &click));
    if (click) s->details = 0;
    float x = r.x + 18, w = r.width - 36, y = r.y + 48, line;
    if (!f->folder) {
        float preview_w = fminf(w, 320);
        TRY(nx_image(s->ui, 302, 300, box(x + (w - preview_w) / 2, y, preview_w, preview_w * .625f), span(f->name), thumbnail(s, (unsigned)s->selected), rgb(255,255,255)));
        y += preview_w * .625f + 16;
    } else {
        REQUIRE(icon(s, NX_ICON_FOLDER, box(x + w / 2 - 32, y + 25, 64, 64), color(s, NX_COLOR_ACCENT)));
        y += 116;
    }
    REQUIRE(wrapped_text(s, 300, f->name, x, y, 16, color(s, NX_COLOR_TEXT), w, &line)); y += line + 12;
    REQUIRE(wrapped_text(s, 300, f->metadata, x, y, 14, color(s, NX_COLOR_TEXT_MUTED), w, &line)); y += line + 24;
    float aw = (w - 24) / 3, ah = 45 * scale;
    REQUIRE(action(s, 303, 300, box(x, y, aw, ah), "Öffnen", NX_ICON_FOLDER, NX_ACTION_SECONDARY, 0, 0, &click));
    if (click) { if (f->folder) { s->folder_depth = s->selected + 1; s->scroll_y = 0; s->details = 0; } else s->dialog = 5; }
    REQUIRE(action(s, 304, 300, box(x + aw + 12, y, aw, ah), "Teilen", NX_ICON_SHARE, NX_ACTION_SECONDARY, 0, 0, &click)); if (click) s->dialog = 3;
    REQUIRE(action(s, 305, 300, box(x + 2 * (aw + 12), y, aw, ah), "Favorit", NX_ICON_STAR, NX_ACTION_SECONDARY, f->favorite, 0, &click)); if (click) f->favorite ^= 1;
    y += ah + 24;
    static const char *labels[] = {"Typ", "Größe", "Erstellt", "Geändert", "Auflösung", "Speicherort"};
    const char *values[] = {f->folder ? "Ordner" : strstr(f->name, ".png") ? "PNG-Bild" : "JPEG-Bild", f->metadata, "14.10.2026 11:02", "14.10.2026 11:02", f->folder ? "–" : "256 × 160 (Demo)", "/Bilder/Natur/"};
    int stacked = w < 460 * scale;
    for (unsigned i = 0; i < 6; ++i) {
        REQUIRE(text(s, 300, labels[i], x, y, 13, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(wrapped_text(s, 300, values[i], stacked ? x : x + w * .40f,
            stacked ? y + 25 * scale : y, 13, color(s, NX_COLOR_TEXT), stacked ? w : w * .60f, &line));
        y += stacked ? 25 * scale + line + 14 : fmaxf(line, 20 * scale) + 11;
    }
    y += 12;
    TRY(nx_fill_rect(s->ui, box(x, y - 12, w, 1), color(s, NX_COLOR_BORDER)));
    REQUIRE(text(s, 300, "Beschreibung", x, y, 15, color(s, NX_COLOR_TEXT)));
    nx_text_field_desc d = {0}; d.id = 306; d.parent_id = 300; d.bounds = box(x, y + 30 * scale, w, 72 * scale);
    d.name = span("Dateibeschreibung bearbeiten"); d.fonts = s->fonts; d.pixels_per_em = 13 * scale;
    d.glyph_cache_bytes = CACHE_BYTES; d.multiline = 1; d.ime_window = s->window;
    nx_interaction_state state; int valid, submitted;
    if (!f->description_field) TRY(nx_text_field_create(f->description, strlen(f->description), 16384, &f->description_field));
    TRY(nx_text_field_widget(s->ui, &d, f->description_field, &state, &valid, &submitted));
    if (state.hovered) s->cursor = NX_CURSOR_TEXT;
    y += 126 * scale;
    REQUIRE(text(s, 300, "Tags", x, y, 15, color(s, NX_COLOR_TEXT)));
    REQUIRE(wrapped_text(s, 300, "Berge · Natur · Sommer", x, y + 34 * scale, 14, color(s, NX_COLOR_TEXT_MUTED), w, NULL));
    return 1;
}

static int file_browser(cloud_scene *s, nx_rect r) {
    int click;
    if (s->selected >= 0 && ((s->page == TRASH) != !!s->files[s->selected].deleted ||
        (s->page == FAVORITES && !s->files[s->selected].favorite) ||
        (s->page == FILES && s->files[s->selected].parent != s->folder_depth))) s->selected = -1;
    float toolbar_y = r.y + 15;
    float scale = s->text_scale, toolbar_h = 44 * scale;
    int compact = r.width < 620 * scale;
    REQUIRE(action(s, 200, 1, box(r.x + 16, toolbar_y, compact ? 44.0f : 148.0f * scale, toolbar_h), "Hochladen", NX_ICON_PLUS, NX_ACTION_PRIMARY, 0, s->page == TRASH, &click)); if (click) s->dialog = 1;
    REQUIRE(action(s, 201, 1, box(r.x + (compact ? 72.0f : 16 + 160 * scale), toolbar_y, compact ? 44.0f : 156.0f * scale, toolbar_h), "Neuer Ordner", NX_ICON_FOLDER, NX_ACTION_SECONDARY, 0, s->file_count == 20 || s->page == TRASH, &click));
    if (click) {
        cloud_file *f = &s->files[s->file_count++];
        snprintf(f->name, sizeof(f->name), "Neuer Ordner %u", s->file_count - 12); f->folder = 1; f->metadata = "Ordner · Leer"; f->parent = s->folder_depth;
        notify(s, "Ordner in den Beispieldaten erstellt.");
    }
    REQUIRE(action(s, 202, 1, box(r.x + (compact ? 128.0f : 16 + 328 * scale), toolbar_y, compact ? 44.0f : 126.0f * scale, toolbar_h), s->page == TRASH ? "Zurücklegen" : "Löschen", NX_ICON_TRASH, NX_ACTION_SECONDARY, 0, s->selected < 0, &click));
    if (click && s->selected >= 0) { s->files[s->selected].deleted ^= 1; s->details = 0; s->selected = -1; notify(s, s->page == TRASH ? "Datei wiederhergestellt." : "Datei in den Papierkorb verschoben."); }
    REQUIRE(action(s, 203, 1, box(r.x + r.width - 56, toolbar_y, 42, toolbar_h), "Dateidetails umschalten", NX_ICON_INFO, NX_ACTION_SECONDARY, s->details, s->selected < 0, &click));
    if (click) { if (r.width < 950 * s->text_scale) { s->dialog = 6; s->details = 1; } else s->details ^= 1; }
    float inspector = s->details && s->selected >= 0 && r.width >= 950 * scale ? 350 * scale : 0.0f;
    nx_rect content = box(r.x + 16, r.y + 38 + toolbar_h, r.width - 32 - (inspector ? inspector + 16 : 0), r.height - 54 - toolbar_h);
    REQUIRE(card(s, 210, 1, content, "Dateien"));
    nx_utf8_span query; TRY(nx_text_field_text(s->search, &query));
    unsigned visible[20], count = 0;
    for (unsigned i = 0; i < s->file_count; ++i) {
        cloud_file *f = &s->files[i];
        if ((s->page == TRASH) != !!f->deleted || (s->page == FAVORITES && !f->favorite) ||
            (s->page == FILES && s->folder_depth != f->parent) || !matches(f->name, query)) continue;
        visible[count++] = i;
    }
    float min_cell = 178;
    for (unsigned v = 0; v < count; ++v) {
        nx_text_layout *name = layout(s, s->files[visible[v]].name, 14); REQUIRE(name);
        nx_text_layout *metadata = layout(s, s->files[visible[v]].metadata, 12); REQUIRE(metadata);
        min_cell = fmaxf(min_cell, 24 + fmaxf((float)nx_text_layout_width(name), (float)nx_text_layout_width(metadata)));
    }
    unsigned cols = s->list_view ? 1 : (unsigned)fmaxf(1, floorf((content.width - 20) / (min_cell + 12)));
    if (cols > 4) cols = 4;
    float gap = 12, cw = (content.width - 32 - (cols - 1) * gap) / cols;
    int stacked_list = s->list_view && cw < min_cell + 90;
    float preview_h = fminf((cw - 12) * .625f, 160);
    float ch = s->list_view ? (stacked_list ? 55 + 73 * scale : 67 * scale) : preview_h + 73 * scale;
    uint64_t focused = nx_focus_id(s->ui), move_focus = 0;
    if (focused >= 1000 && focused < 1000 + s->file_count) {
        int direction = nx_key_pressed(s->ui, NX_KEY_LEFT) ? -1 :
            nx_key_pressed(s->ui, NX_KEY_RIGHT) ? 1 :
            nx_key_pressed(s->ui, NX_KEY_UP) ? -(int)cols :
            nx_key_pressed(s->ui, NX_KEY_DOWN) ? (int)cols : 0;
        for (unsigned v = 0; v < count && direction; ++v) if (visible[v] + 1000 == focused) {
            int target = (int)v + direction;
            if (target >= 0 && target < (int)count) {
                move_focus = 1000 + visible[target];
                float top = (target / (int)cols) * (ch + gap);
                if (top < s->scroll_y) s->scroll_y = top;
                if (top + ch > s->scroll_y + content.height - 32) s->scroll_y = top + ch - content.height + 32;
            }
            break;
        }
    }
    nx_scroll_area_desc scroll = {0}; scroll.id = 211; scroll.parent_id = 210; scroll.name = span("Dateiraster");
    scroll.viewport = box(content.x + 10, content.y + 10, content.width - 20, content.height - 20);
    scroll.content_size = (nx_size){scroll.viewport.width, 16 + ((count + cols - 1) / cols) * (ch + gap)};
    if (!count) {
        nx_text_layout *title = wrapped_layout(s, query.length ? "Keine passenden Dateien" : "Dieser Bereich ist leer", 18, content.width - 48);
        nx_text_layout *caption = wrapped_layout(s, "Ändere die Suche oder wähle einen anderen Bereich.", 13, content.width - 48); REQUIRE(title && caption);
        scroll.content_size.height = 148 + (float)nx_text_layout_height(title) + (float)nx_text_layout_height(caption);
    }
    scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 212;
    nx_scroll_info info;
    TRY(nx_scroll_area_begin(s->ui, &scroll, &s->scroll_x, &s->scroll_y, &info));
    if (!count) {
        REQUIRE(icon(s, NX_ICON_FOLDER, box(content.x + 36, content.y + 45 - s->scroll_y, 48, 48), color(s, NX_COLOR_TEXT_MUTED)));
        float line;
        REQUIRE(wrapped_text(s, 211, query.length ? "Keine passenden Dateien" : "Dieser Bereich ist leer", content.x + 20, content.y + 116 - s->scroll_y, 18, color(s, NX_COLOR_TEXT), content.width - 48, &line));
        REQUIRE(wrapped_text(s, 211, "Ändere die Suche oder wähle einen anderen Bereich.", content.x + 20, content.y + 132 + line - s->scroll_y, 13, color(s, NX_COLOR_TEXT_MUTED), content.width - 48, NULL));
    }
    int next_selected = s->selected;
    for (unsigned v = 0; v < count; ++v) {
        unsigned i = visible[v]; cloud_file *f = &s->files[i];
        nx_rect tile = box(content.x + 16 + (v % cols) * (cw + gap), content.y + 16 + (v / cols) * (ch + gap) - s->scroll_y, cw, ch);
        if (tile.y + tile.height < content.y || tile.y > content.y + content.height) continue;
        nx_action_desc d = {0}; d.control.id = 1000 + i; d.control.parent_id = 211;
        d.control.name = span(f->name); d.control.bounds = tile; d.variant = NX_ACTION_TILE; d.selected = (int)i == s->selected;
        nx_interaction_state state; TRY(nx_action(s->ui, &d, &state));
        if (state.hovered) s->cursor = NX_CURSOR_LINK;
        if (d.control.id == move_focus) TRY(nx_focus_set(s->ui, move_focus));
        if (state.clicked) { next_selected = (int)i; s->details = 1; }
        float image_w = s->list_view ? 65 : preview_h / .625f;
        nx_rect image = box(s->list_view ? tile.x + 6 : tile.x + (cw - image_w) / 2,
            tile.y + 6, image_w, s->list_view ? stacked_list ? 40 : ch - 12 : preview_h);
        if (f->folder) REQUIRE(icon(s, NX_ICON_FOLDER, box(image.x + image.width / 2 - 24, image.y + image.height / 2 - 24, 48, 48), color(s, NX_COLOR_ACCENT)));
        else TRY(nx_image(s->ui, 2000 + i, 1000 + i, image, span(f->name), thumbnail(s, i), rgb(255,255,255)));
        float tx = s->list_view && !stacked_list ? tile.x + 90 : tile.x + 12;
        float ty = s->list_view && !stacked_list ? tile.y + 10 : tile.y + image.height + 18;
        TRY(nx_push_clip(s->ui, box(tx, ty, s->list_view && !stacked_list ? cw - 110 : cw - 22, ch)));
        REQUIRE(text(s, 1000 + i, f->name, tx, ty, 14, color(s, NX_COLOR_TEXT)));
        REQUIRE(text(s, 1000 + i, f->metadata, tx, ty + 25 * s->text_scale, 12, color(s, NX_COLOR_TEXT_MUTED)));
        TRY(nx_pop_clip(s->ui));
        if ((int)i == s->selected && !s->list_view) {
            TRY(nx_fill_round_rect(s->ui, box(tile.x + 12, tile.y + 12, 22, 22), 5, color(s, NX_COLOR_ACCENT)));
            REQUIRE(icon(s, NX_ICON_CHECK, box(tile.x + 15, tile.y + 15, 16, 16), color(s, NX_COLOR_ACCENT_TEXT)));
        }
    }
    if (s->selected != next_selected) {
        s->detail_y = 0;
    }
    s->selected = next_selected;
    TRY(nx_scroll_area_end(s->ui));
    if (inspector) {
        nx_rect panel = box(r.x + r.width - 16 - inspector, content.y, inspector, content.height);
        /* Detail content can exceed a short window without losing actions. */
        nx_scroll_area_desc ds = {0}; ds.id = 310; ds.parent_id = 1; ds.name = span("Dateidetails scrollen");
        ds.viewport = panel; ds.content_size = (nx_size){panel.width, detail_height(s, panel.width - 10)}; ds.axes = NX_SCROLL_Y; ds.thumb_y_id = 311;
        nx_scroll_info di; TRY(nx_scroll_area_begin(s->ui, &ds, &s->detail_x, &s->detail_y, &di));
        REQUIRE(detail_panel(s, box(panel.x, panel.y - s->detail_y, panel.width - 10, fmaxf(panel.height, ds.content_size.height)), 310));
        TRY(nx_scroll_area_end(s->ui));
    }
    return 1;
}

static int settings(cloud_scene *s, nx_rect r) {
    float scale = s->text_scale;
    float x = r.x + 20, w = fminf(r.width - 40, 740 * scale), line;
    nx_text_layout *intro = wrapped_layout(s, "Gestalte deine NexaCloud.", 16, w); REQUIRE(intro);
    nx_text_layout *footer = wrapped_layout(s, "Diese Anwendung zeigt lokale Beispieldaten. Der NexaCloud-Server wird später angebunden.", 15, w); REQUIRE(footer);
    char size_label[80]; snprintf(size_label, sizeof(size_label), "Schriftgröße: %.0f %% · Ändern", scale * 100);
    const char *names[] = {s->light ? "Dunkles Design aktivieren" : "Helles Design aktivieren",
        s->reduced_motion ? "Bewegung reduziert" : "Bewegung reduzieren", "Systeminformationen", size_label};
    float heights[4], top = 32 * scale + 24 + (float)nx_text_layout_height(intro) + 30;
    float card_h = 64 * scale;
    for (unsigned i = 0; i < 4; ++i) {
        nx_text_layout *label = wrapped_layout(s, names[i], 15, w - 60); REQUIRE(label);
        heights[i] = fmaxf(46 * scale, (float)nx_text_layout_height(label) + 24);
        card_h += heights[i] + 12;
    }
    card_h += 30 * scale + 16;
    nx_scroll_area_desc scroll = {0}; scroll.id = 410; scroll.parent_id = 1; scroll.name = span("Einstellungen");
    scroll.viewport = r; scroll.content_size = (nx_size){r.width, 28 + top + card_h + 28 + (float)nx_text_layout_height(footer) + 28};
    scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 411;
    nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &scroll, &s->dashboard_x, &s->dashboard_y, &info));
    float y = r.y + 28 - s->dashboard_y;
    REQUIRE(text(s, 410, "Einstellungen", x, y, r.width < 480 ? 24 : 30, color(s, NX_COLOR_TEXT)));
    REQUIRE(wrapped_text(s, 410, "Gestalte deine NexaCloud.", x, y + 40 * scale + 8, 16, color(s, NX_COLOR_TEXT_MUTED), w, NULL));
    y += top;
    REQUIRE(card(s, 400, 410, box(x, y, w, card_h), "Darstellung"));
    REQUIRE(text(s, 400, "Darstellung", x + 16, y + 16, 20, color(s, NX_COLOR_TEXT)));
    y += 64 * scale;
    for (unsigned i = 0; i < 4; ++i) {
        int click;
        REQUIRE(action(s, 401 + i, 400, box(x + 16, y, w - 32, heights[i]), names[i], NX_ICON_NONE,
            NX_ACTION_SECONDARY, i == 1 && s->reduced_motion, 0, &click));
        if (click) {
            if (i == 0) { s->light ^= 1; s->pending_theme = 1; }
            else if (i == 1) s->reduced_motion ^= 1;
            else if (i == 2) s->dialog = 4;
            else REQUIRE(cloud_set_text_scale(s, scale < 1.5f ? 1.5f : scale < 2 ? 2 : 1));
        }
        y += heights[i] + 12;
    }
    REQUIRE(wrapped_text(s, 400, "NexaUI · C17 · Native Oberfläche", x + 16, y + 4, 13, color(s, NX_COLOR_TEXT_MUTED), w - 32, &line));
    y += line + 44;
    REQUIRE(wrapped_text(s, 410, "Diese Anwendung zeigt lokale Beispieldaten. Der NexaCloud-Server wird später angebunden.", x, y, 15, color(s, NX_COLOR_TEXT_MUTED), w, NULL));
    TRY(nx_scroll_area_end(s->ui));
    return 1;
}

static int dialog(cloud_scene *s) {
    float scale = s->text_scale, w = fminf(540 * scale, s->width - 40);
    float button_h = 44 * scale, header_h = 44 * scale + 24;
    float h = fminf(310 * scale, s->height - 40);
    nx_rect r = box((s->width - w) / 2, (s->height - h) / 2, w, h);
    static const char *titles[] = {"", "Dateien hochladen", "Neuer Ordner", "Freigabelink", "Systeminformationen", "Bildvorschau", "Dateidetails"};
    if (s->dialog == 6) { h = s->height - 40; r = box((s->width - w) / 2, 20, w, h); }
    nx_overlay_desc d = {0}; d.panel.id = 500; d.panel.bounds = r; d.panel.name = span(titles[s->dialog]);
    d.backdrop_id = 501; d.dismiss_on_escape = 1; d.dismiss_on_outside = 1;
    int dismiss; TRY(nx_modal_begin(s->ui, &d, &dismiss));
    REQUIRE(wrapped_text(s, 500, titles[s->dialog], r.x + 24, r.y + 20, 20, color(s, NX_COLOR_TEXT), w - 48, NULL));
    nx_scroll_area_desc ds = {0}; ds.id = 310; ds.parent_id = 500; ds.name = span("Dialoginhalt scrollen");
    ds.viewport = box(r.x + 8, r.y + header_h, r.width - 16, r.height - header_h - button_h - 40);
    ds.axes = NX_SCROLL_Y; ds.thumb_y_id = 311;
    if (s->dialog == 6 && s->selected >= 0) {
        ds.content_size = (nx_size){ds.viewport.width, detail_height(s, ds.viewport.width - 10)};
        nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &ds, &s->detail_x, &s->detail_y, &info));
        REQUIRE(detail_panel(s, box(ds.viewport.x, ds.viewport.y - s->detail_y, ds.viewport.width - 10, ds.content_size.height), 310));
        TRY(nx_scroll_area_end(s->ui));
        if (!s->details) dismiss = 1;
    } else {
        const char *message = s->dialog == 1 ? "Beispielmodus: keine Dateien werden übertragen." :
            s->dialog == 3 ? "Beispiel-Link · kein öffentlicher Server verbunden." : "NexaCloud UI · NexaUI 0.1 · lokale Beispieldaten";
        const char *second = s->dialog == 3 ? "https://nexacloud.example/s/bergsee" : "Gemeinsamer C17-Kern für Windows, Linux und macOS.";
        nx_text_layout *a = wrapped_layout(s, message, 14, w - 64);
        nx_text_layout *b = wrapped_layout(s, second, 13, w - 64); REQUIRE(a && b);
        ds.content_size = (nx_size){ds.viewport.width, s->dialog == 5 ? 240 :
            48 + (float)nx_text_layout_height(a) + (float)nx_text_layout_height(b)};
        nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &ds, &s->detail_x, &s->detail_y, &info));
        float y = ds.viewport.y + 16 - s->detail_y, line;
        if (s->dialog == 5 && s->selected >= 0) {
            float iw = fminf(w - 64, 320);
            TRY(nx_image(s->ui, 505, 310, box(r.x + 24, y, iw, iw * .625f), span(s->files[s->selected].name), thumbnail(s, (unsigned)s->selected), rgb(255,255,255)));
        } else {
            REQUIRE(wrapped_text(s, 310, message, r.x + 24, y, 14, color(s, NX_COLOR_TEXT_MUTED), w - 64, &line));
            REQUIRE(wrapped_text(s, 310, second, r.x + 24, y + line + 16, 13, color(s, NX_COLOR_TEXT), w - 64, NULL));
        }
        TRY(nx_scroll_area_end(s->ui));
    }
    int click;
    REQUIRE(action(s, 502, 500, box(r.x + 24, r.y + h - button_h - 20, w - 48, button_h), "Schließen", NX_ICON_CLOSE, NX_ACTION_PRIMARY, 0, 0, &click));
    TRY(nx_overlay_end(s->ui));
    if (dismiss || click) { TRY(nx_overlay_close(s->ui)); s->dialog = 0; s->detail_y = 0; }
    return 1;
}

/* Reveal newly focused controls after Tab or text reflow. Wheel scrolling with
   unchanged focus stays under the user's control. Geometry comes from the
   completed semantic tree, so wrapped labels and resized viewports agree. */
static int reveal_focus(cloud_scene *s) {
    uint64_t focused = nx_focus_id(s->ui);
    if (focused && (focused != s->previous_focus || s->reflow_changed)) {
        uint32_t index; nx_semantic_view node;
        if (nx_semantics_find(s->ui, focused, &index) == NX_OK && nx_semantics_get(s->ui, index, &node) == NX_OK) {
            nx_rect bounds = node.bounds;
            while (node.parent_id) {
                TRY(nx_semantics_find(s->ui, node.parent_id, &index));
                TRY(nx_semantics_get(s->ui, index, &node));
                if (node.role != NX_ROLE_SCROLL_AREA) continue;
                float *offset = node.id == 35 ? &s->nav_y : node.id == 310 ? &s->detail_y :
                    node.id == 211 ? &s->scroll_y : node.id == 100 || node.id == 410 || node.id == 610 ? &s->dashboard_y : NULL;
                if (offset) {
                    float delta = bounds.y < node.bounds.y ? bounds.y - node.bounds.y :
                        bounds.y + bounds.height > node.bounds.y + node.bounds.height ?
                        bounds.y + bounds.height - node.bounds.y - node.bounds.height : 0;
                    *offset = fmaxf(0, *offset + delta);
                }
                break;
            }
        }
    }
    s->previous_focus = focused; return 1;
}

int cloud_frame(cloud_scene *s, float width, float height, double dt) {
    s->reflow_changed = s->text_scale != s->requested_text_scale || s->width != width || s->height != height;
    s->width = width; s->height = height; s->label_id = 10000; s->cursor = NX_CURSOR_DEFAULT;
    if (s->pending_theme) {
        TRY(nx_theme_preset(s->light ? NX_THEME_LIGHT : NX_THEME_MIDNIGHT, &s->theme));
        TRY(nx_context_set_theme(s->ui, &s->theme)); s->pending_theme = 0;
    }
    TRY(nx_context_set_reduced_motion(s->ui, s->reduced_motion));
    TRY(nx_begin_frame(s->ui, width, height, dt));
    /* The old command stream has now expired; future cache evictions are safe. */
    if (++s->frame_number == 0) {
        s->frame_number = 1;
        for (size_t i = 0; i < s->text_count; ++i) s->text[i].used_frame = 0;
    }
    s->text_scale = s->requested_text_scale;
    nx_input_snapshot input; TRY(nx_frame_input(s->ui, &input));
#ifdef __APPLE__
    const uint32_t primary = NX_MOD_SUPER;
#else
    const uint32_t primary = NX_MOD_CONTROL;
#endif
    if ((input.modifiers & primary) && nx_key_pressed(s->ui, NX_KEY_F))
        (void)nx_focus_set(s->ui, 40);
    if (!s->dialog && (input.modifiers & NX_MOD_ALT) && nx_key_pressed(s->ui, NX_KEY_LEFT))
        s->folder_depth = 0;
    if (!s->dialog && s->page == FILES && s->selected >= 0 &&
        nx_focus_id(s->ui) >= 1000 && nx_focus_id(s->ui) < 1020 && nx_key_pressed(s->ui, NX_KEY_DELETE)) {
        s->files[s->selected].deleted = 1; s->selected = -1; s->details = 0;
        notify(s, "Datei in den Papierkorb verschoben.");
    }
    nx_semantic_desc root = {0}; root.id = 1; root.role = NX_ROLE_WINDOW;
    root.bounds = box(0, 0, width, height); root.name = span("NexaCloud"); TRY(nx_semantics_add(s->ui, &root, NULL));
    TRY(nx_fill_rect(s->ui, root.bounds, color(s, NX_COLOR_BACKGROUND)));
    if (width < 480 || height < 400) {
        REQUIRE(text(s, 1, "NexaCloud", 16, 16, 24, color(s, NX_COLOR_TEXT)));
        REQUIRE(text(s, 1, "Bitte das Fenster vergrößern.", 16, 16 + 40 * s->text_scale, 14, color(s, NX_COLOR_TEXT_MUTED)));
        TRY(nx_end_frame(s->ui)); return 1;
    }
    float sidebar_w = width >= 1000 * s->text_scale ? 236 * s->text_scale : 76.0f;
    REQUIRE(sidebar(s, sidebar_w));
    REQUIRE(topbar(s, sidebar_w));
    float content_w = fminf(width - sidebar_w, 1640);
    float top = 70 + 20 * s->text_scale;
    nx_rect content = box(sidebar_w + (width - sidebar_w - content_w) / 2,
        top, content_w, height - top - 30 * s->text_scale);
    if (s->page == OVERVIEW) REQUIRE(dashboard(s, content));
    else if (s->page == SETTINGS) REQUIRE(settings(s, content));
    else if (s->page == TRANSFERS) {
        nx_scroll_area_desc scroll = {0}; scroll.id = 610; scroll.parent_id = 1; scroll.name = span("Übertragungen");
        float scale = s->text_scale, w = fminf(content.width - 56, 760 * scale);
        float up_h = transfer_height(s, w, 1), down_h = transfer_height(s, w, 0); REQUIRE(up_h > 0 && down_h > 0);
        scroll.viewport = content; scroll.content_size = (nx_size){content.width, 130 * scale + up_h + down_h + 24 + 46 * scale + 28}; scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 611;
        nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &scroll, &s->dashboard_x, &s->dashboard_y, &info));
        content.y -= s->dashboard_y;
        REQUIRE(text(s, 610, "Übertragungen", content.x + 28, content.y + 26,
            content.width < 480 ? 24 : 30, color(s, NX_COLOR_TEXT)));
        float y = content.y + 80 * scale;
        REQUIRE(transfer_card(s, 600, 610, box(content.x + 28, y, w, up_h), 1)); y += up_h + 16;
        REQUIRE(transfer_card(s, 602, 610, box(content.x + 28, y, w, down_h), 0)); y += down_h + 24;
        int click;
        REQUIRE(action(s, 604, 610, box(content.x + 28, y, w, 46 * scale), s->paused ? "Übertragungen fortsetzen" : "Alle pausieren", NX_ICON_TRANSFER, NX_ACTION_SECONDARY, 0, 0, &click));
        if (click) s->paused ^= 1;
        TRY(nx_scroll_area_end(s->ui));
    } else REQUIRE(file_browser(s, content));
    REQUIRE(text(s, 1, "NexaCloud UI · Beispieldaten", sidebar_w + 20, height - 25 * s->text_scale, 11, color(s, NX_COLOR_TEXT_MUTED)));
    if (s->notice_seconds > 0) {
        s->notice_seconds -= dt;
        float toast_w = fminf(width - sidebar_w - 48, 560 * s->text_scale);
        nx_text_layout *notice = wrapped_layout(s, s->notice, 13, toast_w - 28); REQUIRE(notice);
        float toast_h = (float)nx_text_layout_height(notice) + 28;
        nx_rect toast = box(sidebar_w + 24, height - 30 * s->text_scale - toast_h, toast_w, toast_h);
        REQUIRE(card(s, 700, 1, toast, "Statusmeldung"));
        REQUIRE(wrapped_text(s, 700, s->notice, toast.x + 14, toast.y + 14, 13, color(s, NX_COLOR_SUCCESS), toast_w - 28, NULL));
    }
    if (s->dialog) REQUIRE(dialog(s));
    TRY(nx_end_frame(s->ui)); REQUIRE(reveal_focus(s)); return 1;
}

int cloud_export(cloud_scene *s, const char *path, float width, float height) {
    REQUIRE(cloud_frame(s, width, height, 0));
    const nx_command *commands; size_t count;
    TRY(nx_frame_commands(s->ui, &commands, &count));
    uint32_t w = (uint32_t)width, h = (uint32_t)height;
    uint8_t *pixels = malloc((size_t)w * h * 4u); REQUIRE(pixels);
    int okay = nx_headless_render(commands, count, pixels, w, h, w * 4u, color(s, NX_COLOR_BACKGROUND)) == NX_OK;
    FILE *f = okay ? fopen(path, "wb") : NULL;
    if (!f) okay = 0;
    if (okay) okay = fprintf(f, "P6\n%u %u\n255\n", w, h) > 0;
    for (size_t i = 0; okay && i < (size_t)w * h; ++i) okay = fwrite(pixels + i * 4u, 1, 3, f) == 3;
    if (f && fclose(f) != 0) okay = 0;
    free(pixels); return okay;
}

static int click_at(cloud_scene *s, float x, float y) {
    nx_event e = {0}; e.type = NX_EVENT_POINTER_DOWN; e.button = NX_BUTTON_LEFT; e.x = x; e.y = y;
    TRY(nx_context_queue_event(s->ui, &e)); e.type = NX_EVENT_POINTER_UP;
    TRY(nx_context_queue_event(s->ui, &e)); return 1;
}
static int key(cloud_scene *s, nx_key code) {
    nx_event e = {0}; e.type = NX_EVENT_KEY_DOWN; e.key = code;
    TRY(nx_context_queue_event(s->ui, &e)); e.type = NX_EVENT_KEY_UP;
    TRY(nx_context_queue_event(s->ui, &e)); return 1;
}

int cloud_smoke(cloud_scene *s, const char *prefix) {
    char path[1024];
    snprintf(path, sizeof(path), "%s-overview.ppm", prefix); REQUIRE(cloud_export(s, path, 1440, 1040));
    REQUIRE(click_at(s, 100, 195)); REQUIRE(cloud_frame(s, 1440, 1040, 0)); REQUIRE(s->page == FILES);
    snprintf(path, sizeof(path), "%s-files.ppm", prefix); REQUIRE(cloud_export(s, path, 1440, 1040));
    char long_description[256]; memset(long_description, 'x', sizeof(long_description));
    REQUIRE(nx_text_field_set_text(s->files[0].description_field, long_description, sizeof(long_description)) == NX_OK);
    REQUIRE(nx_focus_set(s->ui, 1001) == NX_OK); REQUIRE(key(s, NX_KEY_SPACE));
    REQUIRE(cloud_frame(s, 1440, 1040, 0)); REQUIRE(s->selected == 1);
    REQUIRE(cloud_frame(s, 1440, 1040, 0));
    nx_utf8_span kept;
    REQUIRE(nx_text_field_text(s->files[0].description_field, &kept) == NX_OK && kept.length == sizeof(long_description));
    REQUIRE(nx_text_field_set_text(s->files[0].description_field, s->files[0].description, strlen(s->files[0].description)) == NX_OK);
    REQUIRE(click_at(s, 600, 125)); REQUIRE(cloud_frame(s, 1440, 1040, 0)); REQUIRE(s->files[1].deleted);
    REQUIRE(nx_focus_set(s->ui, 201) == NX_OK); REQUIRE(key(s, NX_KEY_ENTER));
    REQUIRE(cloud_frame(s, 1440, 1040, 0)); REQUIRE(s->file_count == 13);
    REQUIRE(nx_text_field_set_text(s->search, "no-such-file", 12) == NX_OK);
    snprintf(path, sizeof(path), "%s-empty.ppm", prefix); REQUIRE(cloud_export(s, path, 800, 640));
    REQUIRE(nx_text_field_set_text(s->search, "", 0) == NX_OK);
    REQUIRE(cloud_frame(s, 800, 640, 0));
    REQUIRE(click_at(s, 170, 120)); REQUIRE(cloud_frame(s, 800, 640, 0)); REQUIRE(s->dialog == 1);
    REQUIRE(cloud_frame(s, 800, 640, 0)); REQUIRE(key(s, NX_KEY_ESCAPE));
    REQUIRE(cloud_frame(s, 800, 640, 0)); REQUIRE(!s->dialog);
    REQUIRE(nx_focus_id(s->ui) == 200);
    snprintf(path, sizeof(path), "%s-compact.ppm", prefix); REQUIRE(cloud_export(s, path, 800, 640));
    REQUIRE(nx_focus_set(s->ui, 1000) == NX_OK); REQUIRE(key(s, NX_KEY_SPACE));
    REQUIRE(cloud_frame(s, 800, 640, 0)); REQUIRE(s->selected == 0);
    REQUIRE(cloud_frame(s, 800, 640, 0));
    REQUIRE(nx_focus_set(s->ui, 203) == NX_OK); REQUIRE(key(s, NX_KEY_ENTER));
    REQUIRE(cloud_frame(s, 800, 640, 0)); REQUIRE(s->dialog == 6);
    snprintf(path, sizeof(path), "%s-compact-details.ppm", prefix); REQUIRE(cloud_export(s, path, 800, 640));
    REQUIRE(key(s, NX_KEY_ESCAPE)); REQUIRE(cloud_frame(s, 800, 640, 0)); REQUIRE(!s->dialog);
    s->page = SETTINGS; s->light = 1; s->pending_theme = 1;
    snprintf(path, sizeof(path), "%s-light.ppm", prefix); REQUIRE(cloud_export(s, path, 1100, 780));
    s->page = OVERVIEW; s->light = 0; s->pending_theme = 1;
    return 1;
}
