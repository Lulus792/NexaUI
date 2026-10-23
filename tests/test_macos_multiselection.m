#include "macos_multiselection_fixture.h"

typedef struct worker_case {id owner,item;int okay;} worker_case;
static void *off_thread(void *p) {
    worker_case *w=p;@autoreleasepool{w->okay=[w->owner accessibilitySelectedChildren]==nil && [w->owner accessibilitySelectedRows]==nil && ![w->item accessibilityPerformPress];}return NULL;
}
static int operations(void) {
    scene s={0};CHECK(setup(&s)==0);id owner=[element(&s,100) retain],a=[element(&s,1000) retain],b=[element(&s,1001) retain];
    uint64_t all[65];for(size_t i=0;i<65;++i)all[i]=1000+i;
    CHECK(nx_selection_model_set_selected(s.model,all,65)==NX_OK && publish(&s)==0);
    NSArray *snapshot=[[owner accessibilitySelectedChildren] retain];CHECK([snapshot count]==65 && ![snapshot isKindOfClass:[NSMutableArray class]]);
    CHECK([[owner accessibilitySelectedRows] count]==65 && [[owner accessibilityRows] count]==67);
    CHECK([snapshot objectAtIndex:64]==element(&s,1064) && [element(&s,1064) isAccessibilitySelected]);
    CHECK([element(&s,1064) isAccessibilityHidden] && ![[owner accessibilityVisibleRows] count]);
    worker_case w={owner,b,0};pthread_t worker;CHECK(!pthread_create(&worker,NULL,off_thread,&w) && !pthread_join(worker,NULL) && w.okay);
    [owner setAccessibilitySelectedChildren:@[]];CHECK(selected(s.model,all,65,1066)==0 && advance(&s,1)==0 && selected(s.model,NULL,0,1066)==0);
    CHECK([snapshot count]==65 && [snapshot objectAtIndex:0]==a && ![a isAccessibilitySelected]);
    NSMutableArray *reverse=[NSMutableArray array];for(size_t i=65;i>0;--i)[reverse addObject:element(&s,1000+i-1)];
    [owner setAccessibilitySelectedChildren:reverse];CHECK(advance(&s,1)==0 && selected(s.model,all,65,1066)==0);
    [element(&s,1066) setAccessibilitySelected:YES];CHECK(advance(&s,1)==0);
    nx_selection_view v;CHECK(nx_selection_model_get(s.model,&v)==NX_OK && v.selected_count==66 && v.active_id==1066);
    [a setAccessibilitySelected:NO];CHECK(advance(&s,1)==0);
    CHECK(nx_selection_model_get(s.model,&v)==NX_OK && v.selected_count==65 && v.selected_ids[0]==1001);
    CHECK([b accessibilityPerformPress] && advance(&s,1)==0);uint64_t one=1001;CHECK(selected(s.model,&one,1,1066)==0);
    [owner setAccessibilitySelectedChildren:@[a,a]];[owner setAccessibilitySelectedChildren:@[element(&s,2000)]];
    [owner setAccessibilitySelectedChildren:@[@"invalid"]];CHECK(advance(&s,0)==0 && selected(s.model,&one,1,1066)==0);
    [owner setAccessibilitySelectedChildren:@[a,b]];[owner setAccessibilitySelectedChildren:@[a,b]];[owner setAccessibilitySelectedChildren:@[a]];
    CHECK(advance(&s,1)==0);uint64_t two[]={1000,1001};CHECK(selected(s.model,two,2,1066)==0);
    s.required=1;CHECK(publish(&s)==0);[owner setAccessibilitySelectedChildren:@[]];CHECK(advance(&s,0)==0 && selected(s.model,two,2,1066)==0);
    [a release];[b release];[owner release];[snapshot release];CHECK(cleanup(&s)==0);puts("native complete65/add/remove/exclusive/required/immutable/owner serialization passed");return 0;
}
static int table_and_disabled(void) {
    scene s={0};CHECK(setup(&s)==0);id table=element(&s,200),row=element(&s,2001),custom=element(&s,2002);
    uint64_t before[]={2000,2002},after[]={2001,2002};CHECK(nx_selection_model_set_selected(s.table,before,2)==NX_OK && publish(&s)==0);
    CHECK([[table accessibilitySelectedChildren] count]==2 && [[table accessibilitySelectedRows] count]==1);
    CHECK([table isAccessibilitySelectorAllowed:@selector(setAccessibilitySelectedRows:)]);
    [table setAccessibilitySelectedRows:@[row]];CHECK(advance(&s,1)==0 && selected(s.table,after,2,0)==0);
    [table setAccessibilitySelectedRows:@[custom]];[table setAccessibilitySelectedChildren:@[element(&s,210)]];
    CHECK(advance(&s,0)==0 && selected(s.table,after,2,0)==0);
    nx_selection_item items[67];for(uint64_t i=0;i<67;++i)items[i]=(nx_selection_item){1000+i,i==2?NX_SELECTION_ITEM_DISABLED:0};
    uint64_t disabled=1002,both[]={1001,1002};CHECK(nx_selection_model_set_items(s.model,items,67)==NX_OK);
    CHECK(nx_selection_model_set_selected(s.model,&disabled,1)==NX_OK && publish(&s)==0);
    id owner=element(&s,100);[owner setAccessibilitySelectedChildren:@[element(&s,1001)]];
    CHECK(advance(&s,0)==0 && selected(s.model,&disabled,1,1066)==0);
    [owner setAccessibilitySelectedChildren:@[element(&s,1002),element(&s,1001)]];
    CHECK(advance(&s,1)==0 && selected(s.model,both,2,1066)==0);
    CHECK(cleanup(&s)==0);puts("native table row subset preserves custom; disabled selected member policy passed");return 0;
}
static int stale(void) {
    for(int whole=0;whole<2;++whole)for(int forwarded=0;forwarded<2;++forwarded)for(int kind=1;kind<=7;++kind) {
        scene s={0};CHECK(setup(&s)==0);uint64_t one=1000;CHECK(nx_selection_model_set_selected(s.model,&one,1)==NX_OK && publish(&s)==0);
        id owner=[element(&s,100) retain],item=[element(&s,1001) retain];
        if(whole)[owner setAccessibilitySelectedChildren:@[item]];else CHECK([item accessibilityPerformPress]);
        if(forwarded){size_t n=0;CHECK(nx_window_route_events(s.window,s.ctx,&n)==NX_OK && n==1);}
        if(kind<=4)s.kind=kind;
        else if(kind==5){CHECK(nx_window_publish_semantics(s.window,NULL)==NX_OK);}
        else if(kind==6){CHECK(nx_selection_model_set_selected(s.model,&one,1)==NX_OK);}
        else {s.disabled=1;CHECK(publish(&s)==0);s.disabled=0;}
        CHECK(publish(&s)==0 && advance(&s,0)==0 && selected(s.model,&one,1,1066)==0);
        if(kind<=5)CHECK(![item isAccessibilityElement]);
        [owner release];[item release];CHECK(cleanup(&s)==0);
    }
    puts("native pending/forwarded single/whole leases:4 ancestry changes, detach, equal model revision, disable/enable passed");return 0;
}
static void detach(void *p) {scene *s=p;s->callback_result=nx_window_publish_semantics(s->window,NULL);}
static void destroy_window(void *p) {scene *s=p;nx_window *w=s->window;s->window=NULL;nx_window_destroy(w);s->callback_result=NX_OK;}
static void republish(void *p) {scene *s=p;s->callback_result=nx_window_publish_semantics(s->window,s->ctx);}
@interface NXReentryArray : NSArray {
@public
    NSArray *payload;void (*callback)(void *);void *user;NSUInteger visits;
}
@end
@implementation NXReentryArray
- (NSUInteger)count {return [payload count];}
- (id)objectAtIndex:(NSUInteger)index {
    ++visits;if(callback){void (*f)(void *)=callback;callback=NULL;f(user);}return [payload objectAtIndex:index];
}
- (void)dealloc {[payload release];[super dealloc];}
@end
/* Observe actual private snapshot addresses and their first two scalar fields.
   The production interface stays opaque; this proves address ABA rather than
   substituting an address or assuming that two publications reused storage. */
static int snapshot_info(scene *s,void **address,size_t *capacity,size_t *count) {
    NSView *view=nx_window_native_nsview(s->window);CHECK(view);
    Ivar v=class_getInstanceVariable(object_getClass(view),"accessibility");CHECK(v);
    id bridge=nil;memcpy(&bridge,(const char *)(void *)view+ivar_getOffset(v),sizeof(bridge));CHECK(bridge);
    Ivar field=class_getInstanceVariable(object_getClass(bridge),"snapshot");CHECK(field);
    void *snapshot=NULL;memcpy(&snapshot,(const char *)(void *)bridge+ivar_getOffset(field),sizeof(snapshot));CHECK(snapshot);
    size_t scalars[2];memcpy(scalars,snapshot,sizeof(scalars));
    *address=snapshot;*capacity=scalars[0];*count=scalars[1];return 0;
}
typedef struct aba_case {
    scene *s;void *before,*middle,*after;size_t capacity,count,after_count,native_calls;int okay;
} aba_case;
static void twice_publish(void *p) {
    aba_case *a=p;size_t capacity=0,count=0;
    if(publish(a->s) || snapshot_info(a->s,&a->middle,&capacity,&count))return;
    a->s->padding=150;
    if(publish(a->s) || snapshot_info(a->s,&a->after,&capacity,&a->after_count))return;
    a->okay=a->before!=a->middle && a->before==a->after &&
        a->after_count==a->count+150 && capacity==a->capacity && a->s->native.calls==a->native_calls;
    uint32_t index=0;
    if(nx_semantics_find(a->s->ctx,1066,&index)!=NX_OK || (size_t)index<a->count)a->okay=0;
    fprintf(stderr,"NSArray-callback ABA before=%p middle=%p after=%p old-count=%zu new-count=%zu index=%u verified=%d\n",
        a->before,a->middle,a->after,a->count,a->after_count,index,a->okay);
}
static int snapshot_aba(void) {
    scene s={0};CHECK(setup(&s)==0);
    /* Grow both actual native buffers, then shrink their logical node/text
       contents. The callback can add nodes without allocating a third buffer. */
    s.large=1;CHECK(publish(&s)==0 && publish(&s)==0);s.large=0;CHECK(publish(&s)==0);
    id owner=[element(&s,100) retain],item=[element(&s,1066) retain];CHECK(owner && item);
    aba_case a={0};a.s=&s;CHECK(snapshot_info(&s,&a.before,&a.capacity,&a.count)==0);a.native_calls=s.native.calls;
    NXReentryArray *array=[NXReentryArray new];array->payload=[@[item] retain];array->callback=twice_publish;array->user=&a;
    [owner setAccessibilitySelectedChildren:array];
    uint32_t index=0;CHECK(nx_semantics_find(s.ctx,1066,&index)==NX_OK);
    fprintf(stderr,"snapshot-ABA before=%p middle=%p after=%p old-count=%zu new-count=%zu target-index=%u verified=%d\n",
        a.before,a.middle,a.after,a.count,a.after_count,index,a.okay);
    CHECK(array->visits==1 && a.okay && (size_t)index>=a.count);
    CHECK(advance(&s,0)==0 && selected(s.model,NULL,0,1066)==0);
    [array release];[owner release];[item release];CHECK(cleanup(&s)==0);
    puts("native real snapshot ABA rejects stale scratch before shifted row index passed");return 0;
}
static int scope_demotion(void) {
    for(int forwarded=0;forwarded<2;++forwarded) {
        scene s={0};CHECK(setup(&s)==0);uint64_t one=1000;
        CHECK(nx_selection_model_set_selected(s.model,&one,1)==NX_OK && publish(&s)==0);
        id old_owner=[element(&s,100) retain],old_item=[element(&s,1001) retain];CHECK(old_owner && old_item);
        if(forwarded){size_t n=0;CHECK([old_item accessibilityPerformPress]);CHECK(nx_window_route_events(s.window,s.ctx,&n)==NX_OK && n==1);}
        s.demoted=1;s.demoted_choice=1000;CHECK(publish(&s)==0);
        int owner_live=[old_owner isAccessibilityElement],item_live=[old_item isAccessibilityElement];
        [old_item setAccessibilitySelected:YES];size_t routed=0;
        CHECK(nx_window_route_events(s.window,s.ctx,&routed)==NX_OK && publish(&s)==0);
        fprintf(stderr,"multi-to-single forwarded=%d old-owner=%d old-item=%d routed=%zu actual=%llu expected=1000\n",
            forwarded,owner_live,item_live,routed,(unsigned long long)s.demoted_choice);
        CHECK(!owner_live && !item_live && !routed && s.demoted_choice==1000 && selected(s.model,&one,1,1066)==0);
        s.demoted=0;CHECK(publish(&s)==0 && ![old_owner isAccessibilityElement] && ![old_item isAccessibilityElement]);
        CHECK(element(&s,100)!=old_owner && element(&s,1001)!=old_item);
        [old_owner release];[old_item release];CHECK(cleanup(&s)==0);
    }
    puts("native Multi-to-Single revokes old owner/member handles and does not revive on promotion passed");return 0;
}
static void extra_button(void *p) {scene *s=p;s->native_attempt=[element(s,5063) accessibilityPerformPress];}
typedef struct nested_array {id owner;NXReentryArray *next;} nested_array;
static void enqueue_array(void *p) {nested_array *n=p;[n->owner setAccessibilitySelectedChildren:n->next];}
static int byte_backpressure(void) {
    scene s={0};s.big=65500;CHECK(setup(&s)==0);
    id owner=element(&s,100);NXReentryArray *arrays[4];nested_array nested[3];
    for(size_t i=0;i<4;++i){arrays[i]=[NXReentryArray new];arrays[i]->payload=[@[element(&s,1001+i)] retain];}
    for(size_t i=0;i<3;++i){nested[i]=(nested_array){owner,arrays[i+1]};arrays[i]->callback=enqueue_array;arrays[i]->user=&nested[i];}
    [owner setAccessibilitySelectedChildren:arrays[0]];
    /* Each retained full member payload plus its scratch marks exceeds2MiB.
       Three in-flight admissions fit8MiB; a fourth must stop before the next
       user NSArray access, even though all three outer requests are unqueued. */
    CHECK(arrays[0]->visits==1 && arrays[1]->visits==1 && arrays[2]->visits==1 && !arrays[3]->visits);
    CHECK(advance(&s,1)==0);uint64_t one=1003;CHECK(selected(s.model,&one,1,66499)==0);
    CHECK([[owner accessibilitySelectedChildren] count]==1 && [[owner accessibilitySelectedChildren] firstObject]==element(&s,1003));
    for(size_t i=0;i<4;++i)[arrays[i] release];CHECK(cleanup(&s)==0);
    puts("native8MiB includes three retained in-flight65500-member payloads; fourth NSArray admission blocked passed");return 0;
}
static int array_callouts_and_slots(void) {
    scene s={0};CHECK(setup(&s)==0);
    for(uint64_t i=0;i<63;++i)CHECK([element(&s,5000+i) accessibilityPerformPress]);
    NXReentryArray *array=[NXReentryArray new];array->payload=[@[element(&s,1001)] retain];array->callback=extra_button;array->user=&s;
    [element(&s,100) setAccessibilitySelectedChildren:array];
    CHECK(array->visits==1 && !s.native_attempt && ![element(&s,5064) accessibilityPerformPress]);
    CHECK(advance(&s,64)==0 && s.clicks==63);uint64_t one=1001;CHECK(selected(s.model,&one,1,1066)==0);
    CHECK([element(&s,5063) accessibilityPerformPress] && advance(&s,1)==0 && s.clicks==1);
    [array release];CHECK(cleanup(&s)==0);
    for(int destroy=0;destroy<2;++destroy) {
        scene t={0};CHECK(setup(&t)==0);id old=[element(&t,100) retain];
        array=[NXReentryArray new];array->payload=[@[element(&t,1001)] retain];array->callback=destroy?destroy_window:detach;array->user=&t;
        [old setAccessibilitySelectedChildren:array];CHECK(array->visits==1 && ![old isAccessibilityElement] && t.callback_result==NX_OK);
        CHECK(frame(&t)==0 && selected(t.model,NULL,0,1066)==0);[array release];[old release];CHECK(cleanup(&t)==0);
    }
    puts("native NSArray detach/destroy and in-flight64-slot reservation with actual63-button delivery passed");return 0;
}
static int callouts(void) {
    {
        scene s={0};CHECK(setup(&s)==0 && publish(&s)==0);id old=[element(&s,100) retain];s.large=1;CHECK(frame(&s)==0);
        s.native.fail=1;CHECK(nx_window_publish_semantics(s.window,s.ctx)==NX_ERROR_MEMORY && !s.native.fail);
        CHECK([old isAccessibilityElement] && [[old accessibilityLabel] isEqual:@"Choice"]);
        s.native.hook=republish;s.native.user=&s;s.native.when=1;
        CHECK(nx_window_publish_semantics(s.window,s.ctx)==NX_OK && !s.native.hook && s.callback_result==NX_ERROR_STATE);
        CHECK(element(&s,100)==old && [[old accessibilityLabel] length]==65536);
        [old release];CHECK(cleanup(&s)==0);
    }
    for(int action=0;action<3;++action) {
        scene s={0};CHECK(setup(&s)==0);uint64_t one=1000;CHECK(nx_selection_model_set_selected(s.model,&one,1)==NX_OK && publish(&s)==0);
        id old=[element(&s,1001) retain];[element(&s,100) setAccessibilitySelectedChildren:@[old]];
        if(!action) {
            s.core.fail=1;size_t n=0;CHECK(nx_window_route_events(s.window,s.ctx,&n)==NX_ERROR_MEMORY && !n && !s.core.fail);
            CHECK(advance(&s,1)==0);one=1001;CHECK(selected(s.model,&one,1,1066)==0);
        } else {
            s.core.hook=action==1?detach:destroy_window;s.core.user=&s;s.core.when=1;
            size_t n=0;nx_window *w=s.window;CHECK(nx_window_route_events(w,s.ctx,&n)==NX_OK && !n && !s.core.hook && s.callback_result==NX_OK);
            CHECK(frame(&s)==0 && selected(s.model,&one,1,1066)==0 && ![old isAccessibilityElement]);
        }
        [old release];CHECK(cleanup(&s)==0);
    }
    for(int when=1;when<=2;++when)for(int destroy=0;destroy<2;++destroy) {
        scene s={0};CHECK(setup(&s)==0 && publish(&s)==0);id old=[element(&s,1000) retain];s.large=1;
        CHECK(frame(&s)==0);s.native.hook=destroy?destroy_window:detach;s.native.user=&s;s.native.when=when;
        nx_window *w=s.window;CHECK(nx_window_publish_semantics(w,s.ctx)==NX_ERROR_STATE && !s.native.hook && s.callback_result==NX_OK);
        CHECK(![old isAccessibilityElement]);[old release];CHECK(cleanup(&s)==0);
    }
    puts("native core OOM retry and allocator/deallocator detach/destroy lifetime passed");return 0;
}
static int legacy_single(void) {
    scene s={0};CHECK(setup(&s)==0);id held=[element(&s,301) retain];[held setAccessibilitySelected:YES];
    CHECK(advance(&s,1)==0 && s.legacy==301);s.legacy=0;s.moved=1;CHECK(publish(&s)==0);
    CHECK(element(&s,301)==held);[held setAccessibilitySelected:YES];CHECK(advance(&s,1)==0 && s.other==301);
    [held release];CHECK(cleanup(&s)==0);puts("legacy Single retained item remains canonical after reparent passed");return 0;
}
int main(int argc,char **argv) {
    @autoreleasepool {
        if(argc==2) {
            if(!strcmp(argv[1],"operations"))return operations();
            if(!strcmp(argv[1],"table"))return table_and_disabled();
            if(!strcmp(argv[1],"stale"))return stale();
            if(!strcmp(argv[1],"callouts"))return callouts();
            if(!strcmp(argv[1],"array"))return array_callouts_and_slots();
            if(!strcmp(argv[1],"bytes"))return byte_backpressure();
            if(!strcmp(argv[1],"aba"))return snapshot_aba();
            if(!strcmp(argv[1],"demotion"))return scope_demotion();
            if(!strcmp(argv[1],"single"))return legacy_single();
            return 2;
        }
        CHECK(operations()==0 && table_and_disabled()==0 && stale()==0 && callouts()==0 && array_callouts_and_slots()==0 && byte_backpressure()==0 && snapshot_aba()==0 && scope_demotion()==0 && legacy_single()==0);
        puts("Hidden native AppKit multi-selection checks passed; no window fronting, focus requests, external AX or physical acceptance");return 0;
    }
}
