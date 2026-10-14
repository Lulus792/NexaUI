#include "font_internal.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct nx_gpos_table {
    const uint8_t *data;
    size_t length;
} nx_gpos_table;

#define NX_GPOS_WORK_LIMIT 4000000u

static uint16_t nx_gpos_u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t nx_gpos_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) | p[3];
}

static int nx_gpos_has(nx_gpos_table table, size_t offset, size_t length) {
    return offset <= table.length && length <= table.length - offset;
}

static int nx_gpos_child(nx_gpos_table table, size_t parent,
                         size_t relative, size_t minimum, size_t *child) {
    if (!relative || parent > table.length ||
        relative > table.length - parent ||
        !nx_gpos_has(table, parent + relative, minimum)) return 0;
    *child = parent + relative;
    return 1;
}

static int nx_gpos_array(nx_gpos_table table, size_t offset,
                         size_t header, size_t count, size_t stride) {
    return nx_gpos_has(table, offset, header) &&
           (!stride || count <= (table.length - offset - header) / stride);
}

static nx_result nx_gpos_charge(size_t *remaining, size_t cost) {
    if (cost > *remaining) return NX_ERROR_LIMIT;
    *remaining -= cost;
    return NX_OK;
}

static uint32_t nx_gpos_script_tag(uint32_t iso) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 32u; shift += 8u) {
        unsigned c = (iso >> shift) & 0xffu;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        result |= (uint32_t)c << shift;
    }
    return result;
}

static int nx_gpos_feature_enabled(uint32_t tag) {
    return tag == NX_SFNT_TAG('k','e','r','n') ||
           tag == NX_SFNT_TAG('d','i','s','t') ||
           tag == NX_SFNT_TAG('p','a','l','t') ||
           tag == NX_SFNT_TAG('h','a','l','t');
}

static nx_result nx_gpos_coverage(nx_gpos_table table, size_t offset,
                                  size_t *out_count, size_t *remaining) {
    if (!nx_gpos_has(table, offset, 4u)) return NX_ERROR_ARGUMENT;
    uint16_t format = nx_gpos_u16(table.data + offset);
    size_t count = nx_gpos_u16(table.data + offset + 2u);
    if (format == 1u) {
        if (!nx_gpos_array(table, offset, 4u, count, 2u))
            return NX_ERROR_ARGUMENT;
        if (nx_gpos_charge(remaining, count + 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        uint16_t previous = 0;
        for (size_t i = 0; i < count; ++i) {
            uint16_t glyph = nx_gpos_u16(table.data + offset + 4u + 2u * i);
            if (i && glyph <= previous) return NX_ERROR_ARGUMENT;
            previous = glyph;
        }
        *out_count = count;
        return NX_OK;
    }
    if (format == 2u) {
        if (!nx_gpos_array(table, offset, 4u, count, 6u))
            return NX_ERROR_ARGUMENT;
        if (nx_gpos_charge(remaining, count + 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        uint16_t previous_end = 0;
        size_t covered = 0;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t *record = table.data + offset + 4u + 6u * i;
            uint16_t first = nx_gpos_u16(record);
            uint16_t last = nx_gpos_u16(record + 2u);
            uint16_t index = nx_gpos_u16(record + 4u);
            if (first > last || (i && first <= previous_end) ||
                index != covered) return NX_ERROR_ARGUMENT;
            covered += (size_t)last - first + 1u;
            if (covered > 65536u) return NX_ERROR_ARGUMENT;
            previous_end = last;
        }
        *out_count = covered;
        return NX_OK;
    }
    return NX_ERROR_UNSUPPORTED;
}

static size_t nx_gpos_coverage_index(nx_gpos_table table, size_t offset,
                                     uint16_t glyph) {
    uint16_t format = nx_gpos_u16(table.data + offset);
    size_t count = nx_gpos_u16(table.data + offset + 2u);
    size_t low = 0, high = count;
    if (format == 1u) {
        while (low < high) {
            size_t mid = low + (high - low) / 2u;
            uint16_t entry = nx_gpos_u16(table.data + offset + 4u + 2u * mid);
            if (entry < glyph) low = mid + 1u;
            else high = mid;
        }
        return low < count &&
               nx_gpos_u16(table.data + offset + 4u + 2u * low) == glyph ?
               low : SIZE_MAX;
    }
    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        const uint8_t *record = table.data + offset + 4u + 6u * mid;
        uint16_t first = nx_gpos_u16(record), last = nx_gpos_u16(record + 2u);
        if (glyph < first) high = mid;
        else if (glyph > last) low = mid + 1u;
        else return (size_t)nx_gpos_u16(record + 4u) + glyph - first;
    }
    return SIZE_MAX;
}

static size_t nx_gpos_value_size(uint16_t format) {
    size_t words = 0;
    for (unsigned bit = 0; bit < 4u; ++bit)
        words += (format >> bit) & 1u;
    return words * 2u;
}

static void nx_gpos_apply_value(const uint8_t *record, uint16_t format,
                                double scale, nx_shaped_glyph *glyph) {
    size_t word = 0;
    if (format & 1u) {
        glyph->offset_x += (int16_t)nx_gpos_u16(record + 2u * word) * scale;
        ++word;
    }
    if (format & 2u) {
        glyph->offset_y += (int16_t)nx_gpos_u16(record + 2u * word) * scale;
        ++word;
    }
    if (format & 4u) {
        glyph->advance_x += (int16_t)nx_gpos_u16(record + 2u * word) * scale;
    }
}

static nx_result nx_gpos_class_validate(nx_gpos_table table, size_t offset,
                                        uint16_t class_count,
                                        uint16_t glyph_limit,
                                        size_t *remaining) {
    if (!nx_gpos_has(table, offset, 4u)) return NX_ERROR_ARGUMENT;
    uint16_t format = nx_gpos_u16(table.data + offset);
    if (format == 1u) {
        if (!nx_gpos_has(table, offset, 6u)) return NX_ERROR_ARGUMENT;
        uint16_t first = nx_gpos_u16(table.data + offset + 2u);
        size_t count = nx_gpos_u16(table.data + offset + 4u);
        if (!nx_gpos_array(table, offset, 6u, count, 2u) ||
            (size_t)first + count > glyph_limit)
            return NX_ERROR_ARGUMENT;
        if (nx_gpos_charge(remaining, count + 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        for (size_t i = 0; i < count; ++i)
            if (nx_gpos_u16(table.data + offset + 6u + 2u * i) >= class_count)
                return NX_ERROR_ARGUMENT;
        return NX_OK;
    }
    if (format == 2u) {
        size_t count = nx_gpos_u16(table.data + offset + 2u);
        if (!nx_gpos_array(table, offset, 4u, count, 6u))
            return NX_ERROR_ARGUMENT;
        if (nx_gpos_charge(remaining, count + 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        uint16_t previous_end = 0;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t *record = table.data + offset + 4u + 6u * i;
            uint16_t first = nx_gpos_u16(record);
            uint16_t last = nx_gpos_u16(record + 2u);
            if (first > last || last >= glyph_limit ||
                (i && first <= previous_end) ||
                nx_gpos_u16(record + 4u) >= class_count)
                return NX_ERROR_ARGUMENT;
            previous_end = last;
        }
        return NX_OK;
    }
    return NX_ERROR_UNSUPPORTED;
}

static uint16_t nx_gpos_class(nx_gpos_table table, size_t offset,
                              uint16_t glyph) {
    uint16_t format = nx_gpos_u16(table.data + offset);
    if (format == 1u) {
        uint16_t first = nx_gpos_u16(table.data + offset + 2u);
        size_t count = nx_gpos_u16(table.data + offset + 4u);
        return glyph >= first && (size_t)glyph - first < count ?
            nx_gpos_u16(table.data + offset + 6u +
                         2u * ((size_t)glyph - first)) : 0u;
    }
    size_t low = 0, high = nx_gpos_u16(table.data + offset + 2u);
    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        const uint8_t *record = table.data + offset + 4u + 6u * mid;
        uint16_t first = nx_gpos_u16(record), last = nx_gpos_u16(record + 2u);
        if (glyph < first) high = mid;
        else if (glyph > last) low = mid + 1u;
        else return nx_gpos_u16(record + 4u);
    }
    return 0u;
}

static nx_result nx_gpos_validate_subtable(nx_gpos_table table, size_t offset,
                                           uint16_t type, uint16_t glyph_limit,
                                           size_t *remaining) {
    if (!nx_gpos_has(table, offset, 6u)) return NX_ERROR_ARGUMENT;
    uint16_t format = nx_gpos_u16(table.data + offset);
    size_t coverage = 0, covered = 0;
    if (!nx_gpos_child(table, offset,
                       nx_gpos_u16(table.data + offset + 2u),
                       4u, &coverage)) return NX_ERROR_ARGUMENT;
    nx_result result = nx_gpos_coverage(table, coverage, &covered, remaining);
    if (result != NX_OK) return result;
    if (type == 1u && (format == 1u || format == 2u)) {
        uint16_t value_format = nx_gpos_u16(table.data + offset + 4u);
        if (value_format & ~15u) return NX_ERROR_UNSUPPORTED;
        size_t stride = nx_gpos_value_size(value_format);
        if (format == 1u)
            return nx_gpos_has(table, offset, 6u + stride) ?
                   NX_OK : NX_ERROR_ARGUMENT;
        if (!nx_gpos_has(table, offset, 8u)) return NX_ERROR_ARGUMENT;
        size_t count = nx_gpos_u16(table.data + offset + 6u);
        if (count != covered ||
            !nx_gpos_array(table, offset, 8u, count, stride))
            return NX_ERROR_ARGUMENT;
        return nx_gpos_charge(remaining, count);
    }
    if (type == 2u && (format == 1u || format == 2u)) {
        if (!nx_gpos_has(table, offset, format == 1u ? 10u : 16u))
            return NX_ERROR_ARGUMENT;
        uint16_t format1 = nx_gpos_u16(table.data + offset + 4u);
        uint16_t format2 = nx_gpos_u16(table.data + offset + 6u);
        if ((format1 | format2) & ~15u) return NX_ERROR_UNSUPPORTED;
        size_t stride = nx_gpos_value_size(format1) +
                        nx_gpos_value_size(format2);
        if (format == 1u) {
            size_t sets = nx_gpos_u16(table.data + offset + 8u);
            if (sets != covered || sets > 4096u ||
                !nx_gpos_array(table, offset, 10u, sets, 2u))
                return NX_ERROR_ARGUMENT;
            if (nx_gpos_charge(remaining, sets) != NX_OK)
                return NX_ERROR_LIMIT;
            for (size_t s = 0; s < sets; ++s) {
                size_t set = 0;
                if (!nx_gpos_child(table, offset,
                                   nx_gpos_u16(table.data + offset +
                                                10u + 2u * s),
                                   2u, &set)) return NX_ERROR_ARGUMENT;
                size_t pairs = nx_gpos_u16(table.data + set);
                if (pairs > 4096u ||
                    !nx_gpos_array(table, set, 2u, pairs, 2u + stride))
                    return NX_ERROR_ARGUMENT;
                if (nx_gpos_charge(remaining, pairs) != NX_OK)
                    return NX_ERROR_LIMIT;
                uint16_t previous = 0;
                for (size_t p = 0; p < pairs; ++p) {
                    uint16_t second = nx_gpos_u16(table.data + set + 2u +
                                                  p * (2u + stride));
                    if (second >= glyph_limit || (p && second <= previous))
                        return NX_ERROR_ARGUMENT;
                    previous = second;
                }
            }
            return NX_OK;
        }
        size_t class1_count = nx_gpos_u16(table.data + offset + 12u);
        size_t class2_count = nx_gpos_u16(table.data + offset + 14u);
        if (!class1_count || !class2_count ||
            class1_count > 1024u || class2_count > 1024u ||
            class1_count > 65536u / class2_count)
            return NX_ERROR_LIMIT;
        size_t records = class1_count * class2_count;
        if (!nx_gpos_array(table, offset, 16u, records, stride))
            return NX_ERROR_ARGUMENT;
        if (nx_gpos_charge(remaining, records) != NX_OK)
            return NX_ERROR_LIMIT;
        size_t class1 = 0, class2 = 0;
        if (!nx_gpos_child(table, offset,
                           nx_gpos_u16(table.data + offset + 8u),
                           4u, &class1) ||
            !nx_gpos_child(table, offset,
                           nx_gpos_u16(table.data + offset + 10u),
                           4u, &class2)) return NX_ERROR_ARGUMENT;
        result = nx_gpos_class_validate(table, class1,
                                        (uint16_t)class1_count,
                                        glyph_limit, remaining);
        if (result != NX_OK) return result;
        return nx_gpos_class_validate(table, class2,
                                      (uint16_t)class2_count,
                                      glyph_limit, remaining);
    }
    return NX_ERROR_UNSUPPORTED;
}

static nx_result nx_gpos_apply_lookup(nx_gpos_table table, size_t lookup,
                                      uint16_t glyph_limit, double scale,
                                      nx_shaped_glyph *glyphs, size_t count,
                                      uint8_t *pair_mask, size_t *remaining) {
    if (!nx_gpos_has(table, lookup, 6u)) return NX_ERROR_ARGUMENT;
    uint16_t type = nx_gpos_u16(table.data + lookup);
    uint16_t flags = nx_gpos_u16(table.data + lookup + 2u);
    size_t subtables = nx_gpos_u16(table.data + lookup + 4u);
    if (subtables > 64u ||
        !nx_gpos_array(table, lookup, 6u, subtables, 2u) ||
        !nx_gpos_has(table, lookup, 6u + 2u * subtables +
                     ((flags & 0x10u) ? 2u : 0u)))
        return NX_ERROR_ARGUMENT;
    /* GDEF filtering, mark attachment and contextual positioning are deferred. */
    if ((flags & ~1u) || (type != 1u && type != 2u)) return NX_OK;
    size_t offsets[64];
    for (size_t s = 0; s < subtables; ++s) {
        if (!nx_gpos_child(table, lookup,
                           nx_gpos_u16(table.data + lookup + 6u + 2u * s),
                           6u, &offsets[s])) return NX_ERROR_ARGUMENT;
        nx_result result = nx_gpos_validate_subtable(table, offsets[s],
                                                      type, glyph_limit,
                                                      remaining);
        if (result == NX_ERROR_UNSUPPORTED) return NX_OK;
        if (result != NX_OK) return result;
    }
    for (size_t i = 0; i < count;) {
        if (nx_gpos_charge(remaining, 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        size_t consumed = 1u;
        for (size_t s = 0; s < subtables; ++s) {
            if (nx_gpos_charge(remaining, 1u) != NX_OK)
                return NX_ERROR_LIMIT;
            size_t sub = offsets[s];
            size_t coverage = sub + nx_gpos_u16(table.data + sub + 2u);
            size_t index = nx_gpos_coverage_index(table, coverage,
                                                   glyphs[i].glyph_index);
            if (index == SIZE_MAX) continue;
            uint16_t format = nx_gpos_u16(table.data + sub);
            uint16_t value_format1 = nx_gpos_u16(table.data + sub + 4u);
            size_t value_size1 = nx_gpos_value_size(value_format1);
            if (type == 1u) {
                size_t record = sub + (format == 1u ? 6u :
                                       8u + index * value_size1);
                nx_gpos_apply_value(table.data + record, value_format1,
                                    scale, &glyphs[i]);
                break;
            }
            if (i + 1u >= count) continue;
            uint16_t value_format2 = nx_gpos_u16(table.data + sub + 6u);
            size_t value_size2 = nx_gpos_value_size(value_format2);
            size_t record = SIZE_MAX;
            if (format == 1u) {
                size_t set = sub + nx_gpos_u16(table.data + sub +
                                               10u + 2u * index);
                size_t pairs = nx_gpos_u16(table.data + set);
                size_t stride = 2u + value_size1 + value_size2;
                size_t low = 0, high = pairs;
                while (low < high) {
                    size_t mid = low + (high - low) / 2u;
                    uint16_t second = nx_gpos_u16(table.data + set +
                                                   2u + mid * stride);
                    if (second < glyphs[i + 1u].glyph_index) low = mid + 1u;
                    else high = mid;
                }
                if (low < pairs &&
                    nx_gpos_u16(table.data + set + 2u + low * stride) ==
                    glyphs[i + 1u].glyph_index)
                    record = set + 4u + low * stride;
            } else {
                size_t class1 = sub + nx_gpos_u16(table.data + sub + 8u);
                size_t class2 = sub + nx_gpos_u16(table.data + sub + 10u);
                size_t a = nx_gpos_class(table, class1, glyphs[i].glyph_index);
                size_t b = nx_gpos_class(table, class2,
                                         glyphs[i + 1u].glyph_index);
                size_t columns = nx_gpos_u16(table.data + sub + 14u);
                record = sub + 16u + (a * columns + b) *
                         (value_size1 + value_size2);
            }
            if (record == SIZE_MAX) continue;
            nx_gpos_apply_value(table.data + record, value_format1,
                                scale, &glyphs[i]);
            nx_gpos_apply_value(table.data + record + value_size1,
                                value_format2, scale, &glyphs[i + 1u]);
            pair_mask[i] = 1u;
            if (value_format2) consumed = 2u;
            break;
        }
        i += consumed;
    }
    return NX_OK;
}

nx_result nx_gpos_apply(const nx_font *font, uint32_t script_tag,
                        nx_shaped_glyph *glyphs, size_t count,
                        double pixels_per_em, uint8_t *pair_mask) {
    if (!font || (count && (!glyphs || !pair_mask)))
        return NX_ERROR_ARGUMENT;
    nx_bytes_view view;
    if (nx_font_find_table(font, NX_SFNT_TAG('G','P','O','S'), &view) != NX_OK)
        return NX_OK;
    nx_gpos_table table = {view.data, view.length};
    if (!nx_gpos_has(table, 0, 10u)) return NX_ERROR_ARGUMENT;
    uint32_t version = nx_gpos_u32(table.data);
    if (version != UINT32_C(0x00010000) &&
        version != UINT32_C(0x00010001)) return NX_ERROR_UNSUPPORTED;
    if (version == UINT32_C(0x00010001) &&
        !nx_gpos_has(table, 0, 14u)) return NX_ERROR_ARGUMENT;
    size_t header = version == UINT32_C(0x00010001) ? 14u : 10u;
    uint16_t script_offset = nx_gpos_u16(table.data + 4u);
    uint16_t feature_offset = nx_gpos_u16(table.data + 6u);
    uint16_t lookup_offset = nx_gpos_u16(table.data + 8u);
    size_t scripts = 0, features = 0, lookups = 0;
    if (script_offset < header || feature_offset < header ||
        lookup_offset < header ||
        !nx_gpos_child(table, 0, script_offset, 2u, &scripts) ||
        !nx_gpos_child(table, 0, feature_offset, 2u, &features) ||
        !nx_gpos_child(table, 0, lookup_offset, 2u, &lookups))
        return NX_ERROR_ARGUMENT;
    size_t script_count = nx_gpos_u16(table.data + scripts);
    size_t feature_count = nx_gpos_u16(table.data + features);
    size_t lookup_count = nx_gpos_u16(table.data + lookups);
    if (script_count > 4096u || feature_count > 4096u ||
        lookup_count > 4096u) return NX_ERROR_LIMIT;
    if (!nx_gpos_array(table, scripts, 2u, script_count, 6u) ||
        !nx_gpos_array(table, features, 2u, feature_count, 6u) ||
        !nx_gpos_array(table, lookups, 2u, lookup_count, 2u))
        return NX_ERROR_ARGUMENT;
    uint32_t wanted = nx_gpos_script_tag(script_tag);
    size_t selected = SIZE_MAX, fallback = SIZE_MAX;
    for (size_t i = 0; i < script_count; ++i) {
        const uint8_t *record = table.data + scripts + 2u + 6u * i;
        uint32_t tag = nx_gpos_u32(record);
        if (tag == wanted) selected = i;
        if (tag == NX_SFNT_TAG('D','F','L','T')) fallback = i;
    }
    if (selected == SIZE_MAX) selected = fallback;
    if (selected == SIZE_MAX) return NX_OK;
    size_t script = 0;
    if (!nx_gpos_child(table, scripts,
                       nx_gpos_u16(table.data + scripts + 6u * selected + 6u),
                       4u, &script)) return NX_ERROR_ARGUMENT;
    size_t lang_count = nx_gpos_u16(table.data + script + 2u);
    if (!nx_gpos_array(table, script, 4u, lang_count, 6u))
        return NX_ERROR_ARGUMENT;
    uint16_t lang_offset = nx_gpos_u16(table.data + script);
    if (!lang_offset) return NX_OK;
    size_t lang = 0;
    if (!nx_gpos_child(table, script, lang_offset, 6u, &lang))
        return NX_ERROR_ARGUMENT;
    if (nx_gpos_u16(table.data + lang) != 0u) return NX_ERROR_ARGUMENT;
    size_t lang_features = nx_gpos_u16(table.data + lang + 4u);
    if (!nx_gpos_array(table, lang, 6u, lang_features, 2u))
        return NX_ERROR_ARGUMENT;
    uint16_t required = nx_gpos_u16(table.data + lang + 2u);
    if (required != UINT16_MAX && required >= feature_count)
        return NX_ERROR_ARGUMENT;
    if (!lookup_count) return NX_OK;
    uint8_t *active = calloc(lookup_count, 1u);
    if (!active) return NX_ERROR_MEMORY;
    nx_result result = NX_OK;
    size_t remaining = NX_GPOS_WORK_LIMIT;
    for (size_t item = 0; item < lang_features + (required != UINT16_MAX); ++item) {
        if (nx_gpos_charge(&remaining, 1u) != NX_OK) {
            result = NX_ERROR_LIMIT; goto done;
        }
        size_t feature_index = item == 0 && required != UINT16_MAX ? required :
            nx_gpos_u16(table.data + lang + 6u +
                         2u * (item - (required != UINT16_MAX)));
        if (feature_index >= feature_count) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        size_t record = features + 2u + 6u * feature_index;
        uint32_t tag = nx_gpos_u32(table.data + record);
        if (feature_index != required && !nx_gpos_feature_enabled(tag))
            continue;
        size_t feature = 0;
        if (!nx_gpos_child(table, features,
                           nx_gpos_u16(table.data + record + 4u),
                           4u, &feature)) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        size_t feature_lookups = nx_gpos_u16(table.data + feature + 2u);
        if (!nx_gpos_array(table, feature, 4u, feature_lookups, 2u)) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        if (nx_gpos_charge(&remaining, feature_lookups) != NX_OK) {
            result = NX_ERROR_LIMIT; goto done;
        }
        for (size_t i = 0; i < feature_lookups; ++i) {
            size_t index = nx_gpos_u16(table.data + feature + 4u + 2u * i);
            if (index >= lookup_count) {
                result = NX_ERROR_ARGUMENT; goto done;
            }
            active[index] = 1u;
        }
    }
    nx_font_metrics metrics;
    result = nx_font_get_metrics(font, &metrics);
    if (result != NX_OK) goto done;
    double scale = pixels_per_em / metrics.units_per_em;
    for (size_t i = 0; i < lookup_count; ++i) {
        if (!active[i]) continue;
        size_t lookup = 0;
        if (!nx_gpos_child(table, lookups,
                           nx_gpos_u16(table.data + lookups + 2u + 2u * i),
                           6u, &lookup)) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        result = nx_gpos_apply_lookup(table, lookup, metrics.glyph_count,
                                      scale, glyphs, count, pair_mask,
                                      &remaining);
        if (result != NX_OK) goto done;
    }
done:
    free(active);
    return result;
}
