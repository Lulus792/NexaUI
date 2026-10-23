#import "accessibility.h"
#include "../../src/context_identity.h"
#include "../../src/selection_model.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/runtime.h>

#define NX_AX_NONE UINT32_MAX
#define NX_AX_BYTES (32u*1024u*1024u)
#define NX_AX_NODES 65536u
#define NX_AX_SELECTION (NX_SEM_SELECTION_SINGLE|NX_SEM_SELECTION_MULTIPLE)
#define NX_AX_MULTI_BYTES (8u*1024u*1024u)
typedef struct nx_ax_node {
    uint64_t id, incarnation;
    nx_semantic_role role;
    nx_rect bounds;
    uint32_t state, actions, parent, first, last, next, owner, selected, focus;
    uint32_t selected_last, next_selected, selected_count, member_count;
    uint64_t membership, policy_epoch;
    nx_selection_target target;
    int owner_multiple;
    const char *name, *help, *value;
} nx_ax_node;
typedef struct nx_ax_snapshot {
    size_t capacity, count, slot_count;
    uint32_t first, *slots;
    nx_ax_node nodes[];
} nx_ax_snapshot;

/* Main-thread snapshots are borrowed only during current(). Lifetime callbacks
   touch C reference counts and system memory, never an Objective-C deallocator
   or an application allocator. A detached bridge revokes before freeing buffers. */
typedef struct nx_ax_multi_lease {
    atomic_uint references;
    uint64_t generation, context;
    nx_ax_snapshot *snapshot;
    int live;
} nx_ax_multi_lease;
typedef struct nx_ax_multi_item { uint64_t id, incarnation, membership; } nx_ax_multi_item;
typedef struct nx_ax_multi_request {
    atomic_uint references;
    nx_ax_multi_lease *lease;
    uint64_t generation, context, owner_id, owner_incarnation, policy_epoch;
    uint64_t id, incarnation;
    nx_selection_target target;
    uint32_t action;
    int whole;
    size_t bytes, member_count, desired_count;
    nx_ax_multi_item members[];
} nx_ax_multi_request;
typedef struct nx_ax_request {
    uint64_t id, incarnation, owner_id, owner_incarnation, frame;
    uint32_t action;
    int forwarded;
    nx_ax_multi_request *multi;
} nx_ax_request;

@class NXMacAXElement;
@interface NXMacAXBridge : NSObject {
@public
    nx_allocator allocator;
    NSView *view; /* Borrowed only until owner-thread detach. */
    nx_ax_snapshot *snapshot, *spare;
    NSMutableDictionary *elements;
    uint64_t context_identity, frame, incarnation, published_focus, publication;
    int publishing, published_native_focus;
    nx_ax_request requests[64];
    size_t request_count;
    size_t request_bytes, admission_bytes, admission_count;
    nx_ax_multi_lease *lease;
}
- (id)element:(uint64_t)identity;
- (NSArray *)children:(uint32_t)parent;
- (id)hit:(NSPoint)point;
- (id)focused;
- (BOOL)request:(NXMacAXElement *)element action:(uint32_t)action;
- (BOOL)requestSet:(NXMacAXElement *)owner children:(NSArray *)children rowsOnly:(BOOL)rows;
- (NSArray *)selected:(NXMacAXElement *)owner rowsOnly:(BOOL)rows;
- (nx_result)publish:(const nx_context *)context;
- (nx_result)route:(nx_context *)context count:(size_t *)count;
- (void)detach;
@end

@interface NXMacAXElement : NSAccessibilityElement {
@public
    NXMacAXBridge *bridge;
    uint64_t identity, incarnation;
}
- (nx_ax_node *)current;
- (void)retire;
@end

uint64_t nx_native_next_identity(void) {
    static atomic_uint_fast64_t sequence=0;
    uint_fast64_t before=atomic_load_explicit(&sequence,memory_order_relaxed);
    for (;;) {
        if (before==UINT64_MAX) return 0;
        if (atomic_compare_exchange_weak_explicit(&sequence,&before,before+1,
                memory_order_relaxed,memory_order_relaxed)) return before+1;
    }
}
static size_t slot(uint64_t id,size_t count) {
    return (size_t)((id^(id>>33))*UINT64_C(0xff51afd7ed558ccd))&(count-1u);
}
static uint32_t find(nx_ax_snapshot *s,uint64_t id) {
    if (!s || !id) return NX_AX_NONE;
    size_t at=slot(id,s->slot_count);
    while (s->slots[at]) {
        uint32_t index=s->slots[at]-1u;
        if (s->nodes[index].id==id) return index;
        at=(at+1u)&(s->slot_count-1u);
    }
    return NX_AX_NONE;
}
static int selected(nx_ax_node *n) {
    return !!(n->state&(n->role==NX_ROLE_RADIO ? NX_SEM_CHECKED : NX_SEM_SELECTED));
}
static int member(nx_ax_node *n) {
    return (n->actions&NX_ACTION_SELECT) && n->owner!=NX_AX_NONE &&
        (!n->owner_multiple || (n->membership && n->target.identity));
}
static int row_role(nx_semantic_role role) { return role==NX_ROLE_ROW || role==NX_ROLE_LIST_ITEM || role==NX_ROLE_TREE_ITEM; }
static int target_equal(nx_selection_target a,nx_selection_target b) { return a.identity==b.identity && a.revision==b.revision; }
static void lease_retain(nx_ax_multi_lease *l) { atomic_fetch_add_explicit(&l->references,1,memory_order_relaxed); }
static void lease_release(nx_ax_multi_lease *l) {
    if (l && atomic_fetch_sub_explicit(&l->references,1,memory_order_acq_rel)==1) free(l);
}
static void multi_retain(void *user) {
    nx_ax_multi_request *r=user; atomic_fetch_add_explicit(&r->references,1,memory_order_relaxed);
}
static void multi_release(void *user) {
    nx_ax_multi_request *r=user;
    if (r && atomic_fetch_sub_explicit(&r->references,1,memory_order_acq_rel)==1) {
        nx_ax_multi_lease *l=r->lease;free(r);lease_release(l);
    }
}
static uint64_t *multi_ids(nx_ax_multi_request *r) { return (uint64_t *)(r->members+r->member_count); }
static int multi_current(void *user,uint64_t token,uint64_t context,uint64_t id,
    uint64_t owner_id,nx_selection_target target) {
    if (!pthread_main_np()) return 0;
    nx_ax_multi_request *r=user;nx_ax_multi_lease *l=r->lease;
    if (!l->live || !l->snapshot || l->generation!=r->generation || token!=r->generation ||
        context!=r->context || l->context!=context || owner_id!=r->owner_id ||
        id!=(r->whole?r->owner_id:r->id) || !target_equal(target,r->target)) return 0;
    nx_ax_snapshot *s=l->snapshot;uint32_t group=find(s,owner_id);
    if (group==NX_AX_NONE) return 0;
    nx_ax_node *owner=&s->nodes[group];
    if (!(owner->state&NX_SEM_SELECTION_MULTIPLE) || (owner->state&NX_SEM_DISABLED) ||
        owner->incarnation!=r->owner_incarnation || owner->policy_epoch!=r->policy_epoch ||
        !target_equal(owner->target,r->target) || (r->whole && owner->member_count!=r->member_count)) return 0;
    for (size_t i=0;i<r->member_count;++i) {
        uint32_t at=find(s,r->members[i].id);if(at==NX_AX_NONE)return 0;
        nx_ax_node *n=&s->nodes[at];
        if (!member(n) || n->owner!=group || n->incarnation!=r->members[i].incarnation ||
            n->membership!=r->members[i].membership || !target_equal(n->target,r->target) ||
            (!r->whole && (n->state&NX_SEM_DISABLED))) return 0;
    }
    return 1;
}
static void remove_request(NXMacAXBridge *b,size_t at) {
    nx_ax_multi_request *r=b->requests[at].multi;
    if (r) { b->request_bytes-=r->bytes;multi_release(r); }
    --b->request_count;memmove(b->requests+at,b->requests+at+1,(b->request_count-at)*sizeof(*b->requests));
}
static void clear_requests(NXMacAXBridge *b) { while(b->request_count)remove_request(b,b->request_count-1); }
static int queue_space(NXMacAXBridge *b,size_t bytes,size_t own) {
    size_t slots=b->request_count+b->admission_count;
    size_t allocated=b->request_bytes+b->admission_bytes;
    return slots>=own && slots-own<64u && allocated<=NX_AX_MULTI_BYTES && bytes<=NX_AX_MULTI_BYTES-allocated;
}
static void wake_owner(void) {
    NSEvent *wake=[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0
        timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    if (wake) [NSApp postEvent:wake atStart:NO];
}
static nx_ax_multi_request *make_multi(NXMacAXBridge *b,nx_ax_node *owner,size_t members,int whole) {
    if(members>NX_AX_NODES)return NULL;
    size_t bytes=sizeof(nx_ax_multi_request)+members*(sizeof(nx_ax_multi_item)+(whole?sizeof(uint64_t):0u));
    if (bytes>NX_AX_MULTI_BYTES) return NULL;
    nx_ax_multi_request *r=calloc(1,bytes);if(!r)return NULL;
    atomic_init(&r->references,1);r->lease=b->lease;lease_retain(r->lease);
    r->generation=b->lease->generation;r->context=b->context_identity;r->owner_id=owner->id;
    r->owner_incarnation=owner->incarnation;r->policy_epoch=owner->policy_epoch;r->target=owner->target;
    r->bytes=bytes;r->member_count=members;r->whole=whole;return r;
}
static int same_ancestry(nx_ax_snapshot *a,uint32_t ai,nx_ax_snapshot *b,uint32_t bi) {
    while(ai!=NX_AX_NONE && bi!=NX_AX_NONE) {
        nx_ax_node *x=&a->nodes[ai],*y=&b->nodes[bi];
        if(x->id!=y->id || x->role!=y->role || x->incarnation!=y->incarnation)return 0;
        ai=x->parent;bi=y->parent;
    }
    return ai==bi;
}
static int selected_equal(nx_ax_snapshot *a,nx_ax_node *x,nx_ax_snapshot *b,nx_ax_node *y,int rows) {
    size_t before=0,after=0;
    for(uint32_t i=x->selected;i!=NX_AX_NONE;i=a->nodes[i].next_selected)
        if(!rows || row_role(a->nodes[i].role))++before;
    for(uint32_t i=y->selected;i!=NX_AX_NONE;i=b->nodes[i].next_selected) {
        nx_ax_node *n=&b->nodes[i];if(rows && !row_role(n->role))continue;
        ++after;uint32_t old=find(a,n->id);
        if(old==NX_AX_NONE || a->nodes[old].incarnation!=n->incarnation || !selected(&a->nodes[old]) ||
            a->nodes[old].owner!=(uint32_t)(x-a->nodes))return 0;
    }
    return before==after;
}
static int owner_policy_equal(nx_ax_snapshot *a,nx_ax_node *x,nx_ax_snapshot *b,nx_ax_node *y) {
    if(!target_equal(x->target,y->target) || x->member_count!=y->member_count ||
        ((x->state^y->state)&(NX_SEM_DISABLED|NX_SEM_SELECTION_REQUIRED)))return 0;
    uint32_t group=(uint32_t)(y-b->nodes);
    for(size_t i=0;i<b->count;++i) {
        nx_ax_node *n=&b->nodes[i];if(n->owner!=group || !member(n))continue;
        uint32_t old=find(a,n->id);if(old==NX_AX_NONE)return 0;
        nx_ax_node *p=&a->nodes[old];
        if(!member(p) || p->owner!=(uint32_t)(x-a->nodes) || p->incarnation!=n->incarnation ||
            ((p->state^n->state)&NX_SEM_DISABLED))return 0;
    }
    return 1;
}
static int choosing(uint32_t action) {
    return action==NX_ACTION_SELECT || action==NX_ACTION_ADD_SELECTION || action==NX_ACTION_DESELECT;
}
static uint32_t press_action(nx_ax_node *n) {
    return (n->actions&NX_ACTION_PRESS) ? NX_ACTION_PRESS : (n->actions&NX_ACTION_SELECT) ? NX_ACTION_SELECT : 0;
}
static NSString *role(nx_semantic_role value) {
    switch (value) {
    case NX_ROLE_TEXT: case NX_ROLE_STATUS: return NSAccessibilityStaticTextRole;
    case NX_ROLE_BUTTON: case NX_ROLE_COLUMN_HEADER: return NSAccessibilityButtonRole;
    case NX_ROLE_CHECKBOX: case NX_ROLE_SWITCH: return NSAccessibilityCheckBoxRole;
    case NX_ROLE_RADIO: case NX_ROLE_TAB: return NSAccessibilityRadioButtonRole;
    case NX_ROLE_SLIDER: return NSAccessibilitySliderRole;
    case NX_ROLE_TEXT_FIELD: return NSAccessibilityTextFieldRole;
    case NX_ROLE_IMAGE: return NSAccessibilityImageRole;
    case NX_ROLE_LIST: return NSAccessibilityListRole;
    case NX_ROLE_TABLE: return NSAccessibilityTableRole;
    case NX_ROLE_LIST_ITEM: case NX_ROLE_ROW: case NX_ROLE_TREE_ITEM: return NSAccessibilityRowRole;
    case NX_ROLE_CELL: return NSAccessibilityCellRole;
    case NX_ROLE_MENU: return NSAccessibilityMenuRole;
    case NX_ROLE_MENU_ITEM: return NSAccessibilityMenuItemRole;
    case NX_ROLE_MENU_BAR: return NSAccessibilityMenuBarRole;
    case NX_ROLE_TOOLBAR: return NSAccessibilityToolbarRole;
    case NX_ROLE_TREE: return NSAccessibilityOutlineRole;
    case NX_ROLE_PROGRESS: return NSAccessibilityProgressIndicatorRole;
    case NX_ROLE_SCROLL_AREA: return NSAccessibilityScrollAreaRole;
    case NX_ROLE_TAB_LIST: return NSAccessibilityTabGroupRole;
    default: return NSAccessibilityGroupRole;
    }
}
static NSString *string(const char *bytes) {
    return [[[NSString alloc] initWithBytes:bytes length:strlen(bytes) encoding:NSUTF8StringEncoding] autorelease];
}
static const char *value_string(nx_ax_node *n) {
    return n->role==NX_ROLE_TEXT && !n->value[0] ? n->name : n->value;
}
static int has_state_value(nx_ax_node *n) {
    return !(n->state&NX_SEM_PROTECTED) &&
        (n->role==NX_ROLE_RADIO || n->role==NX_ROLE_TAB || n->role==NX_ROLE_CHECKBOX || n->role==NX_ROLE_SWITCH);
}
static int state_value(nx_ax_node *n) {
    if (n->role==NX_ROLE_RADIO || n->role==NX_ROLE_TAB) return selected(n);
    return (n->state&NX_SEM_MIXED) ? 2 : !!(n->state&NX_SEM_CHECKED);
}
static int value_changed(nx_ax_node *a,nx_ax_node *b) {
    if (has_state_value(a)!=has_state_value(b)) return 1;
    if (has_state_value(b)) return state_value(a)!=state_value(b);
    return strcmp((a->state&NX_SEM_PROTECTED) ? "" : value_string(a),
                  (b->state&NX_SEM_PROTECTED) ? "" : value_string(b))!=0;
}
static NSRect logical(nx_ax_node *n) { return NSMakeRect(n->bounds.x,n->bounds.y,n->bounds.width,n->bounds.height); }
static NSRect screen(NXMacAXBridge *b,nx_ax_node *n) {
    NSWindow *window=[b->view window];
    return window ? [window convertRectToScreen:[b->view convertRect:logical(n) toView:nil]] : NSZeroRect;
}
static int hidden(NXMacAXBridge *b,nx_ax_node *n) {
    NSWindow *window=[b->view window];
    return (n->state&NX_SEM_OFFSCREEN) || !window || ![window isVisible] || [window isMiniaturized] ||
        NSIsEmptyRect(NSIntersectionRect(logical(n),[b->view bounds]));
}

@implementation NXMacAXElement
- (nx_ax_node *)current {
    if (!pthread_main_np() || !bridge) return NULL;
    uint32_t index=find(bridge->snapshot,identity);
    return index!=NX_AX_NONE && bridge->snapshot->nodes[index].incarnation==incarnation ? &bridge->snapshot->nodes[index] : NULL;
}
- (void)retire { NXMacAXBridge *old=bridge; bridge=nil; [old release]; }
- (void)dealloc { [bridge release]; [super dealloc]; }
- (BOOL)isAccessibilityElement { return [self current]!=NULL; }
- (BOOL)isAccessibilityEnabled { nx_ax_node *n=[self current]; return n && !(n->state&NX_SEM_DISABLED); }
- (BOOL)isAccessibilityHidden { nx_ax_node *n=[self current]; return !n || hidden(bridge,n); }
- (BOOL)isAccessibilityProtectedContent { nx_ax_node *n=[self current]; return n && (n->state&NX_SEM_PROTECTED); }
- (NSString *)accessibilityIdentifier {
    return [self current] ? [NSString stringWithFormat:@"nexa-%016llx",(unsigned long long)identity] : nil;
}
- (NSString *)accessibilityRole { nx_ax_node *n=[self current]; return n ? role(n->role) : nil; }
- (NSString *)accessibilitySubrole {
    nx_ax_node *n=[self current];
    if (!n) return nil;
    if (n->role==NX_ROLE_TEXT_FIELD && (n->state&NX_SEM_PROTECTED)) return NSAccessibilitySecureTextFieldSubrole;
    return n->role==NX_ROLE_DIALOG ? NSAccessibilityDialogSubrole : nil;
}
- (NSString *)accessibilityRoleDescription {
    return [self current] ? NSAccessibilityRoleDescription([self accessibilityRole],[self accessibilitySubrole]) : nil;
}
- (NSString *)accessibilityLabel { nx_ax_node *n=[self current]; return n ? string(n->name) : nil; }
- (NSString *)accessibilityTitle { return [self accessibilityLabel]; }
- (NSString *)accessibilityHelp { nx_ax_node *n=[self current]; return n ? string(n->help) : nil; }
- (id)accessibilityValue {
    nx_ax_node *n=[self current]; if (!n) return nil;
    if (n->state&NX_SEM_PROTECTED) return @"";
    if (has_state_value(n)) return @(state_value(n));
    return string(value_string(n));
}
- (NSString *)accessibilityValueDescription { nx_ax_node *n=[self current]; return n ? string(n->value) : nil; }
- (NSRect)accessibilityFrame { nx_ax_node *n=[self current]; return n ? screen(bridge,n) : NSZeroRect; }
- (id)accessibilityParent {
    nx_ax_node *n=[self current]; if (!n) return nil;
    return n->parent==NX_AX_NONE ? bridge->view : [bridge element:bridge->snapshot->nodes[n->parent].id];
}
- (id)accessibilityWindow { return [self current] ? [bridge->view window] : nil; }
- (id)accessibilityTopLevelUIElement { return [self accessibilityWindow]; }
- (NSArray *)accessibilityChildren {
    nx_ax_node *n=[self current]; return n ? [bridge children:(uint32_t)(n-bridge->snapshot->nodes)] : @[];
}
- (NSArray *)accessibilityVisibleChildren {
    NSMutableArray *visible=[NSMutableArray array];
    for (NXMacAXElement *child in [self accessibilityChildren]) if (![child isAccessibilityHidden]) [visible addObject:child];
    return visible;
}
- (BOOL)isAccessibilitySelected { nx_ax_node *n=[self current]; return n && selected(n); }
- (NSArray *)accessibilitySelectedChildren {
    if(!pthread_main_np())return nil;
    NXMacAXBridge *b=[bridge retain];NSArray *result=[b selected:self rowsOnly:NO];[b release];return result;
}
- (NSArray *)accessibilityTabs {
    nx_ax_node *n=[self current]; NSMutableArray *tabs=[NSMutableArray array];
    if (!n || n->role!=NX_ROLE_TAB_LIST) return tabs;
    uint32_t index=(uint32_t)(n-bridge->snapshot->nodes);
    for (size_t i=0;i<bridge->snapshot->count;++i)
        if (bridge->snapshot->nodes[i].owner==index && bridge->snapshot->nodes[i].role==NX_ROLE_TAB)
            [tabs addObject:[bridge element:bridge->snapshot->nodes[i].id]];
    return tabs;
}
- (NSArray *)accessibilityRows {
    nx_ax_node *n=[self current]; NSMutableArray *rows=[NSMutableArray array];
    if (!n || (n->role!=NX_ROLE_LIST && n->role!=NX_ROLE_TREE && n->role!=NX_ROLE_TABLE)) return rows;
    uint32_t owner=(uint32_t)(n-bridge->snapshot->nodes);
    for (size_t i=0;i<bridge->snapshot->count;++i) {
        nx_ax_node *item=&bridge->snapshot->nodes[i];
        if (item->role!=NX_ROLE_LIST_ITEM && item->role!=NX_ROLE_ROW && item->role!=NX_ROLE_TREE_ITEM) continue;
        uint32_t parent=item->parent;
        while (parent!=NX_AX_NONE && parent!=owner) {
            nx_ax_node *ancestor=&bridge->snapshot->nodes[parent];
            if (ancestor->role==NX_ROLE_LIST || ancestor->role==NX_ROLE_TREE || ancestor->role==NX_ROLE_TABLE) break;
            parent=ancestor->parent;
        }
        if (parent==owner) { id element=[bridge element:item->id]; if (element) [rows addObject:element]; }
    }
    return rows;
}
- (NSArray *)accessibilityVisibleRows {
    NSMutableArray *rows=[NSMutableArray array];
    for (id item in [self accessibilityRows]) if (![item isAccessibilityHidden]) [rows addObject:item];
    return rows;
}
- (NSArray *)accessibilitySelectedRows {
    if(!pthread_main_np())return nil;
    nx_ax_node *n=[self current];
    if(n && (n->state&NX_AX_SELECTION) && (n->role==NX_ROLE_LIST || n->role==NX_ROLE_TREE || n->role==NX_ROLE_TABLE)) {
        NXMacAXBridge *b=[bridge retain];NSArray *result=[b selected:self rowsOnly:YES];[b release];return result;
    }
    NSMutableArray *rows=[NSMutableArray array];
    for (id item in [self accessibilityRows]) if ([item isAccessibilitySelected]) [rows addObject:item];
    return rows;
}
- (void)setAccessibilitySelectedRows:(NSArray *)rows {
    nx_ax_node *n=[self current];
    if(n && (n->state&NX_SEM_SELECTION_MULTIPLE)) {
        NXMacAXBridge *b=[bridge retain];[b requestSet:self children:rows rowsOnly:YES];[b release];
    } else [self setAccessibilitySelectedChildren:rows];
}
- (BOOL)isAccessibilityFocused {
    nx_ax_node *n=[self current]; NSWindow *window=n ? [bridge->view window] : nil;
    return n && (n->state&NX_SEM_FOCUSED) && [window isKeyWindow] && [window firstResponder]==bridge->view;
}
- (BOOL)accessibilityPerformPress {
    nx_ax_node *n=[self current]; uint32_t action=n ? press_action(n) : 0;
    return action && [bridge request:self action:action];
}
- (void)setAccessibilityFocused:(BOOL)value {
    nx_ax_node *n=[self current]; if (!value || !n || (n->state&NX_SEM_DISABLED)) return;
    NXMacAXElement *target=self;
    if (n->state&NX_AX_SELECTION) {
        if (n->focus==NX_AX_NONE) return;
        target=[bridge element:bridge->snapshot->nodes[n->focus].id];
    }
    [bridge request:target action:NX_ACTION_FOCUS];
}
- (void)setAccessibilitySelected:(BOOL)value {
    nx_ax_node *n=[self current];
    if (n && member(n)) [bridge request:self action:value ?
        (n->owner_multiple?NX_ACTION_ADD_SELECTION:NX_ACTION_SELECT) : NX_ACTION_DESELECT];
}
- (void)setAccessibilitySelectedChildren:(NSArray *)children {
    nx_ax_node *n=[self current];
    if(n && (n->state&NX_SEM_SELECTION_MULTIPLE)) {
        NXMacAXBridge *b=[bridge retain];[b requestSet:self children:children rowsOnly:NO];[b release];return;
    }
    if (!n || !(n->state&NX_SEM_SELECTION_SINGLE) || (n->state&NX_SEM_DISABLED) ||
        ![children isKindOfClass:[NSArray class]] || [children count]>1) return;
    if (![children count]) {
        if (n->selected!=NX_AX_NONE) [[bridge element:bridge->snapshot->nodes[n->selected].id] setAccessibilitySelected:NO];
        return;
    }
    id child=[children objectAtIndex:0]; if (![child isKindOfClass:[NXMacAXElement class]]) return;
    NXMacAXElement *item=child; nx_ax_node *target=[item current];
    if (item->bridge==bridge && target && member(target) && target->owner==(uint32_t)(n-bridge->snapshot->nodes))
        [bridge request:item action:NX_ACTION_SELECT];
}
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
    nx_ax_node *n=[self current]; if (!n) return NO;
    int enabled=!(n->state&NX_SEM_DISABLED);
    if (selector==@selector(accessibilityPerformPress)) return enabled && press_action(n);
    if (selector==@selector(setAccessibilityFocused:))
        return enabled && ((n->actions&NX_ACTION_FOCUS) || ((n->state&NX_AX_SELECTION) && n->focus!=NX_AX_NONE));
    if (selector==@selector(setAccessibilitySelected:)) return enabled && member(n);
    if (selector==@selector(setAccessibilitySelectedChildren:)) return enabled && (n->state&NX_AX_SELECTION);
    if (selector==@selector(setAccessibilitySelectedRows:))
        return enabled && (n->state&NX_AX_SELECTION) && (n->role==NX_ROLE_LIST || n->role==NX_ROLE_TREE ||
            (n->role==NX_ROLE_TABLE && (n->state&NX_SEM_SELECTION_MULTIPLE)));
    NSString *name=NSStringFromSelector(selector);
    if ([name hasPrefix:@"setAccessibility"] || [name hasPrefix:@"accessibilityPerform"]) return NO;
    return [super isAccessibilitySelectorAllowed:selector];
}
@end

@implementation NXMacAXBridge
- (NSArray *)selected:(NXMacAXElement *)ownerElement rowsOnly:(BOOL)rows {
    nx_ax_node *owner=[ownerElement current];
    if(!owner || ownerElement->bridge!=self)return nil;
    if(!(owner->state&NX_AX_SELECTION))return @[];
    nx_ax_snapshot *s=snapshot;size_t count=0;
    for(uint32_t at=owner->selected;at!=NX_AX_NONE;at=s->nodes[at].next_selected)
        if(!rows || row_role(s->nodes[at].role))++count;
    if(!count)return @[];
    nx_ax_multi_item *copy=malloc(count*sizeof(*copy));if(!copy)return nil;
    size_t k=0;
    for(uint32_t at=owner->selected;at!=NX_AX_NONE;at=s->nodes[at].next_selected) {
        nx_ax_node *n=&s->nodes[at];if(rows && !row_role(n->role))continue;
        copy[k++]=(nx_ax_multi_item){n->id,n->incarnation,n->membership};
    }
    uint64_t generation=lease->generation, revision=publication;
    NSMutableArray *result=[NSMutableArray arrayWithCapacity:count];BOOL valid=YES;
    for(size_t i=0;i<count;++i) {
        NXMacAXElement *item=[self element:copy[i].id];
        if(publication!=revision || snapshot!=s || !lease->live || lease->generation!=generation || !item ||
            item->incarnation!=copy[i].incarnation || item->bridge!=self) {valid=NO;break;}
        [result addObject:item];
    }
    free(copy);
    if(!valid || publication!=revision || snapshot!=s || !lease->live || lease->generation!=generation)return nil;
    return [[result copy] autorelease];
}
- (BOOL)requestSet:(NXMacAXElement *)element children:(NSArray *)children rowsOnly:(BOOL)rows {
    if(!pthread_main_np())return NO;
    if(!children)children=@[];
    if(![children isKindOfClass:[NSArray class]])return NO;
    NSUInteger input=[children count];
    nx_ax_node *owner=[element current];
    if(!owner || element->bridge!=self || !(owner->state&NX_SEM_SELECTION_MULTIPLE) ||
        (owner->state&NX_SEM_DISABLED) || input>owner->member_count ||
        (rows && owner->role!=NX_ROLE_LIST && owner->role!=NX_ROLE_TREE && owner->role!=NX_ROLE_TABLE))return NO;
    nx_ax_snapshot *s=snapshot;uint32_t group=(uint32_t)(owner-s->nodes);
    /* The active/spare pair can return to the same address after two nested
       publications. Address and lease generation alone do not identify it. */
    uint64_t source_publication=publication;
    size_t source_count=s->count;
    size_t bytes=sizeof(nx_ax_multi_request)+owner->member_count*(sizeof(nx_ax_multi_item)+sizeof(uint64_t));
    size_t reservation=bytes+source_count;
    if(!queue_space(self,reservation,0))return NO;
    ++admission_count;admission_bytes+=reservation;
    nx_ax_multi_request *r=make_multi(self,owner,owner->member_count,1);
    unsigned char *wanted=calloc(source_count,1);BOOL valid=r && wanted,accepted=NO,queued=NO;
    if(valid) {
        size_t member_index=0;
        for(size_t i=0;i<source_count;++i) {
            nx_ax_node *n=&s->nodes[i];if(n->owner!=group || !member(n))continue;
            r->members[member_index++]=(nx_ax_multi_item){n->id,n->incarnation,n->membership};
            if(rows && !row_role(n->role) && selected(n))wanted[i]=1;
        }
        for(NSUInteger i=0;valid && i<input;++i) {
            id value=[children objectAtIndex:i];
            /* This is an application NSArray call-out. Check the monotone
               identity before dereferencing any borrowed snapshot pointer. */
            if(publication!=source_publication || snapshot!=s || !lease->live ||
                lease->generation!=r->generation || s->count!=source_count ||
                object_getClass(value)!=[NXMacAXElement class]) {valid=NO;break;}
            NXMacAXElement *item=value;uint32_t at=find(s,item->identity);
            if(item->bridge!=self || at==NX_AX_NONE || at>=source_count || item->incarnation!=s->nodes[at].incarnation ||
                !member(&s->nodes[at]) || s->nodes[at].owner!=group || wanted[at] ||
                (rows && !row_role(s->nodes[at].role))) {valid=NO;break;}
            wanted[at]=1;
        }
        for(size_t i=0;valid && i<source_count;++i) {
            nx_ax_node *n=&s->nodes[i];if(n->owner!=group || !member(n))continue;
            if((n->state&NX_SEM_DISABLED) && selected(n)!=!!wanted[i]) {valid=NO;break;}
            if(wanted[i])multi_ids(r)[r->desired_count++]=n->id;
        }
        if(valid && (owner->state&NX_SEM_SELECTION_REQUIRED) && !r->desired_count)valid=NO;
        if(valid && !multi_current(r,r->generation,r->context,r->owner_id,r->owner_id,r->target))valid=NO;
        BOOL busy=NO;
        for(size_t i=0;valid && i<request_count;++i) {
            nx_ax_request *old=&requests[i];if(old->owner_id!=r->owner_id)continue;
            busy=YES;nx_ax_multi_request *prior=old->multi;
            accepted=prior && prior->whole && prior->desired_count==r->desired_count &&
                target_equal(prior->target,r->target) && prior->policy_epoch==r->policy_epoch &&
                !memcmp(multi_ids(prior),multi_ids(r),r->desired_count*sizeof(uint64_t));break;
        }
        if(valid && !busy) {
            BOOL same=r->desired_count==owner->selected_count;
            for(uint32_t at=owner->selected;same && at!=NX_AX_NONE;at=s->nodes[at].next_selected)
                if(!wanted[at])same=NO;
            if(same)accepted=YES;
            else if(queue_space(self,0,1)) {
                nx_ax_request q={0};q.id=q.owner_id=r->owner_id;q.incarnation=q.owner_incarnation=r->owner_incarnation;
                q.multi=r;requests[request_count++]=q;request_bytes+=r->bytes;queued=accepted=YES;
            }
        }
    }
    free(wanted);--admission_count;admission_bytes-=reservation;
    if(!queued)multi_release(r);
    if(queued)wake_owner();return accepted;
}
- (id)element:(uint64_t)identity {
    if (!pthread_main_np()) return nil;
    uint32_t index=find(snapshot,identity); if (index==NX_AX_NONE) return nil;
    NSNumber *key=@(identity); NXMacAXElement *item=[elements objectForKey:key];
    if (!item) {
        item=[[NXMacAXElement alloc] init]; if (!item) return nil;
        item->bridge=[self retain]; item->identity=identity; item->incarnation=snapshot->nodes[index].incarnation;
        [elements setObject:item forKey:key]; [item release];
    }
    return item;
}
- (NSArray *)children:(uint32_t)parent {
    if (!pthread_main_np() || !snapshot) return @[];
    NSMutableArray *result=[NSMutableArray array];
    uint32_t index=parent==NX_AX_NONE ? snapshot->first : snapshot->nodes[parent].first;
    for (;index!=NX_AX_NONE;index=snapshot->nodes[index].next) {
        id item=[self element:snapshot->nodes[index].id]; if (item) [result addObject:item];
    }
    return result;
}
- (id)hit:(NSPoint)point {
    if (!pthread_main_np() || !snapshot || !view) return nil;
    for (size_t i=snapshot->count;i>0;--i) {
        nx_ax_node *n=&snapshot->nodes[i-1u];
        if (!hidden(self,n) && NSPointInRect(point,screen(self,n))) return [self element:n->id];
    }
    return view;
}
- (id)focused {
    if (!pthread_main_np() || !snapshot || ![[view window] isKeyWindow] || [[view window] firstResponder]!=view) return nil;
    for (size_t i=0;i<snapshot->count;++i)
        if (snapshot->nodes[i].state&NX_SEM_FOCUSED) return [self element:snapshot->nodes[i].id];
    return view;
}
- (BOOL)request:(NXMacAXElement *)element action:(uint32_t)action {
    nx_ax_node *n=[element current];
    if (!n || element->bridge!=self || (n->state&NX_SEM_DISABLED)) return NO;
    int selection=choosing(action) && member(n);
    if (choosing(action) && !selection && (action!=NX_ACTION_SELECT || !(n->actions&NX_ACTION_SELECT))) return NO;
    if (!selection && !(n->actions&action)) return NO;
    nx_ax_node *owner=selection ? &snapshot->nodes[n->owner] : NULL;
    if (owner) {
        if (owner->state&NX_SEM_DISABLED) return NO;
        for (size_t i=0;i<request_count;++i) {
            nx_ax_request *r=&requests[i];
            if (r->owner_id==owner->id && r->owner_incarnation==owner->incarnation)
                return r->id==n->id && r->incarnation==n->incarnation && r->action==action;
        }
        if(n->owner_multiple) {
            if((action==NX_ACTION_DESELECT && !selected(n)) || (action==NX_ACTION_ADD_SELECTION && selected(n)) ||
                (action==NX_ACTION_SELECT && selected(n) && owner->selected_count==1))return YES;
            if(action==NX_ACTION_DESELECT && owner->selected_count==1 && (owner->state&NX_SEM_SELECTION_REQUIRED))return NO;
        } else {
            if ((action==NX_ACTION_DESELECT && !selected(n)) || (action!=NX_ACTION_DESELECT && selected(n))) return YES;
            if ((action==NX_ACTION_DESELECT && (owner->state&NX_SEM_SELECTION_REQUIRED)) ||
                (action==NX_ACTION_ADD_SELECTION && owner->selected!=NX_AX_NONE)) return NO;
        }
    }
    if (action==NX_ACTION_FOCUS) {
        for (size_t i=0;i<request_count;) {
            if (requests[i].action==NX_ACTION_FOCUS) {
                remove_request(self,i);
            } else ++i;
        }
    }
    for (size_t i=0;i<request_count;++i)
        if (requests[i].id==n->id && requests[i].incarnation==n->incarnation && requests[i].action==action) return NO;
    size_t multi_bytes=owner && n->owner_multiple ? sizeof(nx_ax_multi_request)+sizeof(nx_ax_multi_item) : 0;
    if (!queue_space(self,multi_bytes,0)) return NO;
    nx_ax_request r={0}; r.id=n->id; r.incarnation=n->incarnation; r.action=action;
    if (owner) { r.owner_id=owner->id; r.owner_incarnation=owner->incarnation; }
    if(multi_bytes) {
        r.multi=make_multi(self,owner,1,0);if(!r.multi)return NO;
        r.multi->id=n->id;r.multi->incarnation=n->incarnation;r.multi->action=action;
        r.multi->members[0]=(nx_ax_multi_item){n->id,n->incarnation,n->membership};
        request_bytes+=r.multi->bytes;
    }
    requests[request_count++]=r;
    wake_owner();
    return YES;
}
- (void)detach {
    if (!pthread_main_np()) return;
    if(lease) {lease->live=0;lease->snapshot=NULL;lease->context=0;if(lease->generation!=UINT64_MAX)++lease->generation;}
    nx_ax_snapshot *active=snapshot,*inactive=spare;snapshot=spare=NULL;view=nil;
    clear_requests(self);
    NSArray *cached=[elements allValues];
    for (NXMacAXElement *item in cached) [item retire];
    [elements removeAllObjects];
    if (active) allocator.deallocate(allocator.user,active,active->capacity);
    if (inactive) allocator.deallocate(allocator.user,inactive,inactive->capacity);
}
- (void)dealloc { [elements release];lease_release(lease);[super dealloc]; }
- (nx_result)publish:(const nx_context *)context {
    if(!lease || !lease->live || !view)return NX_ERROR_STATE;
    uint64_t binding=nx_context_identity(context),source_frame=nx_context_frame_serial(context),generation=lease->generation;
    NSView *source_view=view;
    if(publication==UINT64_MAX)return NX_ERROR_LIMIT;
    if(context_identity!=binding && lease->generation==UINT64_MAX)return NX_ERROR_LIMIT;
    size_t count; nx_result result=nx_semantics_count(context,&count); if (result!=NX_OK) return result;
    if (count>NX_AX_NODES) return NX_ERROR_LIMIT;
    size_t slots=16; while (slots<count*2u) slots*=2u;
    size_t bytes=sizeof(nx_ax_snapshot)+count*sizeof(nx_ax_node)+slots*sizeof(uint32_t);
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v; if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        size_t lengths[3]={v.name.length,v.description.length,(v.state&NX_SEM_PROTECTED) ? 0 : v.value.length};
        for (unsigned j=0;j<3;++j) {
            if (lengths[j]>=NX_AX_BYTES || bytes>NX_AX_BYTES-lengths[j]-1u) return NX_ERROR_LIMIT;
            bytes+=lengths[j]+1u;
        }
    }
    nx_ax_snapshot *s=spare;
    if (!s || s->capacity<bytes) {
        size_t capacity=4096; while (capacity<bytes) capacity*=2u;
        /* Withdraw ownership before either user call-out. Detach must not free
           the spare a second time from inside its own deallocator. */
        nx_ax_snapshot *retired=spare;spare=NULL;
        s=allocator.allocate(allocator.user,capacity);
        int live=lease->live && lease->generation==generation && view==source_view &&
            nx_context_identity(context)==binding && nx_context_frame_serial(context)==source_frame;
        if(!s && live) {spare=retired;return NX_ERROR_MEMORY;}
        if(retired)allocator.deallocate(allocator.user,retired,retired->capacity);
        live=live && lease->live && lease->generation==generation && view==source_view &&
            nx_context_identity(context)==binding && nx_context_frame_serial(context)==source_frame;
        if(!live) {if(s)allocator.deallocate(allocator.user,s,capacity);return NX_ERROR_STATE;}
        if(!s)return NX_ERROR_MEMORY;
        s->capacity=capacity;
        spare=s;
    }
    s->count=count; s->slot_count=slots; s->first=NX_AX_NONE;
    s->slots=(uint32_t *)(s->nodes+count); memset(s->slots,0,slots*sizeof(uint32_t));
    char *text=(char *)(s->slots+slots);
    uint64_t next_incarnation=incarnation;
    uint32_t root_last=NX_AX_NONE;
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v; if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        nx_ax_node *n=&s->nodes[i]; *n=(nx_ax_node){0};
        n->id=v.id; n->role=v.role; n->state=v.state; n->actions=v.actions; n->bounds=v.bounds;
        n->first=n->last=n->next=n->owner=n->selected=n->focus=n->selected_last=n->next_selected=NX_AX_NONE;
        n->target=nx_context_selection_target(context,n->id);
        n->membership=nx_context_selection_membership(context,n->id);
        n->parent=find(s,v.parent_id);
        if (n->parent!=NX_AX_NONE) {
            nx_ax_node *parent=&s->nodes[n->parent];
            n->state|=parent->state&(NX_SEM_DISABLED|NX_SEM_OFFSCREEN);
            n->owner=(parent->state&NX_AX_SELECTION) ? n->parent : parent->owner;
            if(n->owner!=NX_AX_NONE)n->owner_multiple=!!(s->nodes[n->owner].state&NX_SEM_SELECTION_MULTIPLE);
        }
        if (member(n)) {
            nx_ax_node *owner=&s->nodes[n->owner];
            ++owner->member_count;
            if (selected(n)) {
                if(owner->selected_last!=NX_AX_NONE)s->nodes[owner->selected_last].next_selected=(uint32_t)i;
                else owner->selected=(uint32_t)i;
                owner->selected_last=(uint32_t)i;++owner->selected_count;
            }
            if ((n->actions&NX_ACTION_FOCUS) && !(n->state&NX_SEM_DISABLED) &&
                (owner->focus==NX_AX_NONE || (n->owner_multiple ? !!(n->state&NX_SEM_FOCUSED) : selected(n))))
                owner->focus=(uint32_t)i;
        }
        uint32_t old=context_identity==binding ? find(snapshot,n->id) : NX_AX_NONE;
        int same=old!=NX_AX_NONE && snapshot->nodes[old].role==n->role;
        if(same && ((n->state&NX_SEM_SELECTION_MULTIPLE) || n->owner_multiple ||
            (snapshot->nodes[old].state&NX_SEM_SELECTION_MULTIPLE) || snapshot->nodes[old].owner_multiple)) {
            nx_ax_node *prior=&snapshot->nodes[old];
            same=n->target.identity==prior->target.identity && n->membership==prior->membership &&
                n->owner_multiple==prior->owner_multiple &&
                same_ancestry(s,n->parent,snapshot,prior->parent);
        }
        if(same)n->incarnation=snapshot->nodes[old].incarnation;
        else { if (next_incarnation==UINT64_MAX) return NX_ERROR_LIMIT; n->incarnation=++next_incarnation; }
        nx_utf8_span spans[3]={v.name,v.description,(v.state&NX_SEM_PROTECTED) ? (nx_utf8_span){0} : v.value};
        const char **dest[3]={&n->name,&n->help,&n->value};
        for (unsigned j=0;j<3;++j) {
            *dest[j]=text; if (spans[j].length) memcpy(text,spans[j].data,spans[j].length);
            text[spans[j].length]=0; text+=spans[j].length+1u;
        }
        uint32_t *last=n->parent==NX_AX_NONE ? &root_last : &s->nodes[n->parent].last;
        uint32_t *first=n->parent==NX_AX_NONE ? &s->first : &s->nodes[n->parent].first;
        if (*last!=NX_AX_NONE) s->nodes[*last].next=(uint32_t)i; else *first=(uint32_t)i;
        *last=(uint32_t)i;
        size_t at=slot(n->id,slots); while (s->slots[at]) at=(at+1u)&(slots-1u);
        s->slots[at]=(uint32_t)i+1u;
    }
    for(size_t i=0;i<s->count;++i) {
        nx_ax_node *n=&s->nodes[i];if(!(n->state&NX_SEM_SELECTION_MULTIPLE))continue;
        uint32_t old=context_identity==binding?find(snapshot,n->id):NX_AX_NONE;
        if(old!=NX_AX_NONE && snapshot->nodes[old].incarnation==n->incarnation &&
            owner_policy_equal(snapshot,&snapshot->nodes[old],s,n))n->policy_epoch=snapshot->nodes[old].policy_epoch;
        else {n->policy_epoch=nx_native_next_identity();if(!n->policy_epoch)return NX_ERROR_LIMIT;}
    }
    int structure=!snapshot || snapshot->count!=s->count;
    uint64_t focus=0;
    for (size_t i=0;i<s->count;++i) {
        nx_ax_node *n=&s->nodes[i];
        if (!structure && (n->id!=snapshot->nodes[i].id || n->incarnation!=snapshot->nodes[i].incarnation ||
            n->parent!=snapshot->nodes[i].parent || (n->role==NX_ROLE_TEXT_FIELD &&
            ((n->state^snapshot->nodes[i].state)&NX_SEM_PROTECTED)))) structure=1;
        if (n->state&NX_SEM_FOCUSED) focus=n->id;
    }
    /* Retain all event identities before any AppKit call-out. A notification may
       reenter the application and detach/destroy its window and both buffers. */
    NSMutableArray *notifications=[NSMutableArray array];
    for (NXMacAXElement *item in [elements allValues]) {
        uint32_t next=find(s,item->identity), prior=find(snapshot,item->identity);
        if (next==NX_AX_NONE || s->nodes[next].incarnation!=item->incarnation) {
            [notifications addObject:@[item,NSAccessibilityUIElementDestroyedNotification]];
            [item retire]; [elements removeObjectForKey:@(item->identity)];
        } else if (prior!=NX_AX_NONE) {
            nx_ax_node *a=&snapshot->nodes[prior], *b=&s->nodes[next];
            if (value_changed(a,b))
                [notifications addObject:@[item,NSAccessibilityValueChangedNotification]];
            if ((b->state&NX_AX_SELECTION) && !selected_equal(snapshot,a,s,b,0)) {
                [notifications addObject:@[item,NSAccessibilitySelectedChildrenChangedNotification]];
                if ((b->role==NX_ROLE_LIST || b->role==NX_ROLE_TREE || b->role==NX_ROLE_TABLE) &&
                    !selected_equal(snapshot,a,s,b,1))
                    [notifications addObject:@[item,NSAccessibilitySelectedRowsChangedNotification]];
            }
        }
    }
    nx_ax_snapshot *old=snapshot; snapshot=s; spare=old; incarnation=next_incarnation;
    ++publication;
    if (context_identity!=binding) {++lease->generation;clear_requests(self);}
    context_identity=binding; frame=nx_context_frame_serial(context);
    lease->context=binding;lease->snapshot=s;
    for (size_t i=0;i<request_count;) {
        nx_ax_multi_request *r=requests[i].multi;
        if ((requests[i].forwarded && requests[i].frame!=frame) ||
            (r && !multi_current(r,r->generation,r->context,r->whole?r->owner_id:r->id,r->owner_id,r->target))) {
            remove_request(self,i);
        } else ++i;
    }
    int native_focus=[[view window] isKeyWindow] && [[view window] firstResponder]==view;
    if (focus && native_focus && (focus!=published_focus || !published_native_focus)) {
        id target=[self element:focus]; if (target) [notifications addObject:@[target,NSAccessibilityFocusedUIElementChangedNotification]];
    }
    published_focus=focus; published_native_focus=native_focus;
    NSView *root=[view retain];
    if (structure) NSAccessibilityPostNotification(root,NSAccessibilityLayoutChangedNotification);
    for (NSArray *event in notifications) NSAccessibilityPostNotification([event objectAtIndex:0],[event objectAtIndex:1]);
    [root release]; return NX_OK;
}
- (nx_result)route:(nx_context *)context count:(size_t *)count {
    *count=0;
    if (context_identity!=nx_context_identity(context)) return NX_ERROR_STATE;
    if (frame!=nx_context_frame_serial(context)) return NX_OK;
    size_t remaining=request_count; int focus_window=0;
    for (size_t i=0;i<remaining;++i) {
        size_t at=0; while (at<request_count && requests[at].forwarded) ++at;
        if (at==request_count) break;
        nx_ax_request r=requests[at]; uint32_t index=find(snapshot,r.id);
        if(r.multi) {
            nx_ax_multi_request *m=r.multi;multi_retain(m);
            int valid=multi_current(m,m->generation,m->context,m->whole?m->owner_id:m->id,m->owner_id,m->target);
            nx_result result=NX_OK;
            if(valid) {
                nx_selection_guard g={m,m->generation,multi_retain,multi_release,multi_current};
                result=m->whole ? nx_semantics_request_selection_set_guarded(context,m->owner_id,
                    multi_ids(m),m->desired_count,&g) :
                    nx_semantics_request_selection_guarded(context,m->id,m->action,&g);
            }
            /* Core allocation may detach, republish, or remove/reorder this
               native queue. Keep the C payload and find its exact identity. */
            at=0;while(at<request_count && requests[at].multi!=m)++at;
            int live=multi_current(m,m->generation,m->context,m->whole?m->owner_id:m->id,m->owner_id,m->target);
            if(valid && live && at<request_count && (result==NX_ERROR_LIMIT || result==NX_ERROR_MEMORY)) {
                multi_release(m);return result==NX_ERROR_LIMIT?NX_OK:result;
            }
            if(at<request_count) {
                if(valid && live && result==NX_OK) {
                    requests[at].forwarded=1;requests[at].frame=frame;++*count;
                } else remove_request(self,at);
            }
            multi_release(m);continue;
        }
        int valid=index!=NX_AX_NONE && snapshot->nodes[index].incarnation==r.incarnation;
        if (valid && r.owner_id) {
            nx_ax_node *n=&snapshot->nodes[index];
            valid=member(n) && snapshot->nodes[n->owner].id==r.owner_id &&
                snapshot->nodes[n->owner].incarnation==r.owner_incarnation;
        }
        nx_result result=valid ? nx_semantics_request_action(context,r.id,r.action) : NX_OK;
        if (result==NX_ERROR_LIMIT) break;
        if (result==NX_ERROR_STATE) {
            size_t nodes; if (nx_semantics_count(context,&nodes)!=NX_OK) return result;
        }
        if (valid && result==NX_OK) {
            ++*count;
            if (r.action==NX_ACTION_FOCUS) focus_window=1;
        }
        if (valid && result==NX_OK && r.owner_id) {
            requests[at].forwarded=1; requests[at].frame=frame;
        } else {
            remove_request(self,at);
        }
    }
    if (focus_window) {
        NSView *target=[view retain]; NSWindow *window=[[target window] retain];
        [window makeFirstResponder:target]; [window makeKeyWindow];
        [window release]; [target release];
    }
    return NX_OK;
}
@end

NXMacAXBridge *nx_mac_ax_create(NSView *view,nx_allocator allocator) {
    if (!pthread_main_np()) return nil;
    NXMacAXBridge *b=[[NXMacAXBridge alloc] init]; if (!b) return nil;
    b->lease=calloc(1,sizeof(*b->lease));
    if(!b->lease){[b release];return nil;}
    atomic_init(&b->lease->references,1);b->lease->generation=1;b->lease->live=1;
    b->elements=[[NSMutableDictionary alloc] init];
    if (!b->elements) { [b release]; return nil; }
    b->view=view; b->allocator=allocator; return b;
}
void nx_mac_ax_destroy(NXMacAXBridge *b) { if (b && pthread_main_np()) { [b detach]; [b release]; } }
nx_result nx_mac_ax_publish(NXMacAXBridge *b,const nx_context *context) {
    if (!pthread_main_np()) return NX_ERROR_STATE;
    if(!b || !context)return NX_ERROR_ARGUMENT;
    if(b->publishing)return NX_ERROR_STATE;
    [b retain];b->publishing=1;nx_result result=[b publish:context];b->publishing=0;[b release];return result;
}
int nx_mac_ax_matches(NXMacAXBridge *b,const nx_context *context) {
    return !b || (pthread_main_np() && b->context_identity==nx_context_identity(context));
}
nx_result nx_mac_ax_route(NXMacAXBridge *b,nx_context *context,size_t *count) {
    *count=0; if (!b) return NX_OK;
    if (!pthread_main_np()) return NX_ERROR_STATE;
    [b retain]; nx_result result=[b route:context count:count]; [b release]; return result;
}
NSArray *nx_mac_ax_children(NXMacAXBridge *b) { return [b children:NX_AX_NONE]; }
id nx_mac_ax_hit(NXMacAXBridge *b,NSPoint point) { return [b hit:point]; }
id nx_mac_ax_focus(NXMacAXBridge *b) { return [b focused]; }
