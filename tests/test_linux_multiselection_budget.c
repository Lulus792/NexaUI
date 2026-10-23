/* Compile the actual private native adapter into this static component test.
   Its public symbols satisfy the SDK archive's native references; no second
   adapter or test-only SDK export is linked. The independent OS-client suite
   separately verifies IPC. This test isolates the 8 MiB native payload bound
   without emitting hundreds of thousands of registry cache events. */
#define _POSIX_C_SOURCE 200809L
#include "../platform/linux/accessibility.c"
#include <unistd.h>
#define LARGE_MEMBERS (NX_AX_NODES-3u)

#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
typedef struct budget_case {
    nx_window *window;
    nx_context *contexts[4];
    nx_selection_model *model;
    size_t depth,calls,bytes[3];
    int armed,okay,rejected;
} budget_case;
static int descend(budget_case *);
static void *allocate(void *user,size_t bytes) {
    budget_case *c=user;++c->calls;
    if(c->armed){c->armed=0;if(descend(c))c->okay=0;}
    return malloc(bytes);
}
static void deallocate(void *user,void *p,size_t bytes){(void)user;(void)bytes;free(p);}
static int context(budget_case *c,size_t at) {
    nx_context_desc d={0};d.allocator=(nx_allocator){c,allocate,deallocate};
    CHECK(nx_context_create(&d,&c->contexts[at])==NX_OK);return 0;
}
static int frame(budget_case *c,size_t at) {
    nx_context *ctx=c->contexts[at];CHECK(nx_begin_frame(ctx,640,480,0)==NX_OK);
    nx_semantic_desc d={0};d.id=1;d.role=NX_ROLE_WINDOW;d.name=(nx_utf8_span){"Budget",6};CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    d.id=100;d.parent_id=1;d.role=NX_ROLE_LIST;CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(ctx,100,c->model,0)==NX_OK);
    d.id=110;d.parent_id=100;d.role=NX_ROLE_PANEL;CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    for(uint64_t i=0;i<LARGE_MEMBERS;++i) {
        d.id=1000+i;d.parent_id=110;d.role=NX_ROLE_LIST_ITEM;
        CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && nx_selection_bind_item(ctx,100,d.id,1)==NX_OK);
    }
    CHECK(nx_end_frame(ctx)==NX_OK);
    CHECK(nx_window_publish_semantics(c->window,ctx)==NX_ERROR_UNSUPPORTED);
    return 0;
}
static nx_linux_ax *bridge(budget_case *c) {
    for(nx_linux_ax *b=nx_ax_thread_hub->windows;b;b=b->next)if(b->window==c->window)return b;
    return NULL;
}
static int select_all(budget_case *c,int *accepted) {
    nx_linux_ax *b=bridge(c);CHECK(b && b->snapshot);
    uint32_t owner=nx_ax_find(b->snapshot,100);CHECK(owner!=NX_AX_NONE);
    DBusMessage *call=d.message_new_method_call("org.a11y.atspi.Registry",NX_AX_ROOT,NX_AX_SEL,"SelectAll");
    DBusMessage *reply=d.message_new_signal(NX_AX_ROOT,"org.nexaui.Test","Result");CHECK(call && reply);
    DBusMessageIter out;d.message_iter_init_append(reply,&out);
    int built=nx_ax_selection_call((nx_ax_ref){b->hub,b,owner},call,"SelectAll",&out);
    CHECK(built && d.message_has_signature(reply,"b"));
    DBusMessageIter in;dbus_bool_t result=0;d.message_iter_init(reply,&in);d.message_iter_get_basic(&in,&result);
    d.message_unref(reply);d.message_unref(call);*accepted=result;return 0;
}
static int descend(budget_case *c) {
    size_t at=++c->depth;CHECK(at<=3);
    nx_linux_ax *b=bridge(c);CHECK(b && b->request_count==0 && b->admission_count==at && b->inflight);
    c->bytes[at-1]=b->admission_bytes;
    CHECK(context(c,at)==0 && frame(c,at)==0);
    size_t calls=c->calls;int accepted=0;CHECK(select_all(c,&accepted)==0);
    if(at==3) {
        CHECK(!accepted && c->calls==calls && b->request_count==0 && b->admission_bytes==c->bytes[2]);
        CHECK(c->bytes[2]<=NX_AX_MULTI_BYTES && c->bytes[2]+c->bytes[0]>NX_AX_MULTI_BYTES);
        c->rejected=1;
    } else {
        CHECK(accepted);c->armed=1;size_t routed=99;
        CHECK(nx_window_route_events(c->window,c->contexts[at],&routed)==NX_OK && routed==0 && !c->armed);
    }
    --c->depth;return 0;
}
int main(void) {
    char temporary[]="/tmp/nexaui-atspi-budget-XXXXXX";CHECK(mkdtemp(temporary));char address[256];snprintf(address,sizeof(address),"unix:path=%s/absent",temporary);
    CHECK(!setenv("AT_SPI_BUS_ADDRESS",address,1) && !unsetenv("NO_AT_BRIDGE"));
    budget_case c={0};c.okay=1;nx_window_desc wd={"Native budget component",640,480,0,{0}};
    CHECK(nx_window_create(&wd,&c.window)==NX_OK && nx_selection_model_create(LARGE_MEMBERS,NULL,&c.model)==NX_OK);
    nx_selection_item *items=calloc(LARGE_MEMBERS,sizeof(*items));CHECK(items);
    for(uint64_t i=0;i<LARGE_MEMBERS;++i)items[i]=(nx_selection_item){1000+i,0};
    CHECK(nx_selection_model_set_items(c.model,items,LARGE_MEMBERS)==NX_OK && nx_selection_model_set_active(c.model,1000+LARGE_MEMBERS-1)==NX_OK);free(items);
    CHECK(context(&c,0)==0 && frame(&c,0)==0);
    size_t initial=0;CHECK(nx_window_route_events(c.window,c.contexts[0],&initial)==NX_OK);
    int accepted=0;CHECK(select_all(&c,&accepted)==0 && accepted);
    c.armed=1;size_t routed=99;CHECK(nx_window_route_events(c.window,c.contexts[0],&routed)==NX_OK && routed==0 && !c.armed && c.okay && c.rejected);
    nx_selection_view view;CHECK(nx_selection_model_get(c.model,&view)==NX_OK && view.selected_count==0 && view.active_id==1000+LARGE_MEMBERS-1);
    nx_linux_ax *b=bridge(&c);CHECK(b->request_count==0 && b->admission_count==0 && b->admission_bytes==0 && b->request_bytes==0);
    printf("Native C/DBus-method budget: three real in-flight%u-member payloads=%zu/%zu/%zu bytes; fourth rejected before core allocator; model unchanged\n",LARGE_MEMBERS,c.bytes[0],c.bytes[1],c.bytes[2]);
    CHECK(nx_window_publish_semantics(c.window,NULL)==NX_OK);nx_window_destroy(c.window);
    for(size_t i=0;i<4;++i)nx_context_destroy(c.contexts[i]);nx_selection_model_destroy(c.model);CHECK(!rmdir(temporary));
    return 0;
}
