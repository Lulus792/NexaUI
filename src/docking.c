#include "nexaui/nexaui.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static size_t dock_node_index(const nx_dock_model *m, uint64_t id) {
    for (size_t i = 0; i < m->node_count; ++i)
        if (m->nodes[i].id == id) return i;
    return SIZE_MAX;
}

static size_t dock_tab_index(const nx_dock_model *m, uint64_t id) {
    for (size_t i = 0; i < m->tab_count; ++i)
        if (m->tabs[i].panel_id == id) return i;
    return SIZE_MAX;
}

nx_result nx_dock_validate(const nx_dock_model *m) {
    if (!m || !m->nodes || !m->tabs || !m->root_id ||
        !m->node_count || !m->tab_count ||
        m->node_count > m->node_capacity || m->tab_count > m->tab_capacity)
        return NX_ERROR_ARGUMENT;
    size_t root = dock_node_index(m, m->root_id);
    if (root == SIZE_MAX || m->nodes[root].parent_id) return NX_ERROR_STATE;
    for (size_t i = 0; i < m->node_count; ++i) {
        const nx_dock_node *n = &m->nodes[i];
        if (!n->id) return NX_ERROR_STATE;
        for (size_t j = 0; j < i; ++j)
            if (m->nodes[j].id == n->id) return NX_ERROR_STATE;
        if (i != root) {
            size_t parent = dock_node_index(m, n->parent_id);
            if (parent == SIZE_MAX || m->nodes[parent].kind != NX_DOCK_SPLIT ||
                (m->nodes[parent].first_id != n->id &&
                 m->nodes[parent].second_id != n->id)) return NX_ERROR_STATE;
            size_t hops = 0, current = i;
            while (current != root) {
                if (++hops >= m->node_count) return NX_ERROR_STATE;
                current = dock_node_index(m, m->nodes[current].parent_id);
                if (current == SIZE_MAX) return NX_ERROR_STATE;
            }
        }
        if (n->kind == NX_DOCK_SPLIT) {
            if ((n->axis != NX_SPLIT_X && n->axis != NX_SPLIT_Y) ||
                !isfinite(n->ratio) || n->ratio < 0 || n->ratio > 1 ||
                !n->first_id || !n->second_id ||
                n->first_id == n->second_id || n->selected_panel_id)
                return NX_ERROR_STATE;
            size_t a = dock_node_index(m, n->first_id);
            size_t b = dock_node_index(m, n->second_id);
            if (a == SIZE_MAX || b == SIZE_MAX ||
                m->nodes[a].parent_id != n->id ||
                m->nodes[b].parent_id != n->id) return NX_ERROR_STATE;
        } else if (n->kind == NX_DOCK_STACK) {
            if (n->axis || n->ratio || n->first_id || n->second_id ||
                !n->selected_panel_id) return NX_ERROR_STATE;
            size_t count = 0;
            int selected = 0;
            for (size_t j = 0; j < m->tab_count; ++j) {
                if (m->tabs[j].stack_id != n->id) continue;
                ++count;
                if (m->tabs[j].panel_id == n->selected_panel_id) selected = 1;
            }
            if (!count || !selected) return NX_ERROR_STATE;
        } else return NX_ERROR_STATE;
    }
    for (size_t i = 0; i < m->tab_count; ++i) {
        const nx_dock_tab *tab = &m->tabs[i];
        if (!tab->panel_id) return NX_ERROR_STATE;
        size_t stack = dock_node_index(m, tab->stack_id);
        if (stack == SIZE_MAX || m->nodes[stack].kind != NX_DOCK_STACK)
            return NX_ERROR_STATE;
        for (size_t j = 0; j < i; ++j)
            if (m->tabs[j].panel_id == tab->panel_id) return NX_ERROR_STATE;
    }
    return NX_OK;
}

nx_result nx_dock_init(nx_dock_model *m, nx_dock_node *nodes,
                       size_t node_capacity, nx_dock_tab *tabs,
                       size_t tab_capacity, uint64_t root_stack_id,
                       uint64_t first_panel_id) {
    if (!m || !nodes || !tabs || !node_capacity || !tab_capacity ||
        !root_stack_id || !first_panel_id) return NX_ERROR_ARGUMENT;
    *m = (nx_dock_model){nodes, 1, node_capacity, tabs, 1, tab_capacity,
                         root_stack_id};
    nodes[0] = (nx_dock_node){0};
    nodes[0].id = root_stack_id;
    nodes[0].kind = NX_DOCK_STACK;
    nodes[0].selected_panel_id = first_panel_id;
    tabs[0] = (nx_dock_tab){first_panel_id, root_stack_id};
    return NX_OK;
}

nx_result nx_dock_add_tab(nx_dock_model *m, uint64_t stack_id,
                          uint64_t panel_id) {
    if (nx_dock_validate(m) != NX_OK || !panel_id ||
        dock_tab_index(m, panel_id) != SIZE_MAX) return NX_ERROR_ARGUMENT;
    size_t stack = dock_node_index(m, stack_id);
    if (stack == SIZE_MAX || m->nodes[stack].kind != NX_DOCK_STACK)
        return NX_ERROR_ARGUMENT;
    if (m->tab_count == m->tab_capacity) return NX_ERROR_LIMIT;
    m->tabs[m->tab_count++] = (nx_dock_tab){panel_id, stack_id};
    m->nodes[stack].selected_panel_id = panel_id;
    return NX_OK;
}

nx_result nx_dock_split(nx_dock_model *m, uint64_t stack_id,
                        uint64_t split_id, uint64_t new_stack_id,
                        uint64_t new_panel_id, nx_split_axis axis,
                        float ratio, int new_stack_after) {
    if (nx_dock_validate(m) != NX_OK || !split_id || !new_stack_id ||
        !new_panel_id || split_id == new_stack_id ||
        (axis != NX_SPLIT_X && axis != NX_SPLIT_Y) ||
        !isfinite(ratio) || ratio < 0 || ratio > 1)
        return NX_ERROR_ARGUMENT;
    size_t old = dock_node_index(m, stack_id);
    if (old == SIZE_MAX || m->nodes[old].kind != NX_DOCK_STACK ||
        dock_node_index(m, split_id) != SIZE_MAX ||
        dock_node_index(m, new_stack_id) != SIZE_MAX ||
        dock_tab_index(m, new_panel_id) != SIZE_MAX)
        return NX_ERROR_ARGUMENT;
    if (m->node_capacity - m->node_count < 2 ||
        m->tab_count == m->tab_capacity) return NX_ERROR_LIMIT;
    uint64_t parent_id = m->nodes[old].parent_id;
    if (parent_id) {
        nx_dock_node *parent = &m->nodes[dock_node_index(m, parent_id)];
        if (parent->first_id == stack_id) parent->first_id = split_id;
        else parent->second_id = split_id;
    } else m->root_id = split_id;
    m->nodes[old].parent_id = split_id;
    nx_dock_node split = {0}, stack = {0};
    split.id = split_id;
    split.parent_id = parent_id;
    split.kind = NX_DOCK_SPLIT;
    split.axis = axis;
    split.ratio = ratio;
    split.first_id = new_stack_after ? stack_id : new_stack_id;
    split.second_id = new_stack_after ? new_stack_id : stack_id;
    stack.id = new_stack_id;
    stack.parent_id = split_id;
    stack.kind = NX_DOCK_STACK;
    stack.selected_panel_id = new_panel_id;
    m->nodes[m->node_count++] = split;
    m->nodes[m->node_count++] = stack;
    m->tabs[m->tab_count++] = (nx_dock_tab){new_panel_id, new_stack_id};
    return NX_OK;
}

nx_result nx_dock_move(nx_dock_model *m, uint64_t panel_id,
                       uint64_t target_stack_id) {
    if (nx_dock_validate(m) != NX_OK) return NX_ERROR_ARGUMENT;
    size_t tab = dock_tab_index(m, panel_id);
    size_t target = dock_node_index(m, target_stack_id);
    if (tab == SIZE_MAX || target == SIZE_MAX ||
        m->nodes[target].kind != NX_DOCK_STACK) return NX_ERROR_ARGUMENT;
    uint64_t source_id = m->tabs[tab].stack_id;
    if (source_id == target_stack_id) return NX_OK;
    size_t source = dock_node_index(m, source_id);
    size_t source_count = 0;
    uint64_t replacement = 0;
    for (size_t i = 0; i < m->tab_count; ++i) {
        if (m->tabs[i].stack_id != source_id) continue;
        ++source_count;
        if (m->tabs[i].panel_id != panel_id && !replacement)
            replacement = m->tabs[i].panel_id;
    }
    if (source_count < 2) return NX_ERROR_STATE;
    if (m->nodes[source].selected_panel_id == panel_id)
        m->nodes[source].selected_panel_id = replacement;
    nx_dock_tab moved = m->tabs[tab];
    moved.stack_id = target_stack_id;
    memmove(&m->tabs[tab], &m->tabs[tab + 1],
            (m->tab_count - tab - 1) * sizeof(m->tabs[0]));
    m->tabs[m->tab_count - 1] = moved;
    m->nodes[target].selected_panel_id = panel_id;
    return NX_OK;
}

nx_result nx_dock_select(nx_dock_model *m, uint64_t panel_id) {
    if (nx_dock_validate(m) != NX_OK) return NX_ERROR_ARGUMENT;
    size_t tab = dock_tab_index(m, panel_id);
    if (tab == SIZE_MAX) return NX_ERROR_ARGUMENT;
    m->nodes[dock_node_index(m, m->tabs[tab].stack_id)].selected_panel_id =
        panel_id;
    return NX_OK;
}

nx_result nx_dock_set_ratio(nx_dock_model *m, uint64_t split_id, float ratio) {
    if (nx_dock_validate(m) != NX_OK || !isfinite(ratio) ||
        ratio < 0 || ratio > 1) return NX_ERROR_ARGUMENT;
    size_t node = dock_node_index(m, split_id);
    if (node == SIZE_MAX || m->nodes[node].kind != NX_DOCK_SPLIT)
        return NX_ERROR_ARGUMENT;
    m->nodes[node].ratio = ratio;
    return NX_OK;
}

static void dock_write32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static void dock_write64(uint8_t *p, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static uint32_t dock_read32(const uint8_t *p) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)p[i] << (i * 8);
    return value;
}
static uint64_t dock_read64(const uint8_t *p) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (i * 8);
    return value;
}

nx_result nx_dock_encode(const nx_dock_model *m, void *bytes,
                         size_t capacity, size_t *out_size) {
    if (!out_size) return NX_ERROR_ARGUMENT;
    *out_size = 0;
    if (nx_dock_validate(m) != NX_OK) return NX_ERROR_ARGUMENT;
    if (m->node_count > UINT32_MAX || m->tab_count > UINT32_MAX ||
        m->node_count > (SIZE_MAX - 24) / 56 ||
        m->tab_count > (SIZE_MAX - 24 - m->node_count * 56) / 16)
        return NX_ERROR_LIMIT;
    size_t required = 24 + m->node_count * 56 + m->tab_count * 16;
    *out_size = required;
    if (!bytes) return NX_OK;
    if (capacity < required) return NX_ERROR_LIMIT;
    uint8_t *p = (uint8_t *)bytes;
    memcpy(p, "NXDK", 4);
    dock_write32(p + 4, 1);
    dock_write32(p + 8, (uint32_t)m->node_count);
    dock_write32(p + 12, (uint32_t)m->tab_count);
    dock_write64(p + 16, m->root_id);
    p += 24;
    for (size_t i = 0; i < m->node_count; ++i, p += 56) {
        const nx_dock_node *n = &m->nodes[i];
        dock_write64(p, n->id);
        dock_write64(p + 8, n->parent_id);
        dock_write64(p + 16, n->first_id);
        dock_write64(p + 24, n->second_id);
        dock_write64(p + 32, n->selected_panel_id);
        dock_write32(p + 40, (uint32_t)n->kind);
        dock_write32(p + 44, (uint32_t)n->axis);
        uint32_t ratio_bits = 0;
        memcpy(&ratio_bits, &n->ratio, sizeof(ratio_bits));
        dock_write32(p + 48, ratio_bits);
        dock_write32(p + 52, 0);
    }
    for (size_t i = 0; i < m->tab_count; ++i, p += 16) {
        dock_write64(p, m->tabs[i].panel_id);
        dock_write64(p + 8, m->tabs[i].stack_id);
    }
    return NX_OK;
}

nx_result nx_dock_decode(nx_dock_model *m, const void *bytes, size_t length) {
    if (!m || !m->nodes || !m->tabs || !bytes) return NX_ERROR_ARGUMENT;
    m->node_count = m->tab_count = 0;
    m->root_id = 0;
    if (length < 24) return NX_ERROR_ARGUMENT;
    const uint8_t *p = (const uint8_t *)bytes;
    if (memcmp(p, "NXDK", 4) || dock_read32(p + 4) != 1)
        return NX_ERROR_ARGUMENT;
    size_t nodes = dock_read32(p + 8), tabs = dock_read32(p + 12);
    if (!nodes || !tabs || nodes > m->node_capacity || tabs > m->tab_capacity ||
        nodes > (SIZE_MAX - 24) / 56 ||
        tabs > (SIZE_MAX - 24 - nodes * 56) / 16)
        return NX_ERROR_LIMIT;
    if (length != 24 + nodes * 56 + tabs * 16) return NX_ERROR_ARGUMENT;
    m->root_id = dock_read64(p + 16);
    p += 24;
    for (size_t i = 0; i < nodes; ++i, p += 56) {
        nx_dock_node *n = &m->nodes[i];
        *n = (nx_dock_node){0};
        n->id = dock_read64(p);
        n->parent_id = dock_read64(p + 8);
        n->first_id = dock_read64(p + 16);
        n->second_id = dock_read64(p + 24);
        n->selected_panel_id = dock_read64(p + 32);
        n->kind = (nx_dock_kind)dock_read32(p + 40);
        n->axis = (nx_split_axis)dock_read32(p + 44);
        uint32_t bits = dock_read32(p + 48);
        memcpy(&n->ratio, &bits, sizeof(bits));
        if (dock_read32(p + 52)) goto invalid;
    }
    for (size_t i = 0; i < tabs; ++i, p += 16)
        m->tabs[i] = (nx_dock_tab){dock_read64(p), dock_read64(p + 8)};
    m->node_count = nodes;
    m->tab_count = tabs;
    if (nx_dock_validate(m) == NX_OK) return NX_OK;
invalid:
    m->node_count = m->tab_count = 0;
    m->root_id = 0;
    return NX_ERROR_ARGUMENT;
}
