#include "font_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nx_gsub_table {
    const uint8_t *data;
    size_t length;
} nx_gsub_table;

/* Limit work even when many offsets alias the same valid records. */
#define NX_GSUB_WORK_LIMIT 4000000u

static nx_result nx_gsub_charge(size_t *remaining, size_t cost) {
    if (cost > *remaining) return NX_ERROR_LIMIT;
    *remaining -= cost;
    return NX_OK;
}

static uint16_t nx_gsub_u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t nx_gsub_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) | p[3];
}

static int nx_gsub_has(nx_gsub_table table, size_t offset, size_t length) {
    return offset <= table.length && length <= table.length - offset;
}

static int nx_gsub_child(nx_gsub_table table, size_t parent,
                         size_t relative, size_t minimum, size_t *child) {
    if (!relative || parent > table.length ||
        relative > table.length - parent ||
        !nx_gsub_has(table, parent + relative, minimum)) return 0;
    *child = parent + relative;
    return 1;
}

static int nx_gsub_array(nx_gsub_table table, size_t offset,
                         size_t header, size_t count, size_t stride) {
    return nx_gsub_has(table, offset, header) &&
           count <= (table.length - offset - header) / stride;
}

static uint32_t nx_gsub_script_tag(uint32_t iso) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 32u; shift += 8u) {
        unsigned c = (iso >> shift) & 0xffu;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        result |= (uint32_t)c << shift;
    }
    return result;
}

static int nx_gsub_feature_enabled(uint32_t tag) {
    return tag == NX_SFNT_TAG('c','c','m','p') ||
           tag == NX_SFNT_TAG('l','o','c','l') ||
           tag == NX_SFNT_TAG('r','l','i','g') ||
           tag == NX_SFNT_TAG('l','i','g','a') ||
           tag == NX_SFNT_TAG('c','l','i','g');
}

/* Validate a coverage table and return its number of covered glyphs. */
static nx_result nx_gsub_coverage(nx_gsub_table table, size_t offset,
                                  size_t *out_count, size_t *remaining) {
    if (!nx_gsub_has(table, offset, 4u)) return NX_ERROR_ARGUMENT;
    uint16_t format = nx_gsub_u16(table.data + offset);
    size_t count = nx_gsub_u16(table.data + offset + 2u);
    if (format == 1u) {
        if (!nx_gsub_array(table, offset, 4u, count, 2u))
            return NX_ERROR_ARGUMENT;
        if (nx_gsub_charge(remaining, count + 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        uint16_t prior = 0;
        for (size_t i = 0; i < count; ++i) {
            uint16_t glyph = nx_gsub_u16(table.data + offset + 4u + 2u * i);
            if (i && glyph <= prior) return NX_ERROR_ARGUMENT;
            prior = glyph;
        }
        *out_count = count;
        return NX_OK;
    }
    if (format == 2u) {
        if (!nx_gsub_array(table, offset, 4u, count, 6u))
            return NX_ERROR_ARGUMENT;
        if (nx_gsub_charge(remaining, count + 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        uint16_t previous_end = 0;
        size_t covered = 0;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t *entry = table.data + offset + 4u + 6u * i;
            uint16_t first = nx_gsub_u16(entry), last = nx_gsub_u16(entry + 2u);
            uint16_t index = nx_gsub_u16(entry + 4u);
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

static size_t nx_gsub_coverage_index(nx_gsub_table table, size_t offset,
                                     uint16_t glyph) {
    uint16_t format = nx_gsub_u16(table.data + offset);
    size_t count = nx_gsub_u16(table.data + offset + 2u);
    size_t low = 0, high = count;
    if (format == 1u) {
        while (low < high) {
            size_t mid = low + (high - low) / 2u;
            uint16_t entry = nx_gsub_u16(table.data + offset + 4u + 2u * mid);
            if (entry < glyph) low = mid + 1u;
            else high = mid;
        }
        return low < count &&
               nx_gsub_u16(table.data + offset + 4u + 2u * low) == glyph ?
               low : SIZE_MAX;
    }
    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        const uint8_t *entry = table.data + offset + 4u + 6u * mid;
        uint16_t first = nx_gsub_u16(entry), last = nx_gsub_u16(entry + 2u);
        if (glyph < first) high = mid;
        else if (glyph > last) low = mid + 1u;
        else return (size_t)nx_gsub_u16(entry + 4u) + glyph - first;
    }
    return SIZE_MAX;
}

static nx_result nx_gsub_validate_subtable(nx_gsub_table table, size_t offset,
                                           uint16_t type, uint16_t glyph_limit,
                                           size_t *remaining) {
    if (!nx_gsub_has(table, offset, 6u)) return NX_ERROR_ARGUMENT;
    uint16_t format = nx_gsub_u16(table.data + offset);
    size_t coverage = 0, coverage_count = 0;
    if (!nx_gsub_child(table, offset,
                       nx_gsub_u16(table.data + offset + 2u),
                       4u, &coverage)) return NX_ERROR_ARGUMENT;
    nx_result result = nx_gsub_coverage(table, coverage, &coverage_count,
                                        remaining);
    if (result != NX_OK) return result;
    if (type == 1u && format == 1u) return NX_OK;
    if (type == 1u && format == 2u) {
        size_t count = nx_gsub_u16(table.data + offset + 4u);
        if (count != coverage_count ||
            !nx_gsub_array(table, offset, 6u, count, 2u))
            return NX_ERROR_ARGUMENT;
        if (nx_gsub_charge(remaining, count) != NX_OK)
            return NX_ERROR_LIMIT;
        for (size_t i = 0; i < count; ++i)
            if (nx_gsub_u16(table.data + offset + 6u + 2u * i) >= glyph_limit)
                return NX_ERROR_ARGUMENT;
        return NX_OK;
    }
    if (type == 4u && format == 1u) {
        size_t sets = nx_gsub_u16(table.data + offset + 4u);
        if (sets != coverage_count || sets > 4096u ||
            !nx_gsub_array(table, offset, 6u, sets, 2u))
            return NX_ERROR_ARGUMENT;
        if (nx_gsub_charge(remaining, sets) != NX_OK)
            return NX_ERROR_LIMIT;
        for (size_t s = 0; s < sets; ++s) {
            size_t set = 0;
            if (!nx_gsub_child(table, offset,
                               nx_gsub_u16(table.data + offset + 6u + 2u * s),
                               2u, &set)) return NX_ERROR_ARGUMENT;
            size_t ligatures = nx_gsub_u16(table.data + set);
            if (ligatures > 4096u ||
                !nx_gsub_array(table, set, 2u, ligatures, 2u))
                return NX_ERROR_ARGUMENT;
            if (nx_gsub_charge(remaining, ligatures) != NX_OK)
                return NX_ERROR_LIMIT;
            for (size_t l = 0; l < ligatures; ++l) {
                size_t lig = 0;
                if (!nx_gsub_child(table, set,
                                   nx_gsub_u16(table.data + set + 2u + 2u * l),
                                   4u, &lig)) return NX_ERROR_ARGUMENT;
                uint16_t components = nx_gsub_u16(table.data + lig + 2u);
                if (components < 2u || components > 32u ||
                    nx_gsub_u16(table.data + lig) >= glyph_limit ||
                    !nx_gsub_array(table, lig, 4u, components - 1u, 2u))
                    return NX_ERROR_ARGUMENT;
                if (nx_gsub_charge(remaining, components) != NX_OK)
                    return NX_ERROR_LIMIT;
                for (size_t c = 1; c < components; ++c)
                    if (nx_gsub_u16(table.data + lig + 4u + 2u * (c - 1u)) >=
                        glyph_limit) return NX_ERROR_ARGUMENT;
            }
        }
        return NX_OK;
    }
    return NX_ERROR_UNSUPPORTED;
}

static nx_result nx_gsub_apply_lookup(nx_gsub_table table, size_t lookup,
                                      uint16_t glyph_limit,
                                      nx_shaped_glyph *glyphs, size_t *count,
                                      size_t *remaining) {
    if (!nx_gsub_has(table, lookup, 6u)) return NX_ERROR_ARGUMENT;
    uint16_t type = nx_gsub_u16(table.data + lookup);
    uint16_t flags = nx_gsub_u16(table.data + lookup + 2u);
    size_t subtables = nx_gsub_u16(table.data + lookup + 4u);
    if (subtables > 64u || !nx_gsub_array(table, lookup, 6u,
                                           subtables, 2u) ||
        !nx_gsub_has(table, lookup, 6u + 2u * subtables +
                    ((flags & 0x10u) ? 2u : 0u)))
        return NX_ERROR_ARGUMENT;
    /* GDEF-based glyph filtering and unsupported lookup types are deferred. */
    if (flags & ~1u || (type != 1u && type != 4u)) return NX_OK;
    size_t offsets[64];
    for (size_t s = 0; s < subtables; ++s) {
        if (!nx_gsub_child(table, lookup,
                           nx_gsub_u16(table.data + lookup + 6u + 2u * s),
                           6u, &offsets[s])) return NX_ERROR_ARGUMENT;
        nx_result result = nx_gsub_validate_subtable(table, offsets[s],
                                                      type, glyph_limit,
                                                      remaining);
        if (result == NX_ERROR_UNSUPPORTED) return NX_OK;
        if (result != NX_OK) return result;
    }
    for (size_t i = 0; i < *count;) {
        if (nx_gsub_charge(remaining, 1u) != NX_OK)
            return NX_ERROR_LIMIT;
        int applied = 0;
        size_t consumed = 1;
        for (size_t s = 0; s < subtables && !applied; ++s) {
            if (nx_gsub_charge(remaining, 1u) != NX_OK)
                return NX_ERROR_LIMIT;
            size_t sub = offsets[s];
            size_t coverage = sub + nx_gsub_u16(table.data + sub + 2u);
            size_t index = nx_gsub_coverage_index(table, coverage,
                                                   glyphs[i].glyph_index);
            if (index == SIZE_MAX) continue;
            if (type == 1u) {
                uint16_t replacement;
                if (nx_gsub_u16(table.data + sub) == 1u) {
                    replacement = (uint16_t)(glyphs[i].glyph_index +
                                  (int16_t)nx_gsub_u16(table.data + sub + 4u));
                } else replacement = nx_gsub_u16(table.data + sub + 6u +
                                                  2u * index);
                if (replacement >= glyph_limit) return NX_ERROR_ARGUMENT;
                glyphs[i].glyph_index = replacement;
                applied = 1;
            } else {
                size_t set = sub + nx_gsub_u16(table.data + sub + 6u + 2u * index);
                size_t ligatures = nx_gsub_u16(table.data + set);
                for (size_t l = 0; l < ligatures; ++l) {
                    if (nx_gsub_charge(remaining, 1u) != NX_OK)
                        return NX_ERROR_LIMIT;
                    size_t lig = set + nx_gsub_u16(table.data + set + 2u + 2u * l);
                    size_t components = nx_gsub_u16(table.data + lig + 2u);
                    if (components > *count - i) continue;
                    size_t c = 1;
                    for (; c < components; ++c) {
                        if (nx_gsub_charge(remaining, 1u) != NX_OK)
                            return NX_ERROR_LIMIT;
                        if (glyphs[i + c].glyph_index !=
                            nx_gsub_u16(table.data + lig + 4u +
                                        2u * (c - 1u)))
                            break;
                    }
                    if (c != components) continue;
                    glyphs[i].glyph_index = nx_gsub_u16(table.data + lig);
                    glyphs[i].byte_end = glyphs[i + components - 1u].byte_end;
                    memmove(glyphs + i + 1u, glyphs + i + components,
                            (*count - i - components) * sizeof(*glyphs));
                    *count -= components - 1u;
                    consumed = 1;
                    applied = 1;
                    break;
                }
            }
        }
        i += consumed;
    }
    return NX_OK;
}

nx_result nx_gsub_apply(const nx_font *font, uint32_t script_tag,
                        nx_shaped_glyph *glyphs, size_t *inout_count) {
    if (!font || !inout_count || (*inout_count && !glyphs))
        return NX_ERROR_ARGUMENT;
    nx_bytes_view view;
    if (nx_font_find_table(font, NX_SFNT_TAG('G','S','U','B'), &view) != NX_OK)
        return NX_OK;
    nx_gsub_table table = {view.data, view.length};
    if (!nx_gsub_has(table, 0, 10u)) return NX_ERROR_ARGUMENT;
    uint32_t version = nx_gsub_u32(table.data);
    if (version != UINT32_C(0x00010000) &&
        version != UINT32_C(0x00010001)) return NX_ERROR_UNSUPPORTED;
    if (version == UINT32_C(0x00010001) &&
        !nx_gsub_has(table, 0, 14u)) return NX_ERROR_ARGUMENT;
    size_t scripts = 0, features = 0, lookups = 0;
    size_t header = version == UINT32_C(0x00010001) ? 14u : 10u;
    uint16_t script_offset = nx_gsub_u16(table.data + 4u);
    uint16_t feature_offset = nx_gsub_u16(table.data + 6u);
    uint16_t lookup_offset = nx_gsub_u16(table.data + 8u);
    if (script_offset < header || feature_offset < header ||
        lookup_offset < header ||
        !nx_gsub_child(table, 0, script_offset, 2u, &scripts) ||
        !nx_gsub_child(table, 0, feature_offset, 2u, &features) ||
        !nx_gsub_child(table, 0, lookup_offset, 2u, &lookups))
        return NX_ERROR_ARGUMENT;
    size_t script_count = nx_gsub_u16(table.data + scripts);
    size_t feature_count = nx_gsub_u16(table.data + features);
    size_t lookup_count = nx_gsub_u16(table.data + lookups);
    if (script_count > 4096u || feature_count > 4096u ||
        lookup_count > 4096u) return NX_ERROR_LIMIT;
    if (!nx_gsub_array(table, scripts, 2u, script_count, 6u) ||
        !nx_gsub_array(table, features, 2u, feature_count, 6u) ||
        !nx_gsub_array(table, lookups, 2u, lookup_count, 2u))
        return NX_ERROR_ARGUMENT;
    uint32_t wanted = nx_gsub_script_tag(script_tag);
    size_t selected = SIZE_MAX, fallback = SIZE_MAX;
    for (size_t i = 0; i < script_count; ++i) {
        const uint8_t *entry = table.data + scripts + 2u + 6u * i;
        uint32_t tag = nx_gsub_u32(entry);
        if (tag == wanted) selected = i;
        if (tag == NX_SFNT_TAG('D','F','L','T')) fallback = i;
    }
    if (selected == SIZE_MAX) selected = fallback;
    if (selected == SIZE_MAX) return NX_OK;
    size_t script = 0;
    if (!nx_gsub_child(table, scripts,
                       nx_gsub_u16(table.data + scripts + 6u * selected + 6u),
                       4u, &script)) return NX_ERROR_ARGUMENT;
    size_t lang_count = nx_gsub_u16(table.data + script + 2u);
    if (!nx_gsub_array(table, script, 4u, lang_count, 6u))
        return NX_ERROR_ARGUMENT;
    uint16_t lang_offset = nx_gsub_u16(table.data + script);
    if (!lang_offset) return NX_OK;
    size_t lang = 0;
    if (!nx_gsub_child(table, script, lang_offset, 6u, &lang))
        return NX_ERROR_ARGUMENT;
    if (nx_gsub_u16(table.data + lang) != 0u) return NX_ERROR_ARGUMENT;
    size_t lang_features = nx_gsub_u16(table.data + lang + 4u);
    if (!nx_gsub_array(table, lang, 6u, lang_features, 2u))
        return NX_ERROR_ARGUMENT;
    uint16_t required = nx_gsub_u16(table.data + lang + 2u);
    if (required != UINT16_MAX && required >= feature_count)
        return NX_ERROR_ARGUMENT;
    if (!lookup_count) return NX_OK;
    uint8_t *active = calloc(lookup_count, 1u);
    if (!active) return NX_ERROR_MEMORY;
    nx_result result = NX_OK;
    size_t work_remaining = NX_GSUB_WORK_LIMIT;
    for (size_t item = 0; item < lang_features + (required != UINT16_MAX); ++item) {
        if (nx_gsub_charge(&work_remaining, 1u) != NX_OK) {
            result = NX_ERROR_LIMIT; goto done;
        }
        size_t feature_index = item == 0 && required != UINT16_MAX ? required :
            nx_gsub_u16(table.data + lang + 6u +
                        2u * (item - (required != UINT16_MAX)));
        if (feature_index >= feature_count) { result = NX_ERROR_ARGUMENT; goto done; }
        size_t record = features + 2u + 6u * feature_index;
        uint32_t tag = nx_gsub_u32(table.data + record);
        if (feature_index != required && !nx_gsub_feature_enabled(tag)) continue;
        size_t feature = 0;
        if (!nx_gsub_child(table, features,
                           nx_gsub_u16(table.data + record + 4u),
                           4u, &feature)) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        size_t count = nx_gsub_u16(table.data + feature + 2u);
        if (!nx_gsub_array(table, feature, 4u, count, 2u)) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        if (nx_gsub_charge(&work_remaining, count) != NX_OK) {
            result = NX_ERROR_LIMIT; goto done;
        }
        for (size_t i = 0; i < count; ++i) {
            size_t index = nx_gsub_u16(table.data + feature + 4u + 2u * i);
            if (index >= lookup_count) { result = NX_ERROR_ARGUMENT; goto done; }
            active[index] = 1;
        }
    }
    nx_font_metrics metrics;
    result = nx_font_get_metrics(font, &metrics);
    if (result != NX_OK) goto done;
    for (size_t i = 0; i < lookup_count; ++i) {
        if (!active[i]) continue;
        size_t lookup = 0;
        if (!nx_gsub_child(table, lookups,
                           nx_gsub_u16(table.data + lookups + 2u + 2u * i),
                           6u, &lookup)) {
            result = NX_ERROR_ARGUMENT; goto done;
        }
        result = nx_gsub_apply_lookup(table, lookup, metrics.glyph_count,
                                      glyphs, inout_count, &work_remaining);
        if (result != NX_OK) goto done;
    }
done:
    free(active);
    return result;
}
