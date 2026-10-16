#import <AppKit/AppKit.h>
#include "nexaui/nexaui.h"
#include "window_internal.h"
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
extern char **environ;

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct allocation { void *pointer; size_t bytes; } allocation;
typedef struct heap { allocation live[512]; size_t count, calls; int fail_next; } heap;
static void *allocate(void *user,size_t bytes) {
    heap *h=user; ++h->calls; if (h->fail_next) { h->fail_next=0; return NULL; }
    void *p=malloc(bytes);
    if (p) { if (h->count==512) abort(); h->live[h->count++]=(allocation){p,bytes}; }
    return p;
}
static void deallocate(void *user,void *pointer,size_t bytes) {
    heap *h=user; size_t i=0; while (i<h->count && h->live[i].pointer!=pointer) ++i;
    if (i==h->count || h->live[i].bytes!=bytes) abort();
    h->live[i]=h->live[--h->count]; free(pointer);
}
typedef struct scene {
    nx_context *ui;
    int clicks, checked, queued, removed, retyped, large, renamed, moved, disabled, total_clicks;
    uint64_t required, optional, radio, other, legacy;
} scene;
static nx_control_desc control(uint64_t id,uint64_t parent,const char *name,nx_rect bounds) {
    nx_control_desc d={0}; d.id=id; d.parent_id=parent; d.name=(nx_utf8_span){name,strlen(name)}; d.bounds=bounds; return d;
}
static int frame(scene *s) {
    CHECK(nx_begin_frame(s->ui,640,480,0)==NX_OK);
    nx_semantic_desc root={0}; root.id=1; root.role=NX_ROLE_WINDOW; root.bounds=(nx_rect){0,0,640,480};
    root.name=(nx_utf8_span){"NexaCloud",9}; CHECK(nx_semantics_add(s->ui,&root,NULL)==NX_OK);
    char name[64]; strcpy(name,s->renamed ? "Changed" : "\xc3\x96" "ffnen \xf0\x9f\x93\x81");
    nx_control_desc d=control(2,1,name,(nx_rect){10,20,100,30}); d.disabled=s->disabled;
    nx_interaction_state state; CHECK(nx_button(s->ui,&d,&state)==NX_OK); s->clicks=state.clicked; s->total_clicks+=state.clicked;
    memset(name,'X',strlen(name));
    char counter[32]; int counter_length=snprintf(counter,sizeof(counter),"%d",s->total_clicks);
    nx_semantic_desc status={0}; status.id=6; status.parent_id=1; status.role=NX_ROLE_STATUS;
    status.name=(nx_utf8_span){"Button count",12}; status.value=(nx_utf8_span){counter,(size_t)counter_length};
    status.bounds=(nx_rect){10,180,100,30}; CHECK(nx_semantics_add(s->ui,&status,NULL)==NX_OK);
    d=control(3,1,"Sync",(nx_rect){10,60,100,30}); CHECK(nx_checkbox(s->ui,&d,&s->checked,&state)==NX_OK);
    d=control(4,1,"Disabled",(nx_rect){10,100,100,30}); d.disabled=1; CHECK(nx_button(s->ui,&d,&state)==NX_OK);
    nx_widget_desc secret={0}; secret.semantic.id=5; secret.semantic.parent_id=1;
    secret.semantic.role=NX_ROLE_TEXT_FIELD; secret.semantic.bounds=(nx_rect){120,20,100,30};
    secret.semantic.name=(nx_utf8_span){"Password",8}; secret.semantic.value=(nx_utf8_span){"do-not-expose",13};
    secret.semantic.state=NX_SEM_PROTECTED; secret.semantic.actions=NX_ACTION_FOCUS;
    secret.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE; CHECK(nx_widget_register(s->ui,&secret,&state)==NX_OK);
    d=control(9,1,"Legacy radio",(nx_rect){10,220,100,30});
    CHECK(nx_radio(s->ui,&d,9,&s->legacy,&state)==NX_OK);
    if (!s->removed) {
        d=control(8,1,"Dynamic",(nx_rect){10,140,100,30});
        if (s->retyped) { int checked=0; CHECK(nx_checkbox(s->ui,&d,&checked,&state)==NX_OK); }
        else CHECK(nx_button(s->ui,&d,&state)==NX_OK);
    }
    char long_name[32768]; size_t length=s->large ? sizeof(long_name) : 1600u;
    memset(long_name,'Z',length);
    nx_semantic_desc text={0}; text.id=20; text.parent_id=1; text.role=NX_ROLE_TEXT;
    text.name=(nx_utf8_span){long_name,length}; text.bounds=(nx_rect){2000,2000,100,30};
    CHECK(nx_semantics_add(s->ui,&text,NULL)==NX_OK);
    for (unsigned g=0;g<4;++g) {
        nx_semantic_desc group={0}; group.id=50u+g*10u; group.parent_id=1;
        group.role=g==0 ? NX_ROLE_TAB_LIST : g==2 ? NX_ROLE_GROUP : NX_ROLE_LIST;
        group.name=(nx_utf8_span){"Selection",9}; group.bounds=(nx_rect){250,20+(float)g*80,180,70};
        uint64_t *choice=g==0 ? &s->required : g==1 ? &s->optional : g==2 ? &s->radio : &s->other;
        CHECK(nx_selection_group(s->ui,&group,g==0 || g==2,choice)==NX_OK);
        uint64_t parent=group.id;
        if (g==1) {
            nx_semantic_desc panel=group; panel.id=64; panel.parent_id=60; panel.role=NX_ROLE_PANEL;
            CHECK(nx_semantics_add(s->ui,&panel,NULL)==NX_OK); parent=64;
        }
        for (unsigned item=1;item<=3;++item) {
            if (g==3 && (!s->moved || item!=1)) continue;
            if (g==1 && s->moved && item==1) continue;
            nx_widget_desc w={0}; w.semantic.id=g==3 ? 61 : group.id+item; w.semantic.parent_id=parent;
            w.semantic.role=g==0 ? NX_ROLE_TAB : g==2 ? NX_ROLE_RADIO : NX_ROLE_LIST_ITEM;
            w.semantic.name=(nx_utf8_span){"Choice",6}; w.semantic.bounds=(nx_rect){250+(float)(item-1u)*50,20+(float)g*80,45,30};
            w.semantic.actions=NX_ACTION_SELECT|NX_ACTION_FOCUS;
            w.semantic.state=(*choice==w.semantic.id ? (g==2 ? NX_SEM_CHECKED : NX_SEM_SELECTED) : 0u) | (item==3 ? NX_SEM_DISABLED : 0u);
            w.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
            CHECK(nx_widget_register(s->ui,&w,&state)==NX_OK && !state.clicked);
        }
    }
    s->queued=0;
    for (uint64_t id=100;id<=164;++id) {
        d=control(id,1,"Queued",(nx_rect){120,60,100,30}); CHECK(nx_button(s->ui,&d,&state)==NX_OK);
        s->queued+=state.clicked;
    }
    CHECK(nx_end_frame(s->ui)==NX_OK); return 0;
}
static id find(id root,uint64_t id_value) {
    NSString *wanted=[NSString stringWithFormat:@"nexa-%016llx",(unsigned long long)id_value];
    if ([[root accessibilityIdentifier] isEqualToString:wanted]) return root;
    for (id child in [root accessibilityChildren]) { id result=find(child,id_value); if (result) return result; }
    return nil;
}
static int advance(nx_window *window,scene *s) {
    size_t routed=0; CHECK(nx_window_route_events(window,s->ui,&routed)==NX_OK);
    CHECK(frame(s)==0 && nx_window_publish_semantics(window,s->ui)==NX_OK); return 0;
}
typedef struct worker_case { nx_window *window; nx_context *ui; id element; int rejected; } worker_case;
static void *worker(void *argument) {
    worker_case *t=argument;
    @autoreleasepool {
        t->rejected=nx_window_publish_semantics(t->window,t->ui)==NX_ERROR_STATE &&
            [t->element accessibilityLabel]==nil && ![t->element accessibilityPerformPress];
    }
    return NULL;
}
static int run(void) {
    heap h={0}; nx_window_desc desc={"AX fixture",640,480,NX_WINDOW_HIDDEN,{&h,allocate,deallocate}};
    nx_window *window=NULL; CHECK(nx_window_create(&desc,&window)==NX_OK);
    NSView *view=[nx_window_native_nsview(window) retain]; NSWindow *native=nx_window_native_nswindow(window);
    scene s={0}; s.required=51; s.radio=71; CHECK(nx_context_create(NULL,&s.ui)==NX_OK && frame(&s)==0);
    h.fail_next=1; CHECK(nx_window_publish_semantics(window,s.ui)==NX_ERROR_MEMORY && !h.fail_next);
    CHECK(nx_window_publish_semantics(window,s.ui)==NX_OK);
    CHECK([[view accessibilityChildren] count]==1);
    id button=[find(view,2) retain], toggle=[find(view,3) retain], secret=[find(view,5) retain], dynamic=[find(view,8) retain];
    CHECK(button && toggle && secret && dynamic);
    NSString *saved_label=[[button accessibilityLabel] retain];
    CHECK([[button accessibilityRole] isEqualToString:NSAccessibilityButtonRole]);
    CHECK([[button accessibilityLabel] isEqualToString:@"Öffnen 📁"]);
    CHECK([button accessibilityParent]==find(view,1) && [button accessibilityWindow]==native);
    CHECK([[secret accessibilityValue] isEqual:@""] && [secret isAccessibilityProtectedContent]);
    CHECK([[secret accessibilitySubrole] isEqualToString:NSAccessibilitySecureTextFieldSubrole]);
    CHECK(![find(view,4) isAccessibilityEnabled] && ![find(view,4) accessibilityPerformPress]);
    CHECK([[find(view,20) accessibilityLabel] length]==1600 && [find(view,20) isAccessibilityHidden]);
    CHECK(![button isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]);
    CHECK([button isAccessibilitySelectorAllowed:@selector(accessibilityPerformPress)]);
    CHECK(![secret isAccessibilitySelectorAllowed:@selector(accessibilityPerformPress)]);
    [native makeKeyAndOrderFront:nil];
    NSRect before=[button accessibilityFrame]; CHECK(before.size.width==100 && before.size.height==30);
    NSRect content=[native contentRectForFrameRect:[native frame]];
    CHECK(fabs(before.origin.x-content.origin.x-10)<.01);
    CHECK(fabs(before.origin.y-(NSMaxY(content)-20-30))<.01);
    NSPoint origin=[native frame].origin; [native setFrameOrigin:NSMakePoint(origin.x+40,origin.y+30)];
    NSRect after=[button accessibilityFrame]; CHECK(fabs(after.origin.x-before.origin.x-40)<.01 && fabs(after.origin.y-before.origin.y-30)<.01);
    CHECK([view accessibilityHitTest:NSMakePoint(NSMidX(after),NSMidY(after))]==button);
    worker_case off_thread={window,s.ui,button,0}; pthread_t thread;
    CHECK(!pthread_create(&thread,NULL,worker,&off_thread) && !pthread_join(thread,NULL) && off_thread.rejected);
    CHECK([button accessibilityPerformPress] && ![button accessibilityPerformPress]);
    CHECK([toggle accessibilityPerformPress]); CHECK(!s.clicks && !s.checked);
    CHECK(advance(window,&s)==0 && s.clicks && s.checked && [[toggle accessibilityValue] intValue]==1);
    CHECK([find(view,9) accessibilityPerformPress] && advance(window,&s)==0 && s.legacy==9);
    CHECK([[find(view,9) accessibilityValue] boolValue]);
    CHECK(find(view,2)==button); /* Canonical object across consecutive frames. */
    id tabs=[find(view,50) retain], a=[find(view,51) retain], b=[find(view,52) retain];
    id list=[find(view,60) retain], c=[find(view,61) retain], d=[find(view,62) retain];
    CHECK(tabs && a && b && list && c && d);
    CHECK([[list accessibilityRows] count]==3 && [[list accessibilitySelectedRows] count]==0);
    CHECK([[tabs accessibilityTabs] count]==3 && [[tabs accessibilitySelectedChildren] firstObject]==a);
    [a setAccessibilitySelected:NO]; CHECK(advance(window,&s)==0 && s.required==51);
    [b setAccessibilitySelected:YES]; [a setAccessibilitySelected:YES];
    CHECK(s.required==51 && advance(window,&s)==0 && s.required==52);
    CHECK([[tabs accessibilitySelectedChildren] firstObject]==b && [[b accessibilityValue] boolValue]);
    [c setAccessibilitySelected:YES]; size_t routed=0;
    CHECK(nx_window_route_events(window,s.ui,&routed)==NX_OK && nx_window_publish_semantics(window,s.ui)==NX_OK);
    [d setAccessibilitySelected:YES]; CHECK(advance(window,&s)==0 && s.optional==61);
    CHECK([c accessibilityParent]==find(view,64) && [[list accessibilitySelectedChildren] firstObject]==c);
    [list setAccessibilitySelectedChildren:@[]]; CHECK(advance(window,&s)==0 && !s.optional);
    [list setAccessibilitySelectedChildren:@[c,d]]; CHECK(advance(window,&s)==0 && !s.optional);
    [list setAccessibilitySelectedChildren:@[b]]; CHECK(advance(window,&s)==0 && !s.optional);
    [find(view,63) setAccessibilitySelected:YES]; CHECK(advance(window,&s)==0 && !s.optional);
    [c setAccessibilitySelected:YES]; s.moved=1; CHECK(frame(&s)==0);
    CHECK(nx_window_route_events(window,s.ui,&routed)==NX_OK); /* Unpublished frame must defer. */
    CHECK(nx_window_publish_semantics(window,s.ui)==NX_OK && advance(window,&s)==0);
    CHECK(![c isAccessibilitySelected] && !s.optional && !s.other);
    [c setAccessibilitySelected:YES]; CHECK(advance(window,&s)==0 && s.other==61 && [c isAccessibilitySelected]);
    s.moved=0; s.other=0; CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    [list setAccessibilitySelectedRows:@[d]]; [find(view,72) setAccessibilitySelected:YES];
    CHECK(advance(window,&s)==0 && s.optional==62 && s.radio==72);
    [tabs setAccessibilityFocused:YES]; CHECK(advance(window,&s)==0 && nx_focus_id(s.ui)==52);
    CHECK(!![b isAccessibilityFocused]==!!([native isKeyWindow] && [native firstResponder]==view));
    if ([b isAccessibilityFocused]) CHECK([view accessibilityFocusedUIElement]==b);
    nx_context *wrong=NULL; CHECK(nx_context_create(NULL,&wrong)==NX_OK);
    CHECK(nx_window_route_events(window,wrong,&routed)==NX_ERROR_STATE); nx_context_destroy(wrong);
    for (uint64_t id_value=100;id_value<164;++id_value) CHECK([find(view,id_value) accessibilityPerformPress]);
    CHECK(![find(view,164) accessibilityPerformPress] && ![find(view,100) accessibilityPerformPress]);
    CHECK(advance(window,&s)==0 && s.queued==64);
    for (uint64_t id_value=100;id_value<164;++id_value) CHECK(nx_semantics_request_action(s.ui,id_value,NX_ACTION_PRESS)==NX_OK);
    CHECK([find(view,164) accessibilityPerformPress]);
    CHECK(advance(window,&s)==0 && s.queued==64);
    CHECK(advance(window,&s)==0 && s.queued==1); /* Accepted native work survives core backpressure. */
    nx_window *other_window=NULL; nx_window_desc other_desc={"Other AX",640,480,NX_WINDOW_HIDDEN,{0}};
    scene other_scene={0}; other_scene.required=51; other_scene.radio=71;
    CHECK(nx_window_create(&other_desc,&other_window)==NX_OK && nx_context_create(NULL,&other_scene.ui)==NX_OK);
    CHECK(frame(&other_scene)==0 && nx_window_publish_semantics(other_window,other_scene.ui)==NX_OK);
    id other_button=[find(nx_window_native_nsview(other_window),2) retain]; CHECK(other_button && other_button!=button);
    CHECK(nx_window_route_events(other_window,s.ui,&routed)==NX_ERROR_STATE);
    CHECK([button accessibilityPerformPress] && advance(other_window,&other_scene)==0 && !other_scene.clicks);
    CHECK(advance(window,&s)==0 && s.clicks);
    nx_context_destroy(other_scene.ui); nx_window_destroy(other_window);
    CHECK(![other_button isAccessibilityElement] && [button isAccessibilityElement]); [other_button release];
    s.large=1; s.renamed=1; CHECK(frame(&s)==0); h.fail_next=1;
    CHECK(nx_window_publish_semantics(window,s.ui)==NX_ERROR_MEMORY && !h.fail_next);
    CHECK([[button accessibilityLabel] isEqualToString:@"Öffnen 📁"]);
    CHECK(nx_window_publish_semantics(window,s.ui)==NX_OK && [[button accessibilityLabel] isEqual:@"Changed"]);
    s.removed=1; CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    CHECK(![dynamic isAccessibilityElement] && ![dynamic accessibilityPerformPress] && [dynamic accessibilityParent]==nil);
    s.removed=0; s.retyped=1; CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    id replacement=[find(view,8) retain]; CHECK(replacement!=dynamic && [[replacement accessibilityRole] isEqual:NSAccessibilityCheckBoxRole]);
    CHECK([button accessibilityPerformPress]); s.disabled=1;
    CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    CHECK(![button isAccessibilityEnabled] && advance(window,&s)==0 && !s.clicks);
    s.disabled=0; CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    CHECK([button accessibilityPerformPress]);
    nx_context_destroy(s.ui); /* Queries must use owned native data. */
    CHECK([[button accessibilityLabel] isEqual:@"Changed"]);
    CHECK(nx_context_create(NULL,&s.ui)==NX_OK && frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    CHECK(![button isAccessibilityElement] && ![button accessibilityPerformPress]);
    id current=[find(view,2) retain]; CHECK(current && current!=button);
    CHECK(advance(window,&s)==0 && !s.clicks);
    CHECK([current accessibilityPerformPress] && advance(window,&s)==0 && s.clicks);
    CHECK(nx_window_publish_semantics(window,NULL)==NX_OK && ![current isAccessibilityElement]);
    CHECK(nx_window_publish_semantics(window,s.ui)==NX_OK && find(view,2)!=current);
    id final=[find(view,2) retain];
    nx_context_destroy(s.ui); nx_window_destroy(window);
    CHECK(h.count==0 && ![final isAccessibilityElement] && ![final accessibilityPerformPress] && NSEqualRects([final accessibilityFrame],NSZeroRect));
    CHECK([saved_label isEqualToString:@"Öffnen 📁"]); [saved_label release];
    [final release]; [current release]; [replacement release]; [dynamic release]; [secret release]; [toggle release]; [button release];
    [tabs release]; [a release]; [b release]; [list release]; [c release]; [d release]; [view release];
    puts("Native AppKit AX properties/actions/selection/lifetime/allocation checks passed");
    return 0;
}
static int external(const char *client) {
    nx_window_desc desc={"AX fixture",640,480,0,{0}}; nx_window *window=NULL;
    scene s={0}; s.required=51; s.radio=71;
    CHECK(nx_window_create(&desc,&window)==NX_OK && nx_context_create(NULL,&s.ui)==NX_OK);
    CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ui)==NX_OK);
    char parent[32]; snprintf(parent,sizeof(parent),"%ld",(long)getpid());
    char *arguments[]={(char *)client,parent,NULL}; pid_t child;
    int spawn_error=posix_spawn(&child,client,NULL,NULL,arguments,environ);
    if (spawn_error) { nx_context_destroy(s.ui); nx_window_destroy(window); return 1; }
    double started=[[NSProcessInfo processInfo] systemUptime]; int status=0,failed=0;
    for (;;) {
        pid_t result=waitpid(child,&status,WNOHANG);
        if (result==child) break;
        if (result<0) { failed=1; break; }
        if ([[NSProcessInfo processInfo] systemUptime]-started>75 || nx_window_poll(window,0)!=NX_OK || advance(window,&s)) {
            /* Only the child created above is terminated. */
            kill(child,SIGKILL); waitpid(child,&status,0); failed=1; break;
        }
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,.001,true);
    }
    nx_context_destroy(s.ui); nx_window_destroy(window);
    return failed || !WIFEXITED(status) ? 1 : WEXITSTATUS(status);
}
int main(int argc,char **argv) {
    @autoreleasepool { return argc==2 ? external(argv[1]) : run(); }
}
