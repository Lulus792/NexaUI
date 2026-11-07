/* Reuse the immutable original independent peer's native helpers and assertions.
   Its entry point remains compiled, but this executable runs the new workflow. */
#define main original_files_peer_main
#include "files_view_atspi.c"
#undef main
#include <stdint.h>

static int nav_stage(unsigned number, const char *name) {
    printf("STEP %u %s\n", number, name);
    char line[64]; unsigned actual = 999;
    return fgets(line, sizeof(line), stdin) && sscanf(line, "CHECK %u", &actual) == 1 && actual == number;
}

static int nav_focus(AtspiAccessible *app, const char *name) {
    AtspiAccessible *row = await(app, name, ATSPI_ROLE_LIST_ITEM);
    if (!row) return 0;
    GError *error = NULL;
    AtspiComponent *component = atspi_accessible_get_component_iface(row);
    int okay = component && atspi_component_grab_focus(component, &error) && !error;
    if (error) g_error_free(error);
    if (component) g_object_unref(component);
    if (okay) okay = expect_state(row, ATSPI_STATE_FOCUSED);
    g_object_unref(row);
    return okay;
}

static int nav_choose(AtspiAccessible *app, const char *name) {
    AtspiAccessible *list = await(app, "Entries", ATSPI_ROLE_LIST);
    if (!list) return 0;
    GError *error = NULL;
    int count = atspi_accessible_get_child_count(list, &error), index = -1;
    if (error || count < 1 || count > 128) {
        if (error) g_error_free(error);
        g_object_unref(list); return 0;
    }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *row = atspi_accessible_get_child_at_index(list, i, &error);
        char *value = row && !error ? atspi_accessible_get_name(row, &error) : NULL;
        if (!error && value && !strcmp(value, name)) index = i;
        g_free(value); if (row) g_object_unref(row);
        if (error) { g_error_free(error); error = NULL; break; }
        if (index >= 0) break;
    }
    AtspiSelection *selection = atspi_accessible_get_selection_iface(list);
    int okay = index >= 0 && selection && atspi_selection_select_child(selection, index, &error) && !error;
    if (error) g_error_free(error);
    if (selection) g_object_unref(selection);
    g_object_unref(list);
    double deadline = now() + 3;
    while (okay && now() < deadline && !selected(app, name)) pump();
    return okay && selected(app, name) && nav_focus(app, name);
}

static int nav_label(AtspiAccessible *app, const char *name) {
    /* A sidebar's child label can already have this name before Enter is
       handled. Only a direct child of the actual Files window is a heading
       or status label. Wait for that observed native state, not the duplicate. */
    double deadline = now() + 3;
    do {
        /* The semantic window root is a PANEL below the OS frame. */
        AtspiAccessible *window = lookup(app, "Files", ATSPI_ROLE_PANEL);
        GError *error = NULL;
        int count = window ? atspi_accessible_get_child_count(window, &error) : 0;
        int okay = 0;
        for (int i = 0; !error && i < count && count <= 512; ++i) {
            AtspiAccessible *node = atspi_accessible_get_child_at_index(window, i, &error);
            AtspiRole role = node && !error ? atspi_accessible_get_role(node, &error) : ATSPI_ROLE_INVALID;
            char *value = node && !error ? atspi_accessible_get_name(node, &error) : NULL;
            okay = !error && role == ATSPI_ROLE_LABEL && value && !strcmp(value, name);
            g_free(value);
            if (node) g_object_unref(node);
            if (okay) break;
        }
        if (error) g_error_free(error);
        if (window) g_object_unref(window);
        if (okay) return 1;
        pump();
    } while (now() < deadline);
    return 0;
}

static int nav_absent(AtspiAccessible *app, const char *name) {
    AtspiAccessible *list = lookup(app, "Entries", ATSPI_ROLE_LIST);
    if (!list) return 0;
    GError *error = NULL;
    int count = atspi_accessible_get_child_count(list, &error);
    int okay = !error && count >= 0 && count <= 128;
    for (int i = 0; okay && i < count; ++i) {
        AtspiAccessible *row = atspi_accessible_get_child_at_index(list, i, &error);
        char *value = row && !error ? atspi_accessible_get_name(row, &error) : NULL;
        okay = !error && value && strcmp(value, name);
        g_free(value);
        if (row) g_object_unref(row);
    }
    if (error) g_error_free(error);
    g_object_unref(list);
    return okay;
}

static int nav_visible_selected(AtspiAccessible *app, const char *name) {
    AtspiAccessible *row = await(app, name, ATSPI_ROLE_LIST_ITEM);
    AtspiAccessible *list = lookup(app, "Entries", ATSPI_ROLE_LIST);
    AtspiComponent *a = row ? atspi_accessible_get_component_iface(row) : NULL;
    AtspiComponent *b = list ? atspi_accessible_get_component_iface(list) : NULL;
    GError *error = NULL;
    AtspiRect *inner = a ? atspi_component_get_extents(a, ATSPI_COORD_TYPE_WINDOW, &error) : NULL;
    AtspiRect *outer = b && !error ? atspi_component_get_extents(b, ATSPI_COORD_TYPE_WINDOW, &error) : NULL;
    int count = list && !error ? atspi_accessible_get_child_count(list, &error) : 0;
    int okay = !error && inner && outer && count > 0 && count <= 128 &&
        inner->width > 0 && inner->height > 0 && inner->x >= outer->x && inner->y >= outer->y &&
        (int64_t)inner->x + inner->width <= (int64_t)outer->x + outer->width &&
        (int64_t)inner->y + inner->height <= (int64_t)outer->y + outer->height &&
        state(row, ATSPI_STATE_FOCUSED) && state(row, ATSPI_STATE_SELECTED) && selected(app, name);
    if (okay) printf("VISIBLE %s children=%d row=%d,%d,%d,%d viewport=%d,%d,%d,%d\n",
        name, count, inner->x, inner->y, inner->width, inner->height,
        outer->x, outer->y, outer->width, outer->height);
    if (error) g_error_free(error);
    if (inner) g_boxed_free(ATSPI_TYPE_RECT, inner);
    if (outer) g_boxed_free(ATSPI_TYPE_RECT, outer);
    if (a) g_object_unref(a);
    if (b) g_object_unref(b);
    if (row) g_object_unref(row);
    if (list) g_object_unref(list);
    return okay;
}

int main(int argc, char **argv) {
    int result = 1, initialized = 0;
    AtspiAccessible *desktop = NULL, *app = NULL, *old_seed = NULL, *old_place = NULL;
    AtspiAccessible *first = NULL, *old_renamed = NULL, *fresh = NULL;
    CHECK(argc == 2);
    char *end = NULL; unsigned long pid = strtoul(argv[1], &end, 10);
    CHECK(pid && end && !*end && pid <= 2147483647UL);
    CHECK(atspi_init() == 0); initialized = 1; atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop);
    app = target(desktop, pid); CHECK(app);
    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(press(app, "List"));
    CHECK(nav_choose(app, "existing"));
    old_seed = await(app, "seed.txt", ATSPI_ROLE_LIST_ITEM);
    old_place = await(app, "existing", ATSPI_ROLE_PUSH_BUTTON);
    CHECK(old_seed && old_place);

    CHECK(nav_stage(0, "enter-existing"));
    CHECK(nav_label(app, "existing") && nav_choose(app, "nested"));
    CHECK(expect_state(old_seed, ATSPI_STATE_DEFUNCT) && !activate(old_seed));
    CHECK(expect_state(old_place, ATSPI_STATE_DEFUNCT) && !activate(old_place));
    puts("ACK 0");

    CHECK(nav_stage(1, "enter-nested"));
    CHECK(nav_label(app, "nested") && nav_choose(app, "full"));
    CHECK(expect_state(old_seed, ATSPI_STATE_DEFUNCT) && !activate(old_seed));
    CHECK(selected(app, "full")); puts("ACK 1");

    CHECK(nav_stage(2, "reject-4097"));
    CHECK(nav_label(app, "Folder/depth limit; view retained") && nav_label(app, "nested"));
    CHECK(selected(app, "full") && nav_absent(app, "item-0000")); puts("ACK 2");

    CHECK(nav_stage(3, "admit-4096"));
    CHECK(nav_label(app, "full") && nav_choose(app, "item-0000"));
    first = await(app, "item-0000", ATSPI_ROLE_LIST_ITEM); CHECK(first);
    puts("ACK 3");

    CHECK(nav_stage(4, "reveal-last"));
    CHECK(nav_visible_selected(app, "item-4095"));
    CHECK(expect_state(first, ATSPI_STATE_DEFUNCT) && !activate(first)); puts("ACK 4");

    CHECK(nav_stage(5, "up-to-nested"));
    CHECK(nav_label(app, "nested") && selected(app, "full")); puts("ACK 5");

    CHECK(nav_stage(6, "rename-held-parent"));
    CHECK(press(app, "existing"));
    CHECK(nav_label(app, "existing") && nav_choose(app, "nested"));
    CHECK(nav_absent(app, "decoy.txt")); puts("ACK 6");

    CHECK(nav_stage(7, "held-root"));
    CHECK(press(app, "documents"));
    CHECK(nav_label(app, "documents") && nav_choose(app, "existing"));
    CHECK(expect_state(old_place, ATSPI_STATE_DEFUNCT) && !activate(old_place));
    CHECK(selected(app, "existing")); puts("ACK 7");

    CHECK(nav_stage(8, "fresh-decoy"));
    CHECK(nav_label(app, "existing") && selected(app, "decoy.txt") && nav_absent(app, "nested"));
    puts("ACK 8");

    CHECK(nav_stage(9, "up-and-select-renamed"));
    CHECK(nav_label(app, "documents") && nav_choose(app, "renamed"));
    old_renamed = await(app, "renamed", ATSPI_ROLE_LIST_ITEM); CHECK(old_renamed); puts("ACK 9");

    CHECK(nav_stage(10, "reject-replaced-inode"));
    CHECK(nav_label(app, "Folder or file changed; Ctrl+R refreshes") && nav_label(app, "documents"));
    CHECK(selected(app, "renamed") && nav_absent(app, "replacement.txt") && nav_absent(app, "moved"));
    puts("ACK 10");

    CHECK(nav_stage(11, "refresh-original-model"));
    CHECK(nav_label(app, "documents") && nav_choose(app, "renamed"));
    CHECK(expect_state(old_renamed, ATSPI_STATE_DEFUNCT) && !activate(old_renamed));
    fresh = await(app, "renamed", ATSPI_ROLE_LIST_ITEM); CHECK(fresh);
    CHECK(strcmp(ATSPI_OBJECT(fresh)->path, ATSPI_OBJECT(old_renamed)->path)); puts("ACK 11");

    CHECK(nav_stage(12, "fresh-replacement"));
    CHECK(nav_label(app, "renamed") && selected(app, "replacement.txt") && nav_absent(app, "nested"));
    puts("ACK 12");

    CHECK(nav_stage(13, "return-root"));
    CHECK(nav_label(app, "documents"));
    g_object_unref(fresh);
    fresh = await(app, "Up", ATSPI_ROLE_PUSH_BUTTON);
    /* This original app keeps Up enabled at root and reports its model error. */
    CHECK(fresh && state(fresh, ATSPI_STATE_ENABLED) && activate(fresh));
    CHECK(nav_label(app, "At the selected root")); puts("ACK 13");

    CHECK(nav_stage(14, "root-boundary"));
    CHECK(nav_label(app, "At the selected root") && nav_label(app, "documents"));
    CHECK(nav_absent(app, "replacement.txt")); puts("ACK 14");
    puts("FILES_NAVIGATION_VERIFIED"); result = 0;
done:
    if (fresh) g_object_unref(fresh);
    if (old_renamed) g_object_unref(old_renamed);
    if (first) g_object_unref(first);
    if (old_place) g_object_unref(old_place);
    if (old_seed) g_object_unref(old_seed);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (initialized) atspi_exit();
    return result;
}
