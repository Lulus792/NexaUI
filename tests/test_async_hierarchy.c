#include "../examples/30_async_hierarchy/hierarchy_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
static int build(nx_context *ctx, ah_view *v, int *intent) {
    for (unsigned i = 0; i < 3; ++i) {
        int redraw = 0;
        if (nx_begin_frame(ctx, 360, 320, 0) != NX_OK ||
            ah_build(v, ctx, 0, (nx_rect){0, 0, 360, 320}, intent, &redraw) != NX_OK || nx_end_frame(ctx) != NX_OK) return 0;
        if (!redraw) return 1;
    }
    return 0;
}
static int semantic(nx_context *ctx, uint64_t id, nx_semantic_role role, unsigned state, int expected) {
    uint32_t index; nx_semantic_view view;
    return nx_semantics_find(ctx, id, &index) == NX_OK && nx_semantics_get(ctx, index, &view) == NX_OK &&
        view.role == role && !!(view.state & state) == !!expected;
}
static int absent(nx_context *ctx, uint64_t id) {
    uint32_t index; return nx_semantics_find(ctx, id, &index) == NX_ERROR_ARGUMENT;
}
static int command(nx_sim_channel *channel, nx_sim_message *m, ah_key *key) {
    if (nx_sim_channel_take_command(channel, m) != NX_OK || !m->ticket || m->bytes != sizeof(*key)) return 0;
    memcpy(key, m->data, sizeof(*key)); return 1;
}
static void free_payload(void *user, void *data, size_t bytes) { (void)user; (void)bytes; free(data); }
static int complete(nx_sim_channel *channel, nx_sim_message *m, ah_key key, nx_result status, size_t count) {
    ah_response *r = calloc(1, sizeof(*r)); if (!r) return 0;
    r->key = key; r->count = count;
    for (size_t i = 0; i < count && i < AH_CHILD_LIMIT; ++i) {
        r->children[i].id = 100 + i;
        (void)snprintf(r->children[i].name, AH_NAME_BYTES, "Child %zu", i);
    }
    uint64_t ticket = m->ticket; nx_sim_message_dispose(m);
    nx_result posted = nx_sim_channel_post_completion(channel, ticket, status, r, sizeof(*r), free_payload, NULL);
    if (posted != NX_OK) free(r);
    return posted == NX_OK;
}
static int observe(nx_sim_channel *channel, ah_view *v, int expected) {
    nx_sim_message m = {0};
    if (nx_sim_channel_take_completion(channel, &m) != NX_OK || !m.ticket) return 0;
    int accepted = ah_observe(v, &m); nx_sim_message_dispose(&m);
    return accepted == expected;
}
static int test_states(void) {
    nx_context *ctx = NULL; nx_sim_channel *channel = NULL; ah_view v; int intent;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK && nx_sim_channel_create(2, NULL, &channel) == NX_OK);
    CHECK(ah_init(&v, channel, NULL, 1) == NX_OK && build(ctx, &v, &intent));
    CHECK(intent == AH_INTENT_NONE && v.phase == AH_UNQUERIED && absent(ctx, AH_STATUS));
    CHECK(nx_semantics_request_action(ctx, AH_BRANCH, NX_ACTION_EXPAND) == NX_OK);
    CHECK(build(ctx, &v, &intent) && intent == AH_INTENT_LOAD && !v.ticket);
    CHECK(ah_request(&v) == NX_OK && build(ctx, &v, &intent));
    CHECK(intent == AH_INTENT_NONE && v.phase == AH_LOADING && semantic(ctx, AH_STATUS, NX_ROLE_STATUS, NX_SEM_BUSY, 1));
    CHECK(absent(ctx, AH_RETRY) && absent(ctx, 100));
    nx_sim_message first = {0}; ah_key key;
    CHECK(command(channel, &first, &key));
    CHECK(complete(channel, &first, key, NX_ERROR_PLATFORM, 0) && observe(channel, &v, 1));
    CHECK(build(ctx, &v, &intent) && v.phase == AH_FAILED && intent == AH_INTENT_NONE);
    CHECK(semantic(ctx, AH_STATUS, NX_ROLE_STATUS, NX_SEM_BUSY, 0) && semantic(ctx, AH_RETRY, NX_ROLE_BUTTON, NX_SEM_DISABLED, 0));
    CHECK(nx_focus_set(ctx, AH_RETRY) == NX_OK && nx_semantics_request_action(ctx, AH_RETRY, NX_ACTION_PRESS) == NX_OK);
    CHECK(build(ctx, &v, &intent) && intent == AH_INTENT_RETRY && v.phase == AH_FAILED);
    CHECK(ah_request(&v) == NX_OK && build(ctx, &v, &intent) && nx_focus_id(ctx) == AH_BRANCH);
    CHECK(command(channel, &first, &key) && complete(channel, &first, key, NX_OK, 2) && observe(channel, &v, 1));
    CHECK(build(ctx, &v, &intent) && v.phase == AH_READY && v.count == 2);
    CHECK(semantic(ctx, 100, NX_ROLE_TREE_ITEM, NX_SEM_EXPANDED, 0));
    CHECK(nx_semantics_request_action(ctx, 101, NX_ACTION_SELECT) == NX_OK && build(ctx, &v, &intent));
    CHECK(v.selected == 101 && nx_focus_set(ctx, 101) == NX_OK);
    CHECK(nx_semantics_request_action(ctx, AH_BRANCH, NX_ACTION_COLLAPSE) == NX_OK);
    CHECK(build(ctx, &v, &intent) && nx_focus_id(ctx) == AH_BRANCH && v.selected == 101 && absent(ctx, 101));
    CHECK(nx_semantics_request_action(ctx, AH_BRANCH, NX_ACTION_EXPAND) == NX_OK);
    CHECK(build(ctx, &v, &intent) && intent == AH_INTENT_NONE && v.selected == 101 && !v.ticket);
    CHECK(nx_semantics_request_action(ctx, AH_REFRESH, NX_ACTION_PRESS) == NX_OK && build(ctx, &v, &intent) && intent == AH_INTENT_REFRESH);
    CHECK(ah_request(&v) == NX_OK && build(ctx, &v, &intent));
    CHECK(command(channel, &first, &key) && complete(channel, &first, key, NX_OK, 0) && observe(channel, &v, 1));
    CHECK(build(ctx, &v, &intent) && v.phase == AH_EMPTY && v.selected == AH_BRANCH && absent(ctx, 100));
    ah_destroy(&v); nx_context_destroy(ctx); CHECK(nx_sim_channel_destroy(channel) == NX_OK);
    return 0;
}
static int test_stale_and_capacity(void) {
    nx_sim_channel *channel = NULL; ah_view v;
    CHECK(nx_sim_channel_create(2, NULL, &channel) == NX_OK && ah_init(&v, channel, NULL, 41) == NX_OK);
    v.expanded = 1; CHECK(ah_request(&v) == NX_OK);
    nx_sim_message old = {0}, current = {0}; ah_key old_key, new_key;
    CHECK(command(channel, &old, &old_key));
    ah_collapse(&v); v.expanded = 1; CHECK(ah_request(&v) == NX_OK && command(channel, &current, &new_key));
    CHECK(old_key.generation != new_key.generation && old.ticket != current.ticket);
    CHECK(complete(channel, &current, new_key, NX_OK, 2) && observe(channel, &v, 1));
    uint64_t revision = v.revision;
    CHECK(complete(channel, &old, old_key, NX_ERROR_PLATFORM, 0) && observe(channel, &v, 0));
    CHECK(v.phase == AH_READY && v.count == 2 && v.revision == revision);
    CHECK(ah_request(&v) == NX_OK && command(channel, &old, &old_key));
    CHECK(ah_request(&v) == NX_OK && command(channel, &current, &new_key));
    CHECK(ah_request(&v) == NX_ERROR_LIMIT && v.phase == AH_FAILED && !v.ticket);
    CHECK(complete(channel, &old, old_key, NX_OK, 1) && complete(channel, &current, new_key, NX_OK, 1));
    CHECK(observe(channel, &v, 0) && observe(channel, &v, 0));
    CHECK(ah_request(&v) == NX_OK && command(channel, &old, &old_key));
    ah_destroy(&v); CHECK(ah_init(&v, channel, NULL, 42) == NX_OK); v.expanded = 1;
    CHECK(ah_request(&v) == NX_OK && command(channel, &current, &new_key));
    CHECK(complete(channel, &old, old_key, NX_OK, 2) && observe(channel, &v, 0) && v.phase == AH_LOADING);
    CHECK(complete(channel, &current, new_key, NX_OK, 0) && observe(channel, &v, 1) && v.phase == AH_EMPTY);
    ah_destroy(&v); CHECK(nx_sim_channel_destroy(channel) == NX_OK);
    return 0;
}
static int test_malformed_and_exhaustion(void) {
    nx_sim_channel *channel = NULL; ah_view v;
    CHECK(nx_sim_channel_create(1, NULL, &channel) == NX_OK && ah_init(&v, channel, NULL, 9) == NX_OK);
    for (unsigned variant = 0; variant < 4; ++variant) {
        v.expanded = 1; CHECK(ah_request(&v) == NX_OK);
        nx_sim_message cmd = {0}; ah_key key; CHECK(command(channel, &cmd, &key));
        ah_response *r = calloc(1, sizeof(*r)); CHECK(r); r->key = key; r->count = 2;
        r->children[0] = (ah_child){100, "One"}; r->children[1] = (ah_child){101, "Two"};
        if (variant == 0) ++r->key.view;
        if (variant == 1) r->count = AH_CHILD_LIMIT + 1;
        if (variant == 2) r->children[1].id = 100;
        if (variant == 3) memset(r->children[0].name, 'x', AH_NAME_BYTES);
        uint64_t ticket = cmd.ticket; nx_sim_message_dispose(&cmd);
        CHECK(nx_sim_channel_post_completion(channel, ticket, NX_OK, r, sizeof(*r), free_payload, NULL) == NX_OK);
        CHECK(observe(channel, &v, 1) && v.phase == AH_FAILED && v.error == NX_ERROR_ARGUMENT && !v.ticket);
    }
    v.generation = UINT64_MAX;
    CHECK(ah_request(&v) == NX_ERROR_LIMIT && v.exhausted && !v.ticket);
    CHECK(!strcmp(ah_status(&v), "Reopen this view to continue"));
    nx_sim_message m = {0}; CHECK(nx_sim_channel_take_command(channel, &m) == NX_OK && !m.ticket);
    ah_destroy(&v); CHECK(nx_sim_channel_destroy(channel) == NX_OK);
    return 0;
}
static int test_reveal_and_frame_failure(void) {
    nx_context *ctx = NULL; nx_sim_channel *channel = NULL; ah_view v; int intent;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK && nx_sim_channel_create(1, NULL, &channel) == NX_OK);
    CHECK(ah_init(&v, channel, NULL, 80) == NX_OK); v.expanded = 1;
    int redraw = 0; CHECK(nx_begin_frame(ctx, 120, 100, 0) == NX_OK);
    CHECK(ah_build(&v, ctx, 0, (nx_rect){0, 0, 120, 100}, &intent, &redraw) == NX_ERROR_ARGUMENT);
    CHECK(v.phase == AH_UNQUERIED && !v.ticket && nx_end_frame(ctx) == NX_OK);
    nx_sim_message empty = {0}; CHECK(nx_sim_channel_take_command(channel, &empty) == NX_OK && !empty.ticket);
    CHECK(build(ctx, &v, &intent) && intent == AH_INTENT_LOAD && ah_request(&v) == NX_OK);
    nx_sim_message cmd = {0}; ah_key key;
    CHECK(command(channel, &cmd, &key) && complete(channel, &cmd, key, NX_OK, AH_CHILD_LIMIT) && observe(channel, &v, 1));
    CHECK(build(ctx, &v, &intent));
    uint64_t last = 100 + AH_CHILD_LIMIT - 1;
    CHECK(nx_focus_set(ctx, last) == NX_OK && build(ctx, &v, &intent));
    uint32_t index; nx_semantic_view row, area;
    CHECK(nx_semantics_find(ctx, last, &index) == NX_OK && nx_semantics_get(ctx, index, &row) == NX_OK);
    CHECK(nx_semantics_find(ctx, AH_SCROLL, &index) == NX_OK && nx_semantics_get(ctx, index, &area) == NX_OK);
    CHECK(row.bounds.y >= area.bounds.y && row.bounds.y + row.bounds.height <= area.bounds.y + area.bounds.height);
    CHECK(nx_focus_id(ctx) == last && v.scroll_y > 0);
    CHECK(ah_request(&v) == NX_OK && build(ctx, &v, &intent) && nx_focus_id(ctx) == AH_BRANCH);
    CHECK(command(channel, &cmd, &key) && complete(channel, &cmd, key, NX_ERROR_PLATFORM, 0) && observe(channel, &v, 1));
    ah_destroy(&v); nx_context_destroy(ctx); CHECK(nx_sim_channel_destroy(channel) == NX_OK);
    return 0;
}
int main(void) {
    if (test_states() || test_stale_and_capacity() || test_malformed_and_exhaustion() || test_reveal_and_frame_failure()) return 1;
    puts("async hierarchy core tests passed"); return 0;
}
