/* Reuse only the independent OS-peer transport helpers, never toolkit objects. */
#define main async_example_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main

static char paths[32][257];
static size_t path_count = 32;
static int load_paths(const char *file) {
    FILE *f = fopen(file, "rb"); if (!f) return 0;
    char line[259]; size_t count = 0; int okay = 1;
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        if (count == 32 || n < 2 || line[n - 1] != '\n' || n > 257) { okay = 0; break; }
        line[--n] = 0; memcpy(paths[count++], line, n + 1);
    }
    if (ferror(f) || count != 32 || strlen(paths[31]) != 256) okay = 0;
    if (fclose(f)) okay = 0;
    return okay;
}
static int collect(AtspiAccessible *node, unsigned depth, unsigned *seen, unsigned *nodes) {
    if (!node || depth > 16 || ++*nodes > 512) return 0;
    atspi_accessible_clear_cache(node); GError *error = NULL;
    AtspiRole role = atspi_accessible_get_role(node, &error);
    char *name = !error ? atspi_accessible_get_name(node, &error) : NULL;
    int okay = !error && name;
    if (okay && role == ATSPI_ROLE_TREE_ITEM && strcmp(name, branch_name())) {
        size_t i = 0; while (i < path_count && strcmp(name, paths[i])) ++i;
        if (i == path_count || (*seen & (1u << i))) okay = 0;
        else *seen |= 1u << i;
    }
    g_free(name);
    int count = okay ? atspi_accessible_get_child_count(node, &error) : 0;
    okay = okay && !error && count >= 0 && count <= 128;
    for (int i = 0; okay && i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        okay = !error && child && collect(child, depth + 1, seen, nodes);
        if (child) g_object_unref(child);
    }
    if (error) g_error_free(error);
    return okay;
}
static int phase(AtspiAccessible *app, int value, int open) {
    AtspiAccessible *branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM);
    int okay = branch && state(branch, ATSPI_STATE_EXPANDED) == open;
    if (branch) g_object_unref(branch);
    AtspiAccessible *status = lookup(app, status_name(value), ATSPI_ROLE_STATUS_BAR);
    if (open) okay = okay && status && state(status, ATSPI_STATE_BUSY) == (value == 1);
    else okay = okay && !status && !scan_failed;
    if (status) g_object_unref(status);
    AtspiAccessible *retry = lookup(app, retry_name(), ATSPI_ROLE_PUSH_BUTTON);
    okay = okay && ((open && value == 2) ? retry != NULL : !retry && !scan_failed);
    if (retry) g_object_unref(retry);
    unsigned seen = 0, nodes = 0;
    unsigned expected = path_count == 32 ? 0xffffffffu : (1u << path_count) - 1u;
    okay = okay && collect(app, 0, &seen, &nodes) && seen == (open && value == 3 ? expected : 0u);
    return okay;
}
/* Full containment is required whenever the row fits. An over-height row must
   expose its complete width, start at the viewport top and remain focused while
   actual PageDown input reaches its bottom. Never accept a merely partial row
   as proof that the full label can be read. */
static int read_edge(AtspiAccessible *app, AtspiAccessible *node, int bottom, int *pages) {
    AtspiAccessible *area = lookup(app, german ? "Unterelemente" : "Children", ATSPI_ROLE_SCROLL_PANE);
    AtspiComponent *a = node ? atspi_accessible_get_component_iface(node) : NULL;
    AtspiComponent *b = area ? atspi_accessible_get_component_iface(area) : NULL;
    GError *error = NULL;
    AtspiRect *inner = a ? atspi_component_get_extents(a, ATSPI_COORD_TYPE_WINDOW, &error) : NULL;
    AtspiRect *outer = b && !error ? atspi_component_get_extents(b, ATSPI_COORD_TYPE_WINDOW, &error) : NULL;
    int result = 0;
    if (!error && inner && outer && inner->width > 0 && inner->height > 0 && outer->height > 1 &&
        inner->x >= outer->x && (long long)inner->x + inner->width <= (long long)outer->x + outer->width) {
        if (inner->height <= outer->height) {
            result = inner->y >= outer->y && (long long)inner->y + inner->height <= (long long)outer->y + outer->height;
        } else if (bottom ? (long long)inner->y + inner->height == (long long)outer->y + outer->height : inner->y == outer->y) {
            result = 2;
            if (pages) *pages = 2 * (inner->height / outer->height + 1);
        }
    }
    if (error) g_error_free(error);
    if (inner) g_boxed_free(ATSPI_TYPE_RECT, inner);
    if (outer) g_boxed_free(ATSPI_TYPE_RECT, outer);
    if (a) g_object_unref(a);
    if (b) g_object_unref(b);
    if (area) g_object_unref(area);
    return result;
}
