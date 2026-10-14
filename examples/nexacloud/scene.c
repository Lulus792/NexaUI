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
    float width, height, text_scale;
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

static nx_text_layout *layout(cloud_scene *s, const char *text, unsigned ppem) {
    ppem = (unsigned)floorf(ppem * s->text_scale + 0.5f);
    for (size_t i = 0; i < s->text_count; ++i)
        if (s->text[i].size == ppem && strcmp(s->text[i].text, text) == 0)
            return s->text[i].layout;
    if (s->text_count == 512 || strlen(text) >= sizeof(s->text[0].text)) return NULL;
    cloud_text *t = &s->text[s->text_count];
    if (nx_text_layout_create(s->fonts, text, strlen(text), NX_BIDI_LTR,
                              ppem, 10000, &t->layout) != NX_OK) return NULL;
    if (nx_text_stamp_create(t->layout, CACHE_BYTES, NULL, &t->stamp) != NX_OK) {
        nx_text_layout_destroy(t->layout); t->layout = NULL; return NULL;
    }
    memcpy(t->text, text, strlen(text) + 1u); t->size = ppem; ++s->text_count;
    return t->layout;
}

static int text(cloud_scene *s, uint64_t parent, const char *value,
                 float x, float y, unsigned size, nx_color tint) {
    nx_text_layout *t = layout(s, value, size);
    REQUIRE(t);
    const nx_text_stamp *stamp = NULL;
    for (size_t i = 0; i < s->text_count; ++i) if (s->text[i].layout == t) stamp = s->text[i].stamp;
    TRY(nx_label_stamp(s->ui, s->label_id++, parent,
        box(x, y, (float)nx_text_layout_width(t), (float)nx_text_layout_height(t)),
        stamp, (nx_point){floorf(x + .5f), floorf(y + .5f)}, tint));
    return 1;
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
        nx_text_layout *label = layout(s, name, 15); REQUIRE(label);
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
    s->text_scale = 1; s->details = 1; s->selected = 0;
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

static int sidebar(cloud_scene *s, float width) {
    TRY(nx_fill_rect(s->ui, box(0, 0, width, s->height), color(s, NX_COLOR_BACKGROUND)));
    TRY(nx_fill_rect(s->ui, box(width - 1, 0, 1, s->height), color(s, NX_COLOR_BORDER)));
    REQUIRE(icon(s, NX_ICON_CLOUD, box(width > 100 ? 23.0f : 18.0f, 25, 42, 42), color(s, NX_COLOR_ACCENT)));
    if (width > 100) {
        REQUIRE(text(s, 1, "NexaCloud", 78, 25, 26, color(s, NX_COLOR_TEXT)));
        REQUIRE(text(s, 1, "Deine Daten. Dein Zentrum.", 79, 61, 10, color(s, NX_COLOR_TEXT_MUTED)));
    }
    static const char *names[] = {"Übersicht", "Dateien", "Übertragungen", "Favoriten", "Papierkorb", "Einstellungen"};
    static const nx_icon icons[] = {NX_ICON_HOME, NX_ICON_FOLDER, NX_ICON_TRANSFER, NX_ICON_STAR, NX_ICON_TRASH, NX_ICON_SETTINGS};
    int next_page = s->page;
    for (int i = 0; i < 6; ++i) {
        int clicked;
        REQUIRE(action(s, 10u + i, 1, box(12, 112 + i * 58.0f * s->text_scale,
            width - 24, 48 * s->text_scale), names[i], icons[i], NX_ACTION_NAVIGATION,
            s->page == i, 0, &clicked));
        if (clicked) next_page = i;
        if (i == TRANSFERS && width > 200 && s->text_scale == 1) {
            TRY(nx_fill_round_rect(s->ui, box(width - 41, 128 + i * 58.0f, 21, 21), 11, color(s, NX_COLOR_ACCENT)));
            REQUIRE(text(s, 1, "2", width - 35, 130 + i * 58.0f, 13, color(s, NX_COLOR_ACCENT_TEXT)));
        }
    }
    if (next_page != s->page) {
        s->page = next_page; s->scroll_y = s->dashboard_y = 0; s->folder_depth = 0;
        TRY(nx_text_field_set_text(s->search, "", 0));
    }
    if (width > 200 && s->height > 760 * s->text_scale) {
        float y = s->height - 188;
        REQUIRE(card(s, 30, 1, box(16, y, width - 32, 145), "Verbindungsstatus"));
        TRY(nx_fill_round_rect(s->ui, box(32, y + 20, 10, 10), 5, color(s, NX_COLOR_SUCCESS)));
        REQUIRE(text(s, 30, "Verbunden · Demo", 51, y + 16, 14, color(s, NX_COLOR_SUCCESS)));
        REQUIRE(text(s, 30, "Raspberry Pi 5", 51, y + 45, 13, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 30, "192.168.4.1", 51, y + 66, 13, color(s, NX_COLOR_TEXT_MUTED)));
        int click;
        REQUIRE(action(s, 31, 30, box(28, y + 99, width - 56, 32), "Systeminformationen", NX_ICON_NONE, NX_ACTION_SECONDARY, 0, 0, &click));
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
        REQUIRE(text(s, 1, "In NexaCloud suchen …", bounds.x + 12, bounds.y + 10, 15, color(s, NX_COLOR_TEXT_MUTED)));
    if (query.length && s->page == OVERVIEW) s->page = FILES;
    return 1;
}

static int topbar(cloud_scene *s, float x) {
    float available = s->width - x - 32;
    int click;
    if (available > 820) {
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
    float search_width = fminf(380, available - 143 - (s->folder_depth && available <= 820 ? 54 : 0));
    float search_x = s->width - search_width - 140;
    REQUIRE(search_field(s, box(search_x, 26, search_width, 44 * s->text_scale)));
    REQUIRE(action(s, 41, 1, box(s->width - 125, 27, 42, 42), "Rasteransicht", NX_ICON_GRID,
        NX_ACTION_QUIET, !s->list_view, 0, &click)); if (click) s->list_view = 0;
    REQUIRE(action(s, 42, 1, box(s->width - 76, 27, 42, 42), "Listenansicht", NX_ICON_LIST,
        NX_ACTION_QUIET, s->list_view, 0, &click)); if (click) s->list_view = 1;
    TRY(nx_fill_rect(s->ui, box(x, 89 * s->text_scale, s->width - x, 1), color(s, NX_COLOR_BORDER)));
    return 1;
}

static int transfer_card(cloud_scene *s, uint64_t id, uint64_t parent, nx_rect r, int upload) {
    REQUIRE(card(s, id, parent, r, upload ? "Upload simulation.mp4" : "Download data.zip"));
    nx_color tint = upload ? rgb(174, 86, 255) : color(s, NX_COLOR_SUCCESS);
    REQUIRE(icon(s, upload ? NX_ICON_UPLOAD : NX_ICON_DOWNLOAD, box(r.x + 16, r.y + 24, 28, 28), tint));
    float tx = r.x + 62;
    REQUIRE(text(s, id, upload ? "simulation.mp4" : "data.zip", tx, r.y + 13, 15, color(s, NX_COLOR_TEXT)));
    REQUIRE(text(s, id, upload ? "62%" : "28%", r.x + r.width - 56, r.y + 13, 14, color(s, NX_COLOR_TEXT_MUTED)));
    nx_theme override = {0}; TRY(nx_theme_set_color(&override, NX_COLOR_ACCENT, upload ? color(s, NX_COLOR_ACCENT) : tint));
    TRY(nx_push_theme(s->ui, &override));
    REQUIRE(progress(s, id + 1, id, box(tx, r.y + 43, r.width - 86, 10), "Übertragungsfortschritt", upload ? .62 : .28));
    TRY(nx_pop_theme(s->ui));
    REQUIRE(text(s, id, upload ? "892 MB von 1.4 GB" : "122 MB von 436 MB", tx, r.y + 62, 13, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, id, s->paused ? "Pausiert" : upload ? "12 MB/s · Noch 42 Sekunden" : "8.4 MB/s · Noch 1 Minute", tx, r.y + 83, 12, color(s, NX_COLOR_TEXT_MUTED)));
    return 1;
}

static int chart(cloud_scene *s, uint64_t parent, nx_rect r) {
    REQUIRE(text(s, parent, "Datenrate (letzte 10 Minuten)", r.x + 20, r.y + 18, 17, color(s, NX_COLOR_TEXT)));
    REQUIRE(text(s, parent, "Download 28.4 MB/s", r.x + 20, r.y + 48, 12, color(s, NX_COLOR_ACCENT)));
    REQUIRE(text(s, parent, "Upload 6.7 MB/s", r.x + 205, r.y + 48, 12, rgb(190, 111, 255)));
    nx_rect plot = box(r.x + 68, r.y + 95, r.width - 92, r.height - 137);
    for (unsigned i = 0; i < 4; ++i) {
        float y = plot.y + plot.height * i / 3;
        TRY(nx_draw_line(s->ui, (nx_point){plot.x, y}, (nx_point){plot.x + plot.width, y}, 1, color(s, NX_COLOR_BORDER)));
    }
    REQUIRE(text(s, parent, "100 MB/s", r.x + 12, plot.y - 7, 10, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, parent, "50 MB/s", r.x + 15, plot.y + plot.height / 2 - 7, 10, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, parent, "0", r.x + 44, plot.y + plot.height - 10, 11, color(s, NX_COLOR_TEXT_MUTED)));
    for (unsigned i = 0; i <= 5; ++i) {
        float x = plot.x + plot.width * i / 5;
        TRY(nx_draw_line(s->ui, (nx_point){x, plot.y}, (nx_point){x, plot.y + plot.height}, 1, color(s, NX_COLOR_BORDER)));
        static const char *times[] = {"12:26", "12:28", "12:30", "12:32", "12:34", "12:36"};
        REQUIRE(text(s, parent, times[i], x - 15, plot.y + plot.height + 14, 11, color(s, NX_COLOR_TEXT_MUTED)));
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
    float w = viewport.width - 32;
    int narrow = w < 720 * s->text_scale;
    float content_h = narrow ? 1680 * s->text_scale : 930 * s->text_scale;
    nx_scroll_area_desc scroll = {0}; scroll.id = 100; scroll.parent_id = 1; scroll.name = span("Übersicht");
    scroll.viewport = viewport; scroll.content_size = (nx_size){viewport.width, content_h}; scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 101;
    nx_scroll_info info;
    TRY(nx_scroll_area_begin(s->ui, &scroll, &s->dashboard_x, &s->dashboard_y, &info));
    float x = viewport.x + 16, y = viewport.y + 22 - s->dashboard_y;
    REQUIRE(text(s, 100, "Guten Tag!", x, y, 32, color(s, NX_COLOR_TEXT)));
    REQUIRE(text(s, 100, "Hier ist deine NexaCloud auf einen Blick.", x, y + 49 * s->text_scale, 16, color(s, NX_COLOR_TEXT_MUTED)));
    if (!narrow) {
        REQUIRE(card(s, 110, 100, box(x + w - 300, y - 1, 300, 79), "Datum und Wetter · Beispieldaten"));
        REQUIRE(text(s, 110, "Dienstag, 14. Oktober 2026", x + w - 283, y + 11, 12, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 110, "12:36", x + w - 283, y + 35, 24, color(s, NX_COLOR_TEXT)));
        REQUIRE(icon(s, NX_ICON_SUN, box(x + w - 112, y + 23, 32, 32), color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 110, "14 °C", x + w - 65, y + 29, 16, color(s, NX_COLOR_TEXT)));
    }
    y += 100 * s->text_scale;
    unsigned cols = narrow ? 2 : 4;
    float cw = (w - 12 * (cols - 1)) / cols, ch = 153 * s->text_scale;
    static const char *titles[] = {"Speicher (NVMe)", "CPU Temperatur", "WLAN-Hotspot", "Systemstatus"};
    static const char *values[] = {"237 GB / 500 GB", "42 °C", "Aktiv", "Online"};
    static const char *captions[] = {"263 GB frei", "Normal", "5 Geräte verbunden", "Alles in Ordnung"};
    static const nx_icon icons[] = {NX_ICON_STORAGE, NX_ICON_THERMOMETER, NX_ICON_WIFI, NX_ICON_ACTIVITY};
    for (unsigned i = 0; i < 4; ++i) {
        nx_rect r = box(x + (i % cols) * (cw + 12), y + (i / cols) * (ch + 12), cw, ch);
        REQUIRE(card(s, 120 + i, 100, r, titles[i]));
        nx_color tint = color(s, i >= 2 ? NX_COLOR_SUCCESS : NX_COLOR_ACCENT);
        float ix = r.x + 16, tx = r.x + (cw < 230 ? 16 : 73);
        if (cw >= 230) REQUIRE(icon(s, icons[i], box(ix, r.y + 33, 40, 40), tint));
        REQUIRE(text(s, 120 + i, titles[i], tx, r.y + 19, 13, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 120 + i, values[i], tx, r.y + 49, cw < 230 ? 17 : 20, i >= 2 ? tint : color(s, NX_COLOR_TEXT)));
        if (i == 0) REQUIRE(progress(s, 130, 120, box(r.x + 17, r.y + 95, cw - 34, 12), "Speicherbelegung", .474));
        REQUIRE(text(s, 120 + i, captions[i], r.x + 17, r.y + 122 * s->text_scale, 12, color(s, NX_COLOR_TEXT_MUTED)));
    }
    y += (ch + 12) * (4 / cols) + 4;
    float left_w = narrow ? w : (w - 16) * .59f;
    REQUIRE(card(s, 140, 100, box(x, y, left_w, 290), "Datenrate"));
    REQUIRE(chart(s, 140, box(x, y, left_w, 290)));
    float rx = narrow ? x : x + left_w + 16;
    float ry = narrow ? y + 306 : y, rw = narrow ? w : w - left_w - 16;
    REQUIRE(card(s, 150, 100, box(rx, ry, rw, 290), "Aktive Übertragungen"));
    REQUIRE(text(s, 150, "Aktive Übertragungen (2)", rx + 18, ry + 17, 17, color(s, NX_COLOR_TEXT)));
    REQUIRE(transfer_card(s, 151, 150, box(rx + 12, ry + 54, rw - 24, 106), 1));
    REQUIRE(transfer_card(s, 153, 150, box(rx + 12, ry + 172, rw - 24, 106), 0));
    y += narrow ? 612 : 306;
    REQUIRE(card(s, 160, 100, box(x, y, left_w, 278), "Letzte Aktivitäten"));
    REQUIRE(text(s, 160, "Letzte Aktivitäten", x + 19, y + 17, 17, color(s, NX_COLOR_TEXT)));
    static const char *events[] = {"Datei hochgeladen", "Ordner erstellt", "Datei geändert", "Download abgeschlossen", "Datei gelöscht"};
    static const char *event_files[] = {"Bergsee.jpg", "/Projekte", "Notizen.txt", "Wallpapers.zip", "alt_backup.zip"};
    static const char *event_times[] = {"vor 2 Minuten", "vor 18 Minuten", "vor 1 Stunde", "vor 2 Stunden", "vor 3 Stunden"};
    static const nx_icon event_icons[] = {NX_ICON_UPLOAD, NX_ICON_FOLDER, NX_ICON_EDIT, NX_ICON_DOWNLOAD, NX_ICON_TRASH};
    for (unsigned i = 0; i < 5; ++i) {
        float row_y = y + 61 + i * 42;
        REQUIRE(icon(s, event_icons[i], box(x + 19, row_y, 22, 22), color(s, i == 4 ? NX_COLOR_ERROR : NX_COLOR_ACCENT)));
        REQUIRE(text(s, 160, events[i], x + 53, row_y + 1, 12, color(s, NX_COLOR_TEXT_MUTED)));
        if (left_w > 470) REQUIRE(text(s, 160, event_files[i], x + left_w * .5f, row_y + 1, 12, color(s, NX_COLOR_TEXT)));
        REQUIRE(text(s, 160, event_times[i], x + left_w - 108, row_y + 1, 11, color(s, NX_COLOR_TEXT_MUTED)));
        if (i < 4) TRY(nx_fill_rect(s->ui, box(x + 16, row_y + 31, left_w - 32, 1), color(s, NX_COLOR_BORDER)));
    }
    rx = narrow ? x : x + left_w + 16; ry = narrow ? y + 294 : y;
    REQUIRE(card(s, 170, 100, box(rx, ry, rw, 278), "Schnellaktionen"));
    REQUIRE(text(s, 170, "Schnellaktionen", rx + 19, ry + 17, 17, color(s, NX_COLOR_TEXT)));
    static const char *quick[] = {"Dateien öffnen", "Hochladen", "Link erstellen", "Einstellungen"};
    static const nx_icon quick_icons[] = {NX_ICON_FOLDER, NX_ICON_UPLOAD, NX_ICON_LINK, NX_ICON_SETTINGS};
    for (unsigned i = 0; i < 4; ++i) {
        nx_rect r = box(rx + 14 + (i % 2) * ((rw - 40) / 2 + 12), ry + 57 + (i / 2) * 103, (rw - 40) / 2, 91);
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

static int detail_panel(cloud_scene *s, nx_rect r, uint64_t parent) {
    cloud_file *f = &s->files[s->selected];
    REQUIRE(card(s, 300, parent, r, "Dateidetails"));
    int click;
    REQUIRE(action(s, 301, 300, box(r.x + r.width - 43, r.y + 7, 34, 34), "Details schließen", NX_ICON_CLOSE, NX_ACTION_QUIET, 0, 0, &click));
    if (click) s->details = 0;
    float x = r.x + 18, w = r.width - 36, y = r.y + 48;
    if (!f->folder) {
        TRY(nx_image(s->ui, 302, 300, box(x, y, w, w * .625f), span(f->name), thumbnail(s, (unsigned)s->selected), rgb(255,255,255)));
        y += w * .625f + 16;
    } else {
        REQUIRE(icon(s, NX_ICON_FOLDER, box(x + w / 2 - 32, y + 25, 64, 64), color(s, NX_COLOR_ACCENT)));
        y += 116;
    }
    REQUIRE(text(s, 300, f->name, x, y, 21, color(s, NX_COLOR_TEXT)));
    REQUIRE(text(s, 300, f->metadata, x, y + 34, 14, color(s, NX_COLOR_TEXT_MUTED)));
    y += 71;
    float aw = (w - 24) / 3;
    REQUIRE(action(s, 303, 300, box(x, y, aw, 45), "Öffnen", NX_ICON_FOLDER, NX_ACTION_SECONDARY, 0, 0, &click));
    if (click) { if (f->folder) { s->folder_depth = s->selected + 1; s->scroll_y = 0; s->details = 0; } else s->dialog = 5; }
    REQUIRE(action(s, 304, 300, box(x + aw + 12, y, aw, 45), "Teilen", NX_ICON_SHARE, NX_ACTION_SECONDARY, 0, 0, &click)); if (click) s->dialog = 3;
    REQUIRE(action(s, 305, 300, box(x + 2 * (aw + 12), y, aw, 45), "Favorit", NX_ICON_STAR, NX_ACTION_SECONDARY, f->favorite, 0, &click)); if (click) f->favorite ^= 1;
    y += 69;
    static const char *labels[] = {"Typ", "Größe", "Erstellt", "Geändert", "Auflösung", "Speicherort"};
    const char *values[] = {f->folder ? "Ordner" : strstr(f->name, ".png") ? "PNG-Bild" : "JPEG-Bild", f->metadata, "14.10.2026 11:02", "14.10.2026 11:02", f->folder ? "–" : "256 × 160 (Demo)", "/Bilder/Natur/"};
    for (unsigned i = 0; i < 6; ++i) {
        REQUIRE(text(s, 300, labels[i], x, y + i * 31, 13, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 300, values[i], x + w * .40f, y + i * 31, 13, color(s, NX_COLOR_TEXT)));
    }
    y += 205;
    TRY(nx_fill_rect(s->ui, box(x, y - 12, w, 1), color(s, NX_COLOR_BORDER)));
    REQUIRE(text(s, 300, "Beschreibung", x, y, 15, color(s, NX_COLOR_TEXT)));
    nx_text_field_desc d = {0}; d.id = 306; d.parent_id = 300; d.bounds = box(x, y + 30, w, 72);
    d.name = span("Dateibeschreibung bearbeiten"); d.fonts = s->fonts; d.pixels_per_em = 13;
    d.glyph_cache_bytes = CACHE_BYTES; d.multiline = 1; d.ime_window = s->window;
    nx_interaction_state state; int valid, submitted;
    if (!f->description_field) TRY(nx_text_field_create(f->description, strlen(f->description), 16384, &f->description_field));
    TRY(nx_text_field_widget(s->ui, &d, f->description_field, &state, &valid, &submitted));
    if (state.hovered) s->cursor = NX_CURSOR_TEXT;
    y += 126;
    REQUIRE(text(s, 300, "Tags", x, y, 15, color(s, NX_COLOR_TEXT)));
    REQUIRE(text(s, 300, "Berge    ·    Natur    ·    Sommer", x, y + 34, 14, color(s, NX_COLOR_TEXT_MUTED)));
    return 1;
}

static int file_browser(cloud_scene *s, nx_rect r) {
    int click;
    if (s->selected >= 0 && ((s->page == TRASH) != !!s->files[s->selected].deleted ||
        (s->page == FAVORITES && !s->files[s->selected].favorite) ||
        (s->page == FILES && s->files[s->selected].parent != s->folder_depth))) s->selected = -1;
    float toolbar_y = r.y + 15;
    int compact = r.width < 620;
    REQUIRE(action(s, 200, 1, box(r.x + 16, toolbar_y, compact ? 44.0f : 148.0f, 44), "Hochladen", NX_ICON_PLUS, NX_ACTION_PRIMARY, 0, s->page == TRASH, &click)); if (click) s->dialog = 1;
    REQUIRE(action(s, 201, 1, box(r.x + (compact ? 72.0f : 176.0f), toolbar_y, compact ? 44.0f : 156.0f, 44), "Neuer Ordner", NX_ICON_FOLDER, NX_ACTION_SECONDARY, 0, s->file_count == 20 || s->page == TRASH, &click));
    if (click) {
        cloud_file *f = &s->files[s->file_count++];
        snprintf(f->name, sizeof(f->name), "Neuer Ordner %u", s->file_count - 12); f->folder = 1; f->metadata = "Ordner · Leer"; f->parent = s->folder_depth;
        notify(s, "Ordner in den Beispieldaten erstellt.");
    }
    REQUIRE(action(s, 202, 1, box(r.x + (compact ? 128.0f : 344.0f), toolbar_y, compact ? 44.0f : 126.0f, 44), s->page == TRASH ? "Zurücklegen" : "Löschen", NX_ICON_TRASH, NX_ACTION_SECONDARY, 0, s->selected < 0, &click));
    if (click && s->selected >= 0) { s->files[s->selected].deleted ^= 1; s->details = 0; s->selected = -1; notify(s, s->page == TRASH ? "Datei wiederhergestellt." : "Datei in den Papierkorb verschoben."); }
    REQUIRE(action(s, 203, 1, box(r.x + r.width - 56, toolbar_y, 42, 44), "Dateidetails umschalten", NX_ICON_INFO, NX_ACTION_SECONDARY, s->details, s->selected < 0, &click));
    if (click) { if (r.width < 950 * s->text_scale) { s->dialog = 6; s->details = 1; } else s->details ^= 1; }
    float inspector = s->details && s->selected >= 0 && r.width >= 950 * s->text_scale ? 350.0f : 0.0f;
    nx_rect content = box(r.x + 16, r.y + 82, r.width - 32 - (inspector ? inspector + 16 : 0), r.height - 98);
    REQUIRE(card(s, 210, 1, content, "Dateien"));
    nx_utf8_span query; TRY(nx_text_field_text(s->search, &query));
    unsigned visible[20], count = 0;
    for (unsigned i = 0; i < s->file_count; ++i) {
        cloud_file *f = &s->files[i];
        if ((s->page == TRASH) != !!f->deleted || (s->page == FAVORITES && !f->favorite) ||
            (s->page == FILES && s->folder_depth != f->parent) || !matches(f->name, query)) continue;
        visible[count++] = i;
    }
    unsigned cols = s->list_view ? 1 : (unsigned)fmaxf(1, floorf((content.width - 16) / (178 * s->text_scale)));
    if (cols > 4) cols = 4;
    float gap = 12, cw = (content.width - 32 - (cols - 1) * gap) / cols;
    float ch = s->list_view ? 67 * s->text_scale : cw * .625f + 73 * s->text_scale;
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
    scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 212;
    nx_scroll_info info;
    TRY(nx_scroll_area_begin(s->ui, &scroll, &s->scroll_x, &s->scroll_y, &info));
    if (!count) {
        REQUIRE(icon(s, NX_ICON_FOLDER, box(content.x + 36, content.y + 45, 48, 48), color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 211, query.length ? "Keine passenden Dateien" : "Dieser Bereich ist leer", content.x + 36, content.y + 116, 22, color(s, NX_COLOR_TEXT)));
        REQUIRE(text(s, 211, "Ändere die Suche oder wähle einen anderen Bereich.", content.x + 36, content.y + 158, 13, color(s, NX_COLOR_TEXT_MUTED)));
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
        float image_w = s->list_view ? 65 : cw - 12;
        nx_rect image = box(tile.x + 6, tile.y + 6, image_w, s->list_view ? ch - 12 : image_w * .625f);
        if (f->folder) REQUIRE(icon(s, NX_ICON_FOLDER, box(image.x + image.width / 2 - 24, image.y + image.height / 2 - 24, 48, 48), color(s, NX_COLOR_ACCENT)));
        else TRY(nx_image(s->ui, 2000 + i, 1000 + i, image, span(f->name), thumbnail(s, i), rgb(255,255,255)));
        float tx = s->list_view ? tile.x + 90 : tile.x + 12;
        float ty = s->list_view ? tile.y + 10 : tile.y + image.height + 18;
        TRY(nx_push_clip(s->ui, box(tx, ty, s->list_view ? cw - 110 : cw - 22, ch)));
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
        ds.viewport = panel; ds.content_size = (nx_size){panel.width, 940}; ds.axes = NX_SCROLL_Y; ds.thumb_y_id = 311;
        nx_scroll_info di; TRY(nx_scroll_area_begin(s->ui, &ds, &s->detail_x, &s->detail_y, &di));
        REQUIRE(detail_panel(s, box(panel.x, panel.y - s->detail_y, panel.width - 10, fmaxf(panel.height, 940)), 310));
        TRY(nx_scroll_area_end(s->ui));
    }
    return 1;
}

static int settings(cloud_scene *s, nx_rect r) {
    nx_scroll_area_desc scroll = {0}; scroll.id = 410; scroll.parent_id = 1; scroll.name = span("Einstellungen");
    scroll.viewport = r; scroll.content_size = (nx_size){r.width, 570}; scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 411;
    nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &scroll, &s->dashboard_x, &s->dashboard_y, &info));
    float x = r.x + 28, y = r.y + 28 - s->dashboard_y, w = fminf(r.width - 56, 740);
    REQUIRE(text(s, 410, "Einstellungen", x, y, 30, color(s, NX_COLOR_TEXT)));
    REQUIRE(text(s, 410, "Gestalte deine NexaCloud.", x, y + 49, 16, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(card(s, 400, 410, box(x, y + 100, w, 300), "Darstellung"));
    REQUIRE(text(s, 400, "Darstellung", x + 20, y + 120, 20, color(s, NX_COLOR_TEXT)));
    int click;
    REQUIRE(action(s, 401, 400, box(x + 20, y + 168, w - 40, 46), s->light ? "Dunkles Design aktivieren" : "Helles Design aktivieren", NX_ICON_SUN, NX_ACTION_SECONDARY, 0, 0, &click));
    if (click) { s->light ^= 1; s->pending_theme = 1; }
    REQUIRE(action(s, 402, 400, box(x + 20, y + 228, w - 40, 46), s->reduced_motion ? "Bewegung reduziert" : "Bewegung reduzieren", NX_ICON_ACTIVITY, NX_ACTION_SECONDARY, s->reduced_motion, 0, &click));
    if (click) s->reduced_motion ^= 1;
    REQUIRE(action(s, 403, 400, box(x + 20, y + 288, w - 40, 46), "Systeminformationen", NX_ICON_INFO, NX_ACTION_SECONDARY, 0, 0, &click)); if (click) s->dialog = 4;
    REQUIRE(text(s, 400, "NexaUI · C17 · Native Oberfläche", x + 20, y + 364, 13, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, 410, "Diese Anwendung zeigt lokale Beispieldaten.", x, y + 436, 15, color(s, NX_COLOR_TEXT_MUTED)));
    REQUIRE(text(s, 410, "Der NexaCloud-Server wird später angebunden.", x, y + 467, 15, color(s, NX_COLOR_TEXT_MUTED)));
    TRY(nx_scroll_area_end(s->ui));
    return 1;
}

static int dialog(cloud_scene *s) {
    float w = fminf(540, s->width - 40), h = 310;
    nx_rect r = box((s->width - w) / 2, (s->height - h) / 2, w, h);
    static const char *titles[] = {"", "Dateien hochladen", "Neuer Ordner", "Freigabelink", "Systeminformationen", "Bildvorschau", "Dateidetails"};
    if (s->dialog == 6) { h = s->height - 40; r = box((s->width - w) / 2, 20, w, h); }
    nx_overlay_desc d = {0}; d.panel.id = 500; d.panel.bounds = r; d.panel.name = span(titles[s->dialog]);
    d.backdrop_id = 501; d.dismiss_on_escape = 1; d.dismiss_on_outside = 1;
    int dismiss; TRY(nx_modal_begin(s->ui, &d, &dismiss));
    REQUIRE(text(s, 500, titles[s->dialog], r.x + 24, r.y + 24, 24, color(s, NX_COLOR_TEXT)));
    if (s->dialog == 6 && s->selected >= 0) {
        nx_scroll_area_desc ds = {0}; ds.id = 310; ds.parent_id = 500; ds.name = span("Dateidetails scrollen");
        ds.viewport = box(r.x + 8, r.y + 65, r.width - 16, r.height - 145);
        ds.content_size = (nx_size){ds.viewport.width, 1040}; ds.axes = NX_SCROLL_Y; ds.thumb_y_id = 311;
        nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &ds, &s->detail_x, &s->detail_y, &info));
        REQUIRE(detail_panel(s, box(ds.viewport.x, ds.viewport.y - s->detail_y, ds.viewport.width - 10, 1040), 310));
        TRY(nx_scroll_area_end(s->ui));
        if (!s->details) dismiss = 1;
    } else if (s->dialog == 5 && s->selected >= 0) {
        TRY(nx_image(s->ui, 505, 500, box(r.x + 24, r.y + 70, 240, 150), span(s->files[s->selected].name), thumbnail(s, (unsigned)s->selected), rgb(255,255,255)));
    } else {
        const char *message = s->dialog == 1 ? "Beispielmodus: keine Dateien werden übertragen." :
            s->dialog == 3 ? "Beispiel-Link · kein öffentlicher Server verbunden." : "NexaCloud UI · NexaUI 0.1 · lokale Beispieldaten";
        REQUIRE(text(s, 500, message, r.x + 24, r.y + 90, 14, color(s, NX_COLOR_TEXT_MUTED)));
        REQUIRE(text(s, 500, s->dialog == 3 ? "https://nexacloud.example/s/bergsee" : "Gemeinsamer C17-Kern für Windows, Linux und macOS.", r.x + 24, r.y + 134, 13, color(s, NX_COLOR_TEXT)));
    }
    int click;
    REQUIRE(action(s, 502, 500, box(r.x + 24, r.y + h - 68, w - 48, 44), "Schließen", NX_ICON_CLOSE, NX_ACTION_PRIMARY, 0, 0, &click));
    TRY(nx_overlay_end(s->ui));
    if (dismiss || click) { TRY(nx_overlay_close(s->ui)); s->dialog = 0; }
    return 1;
}

int cloud_frame(cloud_scene *s, float width, float height, double dt) {
    s->width = width; s->height = height; s->label_id = 10000; s->cursor = NX_CURSOR_DEFAULT;
    if (s->pending_theme) {
        TRY(nx_theme_preset(s->light ? NX_THEME_LIGHT : NX_THEME_MIDNIGHT, &s->theme));
        TRY(nx_context_set_theme(s->ui, &s->theme)); s->pending_theme = 0;
    }
    TRY(nx_context_set_reduced_motion(s->ui, s->reduced_motion));
    TRY(nx_begin_frame(s->ui, width, height, dt));
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
        REQUIRE(text(s, 1, "Bitte das Fenster vergrößern.", 16, 60, 14, color(s, NX_COLOR_TEXT_MUTED)));
        TRY(nx_end_frame(s->ui)); return 1;
    }
    float sidebar_w = width >= 1000 * s->text_scale ? 236.0f : 76.0f;
    REQUIRE(sidebar(s, sidebar_w));
    REQUIRE(topbar(s, sidebar_w));
    float content_w = fminf(width - sidebar_w, 1640);
    nx_rect content = box(sidebar_w + (width - sidebar_w - content_w) / 2,
        90 * s->text_scale, content_w, height - 120 * s->text_scale);
    if (s->page == OVERVIEW) REQUIRE(dashboard(s, content));
    else if (s->page == SETTINGS) REQUIRE(settings(s, content));
    else if (s->page == TRANSFERS) {
        nx_scroll_area_desc scroll = {0}; scroll.id = 610; scroll.parent_id = 1; scroll.name = span("Übertragungen");
        scroll.viewport = content; scroll.content_size = (nx_size){content.width, 480}; scroll.axes = NX_SCROLL_Y; scroll.thumb_y_id = 611;
        nx_scroll_info info; TRY(nx_scroll_area_begin(s->ui, &scroll, &s->dashboard_x, &s->dashboard_y, &info));
        content.y -= s->dashboard_y;
        REQUIRE(text(s, 610, "Übertragungen", content.x + 28, content.y + 26, 30, color(s, NX_COLOR_TEXT)));
        float w = fminf(content.width - 56, 760);
        REQUIRE(transfer_card(s, 600, 610, box(content.x + 28, content.y + 106, w, 118), 1));
        REQUIRE(transfer_card(s, 602, 610, box(content.x + 28, content.y + 240, w, 118), 0));
        int click;
        REQUIRE(action(s, 604, 610, box(content.x + 28, content.y + 384, w, 46), s->paused ? "Übertragungen fortsetzen" : "Alle pausieren", NX_ICON_TRANSFER, NX_ACTION_SECONDARY, 0, 0, &click));
        if (click) s->paused ^= 1;
        TRY(nx_scroll_area_end(s->ui));
    } else REQUIRE(file_browser(s, content));
    REQUIRE(text(s, 1, "NexaCloud UI · Beispieldaten", sidebar_w + 20, height - 25, 11, color(s, NX_COLOR_TEXT_MUTED)));
    if (s->notice_seconds > 0) {
        s->notice_seconds -= dt;
        nx_rect toast = box(sidebar_w + 24, height - 90, fminf(width - sidebar_w - 48, 560), 48);
        REQUIRE(card(s, 700, 1, toast, "Statusmeldung"));
        REQUIRE(text(s, 700, s->notice, toast.x + 14, toast.y + 14, 13, color(s, NX_COLOR_SUCCESS)));
    }
    if (s->dialog) REQUIRE(dialog(s));
    TRY(nx_end_frame(s->ui)); return 1;
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
