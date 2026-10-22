#define COBJMACROS
#define NOMINMAX
#include "accessibility.h"
#include "../../src/context_identity.h"
#include "../../src/text_value.h"
#include "../../src/selection_model.h"
#include <objbase.h>
#include <oleauto.h>
/* The SDK also defines this external const object in the provider header.
   Namespace it locally so C clients can include UIAutomation.h themselves. */
#define UIA_ScrollPatternNoScroll nx_uia_private_scroll_pattern_no_scroll
#include <uiautomationcore.h>
#undef UIA_ScrollPatternNoScroll
#include <uiautomationcoreapi.h>
#include "uia_ids.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#define NX_UIA_NONE UINT32_MAX
#define NX_UIA_NODES 65536u
#define NX_UIA_BYTES (32u*1024u*1024u)
#define NX_UIA_WAKE (WM_APP+0x4e1u)
#define NX_UIA_SELECTION_FLAGS (NX_SEM_SELECTION_SINGLE|NX_SEM_SELECTION_MULTIPLE)

typedef struct nx_uia_node {
    uint64_t id, incarnation;
    uint32_t parent, first, last, next, previous;
    uint32_t selection_owner, selected, focus_target, selection_events;
    uint32_t selected_last, selected_count, selection_next;
    nx_semantic_role role;
    uint32_t state, actions;
    nx_text_value_target text_value;
    nx_selection_target selection_target;
    uint64_t selection_membership;
    uint32_t property_events;
    nx_rect bounds;
    WCHAR *name, *help, *value;
} nx_uia_node;
#pragma warning(push)
#pragma warning(disable:4200)
typedef struct nx_uia_snapshot {
    size_t capacity, count, slot_count;
    uint32_t first, last;
    uint32_t *slots;
    nx_uia_node nodes[];
} nx_uia_snapshot;
#pragma warning(pop)
typedef struct nx_uia_element nx_uia_element;
typedef struct nx_uia_request {
    uint64_t id, incarnation; uint32_t action;
    uint64_t owner_id, owner_incarnation, source_frame;
    uint64_t value_generation, request_key;
    nx_text_value_target text_value;
    nx_selection_target selection_target;
    uint64_t selection_membership;
    char *text;
    size_t text_length;
    int forwarded, routing;
} nx_uia_request;
typedef struct nx_uia_selection_event { uint64_t id, incarnation; uint32_t flags; } nx_uia_selection_event;
typedef struct nx_uia_property_event {
    uint64_t id, incarnation;
    PROPERTYID property;
    VARIANT before, after;
} nx_uia_property_event;
struct nx_uia_bridge {
    LONG refs;
    SRWLOCK lock;
    HWND hwnd;
    nx_allocator allocator;
    nx_uia_snapshot *snapshot, *spare;
    nx_uia_element *root;
    uint64_t incarnation, context_identity, epoch, source_frame;
    uint64_t published_focus_id, published_focus_inc;
    int published_native_focus;
    int published_enabled;
    uint64_t value_generation, request_sequence;
    POINT origin;
    double scale, width, height;
    int visible, enabled, focused, publishing, com_balance;
    WCHAR *title;
    nx_uia_request requests[64];
    size_t request_count;
};
struct nx_uia_element {
    IRawElementProviderSimple simple;
    IRawElementProviderFragment fragment;
    IRawElementProviderFragmentRoot root;
    IInvokeProvider invoke;
    IToggleProvider toggle;
    ISelectionProvider selection;
    ISelectionItemProvider selection_item;
    IValueProvider value;
    LONG refs;
    nx_uia_bridge *bridge;
    uint64_t id, incarnation;
    IUnknown *marshaler;
};

static void bridge_add(nx_uia_bridge *b) { InterlockedIncrement(&b->refs); }
static void bridge_release(nx_uia_bridge *b) {
    if (!InterlockedDecrement(&b->refs)) {
        if (b->title) HeapFree(GetProcessHeap(),0,b->title);
        HeapFree(GetProcessHeap(),0,b);
    }
}
static void value_guard_retain(void *user) { bridge_add(user); }
static void value_guard_release(void *user) { bridge_release(user); }
static void request_remove(nx_uia_bridge *b, size_t at) {
    if (b->requests[at].text) HeapFree(GetProcessHeap(),0,b->requests[at].text);
    --b->request_count;
    memmove(b->requests+at,b->requests+at+1,(b->request_count-at)*sizeof(*b->requests));
}
static void requests_clear(nx_uia_bridge *b) {
    while (b->request_count) request_remove(b,b->request_count-1u);
}
static ULONG element_add(nx_uia_element *e) { return (ULONG)InterlockedIncrement(&e->refs); }
static ULONG element_release(nx_uia_element *e) {
    ULONG refs=(ULONG)InterlockedDecrement(&e->refs);
    if (!refs) {
        if (e->marshaler) IUnknown_Release(e->marshaler);
        bridge_release(e->bridge); HeapFree(GetProcessHeap(),0,e);
    }
    return refs;
}
static HRESULT element_query(nx_uia_element *e,REFIID iid,void **out) {
    if (!out) return E_POINTER; *out=NULL;
    if (IsEqualIID(iid,&IID_IUnknown) || IsEqualIID(iid,&IID_IRawElementProviderSimple)) *out=&e->simple;
    else if (IsEqualIID(iid,&IID_IRawElementProviderFragment)) *out=&e->fragment;
    else if (!e->id && IsEqualIID(iid,&IID_IRawElementProviderFragmentRoot)) *out=&e->root;
    else if (e->id && IsEqualIID(iid,&IID_IInvokeProvider)) *out=&e->invoke;
    else if (e->id && IsEqualIID(iid,&IID_IToggleProvider)) *out=&e->toggle;
    else if (e->id && IsEqualIID(iid,&IID_ISelectionProvider)) *out=&e->selection;
    else if (e->id && IsEqualIID(iid,&IID_ISelectionItemProvider)) *out=&e->selection_item;
    else if (e->id && IsEqualIID(iid,&IID_IValueProvider)) *out=&e->value;
    else if (IsEqualIID(iid,&IID_IMarshal) && e->marshaler) return IUnknown_QueryInterface(e->marshaler,iid,out);
    else return E_NOINTERFACE;
    element_add(e); return S_OK;
}
#define NX_UIA_IUNKNOWN(prefix,type,member) \
static nx_uia_element *prefix##_object(type *self) { return CONTAINING_RECORD(self,nx_uia_element,member); } \
static HRESULT STDMETHODCALLTYPE prefix##_query(type *self,REFIID iid,void **out) { return element_query(prefix##_object(self),iid,out); } \
static ULONG STDMETHODCALLTYPE prefix##_add(type *self) { return element_add(prefix##_object(self)); } \
static ULONG STDMETHODCALLTYPE prefix##_release(type *self) { return element_release(prefix##_object(self)); }
NX_UIA_IUNKNOWN(simple,IRawElementProviderSimple,simple)
NX_UIA_IUNKNOWN(fragment,IRawElementProviderFragment,fragment)
NX_UIA_IUNKNOWN(root,IRawElementProviderFragmentRoot,root)
NX_UIA_IUNKNOWN(invoke,IInvokeProvider,invoke)
NX_UIA_IUNKNOWN(toggle,IToggleProvider,toggle)
NX_UIA_IUNKNOWN(selection,ISelectionProvider,selection)
NX_UIA_IUNKNOWN(selection_item,ISelectionItemProvider,selection_item)
NX_UIA_IUNKNOWN(value,IValueProvider,value)

static uint32_t find(const nx_uia_snapshot *s,uint64_t id) {
    if (!s || !id) return NX_UIA_NONE;
    size_t slot=(size_t)((id^(id>>33))*UINT64_C(0xff51afd7ed558ccd))&(s->slot_count-1u);
    while (s->slots[slot]) {
        uint32_t index=s->slots[slot]-1u;
        if (s->nodes[index].id==id) return index;
        slot=(slot+1u)&(s->slot_count-1u);
    }
    return NX_UIA_NONE;
}
/* A returned node is borrowed only while the caller holds b->lock. */
static HRESULT current(nx_uia_element *e,nx_uia_node **out) {
    *out=NULL;
    nx_uia_bridge *b=e->bridge;
    if (!b->hwnd) return UIA_E_ELEMENTNOTAVAILABLE;
    if (!e->id) return S_OK;
    uint32_t index=find(b->snapshot,e->id);
    if (index==NX_UIA_NONE || b->snapshot->nodes[index].incarnation!=e->incarnation) return UIA_E_ELEMENTNOTAVAILABLE;
    *out=&b->snapshot->nodes[index]; return S_OK;
}
static HRESULT create_element(nx_uia_bridge *b,uint64_t id,uint64_t incarnation,nx_uia_element **out);
static CONTROLTYPEID control_type(nx_semantic_role role) {
    switch (role) {
    case NX_ROLE_WINDOW: case NX_ROLE_PANEL: case NX_ROLE_TAB_PANEL: case NX_ROLE_POPUP: case NX_ROLE_DIALOG: return NX_UIA_PaneControlTypeId;
    case NX_ROLE_TAB_LIST: return NX_UIA_TabControlTypeId;
    case NX_ROLE_GROUP: return NX_UIA_GroupControlTypeId;
    case NX_ROLE_TEXT: return NX_UIA_TextControlTypeId;
    case NX_ROLE_BUTTON: return NX_UIA_ButtonControlTypeId;
    case NX_ROLE_CHECKBOX: case NX_ROLE_SWITCH: return NX_UIA_CheckBoxControlTypeId;
    case NX_ROLE_RADIO: return NX_UIA_RadioButtonControlTypeId;
    case NX_ROLE_SLIDER: return NX_UIA_SliderControlTypeId;
    case NX_ROLE_TEXT_FIELD: return NX_UIA_EditControlTypeId;
    case NX_ROLE_IMAGE: return NX_UIA_ImageControlTypeId;
    case NX_ROLE_LIST: return NX_UIA_ListControlTypeId;
    case NX_ROLE_LIST_ITEM: return NX_UIA_ListItemControlTypeId;
    case NX_ROLE_TABLE: return NX_UIA_TableControlTypeId;
    case NX_ROLE_ROW: return NX_UIA_DataItemControlTypeId;
    case NX_ROLE_CELL: return NX_UIA_CustomControlTypeId;
    case NX_ROLE_COLUMN_HEADER: return NX_UIA_HeaderItemControlTypeId;
    case NX_ROLE_TAB: return NX_UIA_TabItemControlTypeId;
    case NX_ROLE_MENU: return NX_UIA_MenuControlTypeId;
    case NX_ROLE_MENU_BAR: return NX_UIA_MenuBarControlTypeId;
    case NX_ROLE_MENU_ITEM: return NX_UIA_MenuItemControlTypeId;
    case NX_ROLE_TOOLBAR: return NX_UIA_ToolBarControlTypeId;
    case NX_ROLE_STATUS: return NX_UIA_StatusBarControlTypeId;
    case NX_ROLE_TREE: return NX_UIA_TreeControlTypeId;
    case NX_ROLE_TREE_ITEM: return NX_UIA_TreeItemControlTypeId;
    case NX_ROLE_PROGRESS: return NX_UIA_ProgressBarControlTypeId;
    case NX_ROLE_SEPARATOR: return NX_UIA_SeparatorControlTypeId;
    case NX_ROLE_SCROLL_AREA: return NX_UIA_PaneControlTypeId;
    case NX_ROLE_TOOLTIP: return NX_UIA_ToolTipControlTypeId;
    default: return NX_UIA_CustomControlTypeId;
    }
}
static int can_toggle(const nx_uia_node *n) {
    return n && (n->role==NX_ROLE_CHECKBOX || n->role==NX_ROLE_SWITCH) && (n->actions&NX_ACTION_PRESS);
}
static int selected(const nx_uia_node *n) {
    return n && (n->state & (n->role==NX_ROLE_RADIO ? NX_SEM_CHECKED : NX_SEM_SELECTED))!=0;
}
static int can_select(const nx_uia_snapshot *snapshot,const nx_uia_node *n) {
    if(!snapshot || !n || !(n->actions&NX_ACTION_SELECT) || n->selection_owner==NX_UIA_NONE)return 0;
    const nx_uia_node *owner=&snapshot->nodes[n->selection_owner];
    return (owner->state&NX_SEM_SELECTION_SINGLE) ||
        ((owner->state&NX_SEM_SELECTION_MULTIPLE) && n->selection_membership &&
         n->selection_target.identity==owner->selection_target.identity &&
         n->selection_target.revision==owner->selection_target.revision);
}
static int selection_action(uint32_t action) {
    return action==NX_ACTION_SELECT || action==NX_ACTION_ADD_SELECTION || action==NX_ACTION_DESELECT;
}
static HRESULT selection_array(nx_uia_element *e,SAFEARRAY **out);
static HRESULT selection_owner_provider(nx_uia_element *e,IRawElementProviderSimple **out);
static struct UiaRect bounds(nx_uia_bridge *b,const nx_uia_node *n) {
    struct UiaRect r={b->origin.x,b->origin.y,b->width,b->height};
    if (n) { r.left+=n->bounds.x*b->scale; r.top+=n->bounds.y*b->scale;
        r.width=n->bounds.width*b->scale; r.height=n->bounds.height*b->scale; }
    return r;
}
static int offscreen(nx_uia_bridge *b,const nx_uia_node *n) {
    struct UiaRect r=bounds(b,n);
    return (n && (n->state&NX_SEM_OFFSCREEN)) || !b->visible || r.width<=0 || r.height<=0 || r.left+r.width<=b->origin.x ||
        r.top+r.height<=b->origin.y || r.left>=b->origin.x+b->width || r.top>=b->origin.y+b->height;
}
static HRESULT string_value(VARIANT *out,const WCHAR *text) {
    V_VT(out)=VT_BSTR; V_BSTR(out)=SysAllocString(text ? text : L"");
    return V_BSTR(out) ? S_OK : E_OUTOFMEMORY;
}
/* Custom strings remain read-only. Only standard owned editors have a native
   write target; semantic SET_VALUE alone must not invent a model binding. */
static int can_value(const nx_uia_node *n) {
    return n && !(n->state&NX_SEM_PROTECTED) &&
        (n->role==NX_ROLE_TEXT_FIELD || (n->role==NX_ROLE_CUSTOM && !(n->actions&NX_ACTION_SET_VALUE)));
}
static int editable_value(const nx_uia_node *n) {
    return can_value(n) && n->role==NX_ROLE_TEXT_FIELD && n->text_value.identity &&
        !(n->state&NX_SEM_READONLY) && (n->actions&NX_ACTION_SET_VALUE);
}
static int value_guard_current(void *user,uint64_t token,uint64_t context_id,
                                uint64_t id,nx_text_value_target target) {
    nx_uia_bridge *b=user;
    AcquireSRWLockShared(&b->lock);
    uint32_t i=find(b->snapshot,id);
    const nx_uia_node *n=i==NX_UIA_NONE ? NULL : &b->snapshot->nodes[i];
    int okay=b->hwnd && b->enabled && b->context_identity==context_id &&
        b->value_generation!=UINT64_MAX && b->value_generation==token && editable_value(n) &&
        !(n->state&NX_SEM_DISABLED) && n->text_value.identity==target.identity &&
        n->text_value.revision==target.revision && n->text_value.multiline==target.multiline;
    ReleaseSRWLockShared(&b->lock);return okay;
}
static int multi_request_live(nx_uia_bridge *b,const nx_uia_request *r) {
    uint32_t at=find(b->snapshot,r->id);
    const nx_uia_node *n=at==NX_UIA_NONE?NULL:&b->snapshot->nodes[at];
    if(!b->hwnd||!b->enabled||b->value_generation==UINT64_MAX||r->value_generation!=b->value_generation||
       !can_select(b->snapshot,n)||n->incarnation!=r->incarnation||(n->state&NX_SEM_DISABLED))return 0;
    const nx_uia_node *owner=&b->snapshot->nodes[n->selection_owner];
    return owner->id==r->owner_id&&owner->incarnation==r->owner_incarnation&&
        (owner->state&NX_SEM_SELECTION_MULTIPLE)&&!(owner->state&NX_SEM_DISABLED)&&
        n->selection_target.identity==r->selection_target.identity&&
        n->selection_target.revision==r->selection_target.revision&&
        n->selection_membership==r->selection_membership;
}
static int selection_guard_current(void *user,uint64_t token,uint64_t context_id,
    uint64_t id,uint64_t owner_id,nx_selection_target target) {
    nx_uia_bridge *b=user;AcquireSRWLockShared(&b->lock);
    uint32_t at=find(b->snapshot,id);const nx_uia_node *n=at==NX_UIA_NONE?NULL:&b->snapshot->nodes[at];
    int okay=b->hwnd&&b->enabled&&b->context_identity==context_id&&
        b->value_generation!=UINT64_MAX&&b->value_generation==token&&can_select(b->snapshot,n)&&
        !(n->state&NX_SEM_DISABLED)&&b->snapshot->nodes[n->selection_owner].id==owner_id&&
        n->selection_target.identity==target.identity&&n->selection_target.revision==target.revision;
    ReleaseSRWLockShared(&b->lock);return okay;
}
static HRESULT STDMETHODCALLTYPE simple_options(IRawElementProviderSimple *self,enum ProviderOptions *out) {
    (void)self; if (!out) return E_POINTER;
    *out=ProviderOptions_ServerSideProvider|ProviderOptions_ProviderOwnsSetFocus; return S_OK;
}
static HRESULT STDMETHODCALLTYPE simple_pattern(IRawElementProviderSimple *self,PATTERNID pattern,IUnknown **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_element *e=simple_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && n) {
        if (pattern==NX_UIA_SelectionPatternId && (n->state&NX_UIA_SELECTION_FLAGS)) { *out=(IUnknown *)&e->selection; }
        else if (pattern==NX_UIA_SelectionItemPatternId && can_select(b->snapshot,n)) { *out=(IUnknown *)&e->selection_item; }
        else if (pattern==NX_UIA_TogglePatternId && can_toggle(n)) *out=(IUnknown *)&e->toggle;
        else if (pattern==NX_UIA_InvokePatternId && (n->actions&NX_ACTION_PRESS) && !can_toggle(n)) *out=(IUnknown *)&e->invoke;
        else if (pattern==NX_UIA_ValuePatternId && can_value(n)) *out=(IUnknown *)&e->value;
        if (*out) element_add(e);
    }
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE simple_property(IRawElementProviderSimple *self,PROPERTYID property,VARIANT *out) {
    if (!out) return E_POINTER; VariantInit(out);
    nx_uia_element *e=simple_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    if (property==NX_UIA_SelectionSelectionPropertyId) {
        SAFEARRAY *array=NULL; HRESULT result=selection_array(e,&array);
        if (result==UIA_E_NOTSUPPORTED) return S_OK;
        if (SUCCEEDED(result)) { V_VT(out)=VT_ARRAY|VT_UNKNOWN; V_ARRAY(out)=array; }
        return result;
    }
    if (property==NX_UIA_SelectionItemSelectionContainerPropertyId) {
        IRawElementProviderSimple *owner=NULL; HRESULT result=selection_owner_provider(e,&owner);
        if (result==UIA_E_NOTSUPPORTED) return S_OK;
        if (SUCCEEDED(result)) { V_VT(out)=VT_UNKNOWN; V_UNKNOWN(out)=(IUnknown *)owner; }
        return result;
    }
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (FAILED(hr)) { ReleaseSRWLockShared(&b->lock); return hr; }
    uint32_t state=(n ? n->state : 0) | (b->enabled ? 0u : NX_SEM_DISABLED),actions=n ? n->actions : NX_ACTION_FOCUS;
    if (property==NX_UIA_NamePropertyId) { hr=string_value(out,n ? n->name : b->title);
    } else if (property==NX_UIA_HelpTextPropertyId) { hr=string_value(out,n ? n->help : L"");
    } else if (property==NX_UIA_ValueValuePropertyId) {
        if (state&NX_SEM_PROTECTED) hr=E_ACCESSDENIED;
        else if (can_value(n)) hr=string_value(out,n->value);
    } else if (property==NX_UIA_IsValuePatternAvailablePropertyId) { V_VT(out)=VT_BOOL;V_BOOL(out)=can_value(n)?VARIANT_TRUE:VARIANT_FALSE;
    } else if (property==NX_UIA_ValueIsReadOnlyPropertyId) {
        if(can_value(n)) { V_VT(out)=VT_BOOL;V_BOOL(out)=editable_value(n)?VARIANT_FALSE:VARIANT_TRUE; }
    } else if (property==NX_UIA_FrameworkIdPropertyId) { hr=string_value(out,L"NexaUI");
    } else if (property==NX_UIA_AutomationIdPropertyId) { {
        WCHAR id[48]; swprintf_s(id,48,L"nexa-%016llx",(unsigned long long)e->id); hr=string_value(out,id);
    }
    } else if (property==NX_UIA_ControlTypePropertyId) { V_VT(out)=VT_I4; V_I4(out)=n ? control_type(n->role) : NX_UIA_WindowControlTypeId;
    } else if (property==NX_UIA_IsEnabledPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=(state&NX_SEM_DISABLED) ? VARIANT_FALSE : VARIANT_TRUE;
    } else if (property==NX_UIA_IsKeyboardFocusablePropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=((actions&NX_ACTION_FOCUS) || (n && (state&NX_UIA_SELECTION_FLAGS) && n->focus_target!=NX_UIA_NONE)) && !(state&NX_SEM_DISABLED) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_HasKeyboardFocusPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=b->focused && (state&NX_SEM_FOCUSED) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_IsPasswordPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=(state&NX_SEM_PROTECTED) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_IsRequiredForFormPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=(state&NX_SEM_REQUIRED) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_IsDataValidForFormPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=(state&NX_SEM_INVALID) ? VARIANT_FALSE : VARIANT_TRUE;
    } else if (property==NX_UIA_IsOffscreenPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=offscreen(b,n) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_IsControlElementPropertyId || property==NX_UIA_IsContentElementPropertyId) { V_VT(out)=VT_BOOL; V_BOOL(out)=VARIANT_TRUE;
    } else if (property==NX_UIA_SelectionCanSelectMultiplePropertyId && n && (state&NX_UIA_SELECTION_FLAGS)) { V_VT(out)=VT_BOOL; V_BOOL(out)=(state&NX_SEM_SELECTION_MULTIPLE) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_SelectionIsSelectionRequiredPropertyId && n && (state&NX_UIA_SELECTION_FLAGS)) { V_VT(out)=VT_BOOL; V_BOOL(out)=(state&NX_SEM_SELECTION_REQUIRED) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_SelectionItemIsSelectedPropertyId && can_select(b->snapshot,n)) { V_VT(out)=VT_BOOL; V_BOOL(out)=selected(n) ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (property==NX_UIA_ToggleToggleStatePropertyId) { V_VT(out)=VT_I4; V_I4(out)=(state&NX_SEM_MIXED) ? ToggleState_Indeterminate : (state&NX_SEM_CHECKED) ? ToggleState_On : ToggleState_Off;
    } else if (property==NX_UIA_NativeWindowHandlePropertyId) { if (!n) { V_VT(out)=VT_I4; V_I4(out)=(LONG)(INT_PTR)b->hwnd; }
    } /* VT_EMPTY asks the host/default provider for other properties. */
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE simple_host(IRawElementProviderSimple *self,IRawElementProviderSimple **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_element *e=simple_object(self); nx_uia_bridge *b=e->bridge;
    AcquireSRWLockShared(&b->lock); HWND hwnd=b->hwnd; ReleaseSRWLockShared(&b->lock);
    if (!hwnd) return UIA_E_ELEMENTNOTAVAILABLE;
    if (e->id) return S_OK;
    HRESULT hr=UiaHostProviderFromHwnd(hwnd,out); /* Never call out under the snapshot lock. */
    AcquireSRWLockShared(&b->lock); int alive=b->hwnd==hwnd; ReleaseSRWLockShared(&b->lock);
    if (!alive) { if (*out) IRawElementProviderSimple_Release(*out); *out=NULL; return UIA_E_ELEMENTNOTAVAILABLE; }
    return hr;
}
static HRESULT STDMETHODCALLTYPE fragment_navigate(IRawElementProviderFragment *self,enum NavigateDirection direction,IRawElementProviderFragment **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_element *e=fragment_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    uint32_t next=NX_UIA_NONE; int parent=0;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr)) {
        switch (direction) {
        case NavigateDirection_Parent: if (n) { next=n->parent; parent=1; } break;
        case NavigateDirection_FirstChild: next=n ? n->first : b->snapshot ? b->snapshot->first : NX_UIA_NONE; break;
        case NavigateDirection_LastChild: next=n ? n->last : b->snapshot ? b->snapshot->last : NX_UIA_NONE; break;
        case NavigateDirection_NextSibling: if (n) next=n->next; break;
        case NavigateDirection_PreviousSibling: if (n) next=n->previous; break;
        default: hr=E_INVALIDARG;
        }
        if (next!=NX_UIA_NONE) {
            /* Capture identity while locked, not an index that a newer tree can reuse. */
            uint64_t id=b->snapshot->nodes[next].id,inc=b->snapshot->nodes[next].incarnation;
            ReleaseSRWLockShared(&b->lock);
            nx_uia_element *target=NULL; hr=create_element(b,id,inc,&target);
            if (SUCCEEDED(hr)) *out=&target->fragment; return hr;
        }
        if (parent) { *out=&b->root->fragment; element_add(b->root); }
    }
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE fragment_id(IRawElementProviderFragment *self,SAFEARRAY **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_element *e=fragment_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n); ReleaseSRWLockShared(&b->lock);
    if (FAILED(hr) || !e->id) return hr;
    LONG ids[7]={UiaAppendRuntimeId,(LONG)(uint32_t)e->id,(LONG)(uint32_t)(e->id>>32),
        (LONG)(uint32_t)b->epoch,(LONG)(uint32_t)(b->epoch>>32),
        (LONG)(uint32_t)e->incarnation,(LONG)(uint32_t)(e->incarnation>>32)};
    SAFEARRAY *array=SafeArrayCreateVector(VT_I4,0,7); if (!array) return E_OUTOFMEMORY;
    for (LONG i=0;i<7;++i) if (FAILED(hr=SafeArrayPutElement(array,&i,&ids[i]))) { SafeArrayDestroy(array); return hr; }
    *out=array; return S_OK;
}
static HRESULT STDMETHODCALLTYPE fragment_bounds(IRawElementProviderFragment *self,struct UiaRect *out) {
    if (!out) return E_POINTER; *out=(struct UiaRect){0};
    nx_uia_element *e=fragment_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n); if (SUCCEEDED(hr)) *out=bounds(b,n);
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE fragment_embedded(IRawElementProviderFragment *self,SAFEARRAY **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_element *e=fragment_object(self); nx_uia_node *n;
    AcquireSRWLockShared(&e->bridge->lock); HRESULT hr=current(e,&n);
    ReleaseSRWLockShared(&e->bridge->lock); return hr;
}
static HRESULT request(nx_uia_element *e,uint32_t action) {
    nx_uia_bridge *b=e->bridge; nx_uia_node *n; HWND hwnd=NULL;
    AcquireSRWLockExclusive(&b->lock); HRESULT hr=current(e,&n);
    int choosing=selection_action(action);
    nx_uia_node *owner=SUCCEEDED(hr) && choosing && can_select(b->snapshot,n) ? &b->snapshot->nodes[n->selection_owner] : NULL;
    if (SUCCEEDED(hr) && (!b->enabled || (n && (n->state&NX_SEM_DISABLED)))) hr=UIA_E_ELEMENTNOTENABLED;
    if (SUCCEEDED(hr) && choosing && !owner) hr=UIA_E_NOTSUPPORTED;
    if (SUCCEEDED(hr) && !choosing && n && !(n->actions&action)) hr=UIA_E_NOTSUPPORTED;
    if (SUCCEEDED(hr) && !n && action!=NX_ACTION_FOCUS) hr=UIA_E_NOTSUPPORTED;
    if (SUCCEEDED(hr) && choosing) {
        /* Serialize selection transactions per group through completed publication.
           This includes requests already forwarded to the core but not yet drawn. */
        for (size_t i=0;i<b->request_count;) {
            nx_uia_request *r=&b->requests[i];
            if (r->owner_id==owner->id && r->owner_incarnation==owner->incarnation) {
                if(r->selection_target.identity&&!multi_request_live(b,r)){request_remove(b,i);continue;}
                HRESULT pending=r->id==e->id && r->incarnation==e->incarnation && r->action==action ? S_OK : HRESULT_FROM_WIN32(ERROR_BUSY);
                ReleaseSRWLockExclusive(&b->lock); return pending;
            }
            ++i;
        }
        int multiple=(owner->state&NX_SEM_SELECTION_MULTIPLE)!=0;
        if ((action==NX_ACTION_DESELECT && !selected(n)) ||
            (selected(n) && (action==NX_ACTION_ADD_SELECTION || (action==NX_ACTION_SELECT && (!multiple||owner->selected_count==1u))))) {
            ReleaseSRWLockExclusive(&b->lock); return S_OK;
        }
        if ((action==NX_ACTION_DESELECT && (owner->state&NX_SEM_SELECTION_REQUIRED) && (!multiple||owner->selected_count<=1u)) ||
            (action==NX_ACTION_ADD_SELECTION && !multiple && owner->selected!=NX_UIA_NONE) ||
            (multiple&&b->value_generation==UINT64_MAX)) hr=UIA_E_INVALIDOPERATION;
    }
    if (SUCCEEDED(hr)) {
        if (action==NX_ACTION_FOCUS) {
            for (size_t i=0;i<b->request_count;) {
                if (b->requests[i].action==NX_ACTION_FOCUS) {
                    request_remove(b,i);
                } else ++i;
            }
        }
        for (size_t i=0;i<b->request_count;++i) {
            if (b->requests[i].id==e->id && b->requests[i].incarnation==e->incarnation && b->requests[i].action==action) {
                ReleaseSRWLockExclusive(&b->lock); return HRESULT_FROM_WIN32(ERROR_BUSY);
            }
        }
        if (b->request_count==64) hr=HRESULT_FROM_WIN32(ERROR_BUSY);
        if (SUCCEEDED(hr)) {
            nx_uia_request r={0}; r.id=e->id; r.incarnation=e->incarnation; r.action=action;
            if (owner) { r.owner_id=owner->id; r.owner_incarnation=owner->incarnation; }
            if(owner&&(owner->state&NX_SEM_SELECTION_MULTIPLE)) {
                r.selection_target=n->selection_target;r.selection_membership=n->selection_membership;
                r.value_generation=b->value_generation;
            }
            b->requests[b->request_count++]=r; hwnd=b->hwnd;
        }
    }
    ReleaseSRWLockExclusive(&b->lock);
    if (hwnd) PostMessageW(hwnd,NX_UIA_WAKE,0,0);
    return hr;
}
static HRESULT STDMETHODCALLTYPE fragment_focus(IRawElementProviderFragment *self) {
    nx_uia_element *e=fragment_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    int group=SUCCEEDED(hr) && n && (n->state&NX_UIA_SELECTION_FLAGS);
    uint64_t id=0,inc=0;
    if (group) {
        if (!b->enabled || (n->state&NX_SEM_DISABLED)) hr=UIA_E_ELEMENTNOTENABLED;
        else if (n->focus_target==NX_UIA_NONE) hr=UIA_E_NOTSUPPORTED;
        else { id=b->snapshot->nodes[n->focus_target].id; inc=b->snapshot->nodes[n->focus_target].incarnation; }
    }
    ReleaseSRWLockShared(&b->lock);
    if (FAILED(hr)) return hr;
    if (!group) return request(e,NX_ACTION_FOCUS);
    nx_uia_element *target=NULL; hr=create_element(b,id,inc,&target);
    if (SUCCEEDED(hr)) { hr=request(target,NX_ACTION_FOCUS); element_release(target); }
    return hr;
}
static HRESULT STDMETHODCALLTYPE fragment_root(IRawElementProviderFragment *self,IRawElementProviderFragmentRoot **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_element *e=fragment_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr)) { *out=&b->root->root; element_add(b->root); }
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE root_point(IRawElementProviderFragmentRoot *self,double x,double y,IRawElementProviderFragment **out) {
    if (!out) return E_POINTER; *out=NULL;
    if (!isfinite(x) || !isfinite(y)) return E_INVALIDARG;
    nx_uia_bridge *b=root_object(self)->bridge; uint64_t id=0,inc=0;
    AcquireSRWLockShared(&b->lock);
    if (!b->hwnd) { ReleaseSRWLockShared(&b->lock); return UIA_E_ELEMENTNOTAVAILABLE; }
    if (!b->visible || x<b->origin.x || y<b->origin.y || x>=b->origin.x+b->width || y>=b->origin.y+b->height) {
        ReleaseSRWLockShared(&b->lock); return S_OK;
    }
    if (b->snapshot) for (size_t i=b->snapshot->count;i-- >0;) {
        nx_uia_node *n=&b->snapshot->nodes[i]; struct UiaRect r=bounds(b,n);
        if(n->state&NX_SEM_OFFSCREEN)continue;
        if (x>=r.left && y>=r.top && x<r.left+r.width && y<r.top+r.height) { id=n->id; inc=n->incarnation; break; }
    }
    if (!id) { *out=&b->root->fragment; element_add(b->root); ReleaseSRWLockShared(&b->lock); return S_OK; }
    ReleaseSRWLockShared(&b->lock);
    nx_uia_element *e=NULL; HRESULT hr=create_element(b,id,inc,&e); if (SUCCEEDED(hr)) *out=&e->fragment; return hr;
}
static HRESULT STDMETHODCALLTYPE root_focus(IRawElementProviderFragmentRoot *self,IRawElementProviderFragment **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_bridge *b=root_object(self)->bridge; uint64_t id=0,inc=0;
    AcquireSRWLockShared(&b->lock);
    if (!b->hwnd) { ReleaseSRWLockShared(&b->lock); return UIA_E_ELEMENTNOTAVAILABLE; }
    if (b->focused && b->snapshot) for (size_t i=0;i<b->snapshot->count;++i)
        if (b->snapshot->nodes[i].state&NX_SEM_FOCUSED) { id=b->snapshot->nodes[i].id; inc=b->snapshot->nodes[i].incarnation; break; }
    ReleaseSRWLockShared(&b->lock);
    if (!id) return S_OK;
    nx_uia_element *e=NULL; HRESULT hr=create_element(b,id,inc,&e); if (SUCCEEDED(hr)) *out=&e->fragment; return hr;
}
/* COM interface identity is static. UIA pattern availability remains dynamic in
   GetPatternProvider; every operation revalidates the live capability separately. */
static HRESULT press_pattern(nx_uia_element *e,int toggle) {
    nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && (!n || !(n->actions&NX_ACTION_PRESS) || can_toggle(n)!=toggle)) hr=UIA_E_NOTSUPPORTED;
    ReleaseSRWLockShared(&b->lock); return FAILED(hr) ? hr : request(e,NX_ACTION_PRESS);
}
static HRESULT STDMETHODCALLTYPE invoke_action(IInvokeProvider *self) { return press_pattern(invoke_object(self),0); }
static HRESULT STDMETHODCALLTYPE toggle_action(IToggleProvider *self) { return press_pattern(toggle_object(self),1); }
static HRESULT STDMETHODCALLTYPE value_get(IValueProvider *self,BSTR *out) {
    if(!out) return E_POINTER;*out=NULL;
    nx_uia_element *e=value_object(self);nx_uia_bridge *b=e->bridge;nx_uia_node *n;
    AcquireSRWLockShared(&b->lock);HRESULT hr=current(e,&n);
    if(SUCCEEDED(hr)&&!can_value(n)) hr=UIA_E_NOTSUPPORTED;
    if(SUCCEEDED(hr)) { *out=SysAllocString(n->value?n->value:L"");if(!*out)hr=E_OUTOFMEMORY; }
    ReleaseSRWLockShared(&b->lock);return hr;
}
static HRESULT STDMETHODCALLTYPE value_readonly(IValueProvider *self,BOOL *out) {
    if(!out)return E_POINTER;*out=TRUE;
    nx_uia_element *e=value_object(self);nx_uia_bridge *b=e->bridge;nx_uia_node *n;
    AcquireSRWLockShared(&b->lock);HRESULT hr=current(e,&n);
    if(SUCCEEDED(hr)&&!can_value(n))hr=UIA_E_NOTSUPPORTED;
    if(SUCCEEDED(hr))*out=!editable_value(n);
    ReleaseSRWLockShared(&b->lock);return hr;
}
static HRESULT STDMETHODCALLTYPE value_set(IValueProvider *self,LPCWSTR text) {
    nx_uia_element *e=value_object(self);nx_uia_bridge *b=e->bridge;nx_uia_node *n;
    HWND hwnd=NULL;char *copy=NULL;
    AcquireSRWLockExclusive(&b->lock);HRESULT hr=current(e,&n);
    if(SUCCEEDED(hr)&&!can_value(n))hr=UIA_E_NOTSUPPORTED;
    if(SUCCEEDED(hr)&&(!b->enabled || (n->state&NX_SEM_DISABLED)))hr=UIA_E_ELEMENTNOTENABLED;
    if(SUCCEEDED(hr)&&!editable_value(n))hr=UIA_E_INVALIDOPERATION;
    if(SUCCEEDED(hr)&&(b->value_generation==UINT64_MAX||b->request_sequence==UINT64_MAX))hr=UIA_E_INVALIDOPERATION;
    if(SUCCEEDED(hr)&&!text)hr=E_INVALIDARG;
    if(SUCCEEDED(hr)) {
        for(size_t i=0;i<b->request_count;++i)
            if(b->requests[i].id==e->id && b->requests[i].action==NX_ACTION_SET_VALUE) {hr=HRESULT_FROM_WIN32(ERROR_BUSY);break;}
        if(b->request_count==64)hr=HRESULT_FROM_WIN32(ERROR_BUSY);
    }
    size_t length=0;int bytes=0;
    if(SUCCEEDED(hr)) {
        while(length<=NX_TEXT_VALUE_MAX_BYTES && text[length])++length;
        if(length>NX_TEXT_VALUE_MAX_BYTES)hr=HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        else if(!n->text_value.multiline && (wmemchr(text,L'\r',length)||wmemchr(text,L'\n',length)))hr=E_INVALIDARG;
        else if(length) {
            bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,(int)length,NULL,0,NULL,NULL);
            if(!bytes)hr=E_INVALIDARG;
            else if((size_t)bytes>NX_TEXT_VALUE_MAX_BYTES)hr=HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }
    }
    if(SUCCEEDED(hr)) {
        copy=HeapAlloc(GetProcessHeap(),0,(size_t)bytes+1u);
        if(!copy)hr=E_OUTOFMEMORY;
        else if(bytes && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,(int)length,copy,bytes,NULL,NULL)!=bytes)hr=E_INVALIDARG;
    }
    if(SUCCEEDED(hr)) {
        copy[bytes]=0;
        nx_uia_request r={0};r.id=e->id;r.incarnation=e->incarnation;r.action=NX_ACTION_SET_VALUE;
        r.text_value=n->text_value;r.text=copy;r.text_length=(size_t)bytes;
        r.value_generation=b->value_generation;r.request_key=++b->request_sequence;
        b->requests[b->request_count++]=r;
        copy=NULL;hwnd=b->hwnd;
    }
    ReleaseSRWLockExclusive(&b->lock);
    if(copy)HeapFree(GetProcessHeap(),0,copy);
    if(hwnd)PostMessageW(hwnd,NX_UIA_WAKE,0,0);
    return hr;
}
static HRESULT STDMETHODCALLTYPE toggle_state(IToggleProvider *self,enum ToggleState *out) {
    if (!out) return E_POINTER; *out=ToggleState_Off;
    nx_uia_element *e=toggle_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && can_toggle(n)) *out=(n->state&NX_SEM_MIXED) ? ToggleState_Indeterminate : (n->state&NX_SEM_CHECKED) ? ToggleState_On : ToggleState_Off;
    else if (SUCCEEDED(hr)) hr=UIA_E_NOTSUPPORTED;
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT selection_array(nx_uia_element *e,SAFEARRAY **out) {
    if (!out) return E_POINTER; *out=NULL;
    typedef struct selection_id {uint64_t id,incarnation;} selection_id;
    nx_uia_bridge *b=e->bridge; nx_uia_node *n;selection_id *ids=NULL;size_t count=0;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && (!n || !(n->state&NX_UIA_SELECTION_FLAGS))) hr=UIA_E_NOTSUPPORTED;
    if(SUCCEEDED(hr)) {
        count=n->selected_count;
        if(count>NX_UIA_NODES)hr=E_UNEXPECTED;
        else if(count) {
            /* Process heap only: never an application allocator or COM call
               under the snapshot lock. Copy one coherent set of scalar IDs. */
            ids=HeapAlloc(GetProcessHeap(),0,count*sizeof(*ids));
            if(!ids)hr=E_OUTOFMEMORY;
        }
        uint32_t at=n->selected;
        for(size_t i=0;SUCCEEDED(hr)&&i<count;++i) {
            if(at==NX_UIA_NONE||at>=b->snapshot->count){hr=E_UNEXPECTED;break;}
            const nx_uia_node *item=&b->snapshot->nodes[at];
            ids[i]=(selection_id){item->id,item->incarnation};at=item->selection_next;
        }
        if(SUCCEEDED(hr)&&at!=NX_UIA_NONE)hr=E_UNEXPECTED;
    }
    ReleaseSRWLockShared(&b->lock);
    SAFEARRAY *array=NULL;
    if(SUCCEEDED(hr)) {array=SafeArrayCreateVector(VT_UNKNOWN,0,(ULONG)count);if(!array)hr=E_OUTOFMEMORY;}
    for(size_t i=0;SUCCEEDED(hr)&&i<count;++i) {
        nx_uia_element *item=NULL;hr=create_element(b,ids[i].id,ids[i].incarnation,&item);
        if(SUCCEEDED(hr)){LONG at=(LONG)i;hr=SafeArrayPutElement(array,&at,&item->simple);element_release(item);}
    }
    if(ids)HeapFree(GetProcessHeap(),0,ids);
    if(FAILED(hr)){if(array)SafeArrayDestroy(array);return hr;}
    *out=array; return S_OK;
}
static HRESULT selection_owner_provider(nx_uia_element *e,IRawElementProviderSimple **out) {
    if (!out) return E_POINTER; *out=NULL;
    nx_uia_bridge *b=e->bridge; nx_uia_node *n; uint64_t id=0,inc=0;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && !can_select(b->snapshot,n)) hr=UIA_E_NOTSUPPORTED;
    if (SUCCEEDED(hr)) { nx_uia_node *owner=&b->snapshot->nodes[n->selection_owner]; id=owner->id; inc=owner->incarnation; }
    ReleaseSRWLockShared(&b->lock);
    if (FAILED(hr)) return hr;
    nx_uia_element *owner=NULL; hr=create_element(b,id,inc,&owner);
    if (SUCCEEDED(hr)) *out=&owner->simple; return hr;
}
static HRESULT STDMETHODCALLTYPE selection_get(ISelectionProvider *self,SAFEARRAY **out) { return selection_array(selection_object(self),out); }
static HRESULT selection_boolean(nx_uia_element *e,BOOL *out,int required) {
    if (!out) return E_POINTER; *out=FALSE;
    nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && (!n || !(n->state&NX_UIA_SELECTION_FLAGS))) hr=UIA_E_NOTSUPPORTED;
    if (SUCCEEDED(hr)) *out=(n->state&(required?NX_SEM_SELECTION_REQUIRED:NX_SEM_SELECTION_MULTIPLE)) ? TRUE : FALSE;
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE selection_multiple(ISelectionProvider *self,BOOL *out) { return selection_boolean(selection_object(self),out,0); }
static HRESULT STDMETHODCALLTYPE selection_required(ISelectionProvider *self,BOOL *out) { return selection_boolean(selection_object(self),out,1); }
static HRESULT STDMETHODCALLTYPE item_select(ISelectionItemProvider *self) { return request(selection_item_object(self),NX_ACTION_SELECT); }
static HRESULT STDMETHODCALLTYPE item_add(ISelectionItemProvider *self) { return request(selection_item_object(self),NX_ACTION_ADD_SELECTION); }
static HRESULT STDMETHODCALLTYPE item_remove(ISelectionItemProvider *self) { return request(selection_item_object(self),NX_ACTION_DESELECT); }
static HRESULT STDMETHODCALLTYPE item_selected(ISelectionItemProvider *self,BOOL *out) {
    if (!out) return E_POINTER; *out=FALSE;
    nx_uia_element *e=selection_item_object(self); nx_uia_bridge *b=e->bridge; nx_uia_node *n;
    AcquireSRWLockShared(&b->lock); HRESULT hr=current(e,&n);
    if (SUCCEEDED(hr) && !can_select(b->snapshot,n)) hr=UIA_E_NOTSUPPORTED;
    if (SUCCEEDED(hr)) *out=selected(n) ? TRUE : FALSE;
    ReleaseSRWLockShared(&b->lock); return hr;
}
static HRESULT STDMETHODCALLTYPE item_owner(ISelectionItemProvider *self,IRawElementProviderSimple **out) { return selection_owner_provider(selection_item_object(self),out); }
static ISelectionProviderVtbl selection_vtable={selection_query,selection_add,selection_release,selection_get,selection_multiple,selection_required};
static ISelectionItemProviderVtbl item_vtable={selection_item_query,selection_item_add,selection_item_release,item_select,item_add,item_remove,item_selected,item_owner};
static IRawElementProviderSimpleVtbl simple_vtable={simple_query,simple_add,simple_release,simple_options,simple_pattern,simple_property,simple_host};
static IRawElementProviderFragmentVtbl fragment_vtable={fragment_query,fragment_add,fragment_release,fragment_navigate,fragment_id,fragment_bounds,fragment_embedded,fragment_focus,fragment_root};
static IRawElementProviderFragmentRootVtbl root_vtable={root_query,root_add,root_release,root_point,root_focus};
static IInvokeProviderVtbl invoke_vtable={invoke_query,invoke_add,invoke_release,invoke_action};
static IToggleProviderVtbl toggle_vtable={toggle_query,toggle_add,toggle_release,toggle_action,toggle_state};
static IValueProviderVtbl value_vtable={value_query,value_add,value_release,value_set,value_get,value_readonly};
static HRESULT create_element(nx_uia_bridge *b,uint64_t id,uint64_t incarnation,nx_uia_element **out) {
    *out=NULL;
    nx_uia_element *e=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*e)); if (!e) return E_OUTOFMEMORY;
    e->simple.lpVtbl=&simple_vtable; e->fragment.lpVtbl=&fragment_vtable; e->root.lpVtbl=&root_vtable;
    e->invoke.lpVtbl=&invoke_vtable; e->toggle.lpVtbl=&toggle_vtable;
    e->selection.lpVtbl=&selection_vtable; e->selection_item.lpVtbl=&item_vtable;
    e->value.lpVtbl=&value_vtable;
    e->refs=1; e->bridge=b; e->id=id; e->incarnation=incarnation; bridge_add(b);
    HRESULT hr=CoCreateFreeThreadedMarshaler((IUnknown *)&e->simple,&e->marshaler);
    if (FAILED(hr)) { element_release(e); return hr; }
    *out=e; return S_OK;
}

void nx_uia_geometry(nx_uia_bridge *b) {
    if (!b) return;
    /* Owner-thread Win32 queries precede the lock; workers only read copies. */
    HWND hwnd=b->hwnd; if (!hwnd) return;
    RECT r; POINT origin={0,0};
    if (!GetClientRect(hwnd,&r) || !ClientToScreen(hwnd,&origin)) return;
    UINT dpi=GetDpiForWindow(hwnd); if (!dpi) dpi=96;
    int visible=IsWindowVisible(hwnd) && !IsIconic(hwnd),enabled=IsWindowEnabled(hwnd);
    int focused=enabled && GetFocus()==hwnd && GetForegroundWindow()==hwnd;
    AcquireSRWLockExclusive(&b->lock);
    if(b->enabled && !enabled && b->value_generation!=UINT64_MAX)++b->value_generation;
    b->origin=origin; b->scale=(double)dpi/96.;
    b->width=r.right-r.left; b->height=r.bottom-r.top; b->visible=visible; b->enabled=enabled; b->focused=focused;
    ReleaseSRWLockExclusive(&b->lock);
}
nx_result nx_uia_create(HWND hwnd,nx_allocator allocator,nx_uia_bridge **out) {
    *out=NULL; nx_uia_bridge *b=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*b));
    if (!b) return NX_ERROR_MEMORY;
    b->refs=1; InitializeSRWLock(&b->lock); b->hwnd=hwnd; b->allocator=allocator;
    b->value_generation=1;
    b->epoch=nx_native_next_identity();
    if (!b->epoch) { bridge_release(b); return NX_ERROR_LIMIT; }
    HRESULT hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) b->com_balance=1;
    else if (hr!=RPC_E_CHANGED_MODE) { bridge_release(b); return NX_ERROR_PLATFORM; }
    int title_length=GetWindowTextLengthW(hwnd);
    if (title_length<0 || title_length>1048576) { if (b->com_balance) CoUninitialize(); bridge_release(b); return NX_ERROR_LIMIT; }
    b->title=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,((size_t)title_length+1u)*sizeof(WCHAR));
    if (!b->title) { if (b->com_balance) CoUninitialize(); bridge_release(b); return NX_ERROR_MEMORY; }
    GetWindowTextW(hwnd,b->title,title_length+1);
    hr=create_element(b,0,0,&b->root);
    if (FAILED(hr)) { if (b->com_balance) CoUninitialize(); bridge_release(b); return NX_ERROR_MEMORY; }
    nx_uia_geometry(b); *out=b; return NX_OK;
}
static int units(nx_utf8_span text) {
    if (!text.length) return 0;
    if (text.length>INT_MAX) return -1;
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data,(int)text.length,NULL,0);
    return count ? count : -1;
}
static WCHAR *copy_text(WCHAR **cursor,nx_utf8_span text) {
    WCHAR *start=*cursor; int count=units(text);
    if (count>0) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data,(int)text.length,start,count);
    start[count>0 ? count : 0]=0; *cursor+=count>0 ? (size_t)count+1u : 1u; return start;
}
static void property_events_free(nx_uia_property_event *events,size_t count) {
    for(size_t i=0;i<count;++i) { VariantClear(&events[i].before);VariantClear(&events[i].after); }
    if(events)HeapFree(GetProcessHeap(),0,events);
}
static int selection_ancestry_same(const nx_uia_snapshot *a,uint32_t at,
    const nx_uia_snapshot *b,uint32_t bt) {
    while(at!=NX_UIA_NONE&&bt!=NX_UIA_NONE) {
        const nx_uia_node *x=&a->nodes[at],*y=&b->nodes[bt];
        if(x->id!=y->id||x->incarnation!=y->incarnation)return 0;
        at=x->parent;bt=y->parent;
    }
    return at==bt;
}
static int selection_sets_same(const nx_uia_snapshot *a,const nx_uia_node *x,
    const nx_uia_snapshot *b,const nx_uia_node *y) {
    if(!x||!y)return (!x||!x->selected_count)&&(!y||!y->selected_count);
    if(x->selected_count!=y->selected_count)return 0;
    uint32_t owner=(uint32_t)(y-b->nodes),at=x->selected;
    while(at!=NX_UIA_NONE) {
        const nx_uia_node *u=&a->nodes[at];uint32_t bt=find(b,u->id);
        if(bt==NX_UIA_NONE)return 0;
        const nx_uia_node *v=&b->nodes[bt];
        if(u->incarnation!=v->incarnation||!selected(v)||v->selection_owner!=owner)return 0;
        at=u->selection_next;
    }
    return 1;
}
/* Own event arguments before any UIA call-out. Do not emit old/new values across
   a protected transition, even though the old snapshot once held clear text. */
static nx_result property_events_prepare(nx_uia_bridge *b,nx_uia_snapshot *s,
    nx_uia_property_event **out,size_t *out_count) {
    *out=NULL;*out_count=0;size_t count=0;
    for(size_t i=0;i<s->count;++i) {
        nx_uia_node *n=&s->nodes[i];uint32_t oi=find(b->snapshot,n->id);
        const nx_uia_node *p=oi!=NX_UIA_NONE && b->snapshot->nodes[oi].incarnation==n->incarnation ? &b->snapshot->nodes[oi] : NULL;
        if(!p)continue;
        if(can_value(p)&&can_value(n)&&wcscmp(p->value,n->value))n->property_events|=1u;
        if(can_value(p)&&can_value(n)&&!!editable_value(p)!=!!editable_value(n))n->property_events|=2u;
        if(!!(b->published_enabled&&!(p->state&NX_SEM_DISABLED))!=!!(b->enabled&&!(n->state&NX_SEM_DISABLED)))n->property_events|=4u;
        if(!!can_value(p)!=!!can_value(n))n->property_events|=8u;
        if(!!(p->state&NX_SEM_PROTECTED)!=!!(n->state&NX_SEM_PROTECTED))n->property_events|=16u;
        if((p->state&NX_UIA_SELECTION_FLAGS)&&(n->state&NX_UIA_SELECTION_FLAGS)) {
            if(!!(p->state&NX_SEM_SELECTION_REQUIRED)!=!!(n->state&NX_SEM_SELECTION_REQUIRED))n->property_events|=32u;
            if(!!(p->state&NX_SEM_SELECTION_MULTIPLE)!=!!(n->state&NX_SEM_SELECTION_MULTIPLE))n->property_events|=64u;
        }
        for(unsigned flag=1;flag<=64;flag<<=1)if(n->property_events&flag)++count;
    }
    if(!count)return NX_OK;
    nx_uia_property_event *events=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,count*sizeof(*events));
    if(!events)return NX_ERROR_MEMORY;
    size_t at=0;
    for(size_t i=0;i<s->count;++i) {
        nx_uia_node *n=&s->nodes[i];if(!n->property_events)continue;
        const nx_uia_node *p=&b->snapshot->nodes[find(b->snapshot,n->id)];
        for(unsigned flag=1;flag<=64;flag<<=1)if(n->property_events&flag) {
            nx_uia_property_event *e=&events[at++];e->id=n->id;e->incarnation=n->incarnation;
            if(flag==1u) {
                e->property=NX_UIA_ValueValuePropertyId;
                V_VT(&e->before)=V_VT(&e->after)=VT_BSTR;
                V_BSTR(&e->before)=SysAllocString(p->value);V_BSTR(&e->after)=SysAllocString(n->value);
                if(!V_BSTR(&e->before)||!V_BSTR(&e->after)) {property_events_free(events,at);return NX_ERROR_MEMORY;}
            } else {
                V_VT(&e->before)=V_VT(&e->after)=VT_BOOL;int before=0,after=0;
                if(flag==2u) {e->property=NX_UIA_ValueIsReadOnlyPropertyId;before=!editable_value(p);after=!editable_value(n);}
                if(flag==4u) {e->property=NX_UIA_IsEnabledPropertyId;before=b->published_enabled&&!(p->state&NX_SEM_DISABLED);after=b->enabled&&!(n->state&NX_SEM_DISABLED);}
                if(flag==8u) {e->property=NX_UIA_IsValuePatternAvailablePropertyId;before=can_value(p);after=can_value(n);}
                if(flag==16u) {e->property=NX_UIA_IsPasswordPropertyId;before=!!(p->state&NX_SEM_PROTECTED);after=!!(n->state&NX_SEM_PROTECTED);}
                if(flag==32u) {e->property=NX_UIA_SelectionIsSelectionRequiredPropertyId;before=!!(p->state&NX_SEM_SELECTION_REQUIRED);after=!!(n->state&NX_SEM_SELECTION_REQUIRED);}
                if(flag==64u) {e->property=NX_UIA_SelectionCanSelectMultiplePropertyId;before=!!(p->state&NX_SEM_SELECTION_MULTIPLE);after=!!(n->state&NX_SEM_SELECTION_MULTIPLE);}
                V_BOOL(&e->before)=before?VARIANT_TRUE:VARIANT_FALSE;V_BOOL(&e->after)=after?VARIANT_TRUE:VARIANT_FALSE;
            }
        }
    }
    *out=events;*out_count=at;return NX_OK;
}
nx_result nx_uia_publish(nx_uia_bridge *b,const nx_context *context) {
    if (b->publishing) return NX_ERROR_STATE;
    size_t count; nx_result result=nx_semantics_count(context,&count); if (result!=NX_OK) return result;
    if (count>NX_UIA_NODES) return NX_ERROR_LIMIT;
    size_t slots=16; while (slots<count*2u) slots*=2u;
    size_t bytes=offsetof(nx_uia_snapshot,nodes)+count*sizeof(nx_uia_node)+slots*sizeof(uint32_t);
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v; if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        int a=units(v.name),c=units(v.description),d=(v.state&NX_SEM_PROTECTED) ? 0 : units(v.value);
        if (a<0 || c<0 || d<0) return NX_ERROR_ARGUMENT;
        if ((size_t)a>NX_UIA_BYTES/sizeof(WCHAR) || (size_t)c>NX_UIA_BYTES/sizeof(WCHAR) ||
            (size_t)d>NX_UIA_BYTES/sizeof(WCHAR)) return NX_ERROR_LIMIT;
        size_t text=((size_t)a+(size_t)c+(size_t)d+3u)*sizeof(WCHAR);
        if (text>NX_UIA_BYTES || bytes>NX_UIA_BYTES-text) return NX_ERROR_LIMIT;
        bytes+=text;
    }
    nx_uia_snapshot *s=b->spare;
    if (!s || s->capacity<bytes) {
        size_t capacity=4096; while (capacity<bytes) capacity*=2u;
        s=b->allocator.allocate(b->allocator.user,capacity); if (!s) return NX_ERROR_MEMORY;
        s->capacity=capacity;
        if (b->spare) b->allocator.deallocate(b->allocator.user,b->spare,b->spare->capacity);
        b->spare=s;
    }
    s->count=count; s->slot_count=slots; s->first=s->last=NX_UIA_NONE;
    s->slots=(uint32_t *)(s->nodes+count); memset(s->slots,0,slots*sizeof(uint32_t));
    WCHAR *text=(WCHAR *)(s->slots+slots); uint64_t incarnation=b->incarnation;
    for (size_t i=0;i<count;++i) {
        nx_semantic_view v; if (nx_semantics_get(context,(uint32_t)i,&v)!=NX_OK) return NX_ERROR_STATE;
        nx_uia_node *n=&s->nodes[i]; memset(n,0,sizeof(*n));
        n->id=v.id; n->role=v.role; n->state=v.state; n->actions=v.actions; n->bounds=v.bounds;
        n->text_value=nx_context_text_value_target(context,v.id);
        n->selection_target=nx_context_selection_target(context,v.id);
        n->selection_membership=nx_context_selection_membership(context,v.id);
        n->first=n->last=n->next=n->previous=NX_UIA_NONE;
        n->selected=n->focus_target=n->selection_owner=NX_UIA_NONE;
        n->selected_last=n->selection_next=NX_UIA_NONE;
        n->parent=find(s,v.parent_id);
        if (n->parent!=NX_UIA_NONE) {
            nx_uia_node *parent=&s->nodes[n->parent];
            n->selection_owner=(parent->state&NX_UIA_SELECTION_FLAGS) ? n->parent : parent->selection_owner;
            n->state |= parent->state&(NX_SEM_DISABLED|NX_SEM_OFFSCREEN);
        }
        if (can_select(s,n)) {
            nx_uia_node *owner=&s->nodes[n->selection_owner];
            if (selected(n)) {
                if(owner->selected_last!=NX_UIA_NONE)s->nodes[owner->selected_last].selection_next=(uint32_t)i;
                else owner->selected=(uint32_t)i;
                owner->selected_last=(uint32_t)i;++owner->selected_count;
            }
            if ((n->actions&NX_ACTION_FOCUS) && !(n->state&NX_SEM_DISABLED) &&
                (owner->focus_target==NX_UIA_NONE || selected(n))) owner->focus_target=(uint32_t)i;
        }
        uint32_t old=b->context_identity==nx_context_identity(context) ? find(b->snapshot,v.id) : NX_UIA_NONE;
        if (old!=NX_UIA_NONE && b->snapshot->nodes[old].role==v.role &&
            b->snapshot->nodes[old].text_value.identity==n->text_value.identity &&
            b->snapshot->nodes[old].selection_target.identity==n->selection_target.identity &&
            b->snapshot->nodes[old].selection_membership==n->selection_membership &&
            (!n->selection_target.identity||selection_ancestry_same(b->snapshot,b->snapshot->nodes[old].parent,s,n->parent)))
                n->incarnation=b->snapshot->nodes[old].incarnation;
        else { if (incarnation==UINT64_MAX) return NX_ERROR_LIMIT; n->incarnation=++incarnation; }
        n->name=copy_text(&text,v.name); n->help=copy_text(&text,v.description);
        n->value=copy_text(&text,(v.state&NX_SEM_PROTECTED) ? (nx_utf8_span){0} : v.value);
        uint32_t *last=n->parent==NX_UIA_NONE ? &s->last : &s->nodes[n->parent].last;
        uint32_t *first=n->parent==NX_UIA_NONE ? &s->first : &s->nodes[n->parent].first;
        n->previous=*last; if (*last!=NX_UIA_NONE) s->nodes[*last].next=(uint32_t)i; else *first=(uint32_t)i;
        *last=(uint32_t)i;
        size_t slot=(size_t)((v.id^(v.id>>33))*UINT64_C(0xff51afd7ed558ccd))&(slots-1u);
        while (s->slots[slot]) slot=(slot+1u)&(slots-1u); s->slots[slot]=(uint32_t)i+1u;
    }
    /* Copy event identities before any UIA call-out: reentrant native teardown
       may release both snapshots while external clients are processing an event. */
    size_t event_count=0;
    for (size_t i=0;i<s->count;++i) {
        nx_uia_node *n=&s->nodes[i]; uint32_t old_index=find(b->snapshot,n->id);
        nx_uia_node *prior=old_index!=NX_UIA_NONE && b->snapshot->nodes[old_index].incarnation==n->incarnation ? &b->snapshot->nodes[old_index] : NULL;
        int before=prior && can_select(b->snapshot,prior) && selected(prior);
        int after=can_select(s,n) && selected(n);
        if (before!=after) n->selection_events|=after ? 1u : 2u;
        if(after && (n->selection_events&1u) && s->nodes[n->selection_owner].selected_count>1u)
            n->selection_events|=16u;
        if (n->state&NX_UIA_SELECTION_FLAGS) {
            if(!selection_sets_same(b->snapshot,prior,s,n)) {
                n->selection_events|=4u;
                if(n->selected_count==1u)s->nodes[n->selected].selection_events|=8u;
            }
        }
    }
    for(size_t i=0;i<s->count;++i)if(s->nodes[i].selection_events)++event_count;
    nx_uia_selection_event *events=event_count ? HeapAlloc(GetProcessHeap(),0,event_count*sizeof(*events)) : NULL;
    if (events) {
        size_t at=0;
        for (size_t i=0;i<s->count;++i) if (s->nodes[i].selection_events)
            events[at++]=(nx_uia_selection_event){s->nodes[i].id,s->nodes[i].incarnation,s->nodes[i].selection_events};
    } else if(event_count)return NX_ERROR_MEMORY;
    nx_uia_geometry(b);
    nx_uia_property_event *property_events=NULL;size_t property_count=0;
    result=property_events_prepare(b,s,&property_events,&property_count);
    if(result!=NX_OK) {if(events)HeapFree(GetProcessHeap(),0,events);return result;}
    AcquireSRWLockExclusive(&b->lock); nx_uia_snapshot *old=b->snapshot;
    b->snapshot=s; b->spare=old; b->incarnation=incarnation; b->publishing=1;
    uint64_t binding=nx_context_identity(context);
    if (b->context_identity!=binding) {
        requests_clear(b);
        if(b->value_generation!=UINT64_MAX)++b->value_generation;
    }
    b->context_identity=binding;
    b->published_enabled=b->enabled;
    uint64_t frame_serial=nx_context_frame_serial(context); b->source_frame=frame_serial;
    for (size_t i=0;i<b->request_count;) {
        if ((b->requests[i].forwarded && b->requests[i].source_frame!=frame_serial) ||
            (b->requests[i].action==NX_ACTION_SET_VALUE && b->requests[i].value_generation!=b->value_generation) ||
            (b->requests[i].selection_target.identity && !multi_request_live(b,&b->requests[i]))) {
            request_remove(b,i);
        } else ++i;
    }
    ReleaseSRWLockExclusive(&b->lock);
    /* Property arguments above survive reentrant native teardown, just as the
       copied selection identities do. Rich text/live regions remain separate. */
    int changed=!old || old->count!=s->count;
    uint64_t focus=0;
    for (size_t i=0;i<s->count;++i) {
        if (!changed && (s->nodes[i].id!=old->nodes[i].id || s->nodes[i].parent!=old->nodes[i].parent || s->nodes[i].incarnation!=old->nodes[i].incarnation)) changed=1;
        if (s->nodes[i].state&NX_SEM_FOCUSED) focus=s->nodes[i].id;
    }
    nx_uia_element *event_root=b->root; element_add(event_root); bridge_add(b);
    uint32_t focus_index=find(s,focus);
    uint64_t focus_inc=focus_index==NX_UIA_NONE ? 0 : s->nodes[focus_index].incarnation;
    int focused=b->focused;
    int focus_changed=focus!=b->published_focus_id || focus_inc!=b->published_focus_inc ||
        (focused && !b->published_native_focus);
    b->published_focus_id=focus; b->published_focus_inc=focus_inc; b->published_native_focus=focused;
    if (UiaClientsAreListening()) {
        if (changed) UiaRaiseStructureChangedEvent(&event_root->simple,StructureChangeType_ChildrenInvalidated,NULL,0);
        for (size_t i=0;i<event_count;++i) {
            nx_uia_element *target=NULL;
            if (FAILED(create_element(b,events[i].id,events[i].incarnation,&target))) continue;
            uint32_t flags=events[i].flags;
            if (flags&4u) UiaRaiseAutomationEvent(&target->simple,NX_UIA_Selection_InvalidatedEventId);
            if (flags&3u) {
                VARIANT before,after; VariantInit(&before); VariantInit(&after);
                V_VT(&before)=V_VT(&after)=VT_BOOL;
                V_BOOL(&after)=(flags&1u) ? VARIANT_TRUE : VARIANT_FALSE;
                V_BOOL(&before)=(flags&1u) ? VARIANT_FALSE : VARIANT_TRUE;
                UiaRaiseAutomationPropertyChangedEvent(&target->simple,NX_UIA_SelectionItemIsSelectedPropertyId,before,after);
            }
            if(flags&(1u|2u|8u))UiaRaiseAutomationEvent(&target->simple,(flags&2u) ?
                NX_UIA_SelectionItem_ElementRemovedFromSelectionEventId : (flags&16u) ?
                NX_UIA_SelectionItem_ElementAddedToSelectionEventId : NX_UIA_SelectionItem_ElementSelectedEventId);
            element_release(target);
        }
        for(size_t i=0;i<property_count;++i) {
            nx_uia_property_event *p=&property_events[i];nx_uia_element *target=NULL;
            if(FAILED(create_element(b,p->id,p->incarnation,&target)))continue;
            UiaRaiseAutomationPropertyChangedEvent(&target->simple,p->property,p->before,p->after);
            element_release(target);
        }
        if (focus && focus_changed && focused) {
            nx_uia_element *e=NULL;
            if (SUCCEEDED(create_element(b,focus,focus_inc,&e))) { UiaRaiseAutomationEvent(&e->simple,NX_UIA_AutomationFocusChangedEventId); element_release(e); }
        }
    }
    if (events) HeapFree(GetProcessHeap(),0,events);
    property_events_free(property_events,property_count);
    b->publishing=0; element_release(event_root); bridge_release(b); return NX_OK;
}
int nx_uia_matches_context(nx_uia_bridge *b,const nx_context *context) {
    return !b || b->context_identity==nx_context_identity(context);
}
/* Caller holds b->lock. No borrowed snapshot pointer escapes a call-out. */
static int value_request_current(nx_uia_bridge *b,const nx_uia_request *r,nx_context *context) {
    uint32_t index=find(b->snapshot,r->id);
    if(!b->hwnd||!b->enabled||index==NX_UIA_NONE||
       b->context_identity!=nx_context_identity(context)||b->source_frame!=nx_context_frame_serial(context))return 0;
    const nx_uia_node *n=&b->snapshot->nodes[index];
    return n->incarnation==r->incarnation && editable_value(n) && !(n->state&NX_SEM_DISABLED) &&
        b->value_generation!=UINT64_MAX && r->value_generation==b->value_generation &&
        r->text_value.identity==n->text_value.identity && r->text_value.revision==n->text_value.revision &&
        r->text_value.multiline==n->text_value.multiline;
}
static nx_result route_requests(nx_uia_bridge *b,nx_context *context,size_t *count) {
    *count=0; if (!b) return NX_OK;
    if (b->context_identity!=nx_context_identity(context)) return NX_ERROR_STATE;
    /* The live context can advance before its next publication. Never resolve
       an old displayed role/group ID against a different, unpublished tree. */
    if (b->source_frame!=nx_context_frame_serial(context)) return NX_OK;
    /* Keep the head until the core accepts it. Worker requests may append while
       routing; a full core queue or an active frame must not lose accepted work. */
    size_t remaining;
    AcquireSRWLockShared(&b->lock); remaining=b->request_count; ReleaseSRWLockShared(&b->lock);
    for (size_t i=0;i<remaining;++i) {
        AcquireSRWLockExclusive(&b->lock);
        if(!b->hwnd||b->context_identity!=nx_context_identity(context)||b->source_frame!=nx_context_frame_serial(context)) {
            ReleaseSRWLockExclusive(&b->lock);break;
        }
        size_t at=0; while (at<b->request_count && (b->requests[at].forwarded||b->requests[at].routing)) ++at;
        if (at==b->request_count) { ReleaseSRWLockExclusive(&b->lock); break; }
        nx_uia_request r=b->requests[at]; uint32_t index=find(b->snapshot,r.id);
        int valid=b->enabled && (!r.id || (index!=NX_UIA_NONE && b->snapshot->nodes[index].incarnation==r.incarnation));
        if(valid && r.action==NX_ACTION_SET_VALUE) {
            valid=value_request_current(b,&r,context);
        }
        if(valid&&r.selection_target.identity)valid=multi_request_live(b,&r);
        if (valid && r.owner_id) {
            nx_uia_node *n=&b->snapshot->nodes[index];
            valid=can_select(b->snapshot,n) && b->snapshot->nodes[n->selection_owner].id==r.owner_id &&
                b->snapshot->nodes[n->selection_owner].incarnation==r.owner_incarnation;
        }
        nx_result result=NX_OK;
        if(valid && r.action==NX_ACTION_SET_VALUE) {
            /* The context allocator is user code: WM_ENABLE, detach or nested
               routing may run synchronously. Move payload ownership out of the
               queue, pin the bridge in nx_uia_route and identify the surviving
               stub by a never-reused key, not an index shifted by callbacks. */
            b->requests[at].text=NULL;b->requests[at].routing=1;
            ReleaseSRWLockExclusive(&b->lock);
            nx_text_value_guard guard={b,r.value_generation,value_guard_retain,value_guard_release,value_guard_current};
            result=nx_semantics_request_text_value_guarded(context,r.id,(nx_utf8_span){r.text,r.text_length},&guard);
            size_t semantic_count=0;
            int retry=result==NX_ERROR_LIMIT||result==NX_ERROR_MEMORY||
                (result==NX_ERROR_STATE&&nx_semantics_count(context,&semantic_count)!=NX_OK);
            AcquireSRWLockExclusive(&b->lock);
            at=0;while(at<b->request_count&&b->requests[at].request_key!=r.request_key)++at;
            int live=at<b->request_count&&value_request_current(b,&r,context);
            if(live&&retry) {
                b->requests[at].text=r.text;b->requests[at].routing=0;r.text=NULL;
            } else if(live&&result==NX_OK) {
                b->requests[at].routing=0;b->requests[at].forwarded=1;
                b->requests[at].source_frame=nx_context_frame_serial(context);
                b->requests[at].text_length=0;++*count;
            } else if(at<b->request_count)request_remove(b,at);
            ReleaseSRWLockExclusive(&b->lock);
            if(r.text)HeapFree(GetProcessHeap(),0,r.text);
            if(live&&retry)return result==NX_ERROR_LIMIT?NX_OK:result;
            continue;
        }
        if(valid && r.id) {
            if(r.selection_target.identity) {
                /* This admission is allocation-free and only retains the bridge
                   using Interlocked. The actual lease check runs at end_frame,
                   outside this lock and after preceding application call-outs. */
                nx_selection_guard guard={b,r.value_generation,value_guard_retain,value_guard_release,selection_guard_current};
                result=nx_semantics_request_selection_guarded(context,r.id,r.action,&guard);
            } else result=nx_semantics_request_action(context,r.id,r.action);
        }
        if (result==NX_ERROR_LIMIT) { ReleaseSRWLockExclusive(&b->lock); return NX_OK; } /* Retry after the next frame consumes core work. */
        if (result==NX_ERROR_MEMORY) { ReleaseSRWLockExclusive(&b->lock); return result; }
        /* Active-frame errors also retain work; stale/disabled targets are dropped
           only once the completed tree can authoritatively reject them. */
        if (result==NX_ERROR_STATE) {
            size_t semantic_count;
            if (nx_semantics_count(context,&semantic_count)!=NX_OK) { ReleaseSRWLockExclusive(&b->lock); return result; }
        }
        HWND focus_window=valid && result==NX_OK && (!r.id || r.action==NX_ACTION_FOCUS) ? b->hwnd : NULL;
        if (valid && result==NX_OK) ++*count;
        if (valid && result==NX_OK && (r.owner_id || r.action==NX_ACTION_SET_VALUE)) {
            b->requests[at].forwarded=1; b->requests[at].source_frame=nx_context_frame_serial(context);
            if(b->requests[at].text) {
                HeapFree(GetProcessHeap(),0,b->requests[at].text);
                b->requests[at].text=NULL;b->requests[at].text_length=0;
            }
        } else {
            request_remove(b,at);
        }
        ReleaseSRWLockExclusive(&b->lock);
        if (focus_window) SetFocus(focus_window); /* Native call-out after dequeue/unlock. */
    }
    return NX_OK;
}
nx_result nx_uia_route(nx_uia_bridge *b,nx_context *context,size_t *count) {
    *count=0;if(!b)return NX_OK;
    /* Detach can release every native/provider reference during a user
       allocator callback, including when core admission fails. */
    bridge_add(b);nx_result result=route_requests(b,context,count);bridge_release(b);return result;
}
uint64_t nx_native_next_identity(void) {
    static __declspec(align(8)) volatile LONG64 sequence;
    LONG64 prior=InterlockedCompareExchange64(&sequence,0,0);
    for (;;) {
        if (prior==INT64_MAX) return 0;
        LONG64 seen=InterlockedCompareExchange64(&sequence,prior+1,prior);
        if (seen==prior) return (uint64_t)(prior+1);
        prior=seen;
    }
}
int nx_uia_object_id(LPARAM lparam) { return (LONG)lparam==UiaRootObjectId; }
LRESULT nx_uia_get_object(nx_uia_bridge *b,WPARAM wparam,LPARAM lparam) {
    /* A new bridge is attached before its transactional first publication so
       event callbacks can find it. Never hand out an unfinished empty provider. */
    if (!b->snapshot) return 0;
    nx_uia_element *root=b->root; element_add(root);
    LRESULT result=UiaReturnRawElementProvider(b->hwnd,wparam,lparam,&root->simple);
    element_release(root); return result;
}
void nx_uia_destroy(nx_uia_bridge *b) {
    if (!b) return;
    AcquireSRWLockExclusive(&b->lock); HWND hwnd=b->hwnd; b->hwnd=NULL; requests_clear(b);
    nx_uia_snapshot *snapshot=b->snapshot,*spare=b->spare; b->snapshot=b->spare=NULL;
    nx_uia_element *root=b->root; b->root=NULL;
    ReleaseSRWLockExclusive(&b->lock);
    /* Do not call the RPC-disconnecting API from a synchronous SendMessage
       callback. State is already detached, so every retained provider rejects
       queries even when native teardown cannot disconnect its cache here. */
    if (root && !(InSendMessageEx(NULL)&ISMEX_SEND)) (void)UiaDisconnectProvider(&root->simple);
    if (hwnd) UiaReturnRawElementProvider(hwnd,0,0,NULL);
    if (snapshot) b->allocator.deallocate(b->allocator.user,snapshot,snapshot->capacity);
    if (spare) b->allocator.deallocate(b->allocator.user,spare,spare->capacity);
    if (root) element_release(root);
    if (b->com_balance) CoUninitialize();
    bridge_release(b);
}
