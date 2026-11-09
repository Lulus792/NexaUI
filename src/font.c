#include "nexaui/nexaui.h"
#include "font_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct nx_font_record {
    uint32_t tag, checksum, offset, length;
} nx_font_record;

struct nx_font {
    nx_allocator allocator;
    const uint8_t *bytes;
    size_t length;
    nx_font_record *tables;
    uint32_t count;
    const uint8_t *cmap_subtable;
    uint32_t cmap_entries;
    uint16_t cmap_format;
    uint16_t glyph_limit;
    nx_result cmap_result;
    nx_result kern_result;
};

static nx_result nx_font_select_cmap(nx_font *font);
static nx_result nx_font_validate_kern(const nx_font *font);

static void *nx_font_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_font_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static uint16_t nx_font_u16(const uint8_t *data) {
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static uint32_t nx_font_u32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | data[3];
}

static int16_t nx_font_i16(const uint8_t *data) {
    uint16_t value = nx_font_u16(data);
    return (int16_t)(value < UINT16_C(0x8000) ? (int32_t)value :
                     (int32_t)value - 65536);
}

static int nx_font_compare_tags(const void *left, const void *right) {
    uint32_t a = ((const nx_font_record *)left)->tag;
    uint32_t b = ((const nx_font_record *)right)->tag;
    return a < b ? -1 : a > b ? 1 : 0;
}

nx_result nx_font_open(const uint8_t *bytes, size_t length,
                       const nx_allocator *allocator, nx_font **out_font) {
    if (!out_font) return NX_ERROR_ARGUMENT;
    *out_font = NULL;
    if (!bytes || length < 12u) return NX_ERROR_ARGUMENT;
    uint32_t signature = nx_font_u32(bytes);
    if (signature != UINT32_C(0x00010000) &&
        signature != NX_SFNT_TAG('O', 'T', 'T', 'O') &&
        signature != NX_SFNT_TAG('t', 'r', 'u', 'e'))
        return NX_ERROR_ARGUMENT;
    uint32_t count = nx_font_u16(bytes + 4u);
    if (!count) return NX_ERROR_ARGUMENT;
    if (count > NX_SFNT_MAX_TABLES) return NX_ERROR_LIMIT;
    size_t directory_end = 12u + (size_t)count * 16u;
    if (directory_end > length) return NX_ERROR_ARGUMENT;

    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.allocate = nx_font_allocate;
        a.deallocate = nx_font_deallocate;
        a.user = NULL;
    }
    nx_font *font = (nx_font *)a.allocate(a.user, sizeof(nx_font));
    if (!font) return NX_ERROR_MEMORY;
    memset(font, 0, sizeof(*font));
    font->allocator = a;
    font->bytes = bytes;
    font->length = length;
    font->count = count;
    font->tables = (nx_font_record *)a.allocate(a.user,
                                                (size_t)count * sizeof(nx_font_record));
    if (!font->tables) {
        a.deallocate(a.user, font, sizeof(nx_font));
        return NX_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *entry = bytes + 12u + (size_t)i * 16u;
        nx_font_record *table = &font->tables[i];
        table->tag = nx_font_u32(entry);
        table->checksum = nx_font_u32(entry + 4u);
        table->offset = nx_font_u32(entry + 8u);
        table->length = nx_font_u32(entry + 12u);
        size_t offset = table->offset, table_length = table->length;
        if (offset > length || table_length > length - offset ||
            (table_length && offset < directory_end)) {
            nx_font_close(font);
            return NX_ERROR_ARGUMENT;
        }
    }
    qsort(font->tables, count, sizeof(nx_font_record), nx_font_compare_tags);
    for (uint32_t i = 1; i < count; ++i) {
        if (font->tables[i - 1u].tag == font->tables[i].tag) {
            nx_font_close(font);
            return NX_ERROR_ARGUMENT;
        }
    }
    nx_bytes_view maxp;
    if (nx_font_find_table(font, NX_SFNT_TAG('m', 'a', 'x', 'p'), &maxp) == NX_OK &&
        maxp.length >= 6u) {
        uint32_t version = nx_font_u32(maxp.data);
        if ((version == UINT32_C(0x00010000) && maxp.length >= 32u) ||
            version == UINT32_C(0x00005000))
            font->glyph_limit = nx_font_u16(maxp.data + 4u);
    }
    font->cmap_result = nx_font_select_cmap(font);
    font->kern_result = nx_font_validate_kern(font);
    *out_font = font;
    return NX_OK;
}

void nx_font_close(nx_font *font) {
    if (!font) return;
    nx_allocator a = font->allocator;
    if (font->tables)
        a.deallocate(a.user, font->tables,
                     (size_t)font->count * sizeof(nx_font_record));
    a.deallocate(a.user, font, sizeof(nx_font));
}

void *nx_font_scratch_allocate(const nx_font *font, size_t size) {
    return font->allocator.allocate(font->allocator.user, size);
}

void nx_font_scratch_deallocate(const nx_font *font, void *pointer, size_t size) {
    font->allocator.deallocate(font->allocator.user, pointer, size);
}

nx_result nx_font_table_count(const nx_font *font, size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!font) return NX_ERROR_ARGUMENT;
    *out_count = font->count;
    return NX_OK;
}

static nx_bytes_view nx_font_view(const nx_font *font, const nx_font_record *table) {
    nx_bytes_view view;
    view.data = font->bytes + table->offset;
    view.length = table->length;
    view.checksum = table->checksum;
    return view;
}

nx_result nx_font_table_at(const nx_font *font, uint32_t index,
                           uint32_t *out_tag, nx_bytes_view *out_view) {
    if (!out_tag || !out_view) return NX_ERROR_ARGUMENT;
    *out_tag = 0;
    *out_view = (nx_bytes_view){0};
    if (!font || index >= font->count) return NX_ERROR_ARGUMENT;
    const nx_font_record *table = &font->tables[index];
    *out_tag = table->tag;
    *out_view = nx_font_view(font, table);
    return NX_OK;
}

nx_result nx_font_find_table(const nx_font *font, uint32_t tag,
                             nx_bytes_view *out_view) {
    if (!out_view) return NX_ERROR_ARGUMENT;
    *out_view = (nx_bytes_view){0};
    if (!font) return NX_ERROR_ARGUMENT;
    uint32_t low = 0, high = font->count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2u;
        if (font->tables[mid].tag < tag) low = mid + 1u;
        else high = mid;
    }
    if (low == font->count || font->tables[low].tag != tag)
        return NX_ERROR_ARGUMENT;
    *out_view = nx_font_view(font, &font->tables[low]);
    return NX_OK;
}

/* Decode twice: first validate/count, then write. No error can expose a partial
   caller buffer. This parser does not add state or ownership to nx_font. */
static nx_result nx_font_name_decode(const uint8_t *bytes, size_t length,
                                     char *out, size_t *out_length) {
    size_t used = 0;
    if (length & 1u) return NX_ERROR_ARGUMENT;
    for (size_t at = 0; at < length; at += 2u) {
        uint32_t scalar = nx_font_u16(bytes + at);
        if (scalar >= 0xd800u && scalar <= 0xdbffu) {
            if (length - at < 4u) return NX_ERROR_ARGUMENT;
            uint32_t tail = nx_font_u16(bytes + at + 2u);
            if (tail < 0xdc00u || tail > 0xdfffu) return NX_ERROR_ARGUMENT;
            scalar = 0x10000u + ((scalar - 0xd800u) << 10u) + tail - 0xdc00u;
            at += 2u;
        } else if (scalar >= 0xdc00u && scalar <= 0xdfffu) return NX_ERROR_ARGUMENT;
        if (!scalar) return NX_ERROR_ARGUMENT;
        size_t count = scalar < 0x80u ? 1u : scalar < 0x800u ? 2u : scalar < 0x10000u ? 3u : 4u;
        if (count > 4096u - used) return NX_ERROR_LIMIT;
        if (out) {
            if (count == 1u) out[used] = (char)scalar;
            else {
                out[used] = (char)((count == 2u ? 0xc0u : count == 3u ? 0xe0u : 0xf0u) |
                                   (scalar >> (6u * (unsigned)(count - 1u))));
                for (size_t i = 1; i < count; ++i)
                    out[used + i] = (char)(0x80u | ((scalar >> (6u * (unsigned)(count - i - 1u))) & 0x3fu));
            }
        }
        used += count;
    }
    if (out) out[used] = 0;
    *out_length = used;
    return NX_OK;
}

nx_result nx_font_name_utf8(const nx_font *font, uint16_t name_id,
                            char *buffer, size_t capacity, size_t *out_length) {
    if (!out_length) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    if (!font || (!buffer && capacity)) return NX_ERROR_ARGUMENT;
    nx_bytes_view table;
    if (nx_font_find_table(font, NX_SFNT_TAG('n','a','m','e'), &table) != NX_OK)
        return NX_ERROR_UNSUPPORTED;
    if (table.length < 6u) return NX_ERROR_ARGUMENT;
    uint16_t version = nx_font_u16(table.data), count = nx_font_u16(table.data + 2u);
    if (version > 1u) return NX_ERROR_UNSUPPORTED;
    if (count > 4096u) return NX_ERROR_LIMIT;
    size_t end = 6u + (size_t)count * 12u, storage = nx_font_u16(table.data + 4u), tags = 0;
    if (end > table.length) return NX_ERROR_ARGUMENT;
    if (version == 1u) {
        if (table.length - end < 2u) return NX_ERROR_ARGUMENT;
        tags = nx_font_u16(table.data + end);
        if (tags > 4096u) return NX_ERROR_LIMIT;
        size_t start = end + 2u; end = start + tags * 4u;
        if (end > table.length || storage < end || storage > table.length) return NX_ERROR_ARGUMENT;
        for (size_t i = 0; i < tags; ++i) {
            size_t length = nx_font_u16(table.data + start + i * 4u);
            size_t offset = nx_font_u16(table.data + start + i * 4u + 2u);
            if (offset > table.length - storage || length > table.length - storage - offset)
                return NX_ERROR_ARGUMENT;
        }
    }
    if (storage < end || storage > table.length) return NX_ERROR_ARGUMENT;
    const uint8_t *selected = NULL; size_t selected_length = 0;
    uint64_t best = UINT64_MAX;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *entry = table.data + 6u + i * 12u;
        uint16_t platform = nx_font_u16(entry), encoding = nx_font_u16(entry + 2u);
        uint16_t language = nx_font_u16(entry + 4u), id = nx_font_u16(entry + 6u);
        size_t length = nx_font_u16(entry + 8u), offset = nx_font_u16(entry + 10u);
        if (offset > table.length - storage || length > table.length - storage - offset)
            return NX_ERROR_ARGUMENT;
        if (id != name_id || (language >= 0x8000u && (size_t)(language - 0x8000u) >= tags)) continue;
        if (!((platform == 0u && encoding <= 4u) ||
              (platform == 3u && (encoding == 0u || encoding == 1u || encoding == 10u)))) continue;
        uint64_t priority = platform == 3u && language == 0x0409u ? 0u :
                            platform == 0u && !language ? 1u : 2u;
        uint64_t rank = (priority << 48u) | ((uint64_t)language << 32u) |
                        ((uint64_t)platform << 16u) | encoding;
        if (rank < best) {best = rank; selected = table.data + storage + offset; selected_length = length;}
    }
    if (!selected) return NX_ERROR_UNSUPPORTED;
    size_t needed = 0;
    nx_result result = nx_font_name_decode(selected, selected_length, NULL, &needed);
    if (result != NX_OK) return result;
    *out_length = needed;
    if (!buffer && !capacity) return NX_OK;
    if (capacity <= needed) return NX_ERROR_LIMIT;
    return nx_font_name_decode(selected, selected_length, buffer, out_length);
}

nx_result nx_font_get_style(const nx_font *font, nx_font_style *out_style) {
    if (!out_style) return NX_ERROR_ARGUMENT;
    *out_style = (nx_font_style){0};
    if (!font) return NX_ERROR_ARGUMENT;
    nx_bytes_view table;
    if (nx_font_find_table(font, NX_SFNT_TAG('O','S','/','2'), &table) != NX_OK)
        return NX_ERROR_UNSUPPORTED;
    if (table.length < 2u) return NX_ERROR_ARGUMENT;
    uint16_t version = nx_font_u16(table.data);
    if (version > 5u) return NX_ERROR_UNSUPPORTED;
    const size_t minimum[] = {68u,86u,96u,96u,96u,100u};
    if (table.length < minimum[version]) return NX_ERROR_ARGUMENT;
    uint16_t weight = nx_font_u16(table.data + 4u), width = nx_font_u16(table.data + 6u);
    if (!weight || weight > 1000u || !width || width > 9u) return NX_ERROR_ARGUMENT;
    uint16_t selection = nx_font_u16(table.data + 62u);
    nx_font_slant slant = version >= 4u && (selection & 512u) ? NX_FONT_SLANT_OBLIQUE :
                         (selection & 1u) ? NX_FONT_SLANT_ITALIC : NX_FONT_SLANT_UPRIGHT;
    *out_style = (nx_font_style){weight,width,slant};
    return NX_OK;
}

nx_result nx_font_get_metrics(const nx_font *font, nx_font_metrics *out_metrics) {
    if (!out_metrics) return NX_ERROR_ARGUMENT;
    *out_metrics = (nx_font_metrics){0};
    if (!font) return NX_ERROR_ARGUMENT;
    nx_bytes_view head, maxp, hhea, hmtx;
    if (nx_font_find_table(font, NX_SFNT_TAG('h', 'e', 'a', 'd'), &head) != NX_OK ||
        nx_font_find_table(font, NX_SFNT_TAG('m', 'a', 'x', 'p'), &maxp) != NX_OK ||
        nx_font_find_table(font, NX_SFNT_TAG('h', 'h', 'e', 'a'), &hhea) != NX_OK ||
        nx_font_find_table(font, NX_SFNT_TAG('h', 'm', 't', 'x'), &hmtx) != NX_OK ||
        head.length < 54u || maxp.length < 6u || hhea.length < 36u ||
        nx_font_u32(head.data) != UINT32_C(0x00010000) ||
        nx_font_u32(head.data + 12u) != UINT32_C(0x5f0f3cf5) ||
        (nx_font_u32(maxp.data) != UINT32_C(0x00010000) &&
         nx_font_u32(maxp.data) != UINT32_C(0x00005000)) ||
        (nx_font_u32(maxp.data) == UINT32_C(0x00010000) &&
         maxp.length < 32u) ||
        nx_font_u32(hhea.data) != UINT32_C(0x00010000) ||
        nx_font_i16(hhea.data + 32u) != 0)
        return NX_ERROR_ARGUMENT;
    nx_font_metrics metrics = {0};
    metrics.units_per_em = nx_font_u16(head.data + 18u);
    metrics.index_to_loc_format = nx_font_i16(head.data + 50u);
    metrics.glyph_count = nx_font_u16(maxp.data + 4u);
    metrics.ascender = nx_font_i16(hhea.data + 4u);
    metrics.descender = nx_font_i16(hhea.data + 6u);
    metrics.line_gap = nx_font_i16(hhea.data + 8u);
    metrics.number_of_hmetrics = nx_font_u16(hhea.data + 34u);
    if (metrics.units_per_em < 16u || metrics.units_per_em > 16384u ||
        (metrics.index_to_loc_format != 0 && metrics.index_to_loc_format != 1) ||
        !metrics.glyph_count || !metrics.number_of_hmetrics ||
        metrics.number_of_hmetrics > metrics.glyph_count)
        return NX_ERROR_ARGUMENT;
    size_t required = (size_t)metrics.number_of_hmetrics * 4u +
                      (size_t)(metrics.glyph_count - metrics.number_of_hmetrics) * 2u;
    if (hmtx.length < required) return NX_ERROR_ARGUMENT;
    *out_metrics = metrics;
    return NX_OK;
}

nx_result nx_font_glyph_hmetrics(const nx_font *font, uint16_t glyph_index,
                                 uint16_t *out_advance, int16_t *out_lsb) {
    if (!out_advance || !out_lsb) return NX_ERROR_ARGUMENT;
    *out_advance = 0;
    *out_lsb = 0;
    nx_font_metrics metrics;
    if (nx_font_get_metrics(font, &metrics) != NX_OK ||
        glyph_index >= metrics.glyph_count) return NX_ERROR_ARGUMENT;
    nx_bytes_view hmtx;
    if (nx_font_find_table(font, NX_SFNT_TAG('h', 'm', 't', 'x'), &hmtx) != NX_OK)
        return NX_ERROR_ARGUMENT;
    uint16_t advance_index = glyph_index < metrics.number_of_hmetrics
                             ? glyph_index : (uint16_t)(metrics.number_of_hmetrics - 1u);
    *out_advance = nx_font_u16(hmtx.data + (size_t)advance_index * 4u);
    size_t lsb_offset = glyph_index < metrics.number_of_hmetrics
                        ? (size_t)glyph_index * 4u + 2u
                        : (size_t)metrics.number_of_hmetrics * 4u +
                          (size_t)(glyph_index - metrics.number_of_hmetrics) * 2u;
    *out_lsb = nx_font_i16(hmtx.data + lsb_offset);
    return NX_OK;
}

static nx_result nx_font_validate_kern(const nx_font *font) {
    nx_bytes_view table;
    if (nx_font_find_table(font, NX_SFNT_TAG('k', 'e', 'r', 'n'), &table) != NX_OK)
        return NX_OK;
    if (table.length < 4u || nx_font_u16(table.data) != 0u)
        return NX_ERROR_ARGUMENT;
    uint16_t subtable_count = nx_font_u16(table.data + 2u);
    size_t offset = 4u;
    for (uint32_t sub = 0; sub < subtable_count; ++sub) {
        if (offset > table.length || table.length - offset < 6u)
            return NX_ERROR_ARGUMENT;
        const uint8_t *base = table.data + offset;
        size_t length = nx_font_u16(base + 2u);
        uint16_t coverage = nx_font_u16(base + 4u);
        if (length < 6u || length > table.length - offset)
            return NX_ERROR_ARGUMENT;
        if ((coverage >> 8u) == 0u && (coverage & 7u) == 1u) {
            if (nx_font_u16(base) != 0u || length < 14u)
                return NX_ERROR_ARGUMENT;
            size_t pairs = nx_font_u16(base + 6u);
            if (pairs > (length - 14u) / 6u)
                return NX_ERROR_ARGUMENT;
            uint32_t prior = 0;
            for (size_t i = 0; i < pairs; ++i) {
                const uint8_t *entry = base + 14u + i * 6u;
                uint32_t pair = ((uint32_t)nx_font_u16(entry) << 16u) |
                                nx_font_u16(entry + 2u);
                if (i && pair <= prior) return NX_ERROR_ARGUMENT;
                prior = pair;
            }
        }
        offset += length;
    }
    return NX_OK;
}

nx_result nx_font_kerning(const nx_font *font, uint16_t left_glyph,
                          uint16_t right_glyph, int32_t *out_adjustment) {
    if (!font || !out_adjustment) return NX_ERROR_ARGUMENT;
    if (font->kern_result != NX_OK) return font->kern_result;
    nx_bytes_view table;
    if (nx_font_find_table(font, NX_SFNT_TAG('k', 'e', 'r', 'n'), &table) != NX_OK) {
        *out_adjustment = 0;
        return NX_OK;
    }
    size_t offset = 4u;
    uint16_t subtable_count = nx_font_u16(table.data + 2u);
    int32_t adjustment = 0;
    uint32_t key = ((uint32_t)left_glyph << 16u) | right_glyph;
    for (uint32_t sub = 0; sub < subtable_count; ++sub) {
        const uint8_t *base = table.data + offset;
        size_t length = nx_font_u16(base + 2u);
        uint16_t coverage = nx_font_u16(base + 4u);
        if ((coverage >> 8u) == 0u && (coverage & 7u) == 1u) {
            size_t pairs = nx_font_u16(base + 6u);
            size_t low = 0, high = pairs;
            while (low < high) {
                size_t middle = low + (high - low) / 2u;
                const uint8_t *entry = base + 14u + middle * 6u;
                uint32_t pair = ((uint32_t)nx_font_u16(entry) << 16u) |
                                nx_font_u16(entry + 2u);
                if (pair < key) low = middle + 1u;
                else high = middle;
            }
            if (low < pairs) {
                const uint8_t *entry = base + 14u + low * 6u;
                uint32_t pair = ((uint32_t)nx_font_u16(entry) << 16u) |
                                nx_font_u16(entry + 2u);
                if (pair == key) {
                    int16_t value = nx_font_i16(entry + 4u);
                    if (coverage & 8u) adjustment = value;
                    else adjustment += value;
                }
            }
        }
        offset += length;
    }
    *out_adjustment = adjustment;
    return NX_OK;
}

static int nx_font_valid_cmap4(const uint8_t *data, size_t available,
                               uint32_t *out_segments) {
    if (available < 24u || nx_font_u16(data) != 4u) return 0;
    size_t length = nx_font_u16(data + 2u);
    uint16_t seg_x2 = nx_font_u16(data + 6u);
    if (!seg_x2 || (seg_x2 & 1u) || length > available) return 0;
    size_t segments = seg_x2 / 2u;
    size_t arrays_end = 16u + segments * 8u;
    if (length < arrays_end || nx_font_u16(data + 14u + segments * 2u) != 0u)
        return 0;
    uint16_t previous_end = 0;
    for (size_t i = 0; i < segments; ++i) {
        uint16_t end = nx_font_u16(data + 14u + i * 2u);
        uint16_t start = nx_font_u16(data + 16u + segments * 2u + i * 2u);
        if (start > end || (i && start <= previous_end)) return 0;
        previous_end = end;
        size_t range_word = 16u + segments * 6u + i * 2u;
        uint16_t range_offset = nx_font_u16(data + range_word);
        if (range_offset) {
            size_t first = range_word + range_offset;
            size_t last = first + (size_t)(end - start) * 2u;
            if ((range_offset & 1u) || first < arrays_end ||
                last > length - 2u) return 0;
        }
    }
    if (nx_font_u16(data + 14u + (segments - 1u) * 2u) != UINT16_MAX ||
        nx_font_u16(data + 16u + segments * 2u + (segments - 1u) * 2u)
            != UINT16_MAX) return 0;
    *out_segments = (uint32_t)segments;
    return 1;
}

static int nx_font_valid_cmap12(const uint8_t *data, size_t available,
                                uint16_t glyph_limit, uint32_t *out_groups) {
    if (available < 16u || nx_font_u16(data) != 12u ||
        nx_font_u16(data + 2u) != 0u) return 0;
    size_t length = nx_font_u32(data + 4u);
    uint32_t groups = nx_font_u32(data + 12u);
    if (length < 16u || length > available ||
        groups > (length - 16u) / 12u) return 0;
    uint32_t previous_end = 0;
    for (uint32_t i = 0; i < groups; ++i) {
        const uint8_t *group = data + 16u + (size_t)i * 12u;
        uint32_t start = nx_font_u32(group);
        uint32_t end = nx_font_u32(group + 4u);
        uint32_t first_glyph = nx_font_u32(group + 8u);
        uint64_t last_glyph = (uint64_t)first_glyph + (end - start);
        if (start > end || end > UINT32_C(0x10ffff) ||
            (i && start <= previous_end) || last_glyph > UINT16_MAX ||
            (glyph_limit && last_glyph >= glyph_limit))
            return 0;
        previous_end = end;
    }
    *out_groups = groups;
    return 1;
}

static unsigned nx_font_cmap_rank(uint16_t platform, uint16_t encoding,
                                  uint16_t format) {
    if (format == 12u) {
        if (platform == 3u && encoding == 10u) return 4u;
        if (platform == 0u && encoding <= 4u) return 3u;
    } else if (format == 4u) {
        if (platform == 3u && encoding == 1u) return 2u;
        if (platform == 0u && encoding <= 4u) return 1u;
    }
    return 0u;
}

static nx_result nx_font_select_cmap(nx_font *font) {
    nx_bytes_view cmap;
    if (nx_font_find_table(font, NX_SFNT_TAG('c', 'm', 'a', 'p'), &cmap) != NX_OK)
        return NX_ERROR_UNSUPPORTED;
    if (cmap.length < 4u || nx_font_u16(cmap.data) != 0u) return NX_ERROR_ARGUMENT;
    uint16_t records = nx_font_u16(cmap.data + 2u);
    if (!records || (size_t)records > (cmap.length - 4u) / 8u)
        return NX_ERROR_ARGUMENT;
    size_t records_end = 4u + (size_t)records * 8u;
    unsigned best_rank = 0u;
    int invalid_candidate = 0;
    for (uint32_t i = 0; i < records; ++i) {
        const uint8_t *record = cmap.data + 4u + (size_t)i * 8u;
        uint16_t platform = nx_font_u16(record);
        uint16_t encoding = nx_font_u16(record + 2u);
        if (!(platform == 0u && encoding <= 4u) &&
            !(platform == 3u && (encoding == 1u || encoding == 10u)))
            continue;
        uint32_t offset = nx_font_u32(record + 4u);
        if (offset < records_end || offset > cmap.length ||
            cmap.length - offset < 2u) {
            invalid_candidate = 1;
            continue;
        }
        const uint8_t *subtable = cmap.data + offset;
        uint16_t format = nx_font_u16(subtable);
        unsigned rank = nx_font_cmap_rank(platform, encoding, format);
        if (!rank || rank <= best_rank) continue;
        uint32_t entries = 0;
        int valid = format == 4u
                    ? nx_font_valid_cmap4(subtable, cmap.length - offset,
                                          &entries)
                    : nx_font_valid_cmap12(subtable, cmap.length - offset,
                                           font->glyph_limit, &entries);
        if (!valid) {
            invalid_candidate = 1;
            continue;
        }
        font->cmap_subtable = subtable;
        font->cmap_entries = entries;
        font->cmap_format = format;
        best_rank = rank;
    }
    if (best_rank) return NX_OK;
    return invalid_candidate ? NX_ERROR_ARGUMENT : NX_ERROR_UNSUPPORTED;
}

nx_result nx_font_glyph_index(const nx_font *font, uint32_t scalar,
                              uint16_t *out_glyph) {
    if (!out_glyph) return NX_ERROR_ARGUMENT;
    *out_glyph = 0;
    if (!font || scalar > UINT32_C(0x10ffff) ||
        (scalar >= UINT32_C(0xd800) && scalar <= UINT32_C(0xdfff)))
        return NX_ERROR_ARGUMENT;
    if (font->cmap_result != NX_OK) return font->cmap_result;
    const uint8_t *data = font->cmap_subtable;
    if (font->cmap_format == 4u) {
        if (scalar > UINT16_MAX) return NX_OK;
        size_t segments = font->cmap_entries;
        size_t low = 0, high = segments;
        while (low < high) {
            size_t mid = low + (high - low) / 2u;
            if (nx_font_u16(data + 14u + mid * 2u) < scalar) low = mid + 1u;
            else high = mid;
        }
        if (low == segments) return NX_OK;
        uint16_t start = nx_font_u16(data + 16u + segments * 2u + low * 2u);
        if (scalar < start) return NX_OK;
        uint16_t delta = nx_font_u16(data + 16u + segments * 4u + low * 2u);
        size_t range_word = 16u + segments * 6u + low * 2u;
        uint16_t range_offset = nx_font_u16(data + range_word);
        if (!range_offset) {
            *out_glyph = (uint16_t)(scalar + delta);
        } else {
            size_t position = range_word + range_offset +
                              (size_t)(scalar - start) * 2u;
            uint16_t glyph = nx_font_u16(data + position);
            if (glyph) *out_glyph = (uint16_t)(glyph + delta);
        }
        if (*out_glyph && font->glyph_limit && *out_glyph >= font->glyph_limit) {
            *out_glyph = 0;
            return NX_ERROR_ARGUMENT;
        }
        return NX_OK;
    }
    uint32_t low = 0, high = font->cmap_entries;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2u;
        const uint8_t *group = data + 16u + (size_t)mid * 12u;
        uint32_t start = nx_font_u32(group), end = nx_font_u32(group + 4u);
        if (scalar < start) high = mid;
        else if (scalar > end) low = mid + 1u;
        else {
            *out_glyph = (uint16_t)(nx_font_u32(group + 8u) + scalar - start);
            return NX_OK;
        }
    }
    return NX_OK;
}
