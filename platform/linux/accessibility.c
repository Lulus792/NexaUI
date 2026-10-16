#define _POSIX_C_SOURCE 200809L
#include "accessibility.h"
#include "../../src/context_identity.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <time.h>

/* Linux uses a mutex instead of a 64-bit atomic to avoid an extra libatomic
   dependency on 32-bit targets. Zero means the sequence is exhausted. */
uint64_t nx_native_next_identity(void) {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static uint64_t sequence;
    pthread_mutex_lock(&mutex);
    uint64_t value = sequence == UINT64_MAX ? 0 : ++sequence;
    pthread_mutex_unlock(&mutex);
    return value;
}

#ifndef NEXAUI_HAVE_DBUS_SDK
nx_result nx_linux_ax_publish(nx_linux_ax **b, nx_window *w, nx_allocator a,
                             const nx_context *c) {
    (void)b; (void)w; (void)a; (void)c; return NX_ERROR_UNSUPPORTED;
}
void nx_linux_ax_destroy(nx_linux_ax *b) { (void)b; }
int nx_linux_ax_matches(nx_linux_ax *b, const nx_context *c) { (void)b; (void)c; return 1; }
nx_result nx_linux_ax_route(nx_linux_ax *b, nx_context *c, size_t *n) {
    (void)b; (void)c; *n=0; return NX_OK;
}
int nx_linux_ax_pump(nx_linux_ax *b) { (void)b; return 0; }
int nx_linux_ax_fd(nx_linux_ax *b, int *w) { (void)b; *w=0; return -1; }
#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_linux_ax_test_disconnect(nx_linux_ax *b) { (void)b; return NX_ERROR_UNSUPPORTED; }
#endif
#else
#include "dbus_api.h"
#include <dlfcn.h>

#define NX_AX_NONE UINT32_MAX
#define NX_AX_LIMIT (32u * 1024u * 1024u)
#define NX_AX_ROOT "/org/a11y/atspi/accessible/root"
#define NX_AX_NULL "/org/a11y/atspi/null"
#define NX_AX_CACHE "/org/a11y/atspi/cache"
#define NX_AX_ACC "org.a11y.atspi.Accessible"
#define NX_AX_APP "org.a11y.atspi.Application"
#define NX_AX_COMP "org.a11y.atspi.Component"
#define NX_AX_ACT "org.a11y.atspi.Action"
#define NX_AX_SEL "org.a11y.atspi.Selection"
#define NX_AX_CACHE_IF "org.a11y.atspi.Cache"
#define NX_AX_PROPS "org.freedesktop.DBus.Properties"

typedef struct nx_ax_node {
    uint64_t id, incarnation;
    nx_rect bounds;
    uint32_t parent, raw_parent, first, last, next, owner, selected, child_count, index;
    nx_semantic_role role;
    uint32_t state, actions;
    char *name, *description, *value;
} nx_ax_node;
typedef struct nx_ax_snapshot {
    size_t capacity, count, slots_count;
    uint32_t first, root_count;
    uint32_t *slots;
    nx_ax_node nodes[];
} nx_ax_snapshot;
typedef struct nx_ax_request {
    uint64_t id, incarnation, owner, owner_incarnation, frame;
    uint32_t action;
    int forwarded;
} nx_ax_request;
typedef struct nx_ax_hub nx_ax_hub;
struct nx_linux_ax {
    nx_allocator allocator;
    pthread_t thread;
    nx_window *window;
    nx_ax_hub *hub;
    nx_linux_ax *next;
    uint64_t epoch, binding, frame, incarnation;
    nx_ax_snapshot *snapshot, *spare;
    nx_ax_request requests[64];
    size_t request_count;
    nx_linux_ax_geometry geometry;
};
struct nx_ax_hub {
    DBusConnection *connection;
    DBusPendingCall *embed;
    nx_linux_ax *windows;
    char desktop_bus[256], desktop_path[256];
    char application_name[1024];
    dbus_int32_t application_id;
    uint64_t retry_after;
    int failed;
};
/* One UI thread owns each connection and all its snapshots. No D-Bus worker
   thread reads application state. Separate UI threads use separate endpoints. */
static _Thread_local nx_ax_hub nx_ax_local_hub;
static _Thread_local nx_ax_hub *nx_ax_thread_hub;
static nx_dbus_api d;
static pthread_once_t nx_ax_loader_once=PTHREAD_ONCE_INIT;
static int nx_ax_loaded;
static void nx_ax_load(void) {
    void *library=dlopen("libdbus-1.so.3", RTLD_NOW|RTLD_LOCAL);
    if (!library) return;
    int complete=1;
#define NX_DBUS_LOAD(result, name, args) do { \
    void *symbol=dlsym(library,"dbus_" #name); \
    _Static_assert(sizeof(symbol)==sizeof(d.name),"Linux function pointer ABI"); \
    memcpy(&d.name,&symbol,sizeof(symbol)); if (!symbol) complete=0; \
} while (0);
    NX_DBUS_FUNCTIONS(NX_DBUS_LOAD)
#undef NX_DBUS_LOAD
    if (!complete || !d.threads_init_default()) { memset(&d,0,sizeof(d)); dlclose(library); return; }
    /* Process-lifetime SDK handle: callbacks/function pointers remain valid. */
    nx_ax_loaded=1;
}
static uint64_t nx_ax_now(void) {
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)==0 ?
        (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u : 0;
}
static size_t nx_ax_slot(uint64_t id, size_t count) {
    id^=id>>33; id*=UINT64_C(0xff51afd7ed558ccd); id^=id>>33;
    return (size_t)id&(count-1u);
}
static uint32_t nx_ax_find(const nx_ax_snapshot *s,uint64_t id) {
    if (!s || !id) return NX_AX_NONE;
    size_t at=nx_ax_slot(id,s->slots_count);
    while (s->slots[at]) {
        uint32_t i=s->slots[at]-1u;
        if (s->nodes[i].id==id) return i;
        at=(at+1u)&(s->slots_count-1u);
    }
    return NX_AX_NONE;
}
static int nx_ax_member(const nx_ax_node *n) {
    return n && n->owner!=NX_AX_NONE && (n->actions&NX_ACTION_SELECT);
}
static int nx_ax_selected(const nx_ax_node *n) {
    return n && (n->state&(n->role==NX_ROLE_RADIO?NX_SEM_CHECKED:NX_SEM_SELECTED));
}
static nx_result nx_ax_copy(nx_linux_ax *b,const nx_context *context) {
    size_t count=0;
    nx_result result=nx_semantics_count(context,&count);
    if (result!=NX_OK) return result;
    if (count>65536u) return NX_ERROR_LIMIT;
    size_t slots=2; while (slots<count*2u) slots*=2u;
    size_t bytes=sizeof(nx_ax_snapshot)+count*sizeof(nx_ax_node)+slots*sizeof(uint32_t);
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v;
        if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        size_t lengths[]={v.name.length,v.description.length,(v.state&NX_SEM_PROTECTED)?0:v.value.length};
        for (unsigned j=0;j<3;++j) {
            if (lengths[j]>=NX_AX_LIMIT || bytes>NX_AX_LIMIT-lengths[j]-1u) return NX_ERROR_LIMIT;
            bytes+=lengths[j]+1u;
        }
    }
    nx_ax_snapshot *s=b->spare;
    if (!s || s->capacity<bytes) {
        size_t capacity=4096; while (capacity<bytes) capacity*=2u;
        s=b->allocator.allocate(b->allocator.user,capacity);
        if (!s) return NX_ERROR_MEMORY;
        s->capacity=capacity;
        if (b->spare) b->allocator.deallocate(b->allocator.user,b->spare,b->spare->capacity);
        b->spare=s;
    }
    s->count=count; s->slots_count=slots; s->first=NX_AX_NONE; s->root_count=0;
    s->slots=(uint32_t *)(s->nodes+count); memset(s->slots,0,slots*sizeof(uint32_t));
    char *text=(char *)(s->slots+slots);
    uint64_t binding=nx_context_identity(context), incarnation=b->incarnation;
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v;
        if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        nx_ax_node *n=&s->nodes[i]; *n=(nx_ax_node){0};
        n->id=v.id; n->role=v.role; n->state=v.state; n->actions=v.actions; n->bounds=v.bounds;
        n->raw_parent=n->parent=nx_ax_find(s,v.parent_id);
        n->first=n->last=n->next=n->owner=n->selected=NX_AX_NONE;
        if (n->raw_parent!=NX_AX_NONE) {
            nx_ax_node *p=&s->nodes[n->raw_parent];
            n->state|=p->state&NX_SEM_DISABLED;
            n->owner=(p->state&NX_SEM_SELECTION_SINGLE) ? n->raw_parent : p->owner;
        }
        /* AT-SPI Selection indices address direct native children. Explicit
           selection members bypass intervening layout panels in this projection;
           raw_parent still controls disabled inheritance and scroll clipping. */
        if (nx_ax_member(n)) {
            n->parent=n->owner;
            if (nx_ax_selected(n)) s->nodes[n->owner].selected=(uint32_t)i;
        }
        uint32_t old=binding==b->binding ? nx_ax_find(b->snapshot,n->id) : NX_AX_NONE;
        if (old!=NX_AX_NONE && b->snapshot->nodes[old].role==n->role &&
            !((b->snapshot->nodes[old].state^n->state)&NX_SEM_PROTECTED))
            n->incarnation=b->snapshot->nodes[old].incarnation;
        else { if (incarnation==UINT64_MAX) return NX_ERROR_LIMIT; n->incarnation=++incarnation; }
        nx_utf8_span spans[]={v.name,v.description,(v.state&NX_SEM_PROTECTED)?(nx_utf8_span){0}:v.value};
        char **destinations[]={&n->name,&n->description,&n->value};
        for (unsigned j=0;j<3;++j) {
            *destinations[j]=text;
            if (spans[j].length) memcpy(text,spans[j].data,spans[j].length);
            text[spans[j].length]=0; text+=spans[j].length+1u;
        }
        size_t at=nx_ax_slot(n->id,slots); while (s->slots[at]) at=(at+1u)&(slots-1u);
        s->slots[at]=(uint32_t)i+1u;
    }
    uint32_t last=NX_AX_NONE;
    for (size_t i=0;i<count;++i) {
        nx_ax_node *n=&s->nodes[i];
        n->index=n->parent==NX_AX_NONE?s->root_count++:s->nodes[n->parent].child_count++;
        uint32_t *first=n->parent==NX_AX_NONE ? &s->first : &s->nodes[n->parent].first;
        uint32_t *tail=n->parent==NX_AX_NONE ? &last : &s->nodes[n->parent].last;
        if (*tail!=NX_AX_NONE) s->nodes[*tail].next=(uint32_t)i;
        else *first=(uint32_t)i;
        *tail=(uint32_t)i;
    }
    return NX_OK;
}

/* A reference is application root, native window, or a semantic node. */
typedef struct nx_ax_ref { nx_ax_hub *hub; nx_linux_ax *window; uint32_t index; } nx_ax_ref;
static nx_ax_ref nx_ax_app(nx_ax_hub *h) { return (nx_ax_ref){h,NULL,NX_AX_NONE}; }
static nx_ax_ref nx_ax_window(nx_linux_ax *b) { return (nx_ax_ref){b->hub,b,NX_AX_NONE}; }
static nx_ax_node *nx_ax_node_at(nx_ax_ref r) {
    return r.window && r.index!=NX_AX_NONE ? &r.window->snapshot->nodes[r.index] : NULL;
}
static void nx_ax_path(nx_ax_ref r,char path[128]) {
    nx_ax_node *n=nx_ax_node_at(r);
    if (!r.window) snprintf(path,128,"%s",NX_AX_ROOT);
    else if (!n) snprintf(path,128,"/org/a11y/atspi/accessible/w%llu",(unsigned long long)r.window->epoch);
    else snprintf(path,128,"/org/a11y/atspi/accessible/w%llu/n%llu_%llu",
        (unsigned long long)r.window->epoch,(unsigned long long)n->id,(unsigned long long)n->incarnation);
}
static const char *nx_ax_name(nx_ax_ref r) {
    nx_ax_node *n=nx_ax_node_at(r); if (n) return n->name;
    if (!r.window) return r.hub->application_name[0]?r.hub->application_name:"NexaUI";
    nx_linux_ax *b=r.window;
    return b && b->snapshot && b->snapshot->first!=NX_AX_NONE ?
        b->snapshot->nodes[b->snapshot->first].name : "NexaUI";
}
static nx_ax_ref nx_ax_parent(nx_ax_ref r) {
    nx_ax_node *n=nx_ax_node_at(r);
    if (!n) return nx_ax_app(r.hub);
    return n->parent==NX_AX_NONE ? nx_ax_window(r.window) :
        (nx_ax_ref){r.hub,r.window,n->parent};
}
static int nx_ax_children(nx_ax_ref r) {
    int count=0;
    if (!r.window) { for (nx_linux_ax *b=r.hub->windows;b;b=b->next) ++count; }
    else {
        nx_ax_node *n=nx_ax_node_at(r);
        count=(int)(n?n->child_count:r.window->snapshot->root_count);
    }
    return count;
}
static int nx_ax_child(nx_ax_ref r,int index,nx_ax_ref *out) {
    if (index<0) return 0;
    if (!r.window) {
        nx_linux_ax *b=r.hub->windows; while (b && index--) b=b->next;
        if (!b) return 0;
        *out=nx_ax_window(b); return 1;
    }
    nx_ax_node *n=nx_ax_node_at(r);
    uint32_t i=n?n->first:r.window->snapshot->first;
    while (i!=NX_AX_NONE && index--) i=r.window->snapshot->nodes[i].next;
    if (i==NX_AX_NONE) return 0;
    *out=(nx_ax_ref){r.hub,r.window,i}; return 1;
}
static int nx_ax_index(nx_ax_ref r) {
    if (!r.window) return -1;
    nx_ax_node *n=nx_ax_node_at(r); if (n) return (int)n->index;
    nx_ax_ref p=nx_ax_parent(r), child; int index=0;
    while (nx_ax_child(p,index,&child)) {
        if (child.window==r.window && child.index==r.index) return index;
        ++index;
    }
    return -1;
}
static int nx_ax_resolve(nx_ax_hub *h,const char *path,nx_ax_ref *out) {
    if (!strcmp(path,NX_AX_ROOT)) { *out=nx_ax_app(h); return 1; }
    for (nx_linux_ax *b=h->windows;b;b=b->next) {
        char prefix[128]; nx_ax_path(nx_ax_window(b),prefix);
        size_t len=strlen(prefix);
        if (strncmp(path,prefix,len)) continue;
        if (!path[len]) { *out=nx_ax_window(b); return 1; }
        const char *suffix=path+len;
        if (suffix[0]!='/' || suffix[1]!='n') continue;
        suffix+=2;
        uint64_t values[2]={0}; int valid=1;
        for (unsigned part=0;part<2 && valid;++part) {
            unsigned digits=0;
            while (*suffix>='0' && *suffix<='9') {
                unsigned digit=(unsigned)(*suffix++-'0');
                if (++digits>20 || values[part]>(UINT64_MAX-digit)/10u) { valid=0; break; }
                values[part]=values[part]*10u+digit;
            }
            if (!digits) valid=0;
            if (part==0) { if (*suffix!='_') valid=0; else ++suffix; }
        }
        if (!valid || *suffix) continue;
        uint32_t index=nx_ax_find(b->snapshot,values[0]);
        if (index!=NX_AX_NONE && b->snapshot->nodes[index].incarnation==values[1]) {
            *out=(nx_ax_ref){h,b,index}; return 1;
        }
    }
    return 0;
}

/* Numeric role/state assignments are the stable AT-SPI public protocol, checked
   against the OS SDK by the independent libatspi client tests. No ATK objects. */
static uint32_t nx_ax_role(nx_ax_ref r) {
    nx_ax_node *n=nx_ax_node_at(r);
    if (!r.window) return 75; /* APPLICATION */
    if (!n) return 23; /* FRAME */
    switch (n->role) {
    case NX_ROLE_WINDOW: case NX_ROLE_PANEL: case NX_ROLE_TAB_PANEL: return 39;
    case NX_ROLE_GROUP: return 99;
    case NX_ROLE_TEXT: return 29;
    case NX_ROLE_BUTTON: return 43;
    case NX_ROLE_CHECKBOX: return 7;
    case NX_ROLE_RADIO: return 44;
    case NX_ROLE_SWITCH: return 130;
    case NX_ROLE_SLIDER: return 51;
    case NX_ROLE_TEXT_FIELD: return n->state&NX_SEM_PROTECTED ? 40 : 79;
    case NX_ROLE_IMAGE: return 27;
    case NX_ROLE_LIST: return 31;
    case NX_ROLE_LIST_ITEM: return 32;
    case NX_ROLE_TABLE: return 55;
    case NX_ROLE_ROW: return 90;
    case NX_ROLE_CELL: return 56;
    case NX_ROLE_TAB: return 37;
    case NX_ROLE_MENU: case NX_ROLE_POPUP: return 41;
    case NX_ROLE_MENU_ITEM: return 35;
    case NX_ROLE_TOOLBAR: return 63;
    case NX_ROLE_STATUS: return 54;
    case NX_ROLE_TREE: return 65;
    case NX_ROLE_TREE_ITEM: return 91;
    case NX_ROLE_PROGRESS: return 42;
    case NX_ROLE_SEPARATOR: return 50;
    case NX_ROLE_SCROLL_AREA: return 49;
    case NX_ROLE_COLUMN_HEADER: return 57;
    case NX_ROLE_DIALOG: return 16;
    case NX_ROLE_TOOLTIP: return 64;
    case NX_ROLE_MENU_BAR: return 34;
    case NX_ROLE_TAB_LIST: return 38;
    default: return 67;
    }
}
static const char *nx_ax_role_name(nx_ax_ref r) {
    switch (nx_ax_role(r)) {
    case 75:return "application"; case 23:return "frame"; case 39:return "panel";
    case 99:return "grouping"; case 29:return "label"; case 43:return "push button";
    case 7:return "check box"; case 44:return "radio button"; case 130:return "switch";
    case 51:return "slider"; case 40:return "password text"; case 79:return "entry";
    case 27:return "image"; case 31:return "list"; case 32:return "list item";
    case 55:return "table"; case 90:return "table row"; case 56:return "table cell";
    case 37:return "page tab"; case 41:return "popup menu"; case 35:return "menu item";
    case 63:return "tool bar"; case 54:return "status bar"; case 65:return "tree";
    case 91:return "tree item"; case 42:return "progress bar"; case 50:return "separator";
    case 49:return "scroll pane"; case 57:return "table column header";
    case 16:return "dialog"; case 64:return "tool tip"; case 34:return "menu bar";
    case 38:return "page tab list"; default:return "unknown";
    }
}
static nx_rect nx_ax_bounds(nx_ax_ref r) {
    nx_ax_node *n=nx_ax_node_at(r); if (n) return n->bounds;
    nx_linux_ax_geometry g={0};
    if (r.window) g=r.window->geometry;
    return (nx_rect){0,0,(float)g.width,(float)g.height};
}
static int nx_ax_showing(nx_ax_ref r) {
    if (!r.window) return 1;
    nx_linux_ax_geometry g;
    g=r.window->geometry;
    if (!g.visible) return 0;
    nx_rect v=nx_ax_bounds(r);
    double left=fmax(v.x,0), top=fmax(v.y,0);
    double right=fmin((double)v.x+v.width,g.width), bottom=fmin((double)v.y+v.height,g.height);
    nx_ax_node *n=nx_ax_node_at(r);
    uint32_t p=n?n->raw_parent:NX_AX_NONE;
    while (p!=NX_AX_NONE) {
        nx_ax_node *a=&r.window->snapshot->nodes[p];
        if (a->role==NX_ROLE_SCROLL_AREA) {
            left=fmax(left,a->bounds.x); top=fmax(top,a->bounds.y);
            right=fmin(right,(double)a->bounds.x+a->bounds.width);
            bottom=fmin(bottom,(double)a->bounds.y+a->bounds.height);
        }
        p=a->raw_parent;
    }
    return right>left && bottom>top;
}
static uint64_t nx_ax_states(nx_ax_ref r) {
    nx_ax_node *n=nx_ax_node_at(r); uint64_t state=0;
#define STATE(bit, condition) do { if (condition) state|=UINT64_C(1)<<(bit); } while (0)
    uint32_t s=n?n->state:0, actions=n?n->actions:0;
    STATE(8,!(s&NX_SEM_DISABLED)); STATE(24,!(s&NX_SEM_DISABLED));
    STATE(30,!r.window || r.window->geometry.visible); STATE(25,nx_ax_showing(r));
    STATE(3,s&NX_SEM_BUSY); STATE(4,s&NX_SEM_CHECKED);
    STATE(9,actions&(NX_ACTION_EXPAND|NX_ACTION_COLLAPSE));
    STATE(10,s&NX_SEM_EXPANDED); STATE(5,(actions&(NX_ACTION_EXPAND|NX_ACTION_COLLAPSE)) && !(s&NX_SEM_EXPANDED));
    STATE(11,actions&NX_ACTION_FOCUS); STATE(22,actions&NX_ACTION_SELECT);
    STATE(23,(s&NX_SEM_SELECTED) || (n && nx_ax_member(n) && nx_ax_selected(n)));
    STATE(32,s&NX_SEM_MIXED); STATE(33,s&(NX_SEM_REQUIRED|NX_SEM_SELECTION_REQUIRED)); STATE(36,s&NX_SEM_INVALID);
    STATE(41,n && (n->role==NX_ROLE_CHECKBOX || n->role==NX_ROLE_RADIO || n->role==NX_ROLE_SWITCH));
    STATE(43,s&NX_SEM_READONLY);
    if (r.window) {
        nx_linux_ax_geometry g=r.window->geometry;
        STATE(12,g.focused && g.visible && (s&NX_SEM_FOCUSED)); STATE(1,!n && g.focused && g.visible);
    }
#undef STATE
    return state;
}
static const struct { uint32_t action; const char *name; } nx_ax_actions[]={
    {NX_ACTION_PRESS,"click"}, {NX_ACTION_INCREMENT,"increment"},
    {NX_ACTION_DECREMENT,"decrement"}, {NX_ACTION_EXPAND,"expand"},
    {NX_ACTION_COLLAPSE,"collapse"}, {NX_ACTION_SELECT,"select"},
    {NX_ACTION_ADD_SELECTION,"add to selection"}, {NX_ACTION_DESELECT,"remove from selection"}
};
static int nx_ax_action_count(nx_ax_ref r) {
    nx_ax_node *n=nx_ax_node_at(r); int count=0;
    if (n) for (size_t i=0;i<sizeof(nx_ax_actions)/sizeof(*nx_ax_actions);++i)
        if (n->actions&nx_ax_actions[i].action) ++count;
    return count;
}
static int nx_ax_action_at(nx_ax_ref r,int index) {
    nx_ax_node *n=nx_ax_node_at(r); if (!n || index<0) return -1;
    for (size_t i=0;i<sizeof(nx_ax_actions)/sizeof(*nx_ax_actions);++i)
        if ((n->actions&nx_ax_actions[i].action) && !index--) return (int)i;
    return -1;
}
static int nx_ax_queue(nx_ax_ref r,uint32_t action) {
    nx_ax_node *n=nx_ax_node_at(r); nx_linux_ax *b=r.window;
    if (!n || !(n->actions&action) || (n->state&NX_SEM_DISABLED)) return 0;
    int selection=nx_ax_member(n) && (action&(NX_ACTION_SELECT|NX_ACTION_ADD_SELECTION|NX_ACTION_DESELECT));
    if (selection) {
        for (size_t i=0;i<b->request_count;++i) {
            nx_ax_request *q=&b->requests[i];
            if (q->owner==b->snapshot->nodes[n->owner].id)
                return q->id==n->id && q->action==action;
        }
        if (action==NX_ACTION_ADD_SELECTION && b->snapshot->nodes[n->owner].selected!=NX_AX_NONE && !nx_ax_selected(n)) return 0;
        if (action==NX_ACTION_DESELECT) {
            if ((b->snapshot->nodes[n->owner].state&NX_SEM_SELECTION_REQUIRED) && nx_ax_selected(n)) return 0;
            if (!nx_ax_selected(n)) return 1;
        } else if (nx_ax_selected(n)) return 1;
    }
    if (action==NX_ACTION_FOCUS) {
        /* Wayland can focus a control only in an already keyboard-focused
           surface. X11 performs native activation at routing time. */
        nx_linux_ax_geometry g=b->geometry;
        if (!g.visible || (!g.screen_position && !g.focused)) return 0;
        for (size_t i=0;i<b->request_count;++i) if (b->requests[i].action==action) {
            b->requests[i]=(nx_ax_request){n->id,n->incarnation,0,0,0,action,0}; return 1;
        }
    }
    if (b->request_count==64u) return 0;
    nx_ax_request q={n->id,n->incarnation,0,0,0,action,0};
    if (selection) { q.owner=b->snapshot->nodes[n->owner].id; q.owner_incarnation=b->snapshot->nodes[n->owner].incarnation; }
    b->requests[b->request_count++]=q; return 1;
}
static int nx_ax_put(DBusMessageIter *it,int type,const void *value) {
    return d.message_iter_append_basic(it,type,value);
}
static int nx_ax_string(DBusMessageIter *it,const char *value) { return nx_ax_put(it,DBUS_TYPE_STRING,&value); }
static int nx_ax_int(DBusMessageIter *it,int value) { dbus_int32_t v=value; return nx_ax_put(it,DBUS_TYPE_INT32,&v); }
static int nx_ax_uint(DBusMessageIter *it,uint32_t value) { return nx_ax_put(it,DBUS_TYPE_UINT32,&value); }
static int nx_ax_boolean(DBusMessageIter *it,int value) { dbus_bool_t v=value!=0; return nx_ax_put(it,DBUS_TYPE_BOOLEAN,&v); }
static int nx_ax_ref_pair(DBusMessageIter *it,const char *bus,const char *path) {
    DBusMessageIter sub;
    return d.message_iter_open_container(it,DBUS_TYPE_STRUCT,NULL,&sub) &&
        nx_ax_string(&sub,bus) && nx_ax_put(&sub,DBUS_TYPE_OBJECT_PATH,&path) &&
        d.message_iter_close_container(it,&sub);
}
static int nx_ax_put_ref(DBusMessageIter *it,nx_ax_ref r) {
    char path[128]; nx_ax_path(r,path);
    const char *bus=d.bus_get_unique_name(r.hub->connection);
    return nx_ax_ref_pair(it,bus?bus:"",path);
}
static int nx_ax_put_parent(DBusMessageIter *it,nx_ax_ref r) {
    if (!r.window) return nx_ax_ref_pair(it,r.hub->desktop_bus,
        r.hub->desktop_path[0]?r.hub->desktop_path:NX_AX_NULL);
    return nx_ax_put_ref(it,nx_ax_parent(r));
}
static int nx_ax_interfaces(DBusMessageIter *it,nx_ax_ref r) {
    DBusMessageIter a;
    int ok=d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"s",&a) && nx_ax_string(&a,NX_AX_ACC);
    if (!r.window) ok=ok && nx_ax_string(&a,NX_AX_APP);
    else ok=ok && nx_ax_string(&a,NX_AX_COMP);
    if (nx_ax_action_count(r)) ok=ok && nx_ax_string(&a,NX_AX_ACT);
    nx_ax_node *n=nx_ax_node_at(r);
    if (n && (n->state&NX_SEM_SELECTION_SINGLE)) ok=ok && nx_ax_string(&a,NX_AX_SEL);
    return ok && d.message_iter_close_container(it,&a);
}
static int nx_ax_put_states(DBusMessageIter *it,nx_ax_ref r) {
    uint64_t state=nx_ax_states(r); DBusMessageIter a;
    return d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"u",&a) &&
        nx_ax_uint(&a,(uint32_t)state) && nx_ax_uint(&a,(uint32_t)(state>>32)) &&
        d.message_iter_close_container(it,&a);
}
static int nx_ax_cache_item(DBusMessageIter *it,nx_ax_ref r) {
    DBusMessageIter item; nx_ax_node *n=nx_ax_node_at(r);
    return d.message_iter_open_container(it,DBUS_TYPE_STRUCT,NULL,&item) &&
        nx_ax_put_ref(&item,r) && nx_ax_put_ref(&item,nx_ax_app(r.hub)) && nx_ax_put_parent(&item,r) &&
        nx_ax_int(&item,nx_ax_index(r)) && nx_ax_int(&item,nx_ax_children(r)) &&
        nx_ax_interfaces(&item,r) && nx_ax_string(&item,nx_ax_name(r)) &&
        nx_ax_uint(&item,nx_ax_role(r)) && nx_ax_string(&item,n?n->description:"") &&
        nx_ax_put_states(&item,r) && d.message_iter_close_container(it,&item);
}
static void nx_ax_disconnect(nx_ax_hub *h) {
    if (h->embed) { d.pending_call_cancel(h->embed); d.pending_call_unref(h->embed); h->embed=NULL; }
    if (h->connection) { d.connection_close(h->connection); d.connection_unref(h->connection); h->connection=NULL; }
    h->desktop_bus[0]=h->desktop_path[0]=0; h->retry_after=nx_ax_now()+1000u;
    h->failed=0; /* Consume the failure once; publication must not restart the backoff every frame. */
    for (nx_linux_ax *b=h->windows;b;b=b->next) b->request_count=0;
}
static int nx_ax_send(nx_ax_hub *h,DBusMessage *message,int built) {
    int ok=message && built && h->connection && !h->failed &&
        d.connection_get_outgoing_size(h->connection)<8L*1024L*1024L &&
        d.connection_send(h->connection,message,NULL);
    if (message) d.message_unref(message);
    /* A stalled reader must not accumulate unlimited native event messages. */
    if (!ok && h->connection) h->failed=1;
    return ok;
}
static void nx_ax_error(nx_ax_hub *h,DBusMessage *call,const char *name,const char *message) {
    DBusMessage *reply=d.message_new_error(call,name,message);
    nx_ax_send(h,reply,reply!=NULL);
}
static int nx_ax_event(nx_ax_ref r,const char *member,const char *detail,int first,int second,
                       const char *text,const nx_ax_ref *related) {
    if (!r.hub->connection || r.hub->failed) return 0;
    char path[128]; nx_ax_path(r,path);
    DBusMessage *m=d.message_new_signal(path,"org.a11y.atspi.Event.Object",member);
    if (!m) { r.hub->failed=1; return 0; }
    DBusMessageIter it,v,dict; d.message_iter_init_append(m,&it);
    int ok=nx_ax_string(&it,detail) && nx_ax_int(&it,first) && nx_ax_int(&it,second) &&
        d.message_iter_open_container(&it,DBUS_TYPE_VARIANT,related?"(so)":"s",&v);
    ok=ok && (related?nx_ax_put_ref(&v,*related):nx_ax_string(&v,text?text:"")) &&
        d.message_iter_close_container(&it,&v) &&
        d.message_iter_open_container(&it,DBUS_TYPE_ARRAY,"{sv}",&dict) &&
        d.message_iter_close_container(&it,&dict);
    return nx_ax_send(r.hub,m,ok);
}
static int nx_ax_cache_signal(nx_ax_ref r,int add) {
    if (!r.hub->connection || r.hub->failed) return 0;
    DBusMessage *m=d.message_new_signal(NX_AX_CACHE,NX_AX_CACHE_IF,add?"AddAccessible":"RemoveAccessible");
    if (!m) { r.hub->failed=1; return 0; }
    DBusMessageIter it; d.message_iter_init_append(m,&it);
    return nx_ax_send(r.hub,m,add?nx_ax_cache_item(&it,r):nx_ax_put_ref(&it,r));
}

typedef struct nx_ax_property { const char *interface, *name, *type; } nx_ax_property;
static const nx_ax_property nx_ax_properties[]={
    {NX_AX_ACC,"version","u"}, {NX_AX_ACC,"Name","s"}, {NX_AX_ACC,"Description","s"},
    {NX_AX_ACC,"Parent","(so)"}, {NX_AX_ACC,"ChildCount","i"}, {NX_AX_ACC,"Locale","s"},
    {NX_AX_ACC,"AccessibleId","s"}, {NX_AX_ACC,"HelpText","s"},
    {NX_AX_APP,"ToolkitName","s"}, {NX_AX_APP,"Version","s"},
    {NX_AX_APP,"ToolkitVersion","s"}, {NX_AX_APP,"AtspiVersion","s"},
    {NX_AX_APP,"InterfaceVersion","u"}, {NX_AX_APP,"Id","i"},
    {NX_AX_COMP,"version","u"}, {NX_AX_ACT,"version","u"}, {NX_AX_ACT,"NActions","i"},
    {NX_AX_SEL,"version","u"}, {NX_AX_SEL,"NSelectedChildren","i"}
};
static int nx_ax_supports(nx_ax_ref r,const char *interface) {
    if (!strcmp(interface,NX_AX_ACC)) return 1;
    if (!strcmp(interface,NX_AX_APP)) return !r.window;
    if (!strcmp(interface,NX_AX_COMP)) return r.window!=NULL;
    if (!strcmp(interface,NX_AX_ACT)) return nx_ax_action_count(r)>0;
    nx_ax_node *n=nx_ax_node_at(r);
    return !strcmp(interface,NX_AX_SEL) && n && (n->state&NX_SEM_SELECTION_SINGLE);
}
static int nx_ax_property_value(DBusMessageIter *it,nx_ax_ref r,const nx_ax_property *p) {
    nx_ax_node *n=nx_ax_node_at(r); const char *name=p->name;
    if (!strcmp(p->type,"u")) return nx_ax_uint(it,1);
    if (!strcmp(name,"Name")) return nx_ax_string(it,nx_ax_name(r));
    if (!strcmp(name,"Description")) return nx_ax_string(it,n?n->description:"");
    if (!strcmp(name,"Parent")) return nx_ax_put_parent(it,r);
    if (!strcmp(name,"ChildCount")) return nx_ax_int(it,nx_ax_children(r));
    if (!strcmp(name,"NActions")) return nx_ax_int(it,nx_ax_action_count(r));
    if (!strcmp(name,"NSelectedChildren")) return nx_ax_int(it,n && n->selected!=NX_AX_NONE);
    if (!strcmp(name,"Id")) return nx_ax_int(it,r.hub->application_id);
    if (!strcmp(name,"ToolkitName")) return nx_ax_string(it,"NexaUI");
    if (!strcmp(name,"Version") || !strcmp(name,"ToolkitVersion")) return nx_ax_string(it,"0.1.0");
    if (!strcmp(name,"AtspiVersion")) return nx_ax_string(it,"2.1");
    if (!strcmp(name,"Locale")) return nx_ax_string(it,"C");
    if (!strcmp(name,"AccessibleId")) { char path[128]; nx_ax_path(r,path); return nx_ax_string(it,path); }
    return nx_ax_string(it,"");
}
static int nx_ax_property_variant(DBusMessageIter *it,nx_ax_ref r,const nx_ax_property *p) {
    DBusMessageIter v;
    return d.message_iter_open_container(it,DBUS_TYPE_VARIANT,p->type,&v) &&
        nx_ax_property_value(&v,r,p) && d.message_iter_close_container(it,&v);
}
static const nx_ax_property *nx_ax_property_find(const char *interface,const char *name) {
    for (size_t i=0;i<sizeof(nx_ax_properties)/sizeof(*nx_ax_properties);++i)
        if (!strcmp(interface,nx_ax_properties[i].interface) && !strcmp(name,nx_ax_properties[i].name))
            return &nx_ax_properties[i];
    return NULL;
}
static int nx_ax_properties_call(nx_ax_ref r,DBusMessage *call,const char *member,DBusMessageIter *it) {
    const char *interface=NULL,*name=NULL;
    if (!strcmp(member,"GetAll")) {
        if (!d.message_get_args(call,NULL,DBUS_TYPE_STRING,&interface,DBUS_TYPE_INVALID)) return -1;
        if (!nx_ax_supports(r,interface)) return -2;
        DBusMessageIter a;
        int ok=d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"{sv}",&a);
        for (size_t i=0;ok && i<sizeof(nx_ax_properties)/sizeof(*nx_ax_properties);++i) {
            const nx_ax_property *p=&nx_ax_properties[i];
            if (strcmp(interface,p->interface)) continue;
            DBusMessageIter entry;
            ok=d.message_iter_open_container(&a,DBUS_TYPE_DICT_ENTRY,NULL,&entry) &&
                nx_ax_string(&entry,p->name) && nx_ax_property_variant(&entry,r,p) &&
                d.message_iter_close_container(&a,&entry);
        }
        return ok && d.message_iter_close_container(it,&a);
    }
    if (!strcmp(member,"Get")) {
        if (!d.message_get_args(call,NULL,DBUS_TYPE_STRING,&interface,DBUS_TYPE_STRING,&name,DBUS_TYPE_INVALID)) return -1;
        const nx_ax_property *p=nx_ax_property_find(interface,name);
        if (!p || !nx_ax_supports(r,interface)) return -3;
        return nx_ax_property_variant(it,r,p);
    }
    if (!strcmp(member,"Set")) {
        if (!d.message_has_signature(call,"ssv")) return -1;
        DBusMessageIter args,v; d.message_iter_init(call,&args);
        d.message_iter_get_basic(&args,&interface); d.message_iter_next(&args);
        d.message_iter_get_basic(&args,&name); d.message_iter_next(&args);
        const nx_ax_property *p=nx_ax_property_find(interface,name);
        if (!p || !nx_ax_supports(r,interface)) return -3;
        if (r.window || strcmp(interface,NX_AX_APP) || strcmp(name,"Id")) return -4;
        d.message_iter_recurse(&args,&v);
        if (d.message_iter_get_arg_type(&v)!=DBUS_TYPE_INT32) return -1;
        d.message_iter_get_basic(&v,&r.hub->application_id); return 1;
    }
    return -2;
}
static int nx_ax_accessible_call(nx_ax_ref r,DBusMessage *call,const char *member,DBusMessageIter *it) {
    if (!strcmp(member,"GetChildAtIndex")) {
        dbus_int32_t index; nx_ax_ref child;
        if (!d.message_get_args(call,NULL,DBUS_TYPE_INT32,&index,DBUS_TYPE_INVALID) || !nx_ax_child(r,index,&child)) return -1;
        return nx_ax_put_ref(it,child);
    }
    if (!d.message_has_signature(call,"")) return -1;
    if (!strcmp(member,"GetChildren")) {
        DBusMessageIter a; int ok=d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"(so)",&a);
        if (!r.window) {
            for (nx_linux_ax *b=r.hub->windows;ok && b;b=b->next) ok=nx_ax_put_ref(&a,nx_ax_window(b));
        } else {
            nx_ax_node *n=nx_ax_node_at(r);
            uint32_t index=n?n->first:r.window->snapshot->first;
            for (;ok && index!=NX_AX_NONE;index=r.window->snapshot->nodes[index].next)
                ok=nx_ax_put_ref(&a,(nx_ax_ref){r.hub,r.window,index});
        }
        return ok && d.message_iter_close_container(it,&a);
    }
    if (!strcmp(member,"GetIndexInParent")) return nx_ax_int(it,nx_ax_index(r));
    if (!strcmp(member,"GetRole")) return nx_ax_uint(it,nx_ax_role(r));
    if (!strcmp(member,"GetRoleName") || !strcmp(member,"GetLocalizedRoleName")) return nx_ax_string(it,nx_ax_role_name(r));
    if (!strcmp(member,"GetState")) return nx_ax_put_states(it,r);
    if (!strcmp(member,"GetApplication")) return nx_ax_put_ref(it,nx_ax_app(r.hub));
    if (!strcmp(member,"GetInterfaces")) return nx_ax_interfaces(it,r);
    if (!strcmp(member,"GetRelationSet")) {
        DBusMessageIter a;
        return d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"(ua(so))",&a) && d.message_iter_close_container(it,&a);
    }
    if (!strcmp(member,"GetAttributes")) {
        DBusMessageIter a,entry; nx_ax_node *n=nx_ax_node_at(r);
        int ok=d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"{ss}",&a);
        if (n && n->value[0] && !(n->state&NX_SEM_PROTECTED))
            ok=ok && d.message_iter_open_container(&a,DBUS_TYPE_DICT_ENTRY,NULL,&entry) &&
                nx_ax_string(&entry,"nexaui-value") && nx_ax_string(&entry,n->value) &&
                d.message_iter_close_container(&a,&entry);
        return ok && d.message_iter_close_container(it,&a);
    }
    return -2;
}
static int nx_ax_action_call(nx_ax_ref r,DBusMessage *call,const char *member,DBusMessageIter *it) {
    if (!strcmp(member,"GetActions")) {
        if (!d.message_has_signature(call,"")) return -1;
        DBusMessageIter a,item; int ok=d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"(sss)",&a);
        for (int i=0;ok && i<nx_ax_action_count(r);++i) {
            int at=nx_ax_action_at(r,i);
            ok=d.message_iter_open_container(&a,DBUS_TYPE_STRUCT,NULL,&item) &&
                nx_ax_string(&item,nx_ax_actions[at].name) && nx_ax_string(&item,"") && nx_ax_string(&item,"") &&
                d.message_iter_close_container(&a,&item);
        }
        return ok && d.message_iter_close_container(it,&a);
    }
    dbus_int32_t index;
    if (!d.message_get_args(call,NULL,DBUS_TYPE_INT32,&index,DBUS_TYPE_INVALID)) return -1;
    int at=nx_ax_action_at(r,index);
    if (at<0) return -1;
    if (!strcmp(member,"DoAction")) return nx_ax_boolean(it,nx_ax_queue(r,nx_ax_actions[at].action));
    if (!strcmp(member,"GetName") || !strcmp(member,"GetLocalizedName")) return nx_ax_string(it,nx_ax_actions[at].name);
    if (!strcmp(member,"GetDescription") || !strcmp(member,"GetKeyBinding")) return nx_ax_string(it,"");
    return -2;
}
static int nx_ax_selection_call(nx_ax_ref r,DBusMessage *call,const char *member,DBusMessageIter *it) {
    nx_ax_node *n=nx_ax_node_at(r);
    if (!strcmp(member,"SelectAll") || !strcmp(member,"ClearSelection")) {
        if (!d.message_has_signature(call,"")) return -1;
        int accepted=0;
        if (!strcmp(member,"ClearSelection")) accepted=n->selected==NX_AX_NONE ? 1 :
            nx_ax_queue((nx_ax_ref){r.hub,r.window,n->selected},NX_ACTION_DESELECT);
        return nx_ax_boolean(it,accepted); /* single-selection cannot select all */
    }
    dbus_int32_t index;
    if (!d.message_get_args(call,NULL,DBUS_TYPE_INT32,&index,DBUS_TYPE_INVALID)) return -1;
    if (!strcmp(member,"GetSelectedChild")) {
        if (index!=0 || n->selected==NX_AX_NONE) return -1;
        return nx_ax_put_ref(it,(nx_ax_ref){r.hub,r.window,n->selected});
    }
    if (!strcmp(member,"DeselectSelectedChild")) {
        return nx_ax_boolean(it,index==0 && n->selected!=NX_AX_NONE &&
            nx_ax_queue((nx_ax_ref){r.hub,r.window,n->selected},NX_ACTION_DESELECT));
    }
    nx_ax_ref child;
    int member_child=nx_ax_child(r,index,&child) && nx_ax_member(nx_ax_node_at(child)) &&
        nx_ax_node_at(child)->owner==r.index;
    if (!strcmp(member,"IsChildSelected")) return nx_ax_boolean(it,member_child && nx_ax_selected(nx_ax_node_at(child)));
    if (!strcmp(member,"SelectChild")) return nx_ax_boolean(it,member_child && nx_ax_queue(child,NX_ACTION_SELECT));
    if (!strcmp(member,"DeselectChild")) return nx_ax_boolean(it,member_child && nx_ax_queue(child,NX_ACTION_DESELECT));
    return -2;
}
static int nx_ax_extents(nx_ax_ref r,uint32_t coordinates,int32_t out[4]) {
    if (coordinates>2u) return -1;
    nx_rect bounds=nx_ax_bounds(r);
    /* Round the window-space rectangle once. Coordinate-system changes are
       translations of that same native pixel rectangle, including fractional
       semantic parent positions. Width/height must not change with the origin. */
    double x=floor(bounds.x),y=floor(bounds.y);
    double width=ceil((double)bounds.x+bounds.width)-x;
    double height=ceil((double)bounds.y+bounds.height)-y;
    if (coordinates==0) {
        nx_linux_ax_geometry g=r.window->geometry;
        if (!g.screen_position) return -5;
        x+=g.x; y+=g.y;
    } else if (coordinates==2) {
        nx_rect parent=nx_ax_bounds(nx_ax_parent(r)); x-=floor(parent.x); y-=floor(parent.y);
    }
    double left=floor(x), top=floor(y), right=left+width, bottom=top+height;
    if (!isfinite(left) || !isfinite(top) || !isfinite(right) || !isfinite(bottom) ||
        left<INT32_MIN || left>INT32_MAX || top<INT32_MIN || top>INT32_MAX ||
        right-left<0 || right-left>INT32_MAX || bottom-top<0 || bottom-top>INT32_MAX) return -5;
    out[0]=(int32_t)left; out[1]=(int32_t)top; out[2]=(int32_t)(right-left); out[3]=(int32_t)(bottom-top);
    return 1;
}
static int nx_ax_point(nx_ax_ref r,int x,int y,uint32_t coordinates) {
    int32_t v[4]; int result=nx_ax_extents(r,coordinates,v);
    if (result<0) return result;
    if (!nx_ax_showing(r) || x<v[0] || y<v[1] || (int64_t)x>=(int64_t)v[0]+v[2] || (int64_t)y>=(int64_t)v[1]+v[3]) return 0;
    /* Test ancestor scroll clips in the same coordinate system as this object. */
    nx_ax_node *n=nx_ax_node_at(r); nx_rect bounds=nx_ax_bounds(r);
    double local_x=(double)x-v[0]+floor(bounds.x), local_y=(double)y-v[1]+floor(bounds.y);
    uint32_t p=n?n->raw_parent:NX_AX_NONE;
    while (p!=NX_AX_NONE) {
        nx_ax_node *a=&r.window->snapshot->nodes[p];
        if (a->role==NX_ROLE_SCROLL_AREA && (local_x<a->bounds.x || local_y<a->bounds.y ||
            local_x>=(double)a->bounds.x+a->bounds.width || local_y>=(double)a->bounds.y+a->bounds.height)) return 0;
        p=a->raw_parent;
    }
    return 1;
}
static int nx_ax_component_call(nx_ax_ref r,DBusMessage *call,const char *member,DBusMessageIter *it) {
    uint32_t coord=1; int32_t v[4];
    if (!strcmp(member,"Contains") || !strcmp(member,"GetAccessibleAtPoint")) {
        dbus_int32_t x,y;
        if (!d.message_get_args(call,NULL,DBUS_TYPE_INT32,&x,DBUS_TYPE_INT32,&y,DBUS_TYPE_UINT32,&coord,DBUS_TYPE_INVALID)) return -1;
        int contains=nx_ax_point(r,x,y,coord); if (contains<0) return contains;
        if (!strcmp(member,"Contains")) return nx_ax_boolean(it,contains);
        if (!contains) return nx_ax_ref_pair(it,"",NX_AX_NULL);
        nx_ax_ref target=r;
        /* Hit test in reverse paint order, but only descendants of this native
           object. Parent-relative input is relative to the queried parent. */
        int32_t query[4]; int result=nx_ax_extents(r,coord,query); if (result<0) return result;
        nx_rect root=nx_ax_bounds(r);
        double wx=(double)x-query[0]+floor(root.x), wy=(double)y-query[1]+floor(root.y);
        if (wx<INT32_MIN || wx>INT32_MAX || wy<INT32_MIN || wy>INT32_MAX) return -5;
        for (size_t i=r.window->snapshot->count;i>0;--i) {
            uint32_t at=(uint32_t)i-1u,p=at;
            while (p!=NX_AX_NONE && p!=r.index) p=r.window->snapshot->nodes[p].parent;
            if (r.index!=NX_AX_NONE && p!=r.index) continue;
            nx_ax_ref candidate={r.hub,r.window,at};
            if (nx_ax_point(candidate,(int)wx,(int)wy,1)>0) { target=candidate; break; }
        }
        return nx_ax_put_ref(it,target);
    }
    if (!strcmp(member,"GetExtents") || !strcmp(member,"GetPosition")) {
        if (!d.message_get_args(call,NULL,DBUS_TYPE_UINT32,&coord,DBUS_TYPE_INVALID)) return -1;
    } else if (!strcmp(member,"SetExtents") || !strcmp(member,"SetPosition") || !strcmp(member,"SetSize") ||
               !strcmp(member,"ScrollTo") || !strcmp(member,"ScrollToPoint")) return -5;
    else if (!d.message_has_signature(call,"")) return -1;
    if (!strcmp(member,"GetExtents") || !strcmp(member,"GetPosition") || !strcmp(member,"GetSize")) {
        int result=nx_ax_extents(r,coord,v); if (result<0) return result;
        if (!strcmp(member,"GetSize")) return nx_ax_int(it,v[2]) && nx_ax_int(it,v[3]);
        if (!strcmp(member,"GetPosition")) return nx_ax_int(it,v[0]) && nx_ax_int(it,v[1]);
        DBusMessageIter rect;
        return d.message_iter_open_container(it,DBUS_TYPE_STRUCT,NULL,&rect) &&
            nx_ax_int(&rect,v[0]) && nx_ax_int(&rect,v[1]) && nx_ax_int(&rect,v[2]) && nx_ax_int(&rect,v[3]) &&
            d.message_iter_close_container(it,&rect);
    }
    if (!strcmp(member,"GrabFocus")) return nx_ax_boolean(it,nx_ax_queue(r,NX_ACTION_FOCUS));
    if (!strcmp(member,"GetLayer")) return nx_ax_uint(it,r.index==NX_AX_NONE?7u:3u); /* WINDOW/WIDGET */
    if (!strcmp(member,"GetMDIZOrder")) { dbus_int16_t zero=0; return nx_ax_put(it,DBUS_TYPE_INT16,&zero); }
    if (!strcmp(member,"GetAlpha")) { double alpha=1; return nx_ax_put(it,DBUS_TYPE_DOUBLE,&alpha); }
    return -2;
}
static void nx_ax_bounds_event(nx_ax_ref r) {
    if (!r.hub->connection || r.hub->failed) return;
    int32_t rect[4];
    /* BoundsChanged carries screen extents. Omit it when the compositor does
       not disclose that coordinate system instead of inventing an origin. */
    if (nx_ax_extents(r,0,rect)<0) return;
    char path[128]; nx_ax_path(r,path);
    DBusMessage *m=d.message_new_signal(path,"org.a11y.atspi.Event.Object","BoundsChanged");
    if (!m) { r.hub->failed=1; return; }
    DBusMessageIter it,v,box,dict; d.message_iter_init_append(m,&it);
    int ok=nx_ax_string(&it,"") && nx_ax_int(&it,0) && nx_ax_int(&it,0) &&
        d.message_iter_open_container(&it,DBUS_TYPE_VARIANT,"(iiii)",&v) &&
        d.message_iter_open_container(&v,DBUS_TYPE_STRUCT,NULL,&box) &&
        nx_ax_int(&box,rect[0]) && nx_ax_int(&box,rect[1]) && nx_ax_int(&box,rect[2]) && nx_ax_int(&box,rect[3]) &&
        d.message_iter_close_container(&v,&box) && d.message_iter_close_container(&it,&v) &&
        d.message_iter_open_container(&it,DBUS_TYPE_ARRAY,"{sv}",&dict) && d.message_iter_close_container(&it,&dict);
    nx_ax_send(r.hub,m,ok);
}

#include "atspi_xml.h"
static int nx_ax_introspect(nx_ax_hub *h,nx_ax_ref r,int cache,DBusMessageIter *it) {
    const char *pieces[10]={"<node>",NULL}; size_t count=1,bytes=0;
    if (cache) pieces[count++]=nx_ax_xml_cache;
    else {
        pieces[count++]=nx_ax_xml_accessible;
        if (!r.window) pieces[count++]=nx_ax_xml_application;
        if (r.window) pieces[count++]=nx_ax_xml_component;
        if (nx_ax_supports(r,NX_AX_ACT)) pieces[count++]=nx_ax_xml_action;
        if (nx_ax_supports(r,NX_AX_SEL)) pieces[count++]=nx_ax_xml_selection;
    }
    pieces[count++]="<interface name=\"org.freedesktop.DBus.Properties\">"
        "<method name=\"Get\"><arg direction=\"in\" type=\"s\"/><arg direction=\"in\" type=\"s\"/>"
        "<arg direction=\"out\" type=\"v\"/></method>"
        "<method name=\"GetAll\"><arg direction=\"in\" type=\"s\"/><arg direction=\"out\" type=\"a{sv}\"/></method>"
        "<method name=\"Set\"><arg direction=\"in\" type=\"s\"/><arg direction=\"in\" type=\"s\"/>"
        "<arg direction=\"in\" type=\"v\"/></method></interface>"
        "<interface name=\"org.freedesktop.DBus.Introspectable\"><method name=\"Introspect\">"
        "<arg direction=\"out\" type=\"s\"/></method></interface></node>";
    for (size_t i=0;i<count;++i) bytes+=strlen(pieces[i]);
    (void)h;
    char *xml=malloc(bytes+1u); if (!xml) return 0;
    size_t at=0; for (size_t i=0;i<count;++i) { size_t n=strlen(pieces[i]); memcpy(xml+at,pieces[i],n); at+=n; }
    xml[at]=0; int ok=nx_ax_string(it,xml);
    free(xml); return ok;
}
static int nx_ax_cache_call(nx_ax_hub *h,DBusMessageIter *it) {
    /* Include all live objects, with a bounded reply. Lazy Accessible queries
       remain available when an unusually large tree exceeds the bulk budget. */
    size_t budget=2048;
    for (nx_linux_ax *b=h->windows;b;b=b->next) {
        size_t cost=b->snapshot->capacity+b->snapshot->count*1024u+2048u;
        if (cost>4u*1024u*1024u || budget>4u*1024u*1024u-cost) return -6;
        budget+=cost;
    }
    DBusMessageIter a;
    int ok=d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"((so)(so)(so)iiassusau)",&a) &&
        nx_ax_cache_item(&a,nx_ax_app(h));
    for (nx_linux_ax *b=h->windows;ok && b;b=b->next) {
        ok=nx_ax_cache_item(&a,nx_ax_window(b));
        for (size_t i=0;ok && i<b->snapshot->count;++i) ok=nx_ax_cache_item(&a,(nx_ax_ref){h,b,(uint32_t)i});
    }
    return ok && d.message_iter_close_container(it,&a);
}
static DBusHandlerResult nx_ax_message(DBusConnection *connection,DBusMessage *call,void *user) {
    (void)connection;
    nx_ax_hub *h=user;
    if (d.message_get_type(call)!=DBUS_MESSAGE_TYPE_METHOD_CALL) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char *path=d.message_get_path(call),*interface=d.message_get_interface(call),*member=d.message_get_member(call);
    if (!path || !member || !interface) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    nx_ax_ref r=nx_ax_app(h); int cache=!strcmp(path,NX_AX_CACHE);
    if (!cache && !nx_ax_resolve(h,path,&r)) {
        nx_ax_error(h,call,"org.a11y.atspi.Error.Defunct","The accessible object has retired"); return DBUS_HANDLER_RESULT_HANDLED;
    }
    DBusMessage *reply=d.message_new_method_return(call);
    if (!reply) { h->failed=1; return DBUS_HANDLER_RESULT_HANDLED; }
    DBusMessageIter it; d.message_iter_init_append(reply,&it);
    int ok=-2;
    if (!strcmp(interface,"org.freedesktop.DBus.Introspectable") && !strcmp(member,"Introspect"))
        ok=d.message_has_signature(call,"") ? nx_ax_introspect(h,r,cache,&it) : -1;
    else if (cache && !strcmp(interface,NX_AX_CACHE_IF) && !strcmp(member,"GetItems"))
        ok=d.message_has_signature(call,"") ? nx_ax_cache_call(h,&it) : -1;
    else if (cache && !strcmp(interface,NX_AX_PROPS) && !strcmp(member,"Get")) {
        const char *iface,*name;
        if (!d.message_get_args(call,NULL,DBUS_TYPE_STRING,&iface,DBUS_TYPE_STRING,&name,DBUS_TYPE_INVALID)) ok=-1;
        else if (!strcmp(iface,NX_AX_CACHE_IF) && !strcmp(name,"version")) {
            DBusMessageIter v; ok=d.message_iter_open_container(&it,DBUS_TYPE_VARIANT,"u",&v) &&
                nx_ax_uint(&v,1) && d.message_iter_close_container(&it,&v);
        } else ok=-3;
    } else if (!cache && !strcmp(interface,NX_AX_PROPS)) ok=nx_ax_properties_call(r,call,member,&it);
    else if (!cache && nx_ax_supports(r,interface)) {
        if (!strcmp(interface,NX_AX_ACC)) ok=nx_ax_accessible_call(r,call,member,&it);
        else if (!strcmp(interface,NX_AX_ACT)) ok=nx_ax_action_call(r,call,member,&it);
        else if (!strcmp(interface,NX_AX_SEL)) ok=nx_ax_selection_call(r,call,member,&it);
        else if (!strcmp(interface,NX_AX_COMP)) ok=nx_ax_component_call(r,call,member,&it);
        else if (!strcmp(interface,NX_AX_APP)) {
            if (!strcmp(member,"GetApplicationBusAddress")) ok=d.message_has_signature(call,"")?nx_ax_string(&it,""):-1;
            else if (!strcmp(member,"GetLocale")) {
                dbus_uint32_t category;
                ok=d.message_get_args(call,NULL,DBUS_TYPE_UINT32,&category,DBUS_TYPE_INVALID)?nx_ax_string(&it,"C"):-1;
            }
        }
    }
    if (ok>=0) nx_ax_send(h,reply,ok);
    else {
        d.message_unref(reply);
        const char *error=ok==-1?DBUS_ERROR_INVALID_ARGS:ok==-3?DBUS_ERROR_UNKNOWN_PROPERTY:
            ok==-4?DBUS_ERROR_PROPERTY_READ_ONLY:ok==-5?DBUS_ERROR_NOT_SUPPORTED:
            ok==-6?DBUS_ERROR_LIMITS_EXCEEDED:DBUS_ERROR_UNKNOWN_METHOD;
        nx_ax_error(h,call,error,ok==-5?"This operation or coordinate system is unavailable":
            ok==-6?"The bulk cache exceeds the reply budget":"Invalid or unsupported accessible request");
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}
static int nx_ax_connect(nx_ax_hub *h) {
    if (h->connection) return !h->failed;
    uint64_t now=nx_ax_now(); if (now && now<h->retry_after) return 0;
    h->retry_after=now+1000u;
    const char *disabled=getenv("NO_AT_BRIDGE"); if (disabled && !strcmp(disabled,"1")) return 0;
    pthread_once(&nx_ax_loader_once,nx_ax_load); if (!nx_ax_loaded) return 0;
    DBusError error; d.error_init(&error);
    char *discovered=NULL; const char *address=getenv("AT_SPI_BUS_ADDRESS");
    if (!address || !*address) {
        const char *session_address=getenv("DBUS_SESSION_BUS_ADDRESS");
        if (!session_address || !*session_address) return 0;
        DBusConnection *session=d.connection_open_private(session_address,&error);
        if (!session) { d.error_free(&error); return 0; }
        d.connection_set_exit_on_disconnect(session,0);
        DBusMessage *reply=NULL,*request=NULL;
        if (d.bus_register(session,&error)) {
            request=d.message_new_method_call("org.a11y.Bus","/org/a11y/bus","org.a11y.Bus","GetAddress");
            if (request) reply=d.connection_send_with_reply_and_block(session,request,250,&error);
        }
        const char *value=NULL;
        if (reply && d.message_get_args(reply,NULL,DBUS_TYPE_STRING,&value,DBUS_TYPE_INVALID)) {
            size_t length=strlen(value);
            if (length<65536u) {
                discovered=malloc(length+1u);
                if (discovered) memcpy(discovered,value,length+1u);
            }
        }
        if (reply) d.message_unref(reply);
        if (request) d.message_unref(request);
        d.connection_close(session); d.connection_unref(session); d.error_free(&error); d.error_init(&error);
        address=discovered;
    }
    if (!address || !*address) { if (discovered) free(discovered); return 0; }
    DBusConnection *c=d.connection_open_private(address,&error);
    if (discovered) free(discovered);
    if (!c) { d.error_free(&error); return 0; }
    d.connection_set_exit_on_disconnect(c,0);
    d.connection_set_max_message_size(c,4L*1024L*1024L);
    d.connection_set_max_received_size(c,8L*1024L*1024L);
    static const DBusObjectPathVTable table={.message_function=nx_ax_message};
    if (!d.bus_register(c,&error) || !d.connection_register_fallback(c,"/org/a11y/atspi",&table,h)) {
        d.connection_close(c); d.connection_unref(c); d.error_free(&error); return 0;
    }
    d.error_free(&error); h->connection=c; h->failed=0;
    /* Reconnection creates new paths; an old client reference cannot address a
       replacement window or a later instance of the same widget ID. */
    for (nx_linux_ax *b=h->windows;b;b=b->next) {
        b->epoch=nx_native_next_identity(); b->request_count=0;
        if (!b->epoch) { nx_ax_disconnect(h); return 0; }
    }
    DBusMessage *embed=d.message_new_method_call("org.a11y.atspi.Registry",NX_AX_ROOT,"org.a11y.atspi.Socket","Embed");
    if (!embed) { nx_ax_disconnect(h); return 0; }
    DBusMessageIter it; d.message_iter_init_append(embed,&it);
    int ok=nx_ax_put_ref(&it,nx_ax_app(h)) && d.connection_send_with_reply(c,embed,&h->embed,2000);
    d.message_unref(embed);
    if (!ok || !h->embed) { nx_ax_disconnect(h); return 0; }
    return 1;
}
static void nx_ax_embed_progress(nx_ax_hub *h) {
    if (!h->embed || !d.pending_call_get_completed(h->embed)) return;
    DBusMessage *reply=d.pending_call_steal_reply(h->embed);
    d.pending_call_unref(h->embed); h->embed=NULL;
    if (!reply || !d.message_has_signature(reply,"(so)")) {
        if (reply) d.message_unref(reply);
        h->failed=1; return;
    }
    DBusMessageIter it,sub; const char *bus,*path;
    d.message_iter_init(reply,&it); d.message_iter_recurse(&it,&sub);
    d.message_iter_get_basic(&sub,&bus); d.message_iter_next(&sub); d.message_iter_get_basic(&sub,&path);
    if (strlen(bus)>=sizeof(h->desktop_bus) || strlen(path)>=sizeof(h->desktop_path)) h->failed=1;
    else {
        memcpy(h->desktop_bus,bus,strlen(bus)+1u); memcpy(h->desktop_path,path,strlen(path)+1u);
        nx_ax_cache_signal(nx_ax_app(h),1);
    }
    d.message_unref(reply);
}
static void nx_ax_update_geometry(nx_linux_ax *b);
int nx_linux_ax_pump(nx_linux_ax *b) {
    if (!b || !pthread_equal(pthread_self(),b->thread)) return 0;
    nx_ax_hub *h=b->hub; if (!h->connection) return 0;
    for (nx_linux_ax *w=h->windows;w;w=w->next) nx_ax_update_geometry(w);
    int work=0;
    if (!d.connection_read_write(h->connection,0)) h->failed=1;
    for (unsigned i=0;!h->failed && i<64u &&
         d.connection_get_dispatch_status(h->connection)==DBUS_DISPATCH_DATA_REMAINS;++i) {
        work=1;
        if (d.connection_dispatch(h->connection)==DBUS_DISPATCH_NEED_MEMORY) h->failed=1;
    }
    nx_ax_embed_progress(h);
    if (h->failed || !d.connection_get_is_connected(h->connection)) nx_ax_disconnect(h);
    for (size_t i=0;i<b->request_count;++i) if (!b->requests[i].forwarded) work=1;
    return work;
}
int nx_linux_ax_fd(nx_linux_ax *b,int *writable) {
    *writable=0;
    if (!b || !pthread_equal(pthread_self(),b->thread) || !b->hub->connection) return -1;
    int fd=-1;
    d.connection_get_unix_fd(b->hub->connection,&fd);
    *writable=d.connection_has_messages_to_send(b->hub->connection);
    return fd;
}
int nx_linux_ax_matches(nx_linux_ax *b,const nx_context *context) {
    return !b || (pthread_equal(pthread_self(),b->thread) && b->binding==nx_context_identity(context));
}
#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_linux_ax_test_disconnect(nx_linux_ax *b) {
    if (!b || !pthread_equal(pthread_self(),b->thread) || !b->hub->connection) return NX_ERROR_STATE;
    d.connection_close(b->hub->connection); return NX_OK;
}
#endif
nx_result nx_linux_ax_route(nx_linux_ax *b,nx_context *context,size_t *count) {
    *count=0; if (!b) return NX_OK;
    if (!nx_linux_ax_matches(b,context)) return NX_ERROR_STATE;
    if (b->frame!=nx_context_frame_serial(context)) return NX_OK;
    size_t remaining=b->request_count;
    for (size_t i=0;i<remaining;++i) {
        size_t at=0; while (at<b->request_count && b->requests[at].forwarded) ++at;
        if (at==b->request_count) break;
        nx_ax_request q=b->requests[at]; uint32_t index=nx_ax_find(b->snapshot,q.id);
        int valid=index!=NX_AX_NONE && b->snapshot->nodes[index].incarnation==q.incarnation;
        if (valid && q.owner) {
            nx_ax_node *n=&b->snapshot->nodes[index];
            valid=nx_ax_member(n) && b->snapshot->nodes[n->owner].id==q.owner &&
                b->snapshot->nodes[n->owner].incarnation==q.owner_incarnation;
        }
        nx_result result=valid?nx_semantics_request_action(context,q.id,q.action):NX_OK;
        if (result==NX_ERROR_LIMIT) break;
        if (result==NX_ERROR_STATE) {
            size_t nodes; if (nx_semantics_count(context,&nodes)!=NX_OK) return result;
        }
        if (valid && result==NX_OK) {
            ++*count;
            if (q.action==NX_ACTION_FOCUS) nx_linux_window_ax_focus(b->window);
        }
        if (valid && result==NX_OK && q.owner) { b->requests[at].forwarded=1; b->requests[at].frame=b->frame; }
        else { --b->request_count; memmove(b->requests+at,b->requests+at+1,(b->request_count-at)*sizeof(*b->requests)); }
    }
    return NX_OK;
}

static void nx_ax_update_geometry(nx_linux_ax *b) {
    nx_linux_ax_geometry old=b->geometry;
    nx_linux_window_ax_geometry(b->window,&b->geometry);
    nx_linux_ax_geometry current=b->geometry;
    if (!b->hub->connection || !b->snapshot) return;
    int before_focus=old.focused && old.visible, after_focus=current.focused && current.visible;
    if (before_focus!=after_focus) {
        nx_ax_event(nx_ax_window(b),"StateChanged","active",after_focus,0,NULL,NULL);
        for (size_t i=0;i<b->snapshot->count;++i)
            if (b->snapshot->nodes[i].state&NX_SEM_FOCUSED)
                nx_ax_event((nx_ax_ref){b->hub,b,(uint32_t)i},"StateChanged","focused",after_focus,0,NULL,NULL);
    }
    if (old.visible!=current.visible) {
        nx_ax_event(nx_ax_window(b),"StateChanged","visible",current.visible,0,NULL,NULL);
        for (size_t i=0;i<b->snapshot->count;++i)
            nx_ax_event((nx_ax_ref){b->hub,b,(uint32_t)i},"StateChanged","visible",current.visible,0,NULL,NULL);
    }
    if (old.visible!=current.visible || old.width!=current.width || old.height!=current.height) {
        nx_ax_event(nx_ax_window(b),"StateChanged","showing",nx_ax_showing(nx_ax_window(b)),0,NULL,NULL);
        for (size_t i=0;i<b->snapshot->count;++i) {
            nx_ax_ref r={b->hub,b,(uint32_t)i};
            int after=nx_ax_showing(r); b->geometry=old; int before=nx_ax_showing(r);
            b->geometry=current;
            if (before!=after) nx_ax_event(r,"StateChanged","showing",after,0,NULL,NULL);
        }
    }
    if (old.x!=current.x || old.y!=current.y || old.width!=current.width || old.height!=current.height) {
        nx_ax_bounds_event(nx_ax_window(b));
        for (size_t i=0;i<b->snapshot->count && !b->hub->failed;++i)
            nx_ax_bounds_event((nx_ax_ref){b->hub,b,(uint32_t)i});
    }
}
static int nx_ax_same_node(const nx_ax_snapshot *next,const nx_ax_node *n) {
    uint32_t at=nx_ax_find(next,n->id);
    return at!=NX_AX_NONE && next->nodes[at].incarnation==n->incarnation;
}
static void nx_ax_publish_delta(nx_linux_ax *b,nx_ax_snapshot *next) {
    nx_ax_snapshot *old=b->snapshot;
    /* Removal messages own their old object paths before either buffer is
       reused. No callback can query a borrowed context or an obsolete span. */
    if (old) for (size_t i=old->count;i>0 && !b->hub->failed;--i) {
        nx_ax_node *n=&old->nodes[i-1u];
        uint32_t ni=nx_ax_find(next,n->id);
        int same=nx_ax_same_node(next,n);
        uint64_t old_parent=n->parent==NX_AX_NONE?0:old->nodes[n->parent].id;
        uint64_t new_parent=ni==NX_AX_NONE || next->nodes[ni].parent==NX_AX_NONE?0:
            next->nodes[next->nodes[ni].parent].id;
        uint64_t old_parent_inc=n->parent==NX_AX_NONE?0:old->nodes[n->parent].incarnation;
        uint64_t new_parent_inc=ni==NX_AX_NONE || next->nodes[ni].parent==NX_AX_NONE?0:
            next->nodes[next->nodes[ni].parent].incarnation;
        nx_ax_ref r={b->hub,b,(uint32_t)i-1u};
        if (!same || old_parent!=new_parent || old_parent_inc!=new_parent_inc || n->index!=next->nodes[ni].index)
            nx_ax_event(nx_ax_parent(r),"ChildrenChanged","remove",nx_ax_index(r),0,NULL,&r);
        if (!same) nx_ax_cache_signal(r,0);
    }
    b->snapshot=next;
    for (size_t i=0;i<next->count && !b->hub->failed;++i) {
        nx_ax_node *n=&next->nodes[i]; uint32_t prior=nx_ax_find(old,n->id);
        nx_ax_node *a=prior==NX_AX_NONE?NULL:&old->nodes[prior];
        int same=a && a->incarnation==n->incarnation;
        uint64_t old_parent=!a || a->parent==NX_AX_NONE?0:old->nodes[a->parent].id;
        uint64_t new_parent=n->parent==NX_AX_NONE?0:next->nodes[n->parent].id;
        uint64_t old_parent_inc=!a || a->parent==NX_AX_NONE?0:old->nodes[a->parent].incarnation;
        uint64_t new_parent_inc=n->parent==NX_AX_NONE?0:next->nodes[n->parent].incarnation;
        nx_ax_ref r={b->hub,b,(uint32_t)i};
        if (!same) nx_ax_cache_signal(r,1);
        if (!same || old_parent!=new_parent || old_parent_inc!=new_parent_inc || a->index!=n->index) {
            nx_ax_event(nx_ax_parent(r),"ChildrenChanged","add",nx_ax_index(r),0,NULL,&r);
            if (same) { nx_ax_ref parent=nx_ax_parent(r); nx_ax_event(r,"PropertyChange","accessible-parent",0,0,NULL,&parent); }
        }
        if (!same) continue;
        if (strcmp(a->name,n->name)) nx_ax_event(r,"PropertyChange","accessible-name",0,0,n->name,NULL);
        if (strcmp(a->description,n->description)) nx_ax_event(r,"PropertyChange","accessible-description",0,0,n->description,NULL);
        if (strcmp(a->value,n->value)) nx_ax_event(r,"AttributesChanged","nexaui-value",0,0,n->value,NULL);
        if (a->bounds.x!=n->bounds.x || a->bounds.y!=n->bounds.y ||
            a->bounds.width!=n->bounds.width || a->bounds.height!=n->bounds.height) nx_ax_bounds_event(r);
        uint64_t after=nx_ax_states(r);
        b->snapshot=old; uint64_t before=nx_ax_states((nx_ax_ref){b->hub,b,prior}); b->snapshot=next;
        static const struct { unsigned bit; const char *name; } states[]={
            {3,"busy"},{4,"checked"},{5,"collapsed"},{8,"enabled"},{9,"expandable"},{10,"expanded"},
            {11,"focusable"},{12,"focused"},{22,"selectable"},{23,"selected"},{24,"sensitive"},
            {25,"showing"},{30,"visible"},{32,"indeterminate"},{33,"required"},{36,"invalid-entry"},{41,"checkable"},{43,"read-only"}
        };
        for (size_t j=0;j<sizeof(states)/sizeof(*states);++j) {
            uint64_t bit=UINT64_C(1)<<states[j].bit;
            if ((before^after)&bit) nx_ax_event(r,"StateChanged",states[j].name,(after&bit)!=0,0,NULL,NULL);
        }
        uint64_t selected_before=a->selected==NX_AX_NONE?0:old->nodes[a->selected].id;
        uint64_t selected_after=n->selected==NX_AX_NONE?0:next->nodes[n->selected].id;
        uint64_t selected_before_inc=a->selected==NX_AX_NONE?0:old->nodes[a->selected].incarnation;
        uint64_t selected_after_inc=n->selected==NX_AX_NONE?0:next->nodes[n->selected].incarnation;
        if ((n->state&NX_SEM_SELECTION_SINGLE) &&
            (selected_before!=selected_after || selected_before_inc!=selected_after_inc))
            nx_ax_event(r,"SelectionChanged","",0,0,NULL,NULL);
    }
    b->spare=old;
    const char *before=old && old->first!=NX_AX_NONE?old->nodes[old->first].name:"NexaUI";
    const char *after=nx_ax_name(nx_ax_window(b));
    if (strcmp(before,after)) {
        nx_ax_event(nx_ax_window(b),"PropertyChange","accessible-name",0,0,after,NULL);
    }
}
nx_result nx_linux_ax_publish(nx_linux_ax **address,nx_window *window,nx_allocator allocator,const nx_context *context) {
    nx_linux_ax *b=*address; int created=0;
    if (!b) {
        const char *disabled=getenv("NO_AT_BRIDGE");
        const char *direct=getenv("AT_SPI_BUS_ADDRESS"),*session=getenv("DBUS_SESSION_BUS_ADDRESS");
        if (disabled && !strcmp(disabled,"1")) return NX_ERROR_UNSUPPORTED;
        if ((!nx_ax_thread_hub || !nx_ax_thread_hub->connection) &&
            (!direct || !*direct) && (!session || !*session)) return NX_ERROR_UNSUPPORTED;
        pthread_once(&nx_ax_loader_once,nx_ax_load);
        if (!nx_ax_loaded) return NX_ERROR_UNSUPPORTED;
    }
    if (!b) {
        b=allocator.allocate(allocator.user,sizeof(*b)); if (!b) return NX_ERROR_MEMORY;
        *b=(nx_linux_ax){0}; b->allocator=allocator; b->thread=pthread_self(); b->window=window;
        b->epoch=nx_native_next_identity();
        if (!b->epoch) { allocator.deallocate(allocator.user,b,sizeof(*b)); return NX_ERROR_LIMIT; }
        created=1;
    } else if (!pthread_equal(pthread_self(),b->thread)) return NX_ERROR_STATE;
    nx_result result=nx_ax_copy(b,context);
    if (result!=NX_OK) {
        if (created) {
            if (b->spare) allocator.deallocate(allocator.user,b->spare,b->spare->capacity);
            allocator.deallocate(allocator.user,b,sizeof(*b));
        }
        return result;
    }
    if (created) {
        nx_ax_hub *h=nx_ax_thread_hub;
        if (!h) {
            h=&nx_ax_local_hub;
            *h=(nx_ax_hub){0}; nx_ax_thread_hub=h;
        }
        if (!h->windows) {
            const char *name=b->spare->first==NX_AX_NONE?"NexaUI":b->spare->nodes[b->spare->first].name;
            size_t length=strlen(name);
            if (!length || length>=sizeof(h->application_name)) { name="NexaUI"; length=strlen(name); }
            memcpy(h->application_name,name,length+1u);
        }
        b->hub=h; b->next=h->windows; h->windows=b; *address=b;
    }
    uint64_t binding=nx_context_identity(context);
    if (b->binding!=binding) b->request_count=0;
    b->binding=binding; b->frame=nx_context_frame_serial(context);
    for (size_t i=0;i<b->spare->count;++i)
        if (b->spare->nodes[i].incarnation>b->incarnation) b->incarnation=b->spare->nodes[i].incarnation;
    nx_ax_update_geometry(b);
    nx_ax_snapshot *next=b->spare;
    if (b->hub->connection && !b->hub->failed) {
        if (created) {
            b->snapshot=next; b->spare=NULL;
            nx_ax_cache_signal(nx_ax_window(b),1);
            nx_ax_ref child=nx_ax_window(b);
            nx_ax_event(nx_ax_app(b->hub),"ChildrenChanged","add",nx_ax_index(child),0,NULL,&child);
            b->snapshot=NULL; b->spare=next;
        }
        nx_ax_publish_delta(b,next);
    } else { nx_ax_snapshot *old=b->snapshot; b->snapshot=next; b->spare=old; }
    for (size_t i=0;i<b->request_count;) {
        if (b->requests[i].forwarded && b->requests[i].frame!=b->frame) {
            --b->request_count; memmove(b->requests+i,b->requests+i+1,(b->request_count-i)*sizeof(*b->requests));
        } else ++i;
    }
    if (b->hub->failed) nx_ax_disconnect(b->hub);
    if (!nx_ax_connect(b->hub)) return NX_ERROR_UNSUPPORTED;
    nx_linux_ax_pump(b);
    return b->hub->connection?NX_OK:NX_ERROR_UNSUPPORTED;
}
void nx_linux_ax_destroy(nx_linux_ax *b) {
    if (!b || !pthread_equal(pthread_self(),b->thread)) return;
    nx_ax_hub *h=b->hub;
    if (h->connection && b->snapshot) {
        nx_ax_ref child=nx_ax_window(b);
        nx_ax_event(nx_ax_app(h),"ChildrenChanged","remove",nx_ax_index(child),0,NULL,&child);
        for (size_t i=b->snapshot->count;i>0;--i) nx_ax_cache_signal((nx_ax_ref){h,b,(uint32_t)i-1u},0);
        nx_ax_cache_signal(child,0);
        /* One nonblocking write pass before removing the endpoint. */
        d.connection_read_write(h->connection,0);
    }
    nx_linux_ax **link=&h->windows; while (*link && *link!=b) link=&(*link)->next;
    if (*link) *link=b->next;
    if (b->snapshot) b->allocator.deallocate(b->allocator.user,b->snapshot,b->snapshot->capacity);
    if (b->spare) b->allocator.deallocate(b->allocator.user,b->spare,b->spare->capacity);
    nx_allocator allocator=b->allocator; allocator.deallocate(allocator.user,b,sizeof(*b));
    if (!h->windows) {
        nx_ax_disconnect(h); nx_ax_thread_hub=NULL;
        *h=(nx_ax_hub){0};
    }
}
#endif
