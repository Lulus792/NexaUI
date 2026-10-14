#include "nexaui/nexaui.h"
#include "font_internal.h"

#include <limits.h>
#include <string.h>

#define NX_GLYF_MAX_DEPTH 32u
#define NX_GLYF_MAX_VISITS 65536u
#define NX_GLYF_MAX_POINTS 65536u

static uint16_t nx_glyf_u16(const uint8_t *data) {
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static uint32_t nx_glyf_u32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | data[3];
}

static int16_t nx_glyf_i16(const uint8_t *data) {
    uint16_t value = nx_glyf_u16(data);
    return (int16_t)(value < UINT16_C(0x8000) ? (int32_t)value :
                     (int32_t)value - 65536);
}

static nx_result nx_glyf_validate_flags(const uint8_t *data, size_t length,
                                         size_t flags_start, size_t point_count,
                                         size_t *out_x_start, size_t *out_y_start,
                                         size_t *out_end) {
    size_t position = flags_start, points = 0, x_bytes = 0, y_bytes = 0;
    while (points < point_count) {
        if (position >= length) return NX_ERROR_ARGUMENT;
        uint8_t flag = data[position++];
        if (flag & 0x80u) return NX_ERROR_ARGUMENT;
        size_t run = 1u;
        if (flag & 0x08u) {
            if (position >= length) return NX_ERROR_ARGUMENT;
            run += data[position++];
        }
        if (run > point_count - points) return NX_ERROR_ARGUMENT;
        x_bytes += run * ((flag & 0x02u) ? 1u : (flag & 0x10u) ? 0u : 2u);
        y_bytes += run * ((flag & 0x04u) ? 1u : (flag & 0x20u) ? 0u : 2u);
        points += run;
    }
    if (x_bytes > length - position ||
        y_bytes > length - position - x_bytes) return NX_ERROR_ARGUMENT;
    *out_x_start = position;
    *out_y_start = position + x_bytes;
    *out_end = position + x_bytes + y_bytes;
    return NX_OK;
}

static nx_result nx_glyf_decode_points(const uint8_t *data, size_t flags_start,
                                        size_t x_start, size_t y_start,
                                        size_t end, size_t point_count,
                                        nx_glyph_point *points) {
    size_t flag_position = flags_start, x_position = x_start;
    size_t y_position = y_start, remaining = 0;
    uint8_t flag = 0;
    int64_t x = 0, y = 0;
    for (size_t i = 0; i < point_count; ++i) {
        if (!remaining) {
            flag = data[flag_position++];
            remaining = 1u;
            if (flag & 0x08u) remaining += data[flag_position++];
        }
        --remaining;
        if (flag & 0x02u) {
            int32_t delta = data[x_position++];
            x += (flag & 0x10u) ? delta : -delta;
        } else if (!(flag & 0x10u)) {
            x += nx_glyf_i16(data + x_position);
            x_position += 2u;
        }
        if (flag & 0x04u) {
            int32_t delta = data[y_position++];
            y += (flag & 0x20u) ? delta : -delta;
        } else if (!(flag & 0x20u)) {
            y += nx_glyf_i16(data + y_position);
            y_position += 2u;
        }
        if (x < INT32_MIN || x > INT32_MAX ||
            y < INT32_MIN || y > INT32_MAX) return NX_ERROR_ARGUMENT;
        if (points) {
            points[i].x = (int32_t)x;
            points[i].y = (int32_t)y;
            points[i].on_curve = (uint8_t)(flag & 1u);
        }
    }
    return x_position == y_start && y_position == end
           ? NX_OK : NX_ERROR_ARGUMENT;
}

typedef struct nx_glyf_build {
    nx_bytes_view loca, glyf;
    uint16_t glyph_count;
    int16_t loca_format;
    nx_glyph_point *points;
    uint16_t *contour_ends;
    size_t point_count, contour_count, depth, visits;
    uint16_t active[NX_GLYF_MAX_DEPTH];
    nx_glyph_bounds bounds;
} nx_glyf_build;

static nx_result nx_glyf_view(const nx_glyf_build *build, uint16_t glyph_index,
                              const uint8_t **out_data, size_t *out_length) {
    if (glyph_index >= build->glyph_count) return NX_ERROR_ARGUMENT;
    size_t entry_size = build->loca_format ? 4u : 2u;
    size_t entry = (size_t)glyph_index * entry_size;
    size_t start = build->loca_format
                   ? nx_glyf_u32(build->loca.data + entry)
                   : (size_t)nx_glyf_u16(build->loca.data + entry) * 2u;
    size_t finish = build->loca_format
                    ? nx_glyf_u32(build->loca.data + entry + entry_size)
                    : (size_t)nx_glyf_u16(build->loca.data + entry + entry_size) * 2u;
    if (start > finish || finish > build->glyf.length) return NX_ERROR_ARGUMENT;
    *out_data = build->glyf.data + start;
    *out_length = finish - start;
    return NX_OK;
}

static int64_t nx_glyf_round_fixed(int64_t value) {
    return value >= 0 ? (value + 8192) / 16384 :
           -((-value + 8192) / 16384);
}

static nx_result nx_glyf_transform_component(nx_glyf_build *build,
                                               size_t child_start, size_t child_end,
                                               size_t parent_start, uint16_t flags,
                                               uint16_t arg1, uint16_t arg2,
                                               int32_t xx, int32_t xy,
                                               int32_t yx, int32_t yy) {
    if (!(flags & 0x0002u)) {
        if ((size_t)arg1 >= child_start - parent_start ||
            (size_t)arg2 >= child_end - child_start)
            return NX_ERROR_ARGUMENT;
    }
    if (!build->points) return NX_OK;
    int64_t dx, dy;
    if (flags & 0x0002u) {
        dx = (int16_t)arg1;
        dy = (int16_t)arg2;
        if (flags & 0x0800u) {
            int64_t scaled_x = nx_glyf_round_fixed(xx * dx + yx * dy);
            int64_t scaled_y = nx_glyf_round_fixed(xy * dx + yy * dy);
            dx = scaled_x;
            dy = scaled_y;
        }
    } else {
        dx = dy = 0;
    }
    for (size_t i = child_start; i < child_end; ++i) {
        int64_t x = build->points[i].x, y = build->points[i].y;
        int64_t transformed_x = nx_glyf_round_fixed(xx * x + yx * y);
        int64_t transformed_y = nx_glyf_round_fixed(xy * x + yy * y);
        if (transformed_x < INT32_MIN || transformed_x > INT32_MAX ||
            transformed_y < INT32_MIN || transformed_y > INT32_MAX)
            return NX_ERROR_ARGUMENT;
        build->points[i].x = (int32_t)transformed_x;
        build->points[i].y = (int32_t)transformed_y;
    }
    if (!(flags & 0x0002u)) {
        dx = (int64_t)build->points[parent_start + arg1].x -
             build->points[child_start + arg2].x;
        dy = (int64_t)build->points[parent_start + arg1].y -
             build->points[child_start + arg2].y;
    }
    for (size_t i = child_start; i < child_end; ++i) {
        int64_t x = (int64_t)build->points[i].x + dx;
        int64_t y = (int64_t)build->points[i].y + dy;
        if (x < INT32_MIN || x > INT32_MAX ||
            y < INT32_MIN || y > INT32_MAX) return NX_ERROR_ARGUMENT;
        build->points[i].x = (int32_t)x;
        build->points[i].y = (int32_t)y;
    }
    return NX_OK;
}

static nx_result nx_glyf_append(nx_glyf_build *build, uint16_t glyph_index);

static nx_result nx_glyf_append_composite(nx_glyf_build *build,
                                            const uint8_t *data, size_t length) {
    size_t position = 10u, parent_start = build->point_count;
    uint16_t flags;
    int instructions = 0;
    do {
        if (length - position < 4u) return NX_ERROR_ARGUMENT;
        flags = nx_glyf_u16(data + position);
        uint16_t child_glyph = nx_glyf_u16(data + position + 2u);
        position += 4u;
        if ((flags & 0xe010u) ||
            ((flags & 0x08u) && (flags & 0xc0u)) ||
            ((flags & 0x40u) && (flags & 0x80u)) ||
            ((flags & 0x1800u) == 0x1800u) ||
            (build->point_count == parent_start && !(flags & 0x0002u)))
            return NX_ERROR_ARGUMENT;
        size_t arg_bytes = (flags & 0x0001u) ? 4u : 2u;
        size_t transform_bytes = (flags & 0x0008u) ? 2u :
                                 (flags & 0x0040u) ? 4u :
                                 (flags & 0x0080u) ? 8u : 0u;
        if (arg_bytes + transform_bytes > length - position)
            return NX_ERROR_ARGUMENT;
        uint16_t arg1, arg2;
        if (flags & 0x0001u) {
            arg1 = nx_glyf_u16(data + position);
            arg2 = nx_glyf_u16(data + position + 2u);
        } else if (flags & 0x0002u) {
            arg1 = (uint16_t)(int16_t)(int8_t)data[position];
            arg2 = (uint16_t)(int16_t)(int8_t)data[position + 1u];
        } else {
            arg1 = data[position];
            arg2 = data[position + 1u];
        }
        position += arg_bytes;
        int32_t xx = 16384, xy = 0, yx = 0, yy = 16384;
        if (flags & 0x0008u) {
            xx = yy = nx_glyf_i16(data + position);
        } else if (flags & 0x0040u) {
            xx = nx_glyf_i16(data + position);
            yy = nx_glyf_i16(data + position + 2u);
        } else if (flags & 0x0080u) {
            xx = nx_glyf_i16(data + position);
            xy = nx_glyf_i16(data + position + 2u);
            yx = nx_glyf_i16(data + position + 4u);
            yy = nx_glyf_i16(data + position + 6u);
        }
        position += transform_bytes;
        instructions |= (flags & 0x0100u) != 0;
        size_t child_start = build->point_count;
        nx_result result = nx_glyf_append(build, child_glyph);
        if (result != NX_OK) return result;
        result = nx_glyf_transform_component(build, child_start,
                                               build->point_count, parent_start,
                                               flags, arg1, arg2, xx, xy, yx, yy);
        if (result != NX_OK) return result;
    } while (flags & 0x0020u);
    if (instructions) {
        if (length - position < 2u) return NX_ERROR_ARGUMENT;
        size_t instruction_length = nx_glyf_u16(data + position);
        position += 2u;
        if (instruction_length > length - position) return NX_ERROR_ARGUMENT;
    }
    return NX_OK;
}

static nx_result nx_glyf_append_simple(nx_glyf_build *build,
                                         const uint8_t *data, size_t length,
                                         uint16_t contours) {
    if (contours == 0) {
        if (length > 10u &&
            (length < 12u || nx_glyf_u16(data + 10u) > length - 12u))
            return NX_ERROR_ARGUMENT;
        return NX_OK;
    }
    size_t contour_count = contours;
    size_t end_points_end = 10u + contour_count * 2u;
    if (end_points_end > length || length - end_points_end < 2u)
        return NX_ERROR_ARGUMENT;
    int32_t previous_end = -1;
    for (size_t i = 0; i < contour_count; ++i) {
        int32_t end_point = nx_glyf_u16(data + 10u + i * 2u);
        if (end_point <= previous_end) return NX_ERROR_ARGUMENT;
        previous_end = end_point;
    }
    size_t point_count = (size_t)previous_end + 1u;
    size_t instruction_length = nx_glyf_u16(data + end_points_end);
    size_t flags_start = end_points_end + 2u;
    if (instruction_length > length - flags_start) return NX_ERROR_ARGUMENT;
    flags_start += instruction_length;
    size_t x_start, y_start, end;
    nx_result result = nx_glyf_validate_flags(data, length, flags_start,
                                                 point_count, &x_start, &y_start,
                                                 &end);
    if (result != NX_OK) return result;
    if (point_count > NX_GLYF_MAX_POINTS - build->point_count ||
        contour_count > NX_GLYF_MAX_POINTS - build->contour_count)
        return NX_ERROR_LIMIT;
    size_t point_start = build->point_count;
    result = nx_glyf_decode_points(data, flags_start, x_start, y_start,
                                    end, point_count,
                                    build->points ? build->points + point_start : NULL);
    if (result != NX_OK) return result;
    if (build->contour_ends) {
        for (size_t i = 0; i < contour_count; ++i)
            build->contour_ends[build->contour_count + i] =
                (uint16_t)(point_start + nx_glyf_u16(data + 10u + i * 2u));
    }
    build->point_count += point_count;
    build->contour_count += contour_count;
    return NX_OK;
}

static nx_result nx_glyf_append(nx_glyf_build *build, uint16_t glyph_index) {
    if (build->depth >= NX_GLYF_MAX_DEPTH ||
        build->visits >= NX_GLYF_MAX_VISITS) return NX_ERROR_LIMIT;
    for (size_t i = 0; i < build->depth; ++i)
        if (build->active[i] == glyph_index) return NX_ERROR_ARGUMENT;
    build->active[build->depth++] = glyph_index;
    ++build->visits;
    const uint8_t *data;
    size_t length;
    nx_result result = nx_glyf_view(build, glyph_index, &data, &length);
    if (result == NX_OK && length) {
        if (length < 10u) result = NX_ERROR_ARGUMENT;
        else {
            int16_t contours = nx_glyf_i16(data);
            nx_glyph_bounds bounds = {
                nx_glyf_i16(data + 2u), nx_glyf_i16(data + 4u),
                nx_glyf_i16(data + 6u), nx_glyf_i16(data + 8u)
            };
            if (bounds.x_min > bounds.x_max || bounds.y_min > bounds.y_max)
                result = NX_ERROR_ARGUMENT;
            else {
                if (build->depth == 1u) build->bounds = bounds;
                result = contours < 0
                         ? nx_glyf_append_composite(build, data, length)
                         : nx_glyf_append_simple(build, data, length,
                                                 (uint16_t)contours);
            }
        }
    }
    --build->depth;
    return result;
}

nx_result nx_font_get_glyph_outline(const nx_font *font, uint16_t glyph_index,
                                    nx_glyph_outline_buffer *out_outline) {
    if (!out_outline) return NX_ERROR_ARGUMENT;
    out_outline->point_count = 0;
    out_outline->contour_count = 0;
    out_outline->bounds = (nx_glyph_bounds){0};
    if (!font || (out_outline->point_capacity && !out_outline->points) ||
        (out_outline->contour_capacity && !out_outline->contour_ends))
        return NX_ERROR_ARGUMENT;
    nx_font_metrics metrics;
    if (nx_font_get_metrics(font, &metrics) != NX_OK ||
        glyph_index >= metrics.glyph_count) return NX_ERROR_ARGUMENT;
    nx_bytes_view loca, glyf;
    if (nx_font_find_table(font, NX_SFNT_TAG('l', 'o', 'c', 'a'), &loca) != NX_OK ||
        nx_font_find_table(font, NX_SFNT_TAG('g', 'l', 'y', 'f'), &glyf) != NX_OK)
        return NX_ERROR_UNSUPPORTED;
    size_t entry_size = metrics.index_to_loc_format ? 4u : 2u;
    if (loca.length < ((size_t)metrics.glyph_count + 1u) * entry_size)
        return NX_ERROR_ARGUMENT;
    nx_glyf_build build = {0};
    build.loca = loca;
    build.glyf = glyf;
    build.glyph_count = metrics.glyph_count;
    build.loca_format = metrics.index_to_loc_format;
    nx_result result = nx_glyf_append(&build, glyph_index);
    if (result != NX_OK) return result;
    out_outline->point_count = build.point_count;
    out_outline->contour_count = build.contour_count;
    out_outline->bounds = build.bounds;
    int query_only = !out_outline->points && !out_outline->contour_ends &&
                     !out_outline->point_capacity && !out_outline->contour_capacity;
    if (!query_only &&
        ((build.point_count && (!out_outline->points ||
          out_outline->point_capacity < build.point_count)) ||
         (build.contour_count && (!out_outline->contour_ends ||
          out_outline->contour_capacity < build.contour_count))))
        return NX_ERROR_LIMIT;
    if (!build.point_count) return NX_OK;
    size_t point_bytes = build.point_count * sizeof(nx_glyph_point);
    size_t contour_bytes = build.contour_count * sizeof(uint16_t);
    nx_glyph_point *points = nx_font_scratch_allocate(font, point_bytes);
    if (!points) return NX_ERROR_MEMORY;
    uint16_t *ends = nx_font_scratch_allocate(font, contour_bytes);
    if (!ends) {
        nx_font_scratch_deallocate(font, points, point_bytes);
        return NX_ERROR_MEMORY;
    }
    nx_glyf_build decoded = {0};
    decoded.loca = loca;
    decoded.glyf = glyf;
    decoded.glyph_count = metrics.glyph_count;
    decoded.loca_format = metrics.index_to_loc_format;
    decoded.points = points;
    decoded.contour_ends = ends;
    result = nx_glyf_append(&decoded, glyph_index);
    if (result == NX_OK && decoded.point_count == build.point_count &&
        decoded.contour_count == build.contour_count) {
        if (!query_only) {
            memcpy(out_outline->points, points, point_bytes);
            memcpy(out_outline->contour_ends, ends, contour_bytes);
        }
    } else if (result == NX_OK) {
        result = NX_ERROR_ARGUMENT;
    }
    nx_font_scratch_deallocate(font, ends, contour_bytes);
    nx_font_scratch_deallocate(font, points, point_bytes);
    return result;
}
