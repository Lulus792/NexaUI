#include "nexaui/nexaui.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NX_GLYPH_CACHE_BUCKETS 256u

typedef struct nx_glyph_cache_entry {
    struct nx_glyph_cache_entry *hash_next, *older, *newer;
    size_t allocation_size;
    uint16_t glyph_index, pixels_per_em;
    nx_glyph_mask_view mask;
} nx_glyph_cache_entry;

struct nx_glyph_cache {
    nx_allocator allocator;
    const nx_font *font;
    uint16_t units_per_em, glyph_count;
    size_t max_bytes, used_bytes, entry_count;
    uint64_t hits, misses, evictions;
    nx_glyph_cache_entry *oldest, *newest;
    nx_glyph_cache_entry *buckets[NX_GLYPH_CACHE_BUCKETS];
};

static void *nx_cache_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_cache_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static uint32_t nx_cache_bucket(uint16_t glyph_index, uint16_t pixels_per_em) {
    return (((uint32_t)glyph_index * UINT32_C(0x9e3779b1)) ^
            ((uint32_t)pixels_per_em * UINT32_C(0x85ebca6b))) &
           (NX_GLYPH_CACHE_BUCKETS - 1u);
}

static void nx_cache_unlink_lru(nx_glyph_cache *cache,
                                 nx_glyph_cache_entry *entry) {
    if (entry->older) entry->older->newer = entry->newer;
    else cache->oldest = entry->newer;
    if (entry->newer) entry->newer->older = entry->older;
    else cache->newest = entry->older;
}

static void nx_cache_append_lru(nx_glyph_cache *cache,
                                 nx_glyph_cache_entry *entry) {
    entry->older = cache->newest;
    entry->newer = NULL;
    if (cache->newest) cache->newest->newer = entry;
    else cache->oldest = entry;
    cache->newest = entry;
}

static void nx_cache_evict_oldest(nx_glyph_cache *cache) {
    nx_glyph_cache_entry *entry = cache->oldest;
    if (!entry) return;
    nx_cache_unlink_lru(cache, entry);
    uint32_t bucket = nx_cache_bucket(entry->glyph_index,
                                       entry->pixels_per_em);
    nx_glyph_cache_entry **link = &cache->buckets[bucket];
    while (*link && *link != entry) link = &(*link)->hash_next;
    if (*link) *link = entry->hash_next;
    cache->used_bytes -= entry->allocation_size;
    --cache->entry_count;
    ++cache->evictions;
    cache->allocator.deallocate(cache->allocator.user, entry,
                                 entry->allocation_size);
}

static nx_result nx_cache_dimensions(const nx_glyph_outline_buffer *outline,
                                      double scale, int32_t *out_x,
                                      int32_t *out_y, uint32_t *out_width,
                                      uint32_t *out_height) {
    *out_x = *out_y = 0;
    *out_width = *out_height = 0;
    if (!outline->point_count) return NX_OK;
    int32_t min_x = outline->points[0].x;
    int32_t max_x = min_x;
    int32_t min_y = outline->points[0].y;
    int32_t max_y = min_y;
    for (size_t i = 1; i < outline->point_count; ++i) {
        const nx_glyph_point *point = &outline->points[i];
        if (point->x < min_x) min_x = point->x;
        if (point->x > max_x) max_x = point->x;
        if (point->y < min_y) min_y = point->y;
        if (point->y > max_y) max_y = point->y;
    }
    double left = floor((double)min_x * scale) - 1.0;
    double right = ceil((double)max_x * scale) + 1.0;
    double top = floor(-(double)max_y * scale) - 1.0;
    double bottom = ceil(-(double)min_y * scale) + 1.0;
    double width = right - left, height = bottom - top;
    if (!isfinite(left) || !isfinite(right) || !isfinite(top) ||
        !isfinite(bottom) || left < INT32_MIN || top < INT32_MIN ||
        left > INT32_MAX || top > INT32_MAX ||
        width > NX_GLYPH_RASTER_MAX_DIMENSION ||
        height > NX_GLYPH_RASTER_MAX_DIMENSION ||
        width * height > NX_GLYPH_RASTER_MAX_PIXELS)
        return NX_ERROR_LIMIT;
    *out_x = (int32_t)left;
    *out_y = (int32_t)top;
    *out_width = (uint32_t)width;
    *out_height = (uint32_t)height;
    return NX_OK;
}

nx_result nx_glyph_cache_create(const nx_font *font, size_t max_bytes,
                                const nx_allocator *allocator,
                                nx_glyph_cache **out_cache) {
    if (!out_cache) return NX_ERROR_ARGUMENT;
    *out_cache = NULL;
    if (!font || !max_bytes) return NX_ERROR_ARGUMENT;
    nx_allocator selected = allocator ? *allocator : (nx_allocator){0};
    if ((selected.allocate == NULL) != (selected.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!selected.allocate) {
        selected.allocate = nx_cache_allocate;
        selected.deallocate = nx_cache_deallocate;
        selected.user = NULL;
    }
    nx_font_metrics metrics;
    nx_result result = nx_font_get_metrics(font, &metrics);
    if (result != NX_OK) return result;
    nx_glyph_cache *cache = selected.allocate(selected.user, sizeof(*cache));
    if (!cache) return NX_ERROR_MEMORY;
    memset(cache, 0, sizeof(*cache));
    cache->allocator = selected;
    cache->font = font;
    cache->units_per_em = metrics.units_per_em;
    cache->glyph_count = metrics.glyph_count;
    cache->max_bytes = max_bytes;
    *out_cache = cache;
    return NX_OK;
}

void nx_glyph_cache_clear(nx_glyph_cache *cache) {
    if (!cache) return;
    nx_glyph_cache_entry *entry = cache->oldest;
    while (entry) {
        nx_glyph_cache_entry *next = entry->newer;
        cache->allocator.deallocate(cache->allocator.user, entry,
                                    entry->allocation_size);
        entry = next;
    }
    memset(cache->buckets, 0, sizeof(cache->buckets));
    cache->oldest = cache->newest = NULL;
    cache->used_bytes = cache->entry_count = 0;
}

void nx_glyph_cache_destroy(nx_glyph_cache *cache) {
    if (!cache) return;
    nx_allocator allocator = cache->allocator;
    nx_glyph_cache_clear(cache);
    allocator.deallocate(allocator.user, cache, sizeof(*cache));
}

nx_result nx_glyph_cache_get_stats(const nx_glyph_cache *cache,
                                   nx_glyph_cache_stats *out_stats) {
    if (!out_stats) return NX_ERROR_ARGUMENT;
    *out_stats = (nx_glyph_cache_stats){0};
    if (!cache) return NX_ERROR_ARGUMENT;
    out_stats->used_bytes = cache->used_bytes;
    out_stats->max_bytes = cache->max_bytes;
    out_stats->entry_count = cache->entry_count;
    out_stats->hits = cache->hits;
    out_stats->misses = cache->misses;
    out_stats->evictions = cache->evictions;
    return NX_OK;
}

static nx_result nx_cache_build_entry(nx_glyph_cache *cache,
                                       uint16_t glyph_index,
                                       uint16_t pixels_per_em,
                                       nx_glyph_cache_entry **out_entry) {
    *out_entry = NULL;
    uint16_t advance;
    int16_t lsb;
    nx_result result = nx_font_glyph_hmetrics(cache->font, glyph_index,
                                               &advance, &lsb);
    (void)lsb;
    if (result != NX_OK) return result;
    nx_glyph_outline_buffer outline = {0};
    result = nx_font_get_glyph_outline(cache->font, glyph_index, &outline);
    if (result != NX_OK) return result;
    size_t point_bytes = outline.point_count * sizeof(nx_glyph_point);
    size_t contour_bytes = outline.contour_count * sizeof(uint16_t);
    nx_glyph_point *points = NULL;
    uint16_t *ends = NULL;
    nx_glyph_cache_entry *entry = NULL;
    if (point_bytes) {
        points = cache->allocator.allocate(cache->allocator.user, point_bytes);
        if (!points) return NX_ERROR_MEMORY;
        ends = cache->allocator.allocate(cache->allocator.user, contour_bytes);
        if (!ends) {
            result = NX_ERROR_MEMORY;
            goto cleanup;
        }
        outline.points = points;
        outline.point_capacity = outline.point_count;
        outline.contour_ends = ends;
        outline.contour_capacity = outline.contour_count;
        result = nx_font_get_glyph_outline(cache->font, glyph_index, &outline);
        if (result != NX_OK) goto cleanup;
    }
    double scale = (double)pixels_per_em / cache->units_per_em;
    int32_t x_offset, y_offset;
    uint32_t width, height;
    result = nx_cache_dimensions(&outline, scale, &x_offset, &y_offset,
                                  &width, &height);
    if (result != NX_OK) goto cleanup;
    size_t mask_bytes = (size_t)width * height;
    if (mask_bytes > SIZE_MAX - sizeof(*entry) ||
        sizeof(*entry) + mask_bytes > cache->max_bytes) {
        result = NX_ERROR_LIMIT;
        goto cleanup;
    }
    size_t allocation_size = sizeof(*entry) + mask_bytes;
    while (cache->used_bytes > cache->max_bytes - allocation_size)
        nx_cache_evict_oldest(cache);
    entry = cache->allocator.allocate(cache->allocator.user, allocation_size);
    if (!entry) {
        result = NX_ERROR_MEMORY;
        goto cleanup;
    }
    memset(entry, 0, sizeof(*entry));
    entry->allocation_size = allocation_size;
    entry->glyph_index = glyph_index;
    entry->pixels_per_em = pixels_per_em;
    uint8_t *mask_pixels = mask_bytes ? (uint8_t *)entry + sizeof(*entry) : NULL;
    entry->mask.pixels = mask_pixels;
    entry->mask.width = width;
    entry->mask.height = height;
    entry->mask.stride_bytes = width;
    entry->mask.x_offset = x_offset;
    entry->mask.y_offset = y_offset;
    entry->mask.advance_x = (double)advance * scale;
    if (mask_bytes) {
        result = nx_rasterize_glyph_a8(&outline, scale,
                                        -(double)x_offset, -(double)y_offset,
                                        mask_pixels, width, height, width,
                                        mask_bytes, &cache->allocator);
        if (result != NX_OK) goto cleanup;
    }
    *out_entry = entry;
    entry = NULL;
cleanup:
    if (entry)
        cache->allocator.deallocate(cache->allocator.user, entry,
                                    entry->allocation_size);
    if (ends)
        cache->allocator.deallocate(cache->allocator.user, ends, contour_bytes);
    if (points)
        cache->allocator.deallocate(cache->allocator.user, points, point_bytes);
    return result;
}

nx_result nx_glyph_cache_get(nx_glyph_cache *cache, uint16_t glyph_index,
                             uint16_t pixels_per_em,
                             nx_glyph_mask_view *out_mask) {
    if (!out_mask) return NX_ERROR_ARGUMENT;
    *out_mask = (nx_glyph_mask_view){0};
    if (!cache || !pixels_per_em || glyph_index >= cache->glyph_count)
        return NX_ERROR_ARGUMENT;
    uint32_t bucket = nx_cache_bucket(glyph_index, pixels_per_em);
    for (nx_glyph_cache_entry *entry = cache->buckets[bucket]; entry;
         entry = entry->hash_next) {
        if (entry->glyph_index == glyph_index &&
            entry->pixels_per_em == pixels_per_em) {
            ++cache->hits;
            nx_cache_unlink_lru(cache, entry);
            nx_cache_append_lru(cache, entry);
            *out_mask = entry->mask;
            return NX_OK;
        }
    }
    ++cache->misses;
    nx_glyph_cache_entry *entry;
    nx_result result = nx_cache_build_entry(cache, glyph_index,
                                             pixels_per_em, &entry);
    if (result != NX_OK) return result;
    entry->hash_next = cache->buckets[bucket];
    cache->buckets[bucket] = entry;
    nx_cache_append_lru(cache, entry);
    cache->used_bytes += entry->allocation_size;
    ++cache->entry_count;
    *out_mask = entry->mask;
    return NX_OK;
}
