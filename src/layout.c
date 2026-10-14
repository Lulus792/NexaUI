#include "internal.h"

#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nx_grid_axis {
    nx_grid_track spec;
    double measured, arranged;
} nx_grid_axis;

typedef struct nx_layout_node {
    nx_layout_desc desc;
    uint32_t parent, first_child, last_child, next_sibling;
    unsigned depth;
    nx_size desired;
    nx_rect rect;
    double arranged_main;
    double scroll_x, scroll_y;
    nx_scroll_info scroll_info;
    double split_ratio;
    nx_split_info split_info;
    uint32_t wrap_line;
    nx_grid_axis *grid_columns, *grid_rows;
    size_t grid_column_count, grid_row_count;
    uint32_t grid_row, grid_column, grid_row_span, grid_column_span;
    uint64_t arranged_version;
} nx_layout_node;

struct nx_layout {
    nx_allocator allocator;
    nx_layout_node *nodes;
    size_t count, capacity;
    uint32_t measured_root;
    uint64_t arrange_generation;
    int measured_valid, arranged_valid;
};

static void *nx_layout_default_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_layout_default_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int nx_valid_insets(nx_insets p) {
    return isfinite(p.left) && isfinite(p.top) && isfinite(p.right) &&
           isfinite(p.bottom) && p.left >= 0 && p.top >= 0 &&
           p.right >= 0 && p.bottom >= 0;
}

static int nx_valid_axis(nx_axis_spec a) {
    return isfinite(a.minimum) && a.minimum >= 0 &&
           isfinite(a.preferred) && a.preferred >= 0 &&
           (isfinite(a.maximum) || (isinf(a.maximum) && a.maximum > 0)) &&
           a.maximum >= a.minimum &&
           isfinite(a.grow) && a.grow >= 0 &&
           isfinite(a.shrink) && a.shrink >= 0 &&
           (a.has_preferred == 0 || a.has_preferred == 1);
}

static int nx_valid_desc(const nx_layout_desc *d) {
    return d && d->kind >= NX_LAYOUT_LEAF && d->kind <= NX_LAYOUT_SPLIT &&
           nx_valid_axis(d->width) && nx_valid_axis(d->height) &&
           nx_valid_insets(d->padding) && nx_valid_insets(d->border) &&
           nx_valid_insets(d->margin) && isfinite(d->gap) && d->gap >= 0 &&
           isfinite(d->line_gap) && d->line_gap >= 0 &&
           d->align_x >= NX_ALIGN_START && d->align_x <= NX_ALIGN_STRETCH &&
           d->align_y >= NX_ALIGN_START && d->align_y <= NX_ALIGN_STRETCH &&
           (d->scroll_axes & ~(NX_SCROLL_X | NX_SCROLL_Y)) == 0 &&
           (d->kind == NX_LAYOUT_SCROLL ? d->scroll_axes != 0 : d->scroll_axes == 0) &&
           (d->kind == NX_LAYOUT_SPLIT
                ? ((d->split_axis == NX_SPLIT_X || d->split_axis == NX_SPLIT_Y) &&
                   isfinite(d->split_ratio) && d->split_ratio >= 0 && d->split_ratio <= 1)
                : (d->split_axis == 0 && d->split_ratio == 0));
}

static double nx_axis_clamp(double value, nx_axis_spec a) {
    if (value < a.minimum) value = a.minimum;
    if (value > a.maximum) value = a.maximum;
    return value;
}

static int nx_floatable(double x) {
    return isfinite(x) && x >= -FLT_MAX && x <= FLT_MAX;
}

static double nx_horizontal(nx_insets p) { return (double)p.left + p.right; }
static double nx_vertical(nx_insets p) { return (double)p.top + p.bottom; }

static int nx_valid_grid_track(nx_grid_track t) {
    return t.kind >= NX_GRID_TRACK_FIXED && t.kind <= NX_GRID_TRACK_FLEX &&
           isfinite(t.minimum) && t.minimum >= 0 &&
           (isfinite(t.maximum) || (isinf(t.maximum) && t.maximum > 0)) &&
           t.maximum >= t.minimum && isfinite(t.value) &&
           (t.kind == NX_GRID_TRACK_FIXED ? t.value >= 0 :
            t.kind == NX_GRID_TRACK_FLEX ? t.value > 0 : t.value == 0);
}

static void nx_layout_free_grid_nodes(nx_layout *layout) {
    for (size_t i = 0; i < layout->count; ++i) {
        nx_layout_node *node = &layout->nodes[i];
        if (node->grid_columns)
            layout->allocator.deallocate(layout->allocator.user, node->grid_columns,
                node->grid_column_count * sizeof(nx_grid_axis));
        if (node->grid_rows)
            layout->allocator.deallocate(layout->allocator.user, node->grid_rows,
                node->grid_row_count * sizeof(nx_grid_axis));
    }
}

nx_layout_desc nx_layout_default_desc(nx_layout_kind kind) {
    nx_layout_desc d = {0};
    d.kind = kind;
    d.width.maximum = INFINITY;
    d.height.maximum = INFINITY;
    d.width.shrink = 1.0f;
    d.height.shrink = 1.0f;
    d.align_x = NX_ALIGN_START;
    d.align_y = NX_ALIGN_START;
    if (kind == NX_LAYOUT_SCROLL) d.scroll_axes = NX_SCROLL_Y;
    if (kind == NX_LAYOUT_SPLIT) {
        d.split_axis = NX_SPLIT_X;
        d.split_ratio = 0.5f;
        d.gap = 6.0f;
    }
    return d;
}

nx_result nx_layout_create(const nx_allocator *allocator, size_t initial_node_capacity,
                           nx_layout **out_layout) {
    if (!out_layout) return NX_ERROR_ARGUMENT;
    *out_layout = NULL;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.user = NULL;
        a.allocate = nx_layout_default_allocate;
        a.deallocate = nx_layout_default_deallocate;
    }
    size_t capacity = initial_node_capacity ? initial_node_capacity : 64u;
    if (capacity > UINT32_MAX || capacity > SIZE_MAX / sizeof(nx_layout_node))
        return NX_ERROR_LIMIT;
    nx_layout *layout = (nx_layout *)a.allocate(a.user, sizeof(nx_layout));
    if (!layout) return NX_ERROR_MEMORY;
    memset(layout, 0, sizeof(*layout));
    layout->allocator = a;
    layout->nodes = (nx_layout_node *)a.allocate(a.user,
                                                  capacity * sizeof(nx_layout_node));
    if (!layout->nodes) {
        a.deallocate(a.user, layout, sizeof(nx_layout));
        return NX_ERROR_MEMORY;
    }
    layout->capacity = capacity;
    layout->measured_root = NX_LAYOUT_NO_PARENT;
    *out_layout = layout;
    return NX_OK;
}

void nx_layout_destroy(nx_layout *layout) {
    if (!layout) return;
    nx_allocator a = layout->allocator;
    nx_layout_free_grid_nodes(layout);
    a.deallocate(a.user, layout->nodes, layout->capacity * sizeof(nx_layout_node));
    a.deallocate(a.user, layout, sizeof(nx_layout));
}

void nx_layout_reset(nx_layout *layout) {
    if (!layout) return;
    nx_layout_free_grid_nodes(layout);
    layout->count = 0;
    layout->measured_root = NX_LAYOUT_NO_PARENT;
    layout->measured_valid = 0;
    layout->arranged_valid = 0;
}

static nx_result nx_layout_reserve(nx_layout *layout) {
    if (layout->count < layout->capacity) return NX_OK;
    size_t next = layout->capacity * 2u;
    if (next < layout->capacity || next > UINT32_MAX ||
        next > SIZE_MAX / sizeof(nx_layout_node)) return NX_ERROR_LIMIT;
    nx_layout_node *nodes = (nx_layout_node *)layout->allocator.allocate(
        layout->allocator.user, next * sizeof(nx_layout_node));
    if (!nodes) return NX_ERROR_MEMORY;
    memcpy(nodes, layout->nodes, layout->count * sizeof(nx_layout_node));
    layout->allocator.deallocate(layout->allocator.user, layout->nodes,
                                  layout->capacity * sizeof(nx_layout_node));
    layout->nodes = nodes;
    layout->capacity = next;
    return NX_OK;
}

static nx_result nx_layout_add_impl(nx_layout *layout, uint32_t parent,
                                     const nx_layout_desc *desc,
                                     int grid_child, uint32_t row, uint32_t column,
                                     uint32_t row_span, uint32_t column_span,
                                     uint32_t *out_node) {
    if (!out_node) return NX_ERROR_ARGUMENT;
    *out_node = NX_LAYOUT_NO_PARENT;
    if (!layout || !nx_valid_desc(desc)) return NX_ERROR_ARGUMENT;
    unsigned depth = 0;
    if (parent != NX_LAYOUT_NO_PARENT) {
        if (parent >= layout->count ||
            layout->nodes[parent].desc.kind == NX_LAYOUT_LEAF ||
            (layout->nodes[parent].desc.kind == NX_LAYOUT_SCROLL &&
             layout->nodes[parent].first_child != NX_LAYOUT_NO_PARENT) ||
            (layout->nodes[parent].desc.kind == NX_LAYOUT_SPLIT &&
             layout->nodes[parent].first_child != NX_LAYOUT_NO_PARENT &&
             layout->nodes[parent].last_child != layout->nodes[parent].first_child))
            return NX_ERROR_ARGUMENT;
        nx_layout_node *p = &layout->nodes[parent];
        if (grid_child) {
            if (p->desc.kind != NX_LAYOUT_GRID || !row_span || !column_span ||
                row >= p->grid_row_count || column >= p->grid_column_count ||
                row_span > p->grid_row_count - row ||
                column_span > p->grid_column_count - column)
                return NX_ERROR_ARGUMENT;
        } else if (p->desc.kind == NX_LAYOUT_GRID) {
            return NX_ERROR_ARGUMENT;
        }
        depth = layout->nodes[parent].depth + 1u;
        if (depth >= NX_LAYOUT_MAX_DEPTH) return NX_ERROR_LIMIT;
    } else if (grid_child) {
        return NX_ERROR_ARGUMENT;
    }
    nx_result result = nx_layout_reserve(layout);
    if (result != NX_OK) return result;
    uint32_t index = (uint32_t)layout->count++;
    nx_layout_node *node = &layout->nodes[index];
    memset(node, 0, sizeof(*node));
    node->desc = *desc;
    node->parent = parent;
    node->first_child = NX_LAYOUT_NO_PARENT;
    node->last_child = NX_LAYOUT_NO_PARENT;
    node->next_sibling = NX_LAYOUT_NO_PARENT;
    node->depth = depth;
    node->split_ratio = desc->split_ratio;
    if (grid_child) {
        node->grid_row = row;
        node->grid_column = column;
        node->grid_row_span = row_span;
        node->grid_column_span = column_span;
    }
    if (parent != NX_LAYOUT_NO_PARENT) {
        nx_layout_node *p = &layout->nodes[parent];
        if (p->last_child == NX_LAYOUT_NO_PARENT) p->first_child = index;
        else layout->nodes[p->last_child].next_sibling = index;
        p->last_child = index;
    }
    layout->measured_valid = 0;
    layout->arranged_valid = 0;
    *out_node = index;
    return NX_OK;
}

nx_result nx_layout_add(nx_layout *layout, uint32_t parent,
                        const nx_layout_desc *desc, uint32_t *out_node) {
    return nx_layout_add_impl(layout, parent, desc, 0, 0, 0, 0, 0, out_node);
}

nx_result nx_layout_spacer(nx_layout *layout, uint32_t parent,
                           float width, float height, uint32_t *out_node) {
    if (!isfinite(width) || !isfinite(height) || width < 0.0f ||
        height < 0.0f) return NX_ERROR_ARGUMENT;
    nx_layout_desc desc = nx_layout_default_desc(NX_LAYOUT_LEAF);
    desc.width.minimum = desc.width.preferred = desc.width.maximum = width;
    desc.height.minimum = desc.height.preferred = desc.height.maximum = height;
    desc.width.has_preferred = desc.height.has_preferred = 1;
    desc.width.grow = desc.width.shrink = 0.0f;
    desc.height.grow = desc.height.shrink = 0.0f;
    return nx_layout_add(layout, parent, &desc, out_node);
}

nx_result nx_layout_add_grid(nx_layout *layout, uint32_t grid,
                             const nx_layout_desc *desc,
                             uint32_t row, uint32_t column,
                             uint32_t row_span, uint32_t column_span,
                             uint32_t *out_node) {
    return nx_layout_add_impl(layout, grid, desc, 1, row, column,
                              row_span, column_span, out_node);
}

nx_result nx_layout_grid_configure(nx_layout *layout, uint32_t grid,
                                   const nx_grid_track *columns, size_t column_count,
                                   const nx_grid_track *rows, size_t row_count) {
    if (!layout || grid >= layout->count ||
        layout->nodes[grid].desc.kind != NX_LAYOUT_GRID ||
        layout->nodes[grid].first_child != NX_LAYOUT_NO_PARENT ||
        !columns || !rows || !column_count || !row_count)
        return NX_ERROR_ARGUMENT;
    if (column_count > UINT32_MAX || row_count > UINT32_MAX ||
        column_count > SIZE_MAX / sizeof(nx_grid_axis) ||
        row_count > SIZE_MAX / sizeof(nx_grid_axis)) return NX_ERROR_LIMIT;
    for (size_t i = 0; i < column_count; ++i)
        if (!nx_valid_grid_track(columns[i])) return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < row_count; ++i)
        if (!nx_valid_grid_track(rows[i])) return NX_ERROR_ARGUMENT;
    nx_allocator a = layout->allocator;
    nx_grid_axis *new_columns = (nx_grid_axis *)a.allocate(
        a.user, column_count * sizeof(nx_grid_axis));
    if (!new_columns) return NX_ERROR_MEMORY;
    nx_grid_axis *new_rows = (nx_grid_axis *)a.allocate(
        a.user, row_count * sizeof(nx_grid_axis));
    if (!new_rows) {
        a.deallocate(a.user, new_columns, column_count * sizeof(nx_grid_axis));
        return NX_ERROR_MEMORY;
    }
    for (size_t i = 0; i < column_count; ++i) {
        new_columns[i] = (nx_grid_axis){0};
        new_columns[i].spec = columns[i];
    }
    for (size_t i = 0; i < row_count; ++i) {
        new_rows[i] = (nx_grid_axis){0};
        new_rows[i].spec = rows[i];
    }
    nx_layout_node *node = &layout->nodes[grid];
    if (node->grid_columns)
        a.deallocate(a.user, node->grid_columns,
                     node->grid_column_count * sizeof(nx_grid_axis));
    if (node->grid_rows)
        a.deallocate(a.user, node->grid_rows,
                     node->grid_row_count * sizeof(nx_grid_axis));
    node->grid_columns = new_columns;
    node->grid_rows = new_rows;
    node->grid_column_count = column_count;
    node->grid_row_count = row_count;
    layout->measured_valid = 0;
    layout->arranged_valid = 0;
    return NX_OK;
}

static double nx_grid_base(nx_grid_track t) {
    if (t.kind != NX_GRID_TRACK_FIXED) return t.minimum;
    return nx_axis_clamp(t.value, (nx_axis_spec){.minimum = t.minimum,
                                                .maximum = t.maximum});
}

static double nx_grid_span_size(const nx_grid_axis *axis, size_t start,
                                size_t span, double gap, int arranged) {
    double result = (span - 1u) * gap;
    for (size_t i = start; i < start + span; ++i)
        result += arranged ? axis[i].arranged : axis[i].measured;
    return result;
}

static double nx_grid_extent(const nx_grid_axis *axis, size_t count,
                              double gap, int arranged) {
    return nx_grid_span_size(axis, 0, count, gap, arranged);
}

static void nx_grid_raise(nx_grid_axis *axis, size_t start, size_t span,
                           double gap, double required) {
    double remaining = required - nx_grid_span_size(axis, start, span, gap, 0);
    for (int kind = NX_GRID_TRACK_AUTO; kind <= NX_GRID_TRACK_FLEX && remaining > 1e-9;
         ++kind) {
        for (size_t pass = 0; pass < span && remaining > 1e-9; ++pass) {
            size_t eligible = 0;
            for (size_t i = start; i < start + span; ++i)
                if (axis[i].spec.kind == kind &&
                    axis[i].measured < axis[i].spec.maximum) ++eligible;
            if (!eligible) break;
            double share = remaining / eligible;
            double applied = 0;
            int capped = 0;
            for (size_t i = start; i < start + span; ++i) {
                if (axis[i].spec.kind != kind ||
                    axis[i].measured >= axis[i].spec.maximum) continue;
                double taken = fmin(share, axis[i].spec.maximum - axis[i].measured);
                if (taken < share) capped = 1;
                axis[i].measured += taken;
                applied += taken;
            }
            remaining -= applied;
            if (!capped || applied <= 1e-9) break;
        }
    }
}

static void nx_grid_fit(nx_grid_axis *axis, size_t count, double gap,
                         double available) {
    for (size_t i = 0; i < count; ++i) axis[i].arranged = axis[i].measured;
    if (!isfinite(available)) return;
    double remaining = available - nx_grid_extent(axis, count, gap, 1);
    if (remaining > 1e-9) {
        for (size_t pass = 0; pass < count && remaining > 1e-9; ++pass) {
            double weight = 0;
            for (size_t i = 0; i < count; ++i)
                if (axis[i].spec.kind == NX_GRID_TRACK_FLEX &&
                    axis[i].arranged < axis[i].spec.maximum)
                    weight += axis[i].spec.value;
            if (weight <= 0) break;
            double applied = 0;
            int capped = 0;
            for (size_t i = 0; i < count; ++i) {
                if (axis[i].spec.kind != NX_GRID_TRACK_FLEX ||
                    axis[i].arranged >= axis[i].spec.maximum) continue;
                double share = remaining * axis[i].spec.value / weight;
                double taken = fmin(share, axis[i].spec.maximum - axis[i].arranged);
                if (taken < share) capped = 1;
                axis[i].arranged += taken;
                applied += taken;
            }
            remaining -= applied;
            if (!capped || applied <= 1e-9) break;
        }
    } else if (remaining < -1e-9) {
        remaining = -remaining;
        for (int kind = NX_GRID_TRACK_FLEX; kind >= NX_GRID_TRACK_AUTO && remaining > 1e-9;
             --kind) {
            for (size_t pass = 0; pass < count && remaining > 1e-9; ++pass) {
                double weight = 0;
                for (size_t i = 0; i < count; ++i)
                    if (axis[i].spec.kind == kind &&
                        axis[i].arranged > axis[i].spec.minimum)
                        weight += axis[i].arranged;
                if (weight <= 0) break;
                double applied = 0;
                int capped = 0;
                for (size_t i = 0; i < count; ++i) {
                    if (axis[i].spec.kind != kind ||
                        axis[i].arranged <= axis[i].spec.minimum) continue;
                    double share = remaining * axis[i].arranged / weight;
                    double taken = fmin(share, axis[i].arranged - axis[i].spec.minimum);
                    if (taken < share) capped = 1;
                    axis[i].arranged -= taken;
                    applied += taken;
                }
                remaining -= applied;
                if (!capped || applied <= 1e-9) break;
            }
        }
    }
}

static nx_result nx_measure_node(nx_layout *layout, uint32_t index,
                                 double available_width, double available_height);

static nx_result nx_measure_grid(nx_layout *layout, nx_layout_node *node,
                                 double inner_w,
                                 double *out_width, double *out_height) {
    if (!node->grid_columns || !node->grid_rows) return NX_ERROR_STATE;
    nx_grid_axis *columns = node->grid_columns;
    nx_grid_axis *rows = node->grid_rows;
    double gap_x = node->desc.gap, gap_y = node->desc.line_gap;
    for (size_t i = 0; i < node->grid_column_count; ++i)
        columns[i].measured = nx_grid_base(columns[i].spec);
    for (size_t i = 0; i < node->grid_row_count; ++i)
        rows[i].measured = nx_grid_base(rows[i].spec);
    for (uint32_t child = node->first_child; child != NX_LAYOUT_NO_PARENT;
         child = layout->nodes[child].next_sibling) {
        nx_layout_node *c = &layout->nodes[child];
        int fixed_w = 1, fixed_h = 1;
        for (size_t i = c->grid_column; i < (size_t)c->grid_column + c->grid_column_span; ++i)
            if (columns[i].spec.kind != NX_GRID_TRACK_FIXED) fixed_w = 0;
        for (size_t i = c->grid_row; i < (size_t)c->grid_row + c->grid_row_span; ++i)
            if (rows[i].spec.kind != NX_GRID_TRACK_FIXED) fixed_h = 0;
        double width = fixed_w ? nx_grid_span_size(columns, c->grid_column,
                                                   c->grid_column_span, gap_x, 0) : INFINITY;
        double height = fixed_h ? nx_grid_span_size(rows, c->grid_row,
                                                    c->grid_row_span, gap_y, 0) : INFINITY;
        nx_result result = nx_measure_node(layout, child, width, height);
        if (result != NX_OK) return result;
    }
    for (int phase = 1; phase <= 2; ++phase) {
        for (uint32_t child = node->first_child; child != NX_LAYOUT_NO_PARENT;
             child = layout->nodes[child].next_sibling) {
            nx_layout_node *c = &layout->nodes[child];
            if ((c->grid_column_span == 1u) != (phase == 1)) continue;
            double required = c->desired.width + nx_horizontal(c->desc.margin);
            nx_grid_raise(columns, c->grid_column, c->grid_column_span,
                          gap_x, required);
        }
    }
    nx_grid_fit(columns, node->grid_column_count, gap_x, inner_w);
    for (uint32_t child = node->first_child; child != NX_LAYOUT_NO_PARENT;
         child = layout->nodes[child].next_sibling) {
        nx_layout_node *c = &layout->nodes[child];
        double width = nx_grid_span_size(columns, c->grid_column,
                                          c->grid_column_span, gap_x, 1);
        int fixed_h = 1;
        for (size_t i = c->grid_row; i < (size_t)c->grid_row + c->grid_row_span; ++i)
            if (rows[i].spec.kind != NX_GRID_TRACK_FIXED) fixed_h = 0;
        double height = fixed_h ? nx_grid_span_size(rows, c->grid_row,
                                                    c->grid_row_span, gap_y, 0) : INFINITY;
        nx_result result = nx_measure_node(layout, child, width, height);
        if (result != NX_OK) return result;
    }
    for (int phase = 1; phase <= 2; ++phase) {
        for (uint32_t child = node->first_child; child != NX_LAYOUT_NO_PARENT;
             child = layout->nodes[child].next_sibling) {
            nx_layout_node *c = &layout->nodes[child];
            if ((c->grid_row_span == 1u) != (phase == 1)) continue;
            double required = c->desired.height + nx_vertical(c->desc.margin);
            nx_grid_raise(rows, c->grid_row, c->grid_row_span, gap_y, required);
        }
    }
    *out_width = nx_grid_extent(columns, node->grid_column_count, gap_x, 0);
    *out_height = nx_grid_extent(rows, node->grid_row_count, gap_y, 0);
    if (!nx_floatable(*out_width) || !nx_floatable(*out_height)) return NX_ERROR_LIMIT;
    return NX_OK;
}

static nx_result nx_measure_node(nx_layout *layout, uint32_t index,
                                 double available_width, double available_height) {
    nx_layout_node *node = &layout->nodes[index];
    const nx_layout_desc *d = &node->desc;
    double inset_w = nx_horizontal(d->padding) + nx_horizontal(d->border);
    double inset_h = nx_vertical(d->padding) + nx_vertical(d->border);
    double inner_w = isinf(available_width) ? INFINITY : fmax(0, available_width - inset_w);
    double inner_h = isinf(available_height) ? INFINITY : fmax(0, available_height - inset_h);
    double content_w = 0, content_h = 0;
    if (d->kind == NX_LAYOUT_SPLIT &&
        (node->first_child == NX_LAYOUT_NO_PARENT ||
         layout->nodes[node->first_child].next_sibling == NX_LAYOUT_NO_PARENT))
        return NX_ERROR_STATE;
    if (d->kind == NX_LAYOUT_GRID) {
        nx_result result = nx_measure_grid(layout, node, inner_w,
                                           &content_w, &content_h);
        if (result != NX_OK) return result;
    } else {
    double wrap_main = 0, wrap_cross = 0;
    double line_main = 0, line_cross = 0;
    uint32_t wrap_line = 0;
    int line_has_item = 0;
    size_t child_count = 0;
    for (uint32_t child = node->first_child; child != NX_LAYOUT_NO_PARENT;
         child = layout->nodes[child].next_sibling) {
        double child_w = d->kind == NX_LAYOUT_ROW || d->kind == NX_LAYOUT_WRAP_ROW ||
            (d->kind == NX_LAYOUT_SPLIT && d->split_axis == NX_SPLIT_X) ||
            (d->kind == NX_LAYOUT_SCROLL && (d->scroll_axes & NX_SCROLL_X))
            ? INFINITY : inner_w;
        double child_h = d->kind == NX_LAYOUT_COLUMN || d->kind == NX_LAYOUT_WRAP_COLUMN ||
            (d->kind == NX_LAYOUT_SPLIT && d->split_axis == NX_SPLIT_Y) ||
            (d->kind == NX_LAYOUT_SCROLL && (d->scroll_axes & NX_SCROLL_Y))
            ? INFINITY : inner_h;
        nx_result result = nx_measure_node(layout, child, child_w, child_h);
        if (result != NX_OK) return result;
        nx_layout_node *c = &layout->nodes[child];
        double outer_w = c->desired.width + nx_horizontal(c->desc.margin);
        double outer_h = c->desired.height + nx_vertical(c->desc.margin);
        if (d->kind == NX_LAYOUT_WRAP_ROW || d->kind == NX_LAYOUT_WRAP_COLUMN) {
            int horizontal = d->kind == NX_LAYOUT_WRAP_ROW;
            double outer_main = horizontal ? outer_w : outer_h;
            double outer_cross = horizontal ? outer_h : outer_w;
            double limit = horizontal ? inner_w : inner_h;
            if (line_has_item && isfinite(limit) &&
                line_main + d->gap + outer_main > limit) {
                wrap_main = fmax(wrap_main, line_main);
                wrap_cross += line_cross + (wrap_line ? d->line_gap : 0);
                ++wrap_line;
                line_main = line_cross = 0;
                line_has_item = 0;
            }
            c->wrap_line = wrap_line;
            line_main += outer_main + (line_has_item ? d->gap : 0);
            line_cross = fmax(line_cross, outer_cross);
            line_has_item = 1;
        } else if (d->kind == NX_LAYOUT_ROW ||
                   (d->kind == NX_LAYOUT_SPLIT && d->split_axis == NX_SPLIT_X)) {
            content_w += outer_w;
            content_h = fmax(content_h, outer_h);
        } else if (d->kind == NX_LAYOUT_COLUMN ||
                   (d->kind == NX_LAYOUT_SPLIT && d->split_axis == NX_SPLIT_Y)) {
            content_w = fmax(content_w, outer_w);
            content_h += outer_h;
        } else {
            content_w = fmax(content_w, outer_w);
            content_h = fmax(content_h, outer_h);
        }
        ++child_count;
    }
    if (line_has_item) {
        wrap_main = fmax(wrap_main, line_main);
        wrap_cross += line_cross + (wrap_line ? d->line_gap : 0);
    }
    if (d->kind == NX_LAYOUT_WRAP_ROW) {
        content_w = wrap_main;
        content_h = wrap_cross;
    } else if (d->kind == NX_LAYOUT_WRAP_COLUMN) {
        content_w = wrap_cross;
        content_h = wrap_main;
    }
    if (child_count > 1u) {
        if (d->kind == NX_LAYOUT_ROW) content_w += (child_count - 1u) * (double)d->gap;
        if (d->kind == NX_LAYOUT_COLUMN) content_h += (child_count - 1u) * (double)d->gap;
        if (d->kind == NX_LAYOUT_SPLIT) {
            if (d->split_axis == NX_SPLIT_X) content_w += d->gap;
            else content_h += d->gap;
        }
    }
    }
    double width = d->width.has_preferred ? d->width.preferred : content_w + inset_w;
    double height = d->height.has_preferred ? d->height.preferred : content_h + inset_h;
    if (d->kind == NX_LAYOUT_SCROLL) {
        if ((d->scroll_axes & NX_SCROLL_X) && isfinite(available_width))
            width = fmin(width, available_width);
        if ((d->scroll_axes & NX_SCROLL_Y) && isfinite(available_height))
            height = fmin(height, available_height);
    }
    width = nx_axis_clamp(width, d->width);
    height = nx_axis_clamp(height, d->height);
    if (!nx_floatable(width) || !nx_floatable(height)) return NX_ERROR_LIMIT;
    node->desired = (nx_size){(float)width, (float)height};
    return NX_OK;
}

nx_result nx_layout_measure(nx_layout *layout, uint32_t root,
                            float available_width, float available_height,
                            nx_size *out_desired) {
    if (!out_desired) return NX_ERROR_ARGUMENT;
    *out_desired = (nx_size){0};
    if (!layout || root >= layout->count ||
        layout->nodes[root].parent != NX_LAYOUT_NO_PARENT ||
        !(isfinite(available_width) || (isinf(available_width) && available_width > 0)) ||
        !(isfinite(available_height) || (isinf(available_height) && available_height > 0)) ||
        available_width < 0 || available_height < 0) return NX_ERROR_ARGUMENT;
    layout->measured_valid = 0;
    layout->arranged_valid = 0;
    nx_result result = nx_measure_node(layout, root, available_width, available_height);
    if (result != NX_OK) return result;
    layout->measured_root = root;
    layout->measured_valid = 1;
    *out_desired = layout->nodes[root].desired;
    return NX_OK;
}

static double nx_margin_before(const nx_layout_node *node, int horizontal) {
    return horizontal ? node->desc.margin.left : node->desc.margin.top;
}

static double nx_margin_after(const nx_layout_node *node, int horizontal) {
    return horizontal ? node->desc.margin.right : node->desc.margin.bottom;
}

static double nx_desired_axis(const nx_layout_node *node, int horizontal) {
    return horizontal ? node->desired.width : node->desired.height;
}

static nx_axis_spec nx_spec_axis(const nx_layout_node *node, int horizontal) {
    return horizontal ? node->desc.width : node->desc.height;
}

static nx_align nx_align_axis(const nx_layout_node *node, int horizontal) {
    return horizontal ? node->desc.align_x : node->desc.align_y;
}

static double nx_cross_size(const nx_layout_node *child, double span, int horizontal) {
    nx_axis_spec spec = nx_spec_axis(child, horizontal);
    double size = nx_desired_axis(child, horizontal);
    if (nx_align_axis(child, horizontal) == NX_ALIGN_STRETCH)
        size = nx_axis_clamp(fmax(0, span), spec);
    else if (spec.shrink > 0 && size > span)
        size = fmax(spec.minimum, span);
    return size;
}

static double nx_cross_position(const nx_layout_node *child, double start,
                                double span, double size, int horizontal) {
    nx_align align = nx_align_axis(child, horizontal);
    if (align == NX_ALIGN_CENTER) return start + (span - size) / 2.0;
    if (align == NX_ALIGN_END) return start + span - size;
    return start;
}

static nx_result nx_make_rect(double x, double y, double w, double h, nx_rect *out) {
    if (!nx_floatable(x) || !nx_floatable(y) || !nx_floatable(w) ||
        !nx_floatable(h) || w < 0 || h < 0) return NX_ERROR_LIMIT;
    *out = (nx_rect){(float)x, (float)y, (float)w, (float)h};
    return NX_OK;
}

static void nx_distribute_main(nx_layout *layout, nx_layout_node *parent,
                                uint32_t first, uint32_t end,
                                double available, int horizontal) {
    double base_total = 0;
    size_t count = 0;
    for (uint32_t i = first; i != end;
         i = layout->nodes[i].next_sibling) {
        nx_layout_node *child = &layout->nodes[i];
        child->arranged_main = nx_desired_axis(child, horizontal);
        base_total += child->arranged_main + nx_margin_before(child, horizontal) +
                      nx_margin_after(child, horizontal);
        ++count;
    }
    if (count > 1u) base_total += (count - 1u) * (double)parent->desc.gap;
    double remaining = available - base_total;
    if (fabs(remaining) < 1e-9) return;
    int growing = remaining > 0;
    for (size_t pass = 0; pass < count && fabs(remaining) > 1e-9; ++pass) {
        double weight_sum = 0;
        for (uint32_t i = first; i != end;
             i = layout->nodes[i].next_sibling) {
            nx_layout_node *child = &layout->nodes[i];
            nx_axis_spec spec = nx_spec_axis(child, horizontal);
            double weight = growing ? spec.grow : spec.shrink * nx_desired_axis(child, horizontal);
            if (weight > 0 && (growing ? child->arranged_main < spec.maximum
                                       : child->arranged_main > spec.minimum))
                weight_sum += weight;
        }
        if (weight_sum <= 0) break;
        double applied = 0;
        int capped = 0;
        for (uint32_t i = first; i != end;
             i = layout->nodes[i].next_sibling) {
            nx_layout_node *child = &layout->nodes[i];
            nx_axis_spec spec = nx_spec_axis(child, horizontal);
            double weight = growing ? spec.grow : spec.shrink * nx_desired_axis(child, horizontal);
            if (weight <= 0) continue;
            double capacity = growing ? spec.maximum - child->arranged_main
                                      : child->arranged_main - spec.minimum;
            if (capacity <= 0) continue;
            double share = fabs(remaining) * weight / weight_sum;
            double taken = fmin(capacity, share);
            if (taken < share) capped = 1;
            child->arranged_main += growing ? taken : -taken;
            applied += taken;
        }
        if (applied <= 1e-9) break;
        remaining += growing ? -applied : applied;
        /* Without a cap, every eligible child received its full share.
           Floating-point summation residue must not trigger more full passes. */
        if (!capped) break;
    }
}

static nx_result nx_arrange_node(nx_layout *layout, uint32_t index,
                                 nx_rect rect, uint64_t version) {
    nx_layout_node *node = &layout->nodes[index];
    node->rect = rect;
    node->arranged_version = version;
    const nx_layout_desc *d = &node->desc;
    double inner_x = (double)rect.x + d->border.left + d->padding.left;
    double inner_y = (double)rect.y + d->border.top + d->padding.top;
    double inner_w = fmax(0, (double)rect.width - nx_horizontal(d->border) -
                            nx_horizontal(d->padding));
    double inner_h = fmax(0, (double)rect.height - nx_vertical(d->border) -
                            nx_vertical(d->padding));
    if (d->kind == NX_LAYOUT_SCROLL) {
        nx_scroll_info info = {0};
        nx_result result = nx_make_rect(inner_x, inner_y, inner_w, inner_h,
                                        &info.viewport);
        if (result != NX_OK) return result;
        if (node->first_child != NX_LAYOUT_NO_PARENT) {
            uint32_t i = node->first_child;
            nx_layout_node *child = &layout->nodes[i];
            double span_w = fmax(0, inner_w - nx_horizontal(child->desc.margin));
            double span_h = fmax(0, inner_h - nx_vertical(child->desc.margin));
            double w = (d->scroll_axes & NX_SCROLL_X)
                ? child->desired.width : nx_cross_size(child, span_w, 1);
            double h = (d->scroll_axes & NX_SCROLL_Y)
                ? child->desired.height : nx_cross_size(child, span_h, 0);
            double content_w = w + nx_horizontal(child->desc.margin);
            double content_h = h + nx_vertical(child->desc.margin);
            double max_x = (d->scroll_axes & NX_SCROLL_X) ? fmax(0, content_w - inner_w) : 0;
            double max_y = (d->scroll_axes & NX_SCROLL_Y) ? fmax(0, content_h - inner_h) : 0;
            double offset_x = fmin(node->scroll_x, max_x);
            double offset_y = fmin(node->scroll_y, max_y);
            double x = (d->scroll_axes & NX_SCROLL_X)
                ? inner_x + child->desc.margin.left - offset_x
                : nx_cross_position(child, inner_x + child->desc.margin.left,
                                    span_w, w, 1);
            double y = (d->scroll_axes & NX_SCROLL_Y)
                ? inner_y + child->desc.margin.top - offset_y
                : nx_cross_position(child, inner_y + child->desc.margin.top,
                                    span_h, h, 0);
            if (!nx_floatable(content_w) || !nx_floatable(content_h) ||
                !nx_floatable(max_x) || !nx_floatable(max_y)) return NX_ERROR_LIMIT;
            info.content_size = (nx_size){(float)content_w, (float)content_h};
            info.max_x = (float)max_x;
            info.max_y = (float)max_y;
            info.offset_x = (float)offset_x;
            info.offset_y = (float)offset_y;
            nx_rect child_rect;
            result = nx_make_rect(x, y, w, h, &child_rect);
            if (result != NX_OK) return result;
            result = nx_arrange_node(layout, i, child_rect, version);
            if (result != NX_OK) return result;
        }
        node->scroll_info = info;
    } else if (d->kind == NX_LAYOUT_SPLIT) {
        int horizontal = d->split_axis == NX_SPLIT_X;
        uint32_t first_id = node->first_child;
        uint32_t second_id = layout->nodes[first_id].next_sibling;
        nx_layout_node *first = &layout->nodes[first_id];
        nx_layout_node *second = &layout->nodes[second_id];
        double main_start = horizontal ? inner_x : inner_y;
        double main_available = fmax(0, (horizontal ? inner_w : inner_h) - d->gap);
        double cross_start = horizontal ? inner_y : inner_x;
        double cross_available = horizontal ? inner_h : inner_w;
        nx_axis_spec first_spec = nx_spec_axis(first, horizontal);
        nx_axis_spec second_spec = nx_spec_axis(second, horizontal);
        double first_margin = nx_margin_before(first, horizontal) +
                              nx_margin_after(first, horizontal);
        double second_margin = nx_margin_before(second, horizontal) +
                               nx_margin_after(second, horizontal);
        double first_min = first_spec.minimum + first_margin;
        double second_min = second_spec.minimum + second_margin;
        double first_max = first_spec.maximum + first_margin;
        double second_max = second_spec.maximum + second_margin;
        double used = fmax(first_min + second_min,
                           fmin(main_available, first_max + second_max));
        double lower = fmax(first_min, used - second_max);
        double upper = fmin(first_max, used - second_min);
        double first_outer = fmin(fmax(used * node->split_ratio, lower), upper);
        double second_outer = fmax(0, used - first_outer);
        if (!nx_floatable(first_outer) || !nx_floatable(second_outer))
            return NX_ERROR_LIMIT;
        nx_split_info info = {0};
        nx_result result = horizontal
            ? nx_make_rect(main_start + first_outer, cross_start, d->gap,
                           cross_available, &info.divider)
            : nx_make_rect(cross_start, main_start + first_outer,
                           cross_available, d->gap, &info.divider);
        if (result != NX_OK) return result;
        info.first_extent = (float)first_outer;
        info.second_extent = (float)second_outer;
        info.effective_ratio = (float)(used > 0 ? first_outer / used : node->split_ratio);
        uint32_t children[2] = {first_id, second_id};
        double outer_sizes[2] = {first_outer, second_outer};
        double outer_start = main_start;
        for (unsigned n = 0; n < 2; ++n) {
            nx_layout_node *child = &layout->nodes[children[n]];
            double main_pos = outer_start + nx_margin_before(child, horizontal);
            double main_size = fmax(0, outer_sizes[n] -
                nx_margin_before(child, horizontal) -
                nx_margin_after(child, horizontal));
            double cross_before = nx_margin_before(child, !horizontal);
            double cross_after = nx_margin_after(child, !horizontal);
            double cross_span = fmax(0, cross_available - cross_before - cross_after);
            double cross_size = nx_cross_size(child, cross_span, !horizontal);
            double cross_pos = nx_cross_position(child, cross_start + cross_before,
                                                 cross_span, cross_size, !horizontal);
            nx_rect child_rect;
            result = horizontal
                ? nx_make_rect(main_pos, cross_pos, main_size, cross_size, &child_rect)
                : nx_make_rect(cross_pos, main_pos, cross_size, main_size, &child_rect);
            if (result != NX_OK) return result;
            result = nx_arrange_node(layout, children[n], child_rect, version);
            if (result != NX_OK) return result;
            outer_start += outer_sizes[n] + d->gap;
        }
        node->split_info = info;
    } else if (d->kind == NX_LAYOUT_GRID) {
        nx_grid_fit(node->grid_columns, node->grid_column_count, d->gap, inner_w);
        nx_grid_fit(node->grid_rows, node->grid_row_count, d->line_gap, inner_h);
        for (uint32_t i = node->first_child; i != NX_LAYOUT_NO_PARENT;
             i = layout->nodes[i].next_sibling) {
            nx_layout_node *child = &layout->nodes[i];
            double cell_x = inner_x, cell_y = inner_y;
            for (size_t col = 0; col < child->grid_column; ++col)
                cell_x += node->grid_columns[col].arranged + d->gap;
            for (size_t row = 0; row < child->grid_row; ++row)
                cell_y += node->grid_rows[row].arranged + d->line_gap;
            double cell_w = nx_grid_span_size(node->grid_columns,
                child->grid_column, child->grid_column_span, d->gap, 1);
            double cell_h = nx_grid_span_size(node->grid_rows,
                child->grid_row, child->grid_row_span, d->line_gap, 1);
            double span_w = fmax(0, cell_w - nx_horizontal(child->desc.margin));
            double span_h = fmax(0, cell_h - nx_vertical(child->desc.margin));
            double w = nx_cross_size(child, span_w, 1);
            double h = nx_cross_size(child, span_h, 0);
            double x = nx_cross_position(child, cell_x + child->desc.margin.left,
                                         span_w, w, 1);
            double y = nx_cross_position(child, cell_y + child->desc.margin.top,
                                         span_h, h, 0);
            nx_rect child_rect;
            nx_result result = nx_make_rect(x, y, w, h, &child_rect);
            if (result != NX_OK) return result;
            result = nx_arrange_node(layout, i, child_rect, version);
            if (result != NX_OK) return result;
        }
    } else if (node->first_child == NX_LAYOUT_NO_PARENT) {
        return NX_OK;
    } else if (d->kind == NX_LAYOUT_WRAP_ROW || d->kind == NX_LAYOUT_WRAP_COLUMN) {
        int horizontal = d->kind == NX_LAYOUT_WRAP_ROW;
        double main_start = horizontal ? inner_x : inner_y;
        double cross_cursor = horizontal ? inner_y : inner_x;
        double main_available = horizontal ? inner_w : inner_h;
        uint32_t first = node->first_child;
        while (first != NX_LAYOUT_NO_PARENT) {
            uint32_t end = first;
            double line_cross = 0;
            uint32_t line = layout->nodes[first].wrap_line;
            while (end != NX_LAYOUT_NO_PARENT && layout->nodes[end].wrap_line == line) {
                nx_layout_node *child = &layout->nodes[end];
                line_cross = fmax(line_cross, nx_desired_axis(child, !horizontal) +
                                  nx_margin_before(child, !horizontal) +
                                  nx_margin_after(child, !horizontal));
                end = child->next_sibling;
            }
            nx_distribute_main(layout, node, first, end, main_available, horizontal);
            double cursor = main_start;
            for (uint32_t i = first; i != end; i = layout->nodes[i].next_sibling) {
                nx_layout_node *child = &layout->nodes[i];
                double cross_before = nx_margin_before(child, !horizontal);
                double cross_after = nx_margin_after(child, !horizontal);
                double cross_span = fmax(0, line_cross - cross_before - cross_after);
                double cross_size = nx_cross_size(child, cross_span, !horizontal);
                double cross_pos = nx_cross_position(child, cross_cursor + cross_before,
                                                     cross_span, cross_size, !horizontal);
                cursor += nx_margin_before(child, horizontal);
                nx_rect child_rect;
                nx_result result = horizontal
                    ? nx_make_rect(cursor, cross_pos, child->arranged_main,
                                   cross_size, &child_rect)
                    : nx_make_rect(cross_pos, cursor, cross_size,
                                   child->arranged_main, &child_rect);
                if (result != NX_OK) return result;
                result = nx_arrange_node(layout, i, child_rect, version);
                if (result != NX_OK) return result;
                cursor += child->arranged_main + nx_margin_after(child, horizontal);
                if (child->next_sibling != end) cursor += d->gap;
            }
            cross_cursor += line_cross;
            if (end != NX_LAYOUT_NO_PARENT) cross_cursor += d->line_gap;
            first = end;
        }
    } else if (d->kind == NX_LAYOUT_ROW || d->kind == NX_LAYOUT_COLUMN) {
        int horizontal = d->kind == NX_LAYOUT_ROW;
        nx_distribute_main(layout, node, node->first_child, NX_LAYOUT_NO_PARENT,
                           horizontal ? inner_w : inner_h, horizontal);
        double cursor = horizontal ? inner_x : inner_y;
        double cross_start = horizontal ? inner_y : inner_x;
        double cross_available = horizontal ? inner_h : inner_w;
        for (uint32_t i = node->first_child; i != NX_LAYOUT_NO_PARENT;
             i = layout->nodes[i].next_sibling) {
            nx_layout_node *child = &layout->nodes[i];
            double before = nx_margin_before(child, horizontal);
            double after = nx_margin_after(child, horizontal);
            double cross_before = nx_margin_before(child, !horizontal);
            double cross_after = nx_margin_after(child, !horizontal);
            double cross_span = fmax(0, cross_available - cross_before - cross_after);
            double cross_size = nx_cross_size(child, cross_span, !horizontal);
            double cross_pos = nx_cross_position(child, cross_start + cross_before,
                                                 cross_span, cross_size, !horizontal);
            cursor += before;
            nx_rect child_rect;
            nx_result result = horizontal
                ? nx_make_rect(cursor, cross_pos, child->arranged_main, cross_size, &child_rect)
                : nx_make_rect(cross_pos, cursor, cross_size, child->arranged_main, &child_rect);
            if (result != NX_OK) return result;
            result = nx_arrange_node(layout, i, child_rect, version);
            if (result != NX_OK) return result;
            cursor += child->arranged_main + after + d->gap;
        }
    } else {
        for (uint32_t i = node->first_child; i != NX_LAYOUT_NO_PARENT;
             i = layout->nodes[i].next_sibling) {
            nx_layout_node *child = &layout->nodes[i];
            double span_w = fmax(0, inner_w - nx_horizontal(child->desc.margin));
            double span_h = fmax(0, inner_h - nx_vertical(child->desc.margin));
            double w = nx_cross_size(child, span_w, 1);
            double h = nx_cross_size(child, span_h, 0);
            double x = nx_cross_position(child, inner_x + child->desc.margin.left,
                                         span_w, w, 1);
            double y = nx_cross_position(child, inner_y + child->desc.margin.top,
                                         span_h, h, 0);
            nx_rect child_rect;
            nx_result result = nx_make_rect(x, y, w, h, &child_rect);
            if (result != NX_OK) return result;
            result = nx_arrange_node(layout, i, child_rect, version);
            if (result != NX_OK) return result;
        }
    }
    return NX_OK;
}

nx_result nx_layout_arrange(nx_layout *layout, uint32_t root, nx_rect rect) {
    if (!layout || !nx_valid_rect(rect)) return NX_ERROR_ARGUMENT;
    if (!layout->measured_valid || layout->measured_root != root)
        return NX_ERROR_STATE;
    layout->arranged_valid = 0;
    ++layout->arrange_generation;
    if (!layout->arrange_generation) {
        for (size_t i = 0; i < layout->count; ++i) layout->nodes[i].arranged_version = 0;
        ++layout->arrange_generation;
    }
    nx_result result = nx_arrange_node(layout, root, rect, layout->arrange_generation);
    if (result != NX_OK) return result;
    layout->arranged_valid = 1;
    return NX_OK;
}

nx_result nx_layout_get_rect(const nx_layout *layout, uint32_t node,
                             nx_rect *out_rect) {
    if (!out_rect) return NX_ERROR_ARGUMENT;
    *out_rect = (nx_rect){0};
    if (!layout || node >= layout->count) return NX_ERROR_ARGUMENT;
    if (!layout->arranged_valid ||
        layout->nodes[node].arranged_version != layout->arrange_generation)
        return NX_ERROR_STATE;
    *out_rect = layout->nodes[node].rect;
    return NX_OK;
}

nx_result nx_layout_set_scroll(nx_layout *layout, uint32_t node,
                               float offset_x, float offset_y) {
    if (!layout || node >= layout->count ||
        layout->nodes[node].desc.kind != NX_LAYOUT_SCROLL ||
        !isfinite(offset_x) || !isfinite(offset_y) ||
        offset_x < 0 || offset_y < 0) return NX_ERROR_ARGUMENT;
    layout->nodes[node].scroll_x = offset_x;
    layout->nodes[node].scroll_y = offset_y;
    layout->arranged_valid = 0;
    return NX_OK;
}

nx_result nx_layout_get_scroll_info(const nx_layout *layout, uint32_t node,
                                    nx_scroll_info *out_info) {
    if (!out_info) return NX_ERROR_ARGUMENT;
    *out_info = (nx_scroll_info){0};
    if (!layout || node >= layout->count ||
        layout->nodes[node].desc.kind != NX_LAYOUT_SCROLL) return NX_ERROR_ARGUMENT;
    if (!layout->arranged_valid ||
        layout->nodes[node].arranged_version != layout->arrange_generation)
        return NX_ERROR_STATE;
    *out_info = layout->nodes[node].scroll_info;
    return NX_OK;
}

nx_result nx_layout_set_split(nx_layout *layout, uint32_t node, float ratio) {
    if (!layout || node >= layout->count ||
        layout->nodes[node].desc.kind != NX_LAYOUT_SPLIT ||
        !isfinite(ratio) || ratio < 0 || ratio > 1) return NX_ERROR_ARGUMENT;
    layout->nodes[node].split_ratio = ratio;
    layout->arranged_valid = 0;
    return NX_OK;
}

nx_result nx_layout_get_split_info(const nx_layout *layout, uint32_t node,
                                   nx_split_info *out_info) {
    if (!out_info) return NX_ERROR_ARGUMENT;
    *out_info = (nx_split_info){0};
    if (!layout || node >= layout->count ||
        layout->nodes[node].desc.kind != NX_LAYOUT_SPLIT) return NX_ERROR_ARGUMENT;
    if (!layout->arranged_valid ||
        layout->nodes[node].arranged_version != layout->arrange_generation)
        return NX_ERROR_STATE;
    *out_info = layout->nodes[node].split_info;
    return NX_OK;
}
