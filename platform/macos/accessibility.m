#import "accessibility.h"
#include "../../src/context_identity.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NX_AX_NONE UINT32_MAX
#define NX_AX_BYTES (32u*1024u*1024u)
#define NX_AX_NODES 65536u
typedef struct nx_ax_node {
    uint64_t id, incarnation;
    nx_semantic_role role;
    nx_rect bounds;
    uint32_t state, actions, parent, first, last, next, owner, selected, focus;
    const char *name, *help, *value;
} nx_ax_node;
typedef struct nx_ax_snapshot {
    size_t capacity, count, slot_count;
    uint32_t first, *slots;
    nx_ax_node nodes[];
} nx_ax_snapshot;
typedef struct nx_ax_request {
    uint64_t id, incarnation, owner_id, owner_incarnation, frame;
    uint32_t action;
    int forwarded;
} nx_ax_request;

@class NXMacAXElement;
@interface NXMacAXBridge : NSObject {
@public
    nx_allocator allocator;
    NSView *view; /* Borrowed only until owner-thread detach. */
    nx_ax_snapshot *snapshot, *spare;
    NSMutableDictionary *elements;
    uint64_t context_identity, frame, incarnation, published_focus;
    int publishing, published_native_focus;
    nx_ax_request requests[64];
    size_t request_count;
}
- (id)element:(uint64_t)identity;
- (NSArray *)children:(uint32_t)parent;
- (id)hit:(NSPoint)point;
- (id)focused;
- (BOOL)request:(NXMacAXElement *)element action:(uint32_t)action;
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
static int member(nx_ax_node *n) { return (n->actions&NX_ACTION_SELECT) && n->owner!=NX_AX_NONE; }
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
    return !window || ![window isVisible] || [window isMiniaturized] ||
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
    nx_ax_node *n=[self current];
    if (!n || !(n->state&NX_SEM_SELECTION_SINGLE) || n->selected==NX_AX_NONE) return @[];
    id item=[bridge element:bridge->snapshot->nodes[n->selected].id]; return item ? @[item] : @[];
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
    NSMutableArray *rows=[NSMutableArray array];
    for (id item in [self accessibilityRows]) if ([item isAccessibilitySelected]) [rows addObject:item];
    return rows;
}
- (void)setAccessibilitySelectedRows:(NSArray *)rows { [self setAccessibilitySelectedChildren:rows]; }
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
    if (n->state&NX_SEM_SELECTION_SINGLE) {
        if (n->focus==NX_AX_NONE) return;
        target=[bridge element:bridge->snapshot->nodes[n->focus].id];
    }
    [bridge request:target action:NX_ACTION_FOCUS];
}
- (void)setAccessibilitySelected:(BOOL)value {
    nx_ax_node *n=[self current];
    if (n && member(n)) [bridge request:self action:value ? NX_ACTION_SELECT : NX_ACTION_DESELECT];
}
- (void)setAccessibilitySelectedChildren:(NSArray *)children {
    nx_ax_node *n=[self current];
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
        return enabled && ((n->actions&NX_ACTION_FOCUS) || ((n->state&NX_SEM_SELECTION_SINGLE) && n->focus!=NX_AX_NONE));
    if (selector==@selector(setAccessibilitySelected:)) return enabled && member(n);
    if (selector==@selector(setAccessibilitySelectedChildren:)) return enabled && (n->state&NX_SEM_SELECTION_SINGLE);
    if (selector==@selector(setAccessibilitySelectedRows:))
        return enabled && (n->state&NX_SEM_SELECTION_SINGLE) && (n->role==NX_ROLE_LIST || n->role==NX_ROLE_TREE);
    NSString *name=NSStringFromSelector(selector);
    if ([name hasPrefix:@"setAccessibility"] || [name hasPrefix:@"accessibilityPerform"]) return NO;
    return [super isAccessibilitySelectorAllowed:selector];
}
@end

@implementation NXMacAXBridge
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
        if ((action==NX_ACTION_DESELECT && !selected(n)) || (action!=NX_ACTION_DESELECT && selected(n))) return YES;
        if ((action==NX_ACTION_DESELECT && (owner->state&NX_SEM_SELECTION_REQUIRED)) ||
            (action==NX_ACTION_ADD_SELECTION && owner->selected!=NX_AX_NONE)) return NO;
    }
    if (action==NX_ACTION_FOCUS) {
        for (size_t i=0;i<request_count;) {
            if (requests[i].action==NX_ACTION_FOCUS) {
                --request_count; memmove(requests+i,requests+i+1,(request_count-i)*sizeof(*requests));
            } else ++i;
        }
    }
    for (size_t i=0;i<request_count;++i)
        if (requests[i].id==n->id && requests[i].incarnation==n->incarnation && requests[i].action==action) return NO;
    if (request_count==64) return NO;
    nx_ax_request r={0}; r.id=n->id; r.incarnation=n->incarnation; r.action=action;
    if (owner) { r.owner_id=owner->id; r.owner_incarnation=owner->incarnation; }
    requests[request_count++]=r;
    NSEvent *wake=[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0
        timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    if (wake) [NSApp postEvent:wake atStart:NO];
    return YES;
}
- (void)detach {
    if (!pthread_main_np()) return;
    NSArray *cached=[elements allValues];
    for (NXMacAXElement *item in cached) [item retire];
    [elements removeAllObjects]; view=nil; request_count=0;
    if (snapshot) allocator.deallocate(allocator.user,snapshot,snapshot->capacity);
    if (spare) allocator.deallocate(allocator.user,spare,spare->capacity);
    snapshot=spare=NULL;
}
- (void)dealloc { [elements release]; [super dealloc]; }
- (nx_result)publish:(const nx_context *)context {
    if (publishing) return NX_ERROR_STATE;
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
        s=allocator.allocate(allocator.user,capacity); if (!s) return NX_ERROR_MEMORY;
        s->capacity=capacity;
        if (spare) allocator.deallocate(allocator.user,spare,spare->capacity);
        spare=s;
    }
    s->count=count; s->slot_count=slots; s->first=NX_AX_NONE;
    s->slots=(uint32_t *)(s->nodes+count); memset(s->slots,0,slots*sizeof(uint32_t));
    char *text=(char *)(s->slots+slots);
    uint64_t next_incarnation=incarnation, binding=nx_context_identity(context);
    uint32_t root_last=NX_AX_NONE;
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v; if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        nx_ax_node *n=&s->nodes[i]; *n=(nx_ax_node){0};
        n->id=v.id; n->role=v.role; n->state=v.state; n->actions=v.actions; n->bounds=v.bounds;
        n->first=n->last=n->next=n->owner=n->selected=n->focus=NX_AX_NONE;
        n->parent=find(s,v.parent_id);
        if (n->parent!=NX_AX_NONE) {
            nx_ax_node *parent=&s->nodes[n->parent];
            n->state|=parent->state&NX_SEM_DISABLED;
            n->owner=(parent->state&NX_SEM_SELECTION_SINGLE) ? n->parent : parent->owner;
        }
        if (member(n)) {
            nx_ax_node *owner=&s->nodes[n->owner];
            if (selected(n)) owner->selected=(uint32_t)i;
            if ((n->actions&NX_ACTION_FOCUS) && !(n->state&NX_SEM_DISABLED) && (owner->focus==NX_AX_NONE || selected(n)))
                owner->focus=(uint32_t)i;
        }
        uint32_t old=context_identity==binding ? find(snapshot,n->id) : NX_AX_NONE;
        if (old!=NX_AX_NONE && snapshot->nodes[old].role==n->role) n->incarnation=snapshot->nodes[old].incarnation;
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
            uint64_t before=a->selected==NX_AX_NONE ? 0 : snapshot->nodes[a->selected].id;
            uint64_t after=b->selected==NX_AX_NONE ? 0 : s->nodes[b->selected].id;
            uint64_t before_inc=a->selected==NX_AX_NONE ? 0 : snapshot->nodes[a->selected].incarnation;
            uint64_t after_inc=b->selected==NX_AX_NONE ? 0 : s->nodes[b->selected].incarnation;
            if ((b->state&NX_SEM_SELECTION_SINGLE) && (before!=after || before_inc!=after_inc)) {
                [notifications addObject:@[item,NSAccessibilitySelectedChildrenChangedNotification]];
                if (b->role==NX_ROLE_LIST || b->role==NX_ROLE_TREE)
                    [notifications addObject:@[item,NSAccessibilitySelectedRowsChangedNotification]];
            }
        }
    }
    nx_ax_snapshot *old=snapshot; snapshot=s; spare=old; incarnation=next_incarnation;
    if (context_identity!=binding) request_count=0;
    context_identity=binding; frame=nx_context_frame_serial(context);
    for (size_t i=0;i<request_count;) {
        if (requests[i].forwarded && requests[i].frame!=frame) {
            --request_count; memmove(requests+i,requests+i+1,(request_count-i)*sizeof(*requests));
        } else ++i;
    }
    int native_focus=[[view window] isKeyWindow] && [[view window] firstResponder]==view;
    if (focus && native_focus && (focus!=published_focus || !published_native_focus)) {
        id target=[self element:focus]; if (target) [notifications addObject:@[target,NSAccessibilityFocusedUIElementChangedNotification]];
    }
    published_focus=focus; published_native_focus=native_focus;
    NSView *root=[view retain]; publishing=1;
    if (structure) NSAccessibilityPostNotification(root,NSAccessibilityLayoutChangedNotification);
    for (NSArray *event in notifications) NSAccessibilityPostNotification([event objectAtIndex:0],[event objectAtIndex:1]);
    publishing=0; [root release]; return NX_OK;
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
            --request_count; memmove(requests+at,requests+at+1,(request_count-at)*sizeof(*requests));
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
    b->elements=[[NSMutableDictionary alloc] init];
    if (!b->elements) { [b release]; return nil; }
    b->view=view; b->allocator=allocator; return b;
}
void nx_mac_ax_destroy(NXMacAXBridge *b) { if (b && pthread_main_np()) { [b detach]; [b release]; } }
nx_result nx_mac_ax_publish(NXMacAXBridge *b,const nx_context *context) {
    if (!pthread_main_np()) return NX_ERROR_STATE;
    [b retain]; nx_result result=[b publish:context]; [b release]; return result;
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
