/* Independent native UIA client: no NexaUI headers or library links. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "uia_protocol.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define OK(x) do { HRESULT checked_hr=(x); if (FAILED(checked_hr)) { fprintf(stderr,"%s:%d: %s returned %08lx\n",__FILE__,__LINE__,#x,(unsigned long)checked_hr); return 1; } } while (0)
static HANDLE commands,acks,read_complete;
static HRESULT set_value(IUIAutomationValuePattern *pattern,const WCHAR *text) {
    BSTR value=SysAllocString(text);
    if(!value){fputs("UIA client BSTR allocation failed\n",stderr);exit(EXIT_FAILURE);}
    HRESULT hr=IUIAutomationValuePattern_SetValue(pattern,value);SysFreeString(value);return hr;
}
static int host(unsigned command,unsigned *reply) {
    DWORD bytes=0;
    if (!WriteFile(commands,&command,sizeof(command),&bytes,NULL) || bytes!=sizeof(command)) return 1;
    if (!ReadFile(acks,reply,sizeof(*reply),&bytes,NULL) || bytes!=sizeof(*reply)) return 1;
    fprintf(stderr,"Host command %u reply %u\n",command,*reply);
    return 0;
}
static IUIAutomationElement *find(IUIAutomation *uia,IUIAutomationElement *root,unsigned id) {
    WCHAR text[48]; swprintf_s(text,48,L"nexa-%016llx",(unsigned long long)id);
    VARIANT value; VariantInit(&value); V_VT(&value)=VT_BSTR; V_BSTR(&value)=SysAllocString(text);
    IUIAutomationCondition *condition=NULL; IUIAutomationElement *element=NULL;
    HRESULT hr=IUIAutomation_CreatePropertyCondition(uia,UIA_AutomationIdPropertyId,value,&condition);
    if (SUCCEEDED(hr)) hr=IUIAutomationElement_FindFirst(root,TreeScope_Descendants,condition,&element);
    if (condition) IUIAutomationCondition_Release(condition); VariantClear(&value);
    return SUCCEEDED(hr) ? element : NULL;
}
static int name_is(IUIAutomationElement *e,const WCHAR *expected) {
    BSTR name=NULL; HRESULT hr=IUIAutomationElement_get_CurrentName(e,&name);
    int result=SUCCEEDED(hr) && name && wcscmp(name,expected)==0;
    if (!result) fprintf(stderr,"Name hr=%08lx, units=%u\n",(unsigned long)hr,name ? SysStringLen(name) : 0);
    SysFreeString(name); return result;
}
static int unavailable(IUIAutomationElement *e) {
    BSTR name=NULL; HRESULT hr=IUIAutomationElement_get_CurrentName(e,&name); SysFreeString(name);
    if (hr!=(HRESULT)UIA_E_ELEMENTNOTAVAILABLE) fprintf(stderr,"Expected stale handle; got %08lx\n",(unsigned long)hr);
    return hr==(HRESULT)UIA_E_ELEMENTNOTAVAILABLE;
}
static int closed(IUIAutomationElement *e) {
    BSTR name=NULL; HRESULT hr=IUIAutomationElement_get_CurrentName(e,&name); SysFreeString(name);
    return hr==(HRESULT)UIA_E_ELEMENTNOTAVAILABLE || hr==RPC_E_DISCONNECTED || hr==RPC_E_SERVER_DIED_DNE;
}
static int invoke(IUIAutomationElement *e,int success) {
    IUIAutomationInvokePattern *pattern=NULL;
    HRESULT hr=IUIAutomationElement_GetCurrentPatternAs(e,UIA_InvokePatternId,&IID_IUIAutomationInvokePattern,(void **)&pattern);
    if (SUCCEEDED(hr)) { hr=IUIAutomationInvokePattern_Invoke(pattern); IUIAutomationInvokePattern_Release(pattern); }
    return success ? SUCCEEDED(hr) : FAILED(hr);
}
static int runtime_equal(SAFEARRAY *a,SAFEARRAY *b) {
    LONG lo1,hi1,lo2,hi2; if (!a || !b) return 0;
    if (FAILED(SafeArrayGetLBound(a,1,&lo1)) || FAILED(SafeArrayGetUBound(a,1,&hi1)) ||
        FAILED(SafeArrayGetLBound(b,1,&lo2)) || FAILED(SafeArrayGetUBound(b,1,&hi2)) || lo1!=lo2 || hi1!=hi2) return 0;
    for (LONG i=lo1;i<=hi1;++i) { LONG x,y; if (FAILED(SafeArrayGetElement(a,&i,&x)) || FAILED(SafeArrayGetElement(b,&i,&y)) || x!=y) return 0; }
    return 1;
}
/* Independent native properties/actions; no NexaUI linkage or simulated
   screenreader claim. The test host does not present this geometry fixture. */
static int design_value(IUIAutomationElement *plot,WCHAR value[64]) {
    VARIANT v;VariantInit(&v);
    HRESULT hr=IUIAutomationElement_GetCurrentPropertyValue(plot,UIA_ValueValuePropertyId,&v);
    int ok=SUCCEEDED(hr)&&V_VT(&v)==VT_BSTR&&V_BSTR(&v)&&SysStringLen(V_BSTR(&v))<64;
    if(ok) wcscpy_s(value,64,V_BSTR(&v));VariantClear(&v);return ok;
}
static int design_arrow(IUIAutomationElement *plot,HWND hwnd) {
    unsigned reply;WCHAR before[64]={0},after[64]={0};
    OK(IUIAutomationElement_SetFocus(plot));CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply)&&(reply&8u));
    BOOL native_focus=FALSE;OK(IUIAutomationElement_get_CurrentHasKeyboardFocus(plot,&native_focus));
    CHECK(!!native_focus==!!(reply&16u));
    CHECK(design_value(plot,before));
    CHECK(PostMessageW(hwnd,WM_KEYDOWN,VK_RIGHT,1));CHECK(PostMessageW(hwnd,WM_KEYUP,VK_RIGHT,((LPARAM)3<<30)|1));
    for(unsigned i=0;i<8;++i) {
        CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply)&&design_value(plot,after));
        fwprintf(stderr,L"Design cursor before='%ls' after='%ls'\n",before,after);
        if(wcscmp(before,after)&&wcsncmp(after,L"x=",2)==0) return 0;
    }
    CHECK(0);return 1;
}
static int design_probe(IUIAutomation *uia,IUIAutomationElement *root,HWND hwnd) {
    unsigned reply;CHECK(!host(UIA_TEST_DESIGN_START,&reply)&&reply>>8u==4);
    IUIAutomationElement *status=find(uia,root,1900),*hc=find(uia,root,1901),
        *effects=find(uia,root,1902),*plot=find(uia,root,1903);
    CHECK(status&&hc&&effects&&plot&&name_is(status,L"Local WLAN connected; sample data")&&name_is(plot,L"Transfer rates"));
    CONTROLTYPEID type=0;BOOL focusable=FALSE;BSTR help=NULL;
    OK(IUIAutomationElement_get_CurrentControlType(plot,&type));CHECK(type==UIA_CustomControlTypeId);
    OK(IUIAutomationElement_get_CurrentIsKeyboardFocusable(plot,&focusable));CHECK(focusable);
    OK(IUIAutomationElement_get_CurrentHelpText(plot,&help));
    CHECK(help&&wcsstr(help,L"Download; Upload; x [s]; y [MB/s]"));SysFreeString(help);
    CHECK(!design_arrow(plot,hwnd));
    IUIAutomationValuePattern *value=NULL;BOOL readonly=FALSE;BSTR current=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(plot,UIA_ValuePatternId,&IID_IUIAutomationValuePattern,(void **)&value));
    OK(IUIAutomationValuePattern_get_CurrentIsReadOnly(value,&readonly));CHECK(readonly);
    OK(IUIAutomationValuePattern_get_CurrentValue(value,&current));CHECK(current&&wcsncmp(current,L"x=",2)==0);SysFreeString(current);
    CHECK(FAILED(set_value(value,L"x=999")));
    IUIAutomationElement *guard=find(uia,root,1904);IUIAutomationValuePattern *guard_value=NULL;CHECK(guard);
    OK(IUIAutomationElement_GetCurrentPatternAs(guard,UIA_ValuePatternId,&IID_IUIAutomationValuePattern,(void **)&guard_value));
    OK(IUIAutomationValuePattern_get_CurrentValue(guard_value,&current));CHECK(current&&!wcscmp(current,L"fixture-value"));SysFreeString(current);
    for(unsigned guard_mode=0;guard_mode<2;++guard_mode) {
        CHECK(!host(guard_mode?UIA_TEST_DESIGN_EDITABLE:UIA_TEST_DESIGN_PROTECT,&reply));
        current=NULL;HRESULT hr=IUIAutomationValuePattern_get_CurrentValue(guard_value,&current);
        CHECK(FAILED(hr)||!current||!*current);SysFreeString(current);
        CHECK(FAILED(set_value(guard_value,L"overwrite")));
        IUIAutomationValuePattern *unsupported=NULL;
        hr=IUIAutomationElement_GetCurrentPatternAs(guard,UIA_ValuePatternId,&IID_IUIAutomationValuePattern,(void **)&unsupported);
        /* UIAutomationCore can retain the static COM pattern interface. Its
           live methods must still refuse the now-protected/editable value. */
        if(SUCCEEDED(hr)&&unsupported) {
            current=NULL;hr=IUIAutomationValuePattern_get_CurrentValue(unsupported,&current);
            CHECK(FAILED(hr)||!current||!*current);SysFreeString(current);
            CHECK(FAILED(set_value(unsupported,L"overwrite")));
            IUIAutomationValuePattern_Release(unsupported);
        } else CHECK(!unsupported);
    }
    CHECK(!host(UIA_TEST_DESIGN_UNGUARD,&reply));
    OK(IUIAutomationValuePattern_get_CurrentValue(guard_value,&current));CHECK(current&&!wcscmp(current,L"fixture-value"));SysFreeString(current);
    IUIAutomationValuePattern_Release(guard_value);IUIAutomationElement_Release(guard);
    IUIAutomationTogglePattern *toggle=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(effects,UIA_TogglePatternId,&IID_IUIAutomationTogglePattern,(void **)&toggle));
    OK(IUIAutomationTogglePattern_Toggle(toggle));CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply)&&(reply&1u)&&reply>>8u==0);
    enum ToggleState state=ToggleState_Off;OK(IUIAutomationTogglePattern_get_CurrentToggleState(toggle,&state));CHECK(state==ToggleState_On);
    CHECK(!design_arrow(plot,hwnd));
    OK(IUIAutomationTogglePattern_Toggle(toggle));CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply)&&!(reply&1u)&&reply>>8u==4);
    IUIAutomationTogglePattern_Release(toggle);
    /* Keyboard control of HC through this actual HWND, with completed frame
       acknowledgements rather than a timing-only sleep. */
    OK(IUIAutomationElement_SetFocus(hc));CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply));
    CHECK(PostMessageW(hwnd,WM_KEYDOWN,VK_SPACE,1));CHECK(PostMessageW(hwnd,WM_KEYUP,VK_SPACE,((LPARAM)3<<30)|1));
    for(unsigned i=0;i<8&&!(reply&4u);++i)CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply));
    CHECK((reply&6u)==6u&&!(reply&1u)&&reply>>8u==0);
    CHECK(!design_arrow(plot,hwnd));
    OK(IUIAutomationElement_GetCurrentPatternAs(hc,UIA_TogglePatternId,&IID_IUIAutomationTogglePattern,(void **)&toggle));
    OK(IUIAutomationTogglePattern_get_CurrentToggleState(toggle,&state));CHECK(state==ToggleState_On);
    OK(IUIAutomationTogglePattern_Toggle(toggle));
    for(unsigned i=0;i<8&&(reply&6u);++i)CHECK(!host(UIA_TEST_DESIGN_FRAME,&reply));
    CHECK(!(reply&6u)&&reply>>8u==4);IUIAutomationTogglePattern_Release(toggle);
    IUIAutomationElement_Release(status);IUIAutomationElement_Release(hc);
    IUIAutomationElement_Release(effects);
    CHECK(!host(UIA_TEST_DESIGN_END,&reply));
    current=NULL;HRESULT stale_value=IUIAutomationValuePattern_get_CurrentValue(value,&current);
    CHECK(FAILED(stale_value)||!current||!*current);SysFreeString(current);CHECK(unavailable(plot));
    CHECK(FAILED(set_value(value,L"x=999")));IUIAutomationValuePattern_Release(value);
    IUIAutomationElement_Release(plot);
    puts("Design A independent UIA properties, effect toggles and posted HWND keys passed");return 0;
}
typedef struct observer {
    IUIAutomationStructureChangedEventHandler structure;
    IUIAutomationFocusChangedEventHandler focus;
    IUIAutomationEventHandler automation;
    LONG refs, structures, focuses, selections, removals, invalidations;
    int process_id;
} observer;
static ULONG observer_add(observer *o) { return (ULONG)InterlockedIncrement(&o->refs); }
static ULONG observer_release(observer *o) {
    ULONG refs=(ULONG)InterlockedDecrement(&o->refs);
    if (!refs) HeapFree(GetProcessHeap(),0,o); return refs;
}
static HRESULT observer_query(observer *o,REFIID iid,void **out) {
    if (!out) return E_POINTER; *out=NULL;
    if (IsEqualIID(iid,&IID_IUnknown) || IsEqualIID(iid,&IID_IUIAutomationStructureChangedEventHandler)) *out=&o->structure;
    else if (IsEqualIID(iid,&IID_IUIAutomationFocusChangedEventHandler)) *out=&o->focus;
    else if (IsEqualIID(iid,&IID_IUIAutomationEventHandler)) *out=&o->automation;
    else return E_NOINTERFACE;
    observer_add(o); return S_OK;
}
#define OBSERVER_INTERFACE(prefix,type,member) \
static HRESULT STDMETHODCALLTYPE prefix##_query(type *self,REFIID iid,void **out) { return observer_query(CONTAINING_RECORD(self,observer,member),iid,out); } \
static ULONG STDMETHODCALLTYPE prefix##_add(type *self) { return observer_add(CONTAINING_RECORD(self,observer,member)); } \
static ULONG STDMETHODCALLTYPE prefix##_release(type *self) { return observer_release(CONTAINING_RECORD(self,observer,member)); }
OBSERVER_INTERFACE(structure,IUIAutomationStructureChangedEventHandler,structure)
OBSERVER_INTERFACE(focus,IUIAutomationFocusChangedEventHandler,focus)
OBSERVER_INTERFACE(automation,IUIAutomationEventHandler,automation)
static int our_sender(observer *o,IUIAutomationElement *sender) {
    if (!sender) return 0;
    int pid=0; BSTR framework=NULL;
    if (FAILED(IUIAutomationElement_get_CurrentProcessId(sender,&pid)) || pid!=o->process_id) return 0;
    HRESULT hr=IUIAutomationElement_get_CurrentFrameworkId(sender,&framework);
    int result=SUCCEEDED(hr) && framework && wcscmp(framework,L"NexaUI")==0;
    SysFreeString(framework); return result;
}
static HRESULT STDMETHODCALLTYPE structure_event(IUIAutomationStructureChangedEventHandler *self,IUIAutomationElement *sender,enum StructureChangeType kind,SAFEARRAY *runtime) {
    (void)runtime; observer *o=CONTAINING_RECORD(self,observer,structure);
    if (kind==StructureChangeType_ChildrenInvalidated && our_sender(o,sender)) InterlockedIncrement(&o->structures);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE focus_event(IUIAutomationFocusChangedEventHandler *self,IUIAutomationElement *sender) {
    observer *o=CONTAINING_RECORD(self,observer,focus);
    if (our_sender(o,sender)) InterlockedIncrement(&o->focuses);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE automation_event(IUIAutomationEventHandler *self,IUIAutomationElement *sender,EVENTID id) {
    observer *o=CONTAINING_RECORD(self,observer,automation);
    if (our_sender(o,sender)) {
        if (id==UIA_SelectionItem_ElementSelectedEventId) InterlockedIncrement(&o->selections);
        if (id==UIA_SelectionItem_ElementRemovedFromSelectionEventId) InterlockedIncrement(&o->removals);
        if (id==UIA_Selection_InvalidatedEventId) InterlockedIncrement(&o->invalidations);
    }
    return S_OK;
}
static IUIAutomationEventHandlerVtbl automation_vtable={automation_query,automation_add,automation_release,automation_event};
static IUIAutomationStructureChangedEventHandlerVtbl structure_vtable={structure_query,structure_add,structure_release,structure_event};
static IUIAutomationFocusChangedEventHandlerVtbl focus_vtable={focus_query,focus_add,focus_release,focus_event};
static int wait_event(LONG *counter,LONG previous) {
    ULONGLONG end=GetTickCount64()+3000;
    while (InterlockedCompareExchange(counter,0,0)<=previous && GetTickCount64()<end) Sleep(10);
    return InterlockedCompareExchange(counter,0,0)>previous;
}
static int selection_id(IUIAutomationSelectionPattern *pattern,unsigned expected) {
    IUIAutomationElementArray *array=NULL; OK(IUIAutomationSelectionPattern_GetCurrentSelection(pattern,&array));
    int count=0; OK(IUIAutomationElementArray_get_Length(array,&count)); CHECK(count==(expected ? 1 : 0));
    if (expected) {
        IUIAutomationElement *item=NULL; OK(IUIAutomationElementArray_GetElement(array,0,&item));
        WCHAR id[48]; swprintf_s(id,48,L"nexa-%016llx",(unsigned long long)expected);
        BSTR actual=NULL; OK(IUIAutomationElement_get_CurrentAutomationId(item,&actual)); CHECK(wcscmp(actual,id)==0);
        SysFreeString(actual); IUIAutomationElement_Release(item);
    }
    IUIAutomationElementArray_Release(array); return 0;
}
static int selection_probe(IUIAutomation *uia,IUIAutomationElement *root,observer *events,HWND hwnd) {
    IUIAutomationElement *required=find(uia,root,50),*optional=find(uia,root,60),*radio=find(uia,root,70);
    IUIAutomationElement *a=find(uia,root,51),*b=find(uia,root,52),*c=find(uia,root,61),*d=find(uia,root,62),*disabled=find(uia,root,63),*r=find(uia,root,72);
    CHECK(required && optional && radio && a && b && c && d && disabled && r);
    IUIAutomationSelectionPattern *req=NULL,*opt=NULL,*rad=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(required,UIA_SelectionPatternId,&IID_IUIAutomationSelectionPattern,(void **)&req));
    OK(IUIAutomationElement_GetCurrentPatternAs(optional,UIA_SelectionPatternId,&IID_IUIAutomationSelectionPattern,(void **)&opt));
    OK(IUIAutomationElement_GetCurrentPatternAs(radio,UIA_SelectionPatternId,&IID_IUIAutomationSelectionPattern,(void **)&rad));
    BOOL value=TRUE; OK(IUIAutomationSelectionPattern_get_CurrentCanSelectMultiple(req,&value)); CHECK(!value);
    OK(IUIAutomationSelectionPattern_get_CurrentIsSelectionRequired(req,&value)); CHECK(value);
    OK(IUIAutomationSelectionPattern_get_CurrentIsSelectionRequired(opt,&value)); CHECK(!value);
    CHECK(selection_id(req,51)==0 && selection_id(opt,0)==0 && selection_id(rad,71)==0);
    IUIAutomationSelectionItemPattern *pa=NULL,*pb=NULL,*pc=NULL,*pd=NULL,*px=NULL,*pr=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(a,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&pa));
    OK(IUIAutomationElement_GetCurrentPatternAs(b,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&pb));
    OK(IUIAutomationElement_GetCurrentPatternAs(c,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&pc));
    OK(IUIAutomationElement_GetCurrentPatternAs(d,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&pd));
    OK(IUIAutomationElement_GetCurrentPatternAs(disabled,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&px));
    OK(IUIAutomationElement_GetCurrentPatternAs(r,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&pr));
    IUIAutomationElement *owner=NULL;
    OK(IUIAutomationSelectionItemPattern_get_CurrentSelectionContainer(pc,&owner)); CHECK(owner);
    BSTR owner_id=NULL; OK(IUIAutomationElement_get_CurrentAutomationId(owner,&owner_id)); CHECK(wcscmp(owner_id,L"nexa-000000000000003c")==0);
    SysFreeString(owner_id); IUIAutomationElement_Release(owner);
    CHECK(IUIAutomationSelectionItemPattern_RemoveFromSelection(pa)==(HRESULT)UIA_E_INVALIDOPERATION);
    CHECK(IUIAutomationSelectionItemPattern_AddToSelection(pb)==(HRESULT)UIA_E_INVALIDOPERATION);
    CHECK(FAILED(IUIAutomationSelectionItemPattern_Select(px)));
    LONG selected_before=InterlockedCompareExchange(&events->selections,0,0);
    LONG invalidated_before=InterlockedCompareExchange(&events->invalidations,0,0);
    OK(IUIAutomationSelectionItemPattern_Select(pb));
    OK(IUIAutomationSelectionItemPattern_Select(pb)); /* Same pending operation is idempotent. */
    CHECK(FAILED(IUIAutomationSelectionItemPattern_Select(pa)));
    CHECK(selection_id(req,51)==0); /* Acceptance does not publish a fake selection. */
    unsigned reply=0; CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0 && (reply&255u)==52);
    ULONGLONG selection_event_start=GetTickCount64();
    int selected_event=wait_event(&events->selections,selected_before);
    int invalidated_event=selected_event && wait_event(&events->invalidations,invalidated_before);
    if (!selected_event || !invalidated_event)
        fprintf(stderr,"Selection events after %llu ms: selected %ld -> %ld, invalidated %ld -> %ld; waits selected=%d invalidated=%d\n",
            GetTickCount64()-selection_event_start,selected_before,InterlockedCompareExchange(&events->selections,0,0),
            invalidated_before,InterlockedCompareExchange(&events->invalidations,0,0),selected_event,invalidated_event);
    CHECK(selected_event && invalidated_event);
    CHECK(selection_id(req,52)==0); OK(IUIAutomationSelectionItemPattern_get_CurrentIsSelected(pa,&value)); CHECK(!value);
    OK(IUIAutomationSelectionItemPattern_AddToSelection(pc));
    CHECK(host(UIA_TEST_ROUTE_SELECTION,&reply)==0); /* Forward, republish the same completed frame. */
    CHECK(FAILED(IUIAutomationSelectionItemPattern_Select(pd))); /* Still busy until a new frame publishes. */
    CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0 && ((reply>>8u)&255u)==61);
    CHECK(selection_id(opt,61)==0);
    OK(IUIAutomationSelectionItemPattern_AddToSelection(pc));
    CHECK(IUIAutomationSelectionItemPattern_AddToSelection(pd)==(HRESULT)UIA_E_INVALIDOPERATION);
    OK(IUIAutomationSelectionItemPattern_RemoveFromSelection(pd)); /* Unselected removal is a no-op. */
    LONG removed_before=InterlockedCompareExchange(&events->removals,0,0);
    OK(IUIAutomationSelectionItemPattern_RemoveFromSelection(pc));
    CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0 && ((reply>>8u)&255u)==0);
    CHECK(selection_id(opt,0)==0 && wait_event(&events->removals,removed_before));
    OK(IUIAutomationSelectionItemPattern_Select(pc));
    CHECK(host(UIA_TEST_MOVE_SELECTION,&reply)==0);
    CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0 && ((reply>>8u)&255u)==0);
    IUIAutomationElement *moved_owner=NULL; OK(IUIAutomationSelectionItemPattern_get_CurrentSelectionContainer(pc,&moved_owner));
    BSTR moved_id=NULL; OK(IUIAutomationElement_get_CurrentAutomationId(moved_owner,&moved_id)); CHECK(wcscmp(moved_id,L"nexa-0000000000000050")==0);
    SysFreeString(moved_id); IUIAutomationElement_Release(moved_owner);
    OK(IUIAutomationSelectionItemPattern_get_CurrentIsSelected(pc,&value)); CHECK(!value); /* Old queued request did not retarget group 80. */
    OK(IUIAutomationSelectionItemPattern_Select(pc)); CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0);
    OK(IUIAutomationSelectionItemPattern_get_CurrentIsSelected(pc,&value)); CHECK(value);
    CHECK(host(UIA_TEST_RESTORE_SELECTION,&reply)==0);
    OK(IUIAutomationSelectionItemPattern_Select(pc));
    CHECK(host(UIA_TEST_MOVE_UNPUBLISHED,&reply)==0);
    CHECK(host(UIA_TEST_ROUTE_SELECTION,&reply)==0); /* Must defer against the older displayed frame. */
    CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0);
    OK(IUIAutomationSelectionItemPattern_get_CurrentIsSelected(pc,&value)); CHECK(!value);
    CHECK(host(UIA_TEST_RESTORE_SELECTION,&reply)==0);
    OK(IUIAutomationSelectionItemPattern_Select(pd)); OK(IUIAutomationSelectionItemPattern_Select(pr));
    CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0 && ((reply>>8u)&255u)==62 && ((reply>>16u)&255u)==72);
    CHECK(selection_id(opt,62)==0 && selection_id(rad,72)==0);
    OK(IUIAutomationSelectionItemPattern_get_CurrentIsSelected(pr,&value)); CHECK(value);
    CHECK(host(UIA_TEST_SHOW,&reply)==0);
    OK(IUIAutomationElement_get_CurrentIsKeyboardFocusable(required,&value)); CHECK(value);
    OK(IUIAutomationElement_SetFocus(required)); CHECK(host(UIA_TEST_SELECTION_FOCUS,&reply)==0 && reply==52);
    OK(IUIAutomationElement_get_CurrentHasKeyboardFocus(b,&value)); CHECK(!!value==!!(GetForegroundWindow()==hwnd));
    IUIAutomationSelectionItemPattern_Release(pa); IUIAutomationSelectionItemPattern_Release(pb); IUIAutomationSelectionItemPattern_Release(pc);
    IUIAutomationSelectionItemPattern_Release(pd); IUIAutomationSelectionItemPattern_Release(px); IUIAutomationSelectionItemPattern_Release(pr);
    IUIAutomationSelectionPattern_Release(req); IUIAutomationSelectionPattern_Release(opt); IUIAutomationSelectionPattern_Release(rad);
    IUIAutomationElement_Release(required); IUIAutomationElement_Release(optional); IUIAutomationElement_Release(radio);
    IUIAutomationElement_Release(a); IUIAutomationElement_Release(b); IUIAutomationElement_Release(c); IUIAutomationElement_Release(d); IUIAutomationElement_Release(disabled); IUIAutomationElement_Release(r);
    return 0;
}
static int run(HWND hwnd) {
    OK(CoInitializeEx(NULL,COINIT_MULTITHREADED));
    IUIAutomation *uia=NULL;
    OK(CoCreateInstance(&CLSID_CUIAutomation,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAutomation,(void **)&uia));
    IUIAutomationElement *root=NULL; OK(IUIAutomation_ElementFromHandle(uia,hwnd,&root)); CHECK(root);
    BSTR text=NULL; OK(IUIAutomationElement_get_CurrentFrameworkId(root,&text)); CHECK(wcscmp(text,L"NexaUI")==0); SysFreeString(text);
    OK(IUIAutomationElement_get_CurrentName(root,&text)); CHECK(SysStringLen(text)==1600); SysFreeString(text);
    IUIAutomationElement *button=find(uia,root,2),*toggle=find(uia,root,3),*disabled=find(uia,root,4);
    IUIAutomationElement *secret=find(uia,root,5),*outside=find(uia,root,7),*dynamic=find(uia,root,8);
    CHECK(button && toggle && disabled && secret && outside && dynamic);
    observer *events=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*events)); CHECK(events);
    events->structure.lpVtbl=&structure_vtable; events->focus.lpVtbl=&focus_vtable; events->automation.lpVtbl=&automation_vtable; events->refs=1;
    DWORD process_id=0; CHECK(GetWindowThreadProcessId(hwnd,&process_id)); events->process_id=(int)process_id;
    OK(IUIAutomation_AddStructureChangedEventHandler(uia,root,TreeScope_Subtree,NULL,&events->structure));
    OK(IUIAutomation_AddFocusChangedEventHandler(uia,NULL,&events->focus));
    OK(IUIAutomation_AddAutomationEventHandler(uia,UIA_SelectionItem_ElementSelectedEventId,root,TreeScope_Subtree,NULL,&events->automation));
    OK(IUIAutomation_AddAutomationEventHandler(uia,UIA_SelectionItem_ElementRemovedFromSelectionEventId,root,TreeScope_Subtree,NULL,&events->automation));
    OK(IUIAutomation_AddAutomationEventHandler(uia,UIA_Selection_InvalidatedEventId,root,TreeScope_Subtree,NULL,&events->automation));
    CHECK(name_is(button,L"\x00d6" L"ffnen \xd83d\xdcc1"));
    unsigned read_reply=0;
    CHECK(host(UIA_TEST_READ_WITHOUT_OWNER_PUMP,&read_reply)==0);
    CHECK(name_is(button,L"\x00d6" L"ffnen \xd83d\xdcc1") && SetEvent(read_complete));
    CONTROLTYPEID type=0; OK(IUIAutomationElement_get_CurrentControlType(button,&type)); CHECK(type==UIA_ButtonControlTypeId);
    BOOL value=FALSE; OK(IUIAutomationElement_get_CurrentIsEnabled(disabled,&value)); CHECK(!value);
    CHECK(invoke(disabled,0));
    OK(IUIAutomationElement_get_CurrentIsPassword(secret,&value)); CHECK(value);
    VARIANT property; VariantInit(&property);
    HRESULT protected_hr=IUIAutomationElement_GetCurrentPropertyValueEx(secret,UIA_ValueValuePropertyId,TRUE,&property);
    if(SUCCEEDED(protected_hr)) {
        BOOL unsupported=FALSE;OK(IUIAutomation_CheckNotSupported(uia,property,&unsupported));
        CHECK(unsupported&&V_VT(&property)==VT_UNKNOWN);
    } else CHECK(protected_hr==E_ACCESSDENIED || protected_hr==(HRESULT)UIA_E_NOTSUPPORTED);
    CHECK(V_VT(&property)!=VT_BSTR);VariantClear(&property);
    OK(IUIAutomationElement_get_CurrentIsOffscreen(outside,&value)); CHECK(value);
    RECT first_bounds,second_bounds; OK(IUIAutomationElement_get_CurrentBoundingRectangle(button,&first_bounds));
    CHECK(first_bounds.right>first_bounds.left && first_bounds.bottom>first_bounds.top);
    unsigned reply=0; CHECK(host(UIA_TEST_MOVE,&reply)==0);
    OK(IUIAutomationElement_get_CurrentBoundingRectangle(button,&second_bounds));
    CHECK(second_bounds.left-first_bounds.left==40 && second_bounds.top-first_bounds.top==30);
    CHECK(selection_probe(uia,root,events,hwnd)==0);
    CHECK(host(UIA_TEST_STRESS_START,&reply)==0);
    unsigned stress_accepted=0;
    for (unsigned i=0;i<32;++i) {
        CHECK(name_is(button,L"\x00d6" L"ffnen \xd83d\xdcc1"));
        OK(IUIAutomationElement_get_CurrentBoundingRectangle(button,&second_bounds));
        stress_accepted+=(unsigned)invoke(button,1);
    }
    CHECK(host(UIA_TEST_STRESS_STOP,&reply)==0 && stress_accepted>0 && reply==stress_accepted);
    /* Native owner disablement must block both new and still-buffered requests. */
    CHECK(invoke(button,1)); CHECK(host(UIA_TEST_DISABLE_HOST,&reply)==0);
    OK(IUIAutomationElement_get_CurrentIsEnabled(root,&value)); CHECK(!value);
    OK(IUIAutomationElement_get_CurrentIsEnabled(button,&value)); CHECK(!value);
    CHECK(invoke(button,0));
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && !(reply&1u));
    CHECK(host(UIA_TEST_ENABLE_HOST,&reply)==0);
    OK(IUIAutomationElement_get_CurrentIsEnabled(button,&value)); CHECK(value);
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && !(reply&1u));
    /* Real invocation, duplicate rejection, toggle and owner-thread focus dispatch. */
    CHECK(invoke(button,1)); CHECK(invoke(button,0));
    IUIAutomationTogglePattern *toggle_pattern=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(toggle,UIA_TogglePatternId,&IID_IUIAutomationTogglePattern,(void **)&toggle_pattern));
    OK(IUIAutomationTogglePattern_Toggle(toggle_pattern));
    CHECK(host(UIA_TEST_SHOW,&reply)==0);
    OK(IUIAutomationElement_SetFocus(button));
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && (reply&7u)==7u);
    BOOL native_focus=FALSE; OK(IUIAutomationElement_get_CurrentHasKeyboardFocus(button,&native_focus));
    CHECK(!!native_focus==!!(reply&8u));
    if (reply&8u) CHECK(wait_event(&events->focuses,0));
    enum ToggleState state=ToggleState_Off; OK(IUIAutomationTogglePattern_get_CurrentToggleState(toggle_pattern,&state)); CHECK(state==ToggleState_On);
    CHECK(host(UIA_TEST_FILL_CORE,&reply)==0);
    CHECK(invoke(button,1)); OK(IUIAutomationTogglePattern_Toggle(toggle_pattern));
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && !(reply&1u) && (reply&2u) && (reply>>8u)==64);
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && (reply&1u) && !(reply&2u) && !(reply>>8u));
    /* The native queue itself is bounded, preserving all accepted requests. */
    IUIAutomationCondition *all=NULL; IUIAutomationCacheRequest *cache=NULL;
    IUIAutomationElementArray *array=NULL; IUIAutomationElement *queued[65]={0};
    OK(IUIAutomation_CreateTrueCondition(uia,&all));
    OK(IUIAutomation_CreateCacheRequest(uia,&cache));
    OK(IUIAutomationCacheRequest_AddProperty(cache,UIA_AutomationIdPropertyId));
    OK(IUIAutomationElement_FindAllBuildCache(root,TreeScope_Descendants,all,cache,&array));
    int length=0; OK(IUIAutomationElementArray_get_Length(array,&length));
    for (int i=0;i<length;++i) {
        IUIAutomationElement *e=NULL; BSTR id=NULL;
        OK(IUIAutomationElementArray_GetElement(array,i,&e));
        OK(IUIAutomationElement_get_CachedAutomationId(e,&id));
        unsigned long long number=0;
        if (id && swscanf_s(id,L"nexa-%llx",&number)==1 && number>=100 && number<=164) queued[number-100]=e;
        else IUIAutomationElement_Release(e);
        SysFreeString(id);
    }
    IUIAutomationElementArray_Release(array); IUIAutomationCondition_Release(all); IUIAutomationCacheRequest_Release(cache);
    unsigned accepted=0;
    for (unsigned i=0;i<65;++i) {
        CHECK(queued[i]);
        if (!invoke(queued[i],1)) break;
        ++accepted;
    }
    /* UIA can queue one coalesced focus operation itself while invoking. */
    CHECK(accepted>=63 && accepted<=64);
    CHECK(invoke(queued[64],0));
    for (unsigned i=0;i<65;++i) IUIAutomationElement_Release(queued[i]);
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && (reply>>8u)==accepted);
    SAFEARRAY *runtime_before=NULL,*runtime_after=NULL;
    OK(IUIAutomationElement_GetRuntimeId(button,&runtime_before));
    CHECK(host(UIA_TEST_FAIL_PUBLISH,&reply)==0);
    CHECK(name_is(button,L"\x00d6" L"ffnen \xd83d\xdcc1"));
    CHECK(host(UIA_TEST_REPUBLISH,&reply)==0 && name_is(button,L"Changed"));
    OK(IUIAutomationElement_GetRuntimeId(button,&runtime_after)); CHECK(runtime_equal(runtime_before,runtime_after));
    SafeArrayDestroy(runtime_before); SafeArrayDestroy(runtime_after);
    OK(IUIAutomationElement_GetRuntimeId(dynamic,&runtime_before));
    CHECK(invoke(dynamic,1));
    LONG old_structures=InterlockedCompareExchange(&events->structures,0,0);
    CHECK(host(UIA_TEST_REMOVE,&reply)==0 && unavailable(dynamic));
    CHECK(wait_event(&events->structures,old_structures));
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && !(reply&1u) && !(reply>>8u));
    CHECK(host(UIA_TEST_RECREATE,&reply)==0 && unavailable(dynamic));
    IUIAutomationElement *replacement=find(uia,root,8); CHECK(replacement);
    OK(IUIAutomationElement_get_CurrentControlType(replacement,&type)); CHECK(type==UIA_CheckBoxControlTypeId);
    OK(IUIAutomationElement_GetRuntimeId(replacement,&runtime_after)); CHECK(!runtime_equal(runtime_before,runtime_after));
    SafeArrayDestroy(runtime_before); SafeArrayDestroy(runtime_after);
    IUIAutomationElement *old_selection=find(uia,root,61),*old_group=find(uia,root,60); CHECK(old_selection && old_group);
    IUIAutomationSelectionItemPattern *old_item_pattern=NULL; IUIAutomationSelectionPattern *old_group_pattern=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(old_selection,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&old_item_pattern));
    OK(IUIAutomationElement_GetCurrentPatternAs(old_group,UIA_SelectionPatternId,&IID_IUIAutomationSelectionPattern,(void **)&old_group_pattern));
    CHECK(host(UIA_TEST_REPLACE_CONTEXT,&reply)==0 && unavailable(button) && unavailable(replacement));
    HRESULT stale_value=IUIAutomationSelectionItemPattern_get_CurrentIsSelected(old_item_pattern,&value);
    HRESULT stale_action=IUIAutomationSelectionItemPattern_Select(old_item_pattern);
    IUIAutomationElementArray *stale_array=NULL; HRESULT stale_group=IUIAutomationSelectionPattern_GetCurrentSelection(old_group_pattern,&stale_array);
    int diagnostic_count=-1;
    if (stale_array) OK(IUIAutomationElementArray_get_Length(stale_array,&diagnostic_count));
    fprintf(stderr,"Stale selection: value=%08lx selected=%d action=%08lx group=%08lx count=%d\n",(unsigned long)stale_value,value,(unsigned long)stale_action,(unsigned long)stale_group,diagnostic_count);
    CHECK(unavailable(old_selection) && unavailable(old_group));
    /* UIAutomationCore normalizes these retired pattern property reads to
       false/empty. The live new container still selects 62; no read may retarget. */
    CHECK(stale_value==(HRESULT)UIA_E_ELEMENTNOTAVAILABLE || (SUCCEEDED(stale_value) && !value));
    CHECK(stale_action==(HRESULT)UIA_E_ELEMENTNOTAVAILABLE);
    if (SUCCEEDED(stale_group)) {
        int stale_count=-1; CHECK(stale_array);
        OK(IUIAutomationElementArray_get_Length(stale_array,&stale_count)); CHECK(!stale_count);
        IUIAutomationElementArray_Release(stale_array);
    } else CHECK(stale_group==(HRESULT)UIA_E_ELEMENTNOTAVAILABLE && !stale_array);
    CHECK(host(UIA_TEST_SELECTION_APPLY,&reply)==0 && ((reply>>8u)&255u)==62);
    IUIAutomationSelectionItemPattern_Release(old_item_pattern); IUIAutomationSelectionPattern_Release(old_group_pattern);
    IUIAutomationElement_Release(old_selection); IUIAutomationElement_Release(old_group);
    IUIAutomationElement_Release(replacement); replacement=find(uia,root,2); CHECK(replacement && invoke(replacement,1));
    CHECK(host(UIA_TEST_WRONG_CONTEXT,&reply)==0);
    CHECK(host(UIA_TEST_APPLY,&reply)==0 && (reply&1u));
    OK(IUIAutomation_RemoveAllEventHandlers(uia)); observer_release(events);
    CHECK(host(UIA_TEST_DESTROY_CONTEXT,&reply)==0 && name_is(replacement,L"Changed"));
    OK(IUIAutomationElement_GetRuntimeId(replacement,&runtime_before));
    CHECK(host(UIA_TEST_DETACH,&reply)==0 && closed(replacement) && closed(root));
    CHECK(host(UIA_TEST_REATTACH,&reply)==0);
    IUIAutomationElement *new_root=NULL; OK(IUIAutomation_ElementFromHandle(uia,hwnd,&new_root)); CHECK(new_root);
    IUIAutomationElement *new_button=find(uia,new_root,2); CHECK(new_button && name_is(new_button,L"Changed"));
    CHECK(closed(replacement) && closed(root));
    OK(IUIAutomationElement_GetRuntimeId(new_button,&runtime_after)); CHECK(!runtime_equal(runtime_before,runtime_after));
    SafeArrayDestroy(runtime_before); SafeArrayDestroy(runtime_after);
    CHECK(invoke(new_button,1)); CHECK(host(UIA_TEST_APPLY,&reply)==0 && (reply&1u));
    CHECK(!design_probe(uia,new_root,hwnd));
    CHECK(unavailable(new_button));IUIAutomationElement_Release(new_button);
    new_button=find(uia,new_root,2);CHECK(new_button&&name_is(new_button,L"Changed")&&invoke(new_button,1));
    CHECK(!host(UIA_TEST_APPLY,&reply)&&(reply&1u));
    CHECK(host(UIA_TEST_DESTROY_WINDOW,&reply)==0 && closed(new_root) && closed(new_button));
    IUIAutomationElement_Release(new_button); IUIAutomationElement_Release(new_root);
    IUIAutomationElement_Release(replacement); IUIAutomationElement_Release(dynamic);
    IUIAutomationTogglePattern_Release(toggle_pattern);
    IUIAutomationElement_Release(button); IUIAutomationElement_Release(toggle); IUIAutomationElement_Release(disabled);
    IUIAutomationElement_Release(secret); IUIAutomationElement_Release(outside); IUIAutomationElement_Release(root);
    IUIAutomation_Release(uia); CoUninitialize();
    puts("Independent UIA client passed"); return 0;
}
static int cloud_wait(IUIAutomation *uia,IUIAutomationElement *root,unsigned group_id,unsigned expected,const WCHAR *preview) {
    ULONGLONG end=GetTickCount64()+15000;
    do {
        IUIAutomationElement *group=find(uia,root,group_id); int ready=0;
        if (group) {
            IUIAutomationSelectionPattern *pattern=NULL; IUIAutomationElementArray *array=NULL;
            HRESULT hr=IUIAutomationElement_GetCurrentPatternAs(group,UIA_SelectionPatternId,&IID_IUIAutomationSelectionPattern,(void **)&pattern);
            if (SUCCEEDED(hr)) hr=IUIAutomationSelectionPattern_GetCurrentSelection(pattern,&array);
            int count=-1; if (SUCCEEDED(hr)) hr=IUIAutomationElementArray_get_Length(array,&count);
            if (SUCCEEDED(hr) && count==(expected ? 1 : 0)) {
                ready=!expected;
                if (expected) {
                    IUIAutomationElement *e=NULL; BSTR actual=NULL; WCHAR wanted[48]; swprintf_s(wanted,48,L"nexa-%016llx",(unsigned long long)expected);
                    if (SUCCEEDED(IUIAutomationElementArray_GetElement(array,0,&e)) && SUCCEEDED(IUIAutomationElement_get_CurrentAutomationId(e,&actual))) ready=wcscmp(wanted,actual)==0;
                    SysFreeString(actual); if (e) IUIAutomationElement_Release(e);
                }
            }
            if (array) IUIAutomationElementArray_Release(array); if (pattern) IUIAutomationSelectionPattern_Release(pattern);
            IUIAutomationElement_Release(group);
        }
        if (ready && preview) {
            IUIAutomationElement *image=find(uia,root,302); BSTR name=NULL; ready=0;
            if (image) {
                if (SUCCEEDED(IUIAutomationElement_get_CurrentName(image,&name))) ready=wcscmp(name,preview)==0;
                SysFreeString(name); IUIAutomationElement_Release(image);
            }
        }
        if (ready) return 0;
        Sleep(20);
    } while (GetTickCount64()<end);
    fprintf(stderr,"NexaCloud UIA selection/preview timed out for group %u target %u\n",group_id,expected); return 1;
}
static int cloud_select(IUIAutomation *uia,IUIAutomationElement *root,unsigned id,int remove) {
    IUIAutomationElement *e=find(uia,root,id); CHECK(e);
    IUIAutomationSelectionItemPattern *pattern=NULL;
    OK(IUIAutomationElement_GetCurrentPatternAs(e,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&pattern));
    HRESULT hr=remove ? IUIAutomationSelectionItemPattern_RemoveFromSelection(pattern) : IUIAutomationSelectionItemPattern_Select(pattern);
    IUIAutomationSelectionItemPattern_Release(pattern); IUIAutomationElement_Release(e); CHECK(SUCCEEDED(hr)); return 0;
}
/* Narrow windows present details in a modal instead of the side inspector.
   Use the same exposed commands as a user, then wait for the modal to close
   before selecting another file outside its interaction scope. */
static int cloud_preview(IUIAutomation *uia,IUIAutomationElement *root,unsigned id,const WCHAR *name) {
    CHECK(cloud_wait(uia,root,219,id,NULL)==0);
    IUIAutomationElement *image=find(uia,root,302);
    if (image) IUIAutomationElement_Release(image);
    else {
        IUIAutomationElement *details=find(uia,root,203); CHECK(details);
        int opened=invoke(details,1); IUIAutomationElement_Release(details); CHECK(opened);
        puts("NexaCloud UIA opens responsive details dialog");
    }
    CHECK(cloud_wait(uia,root,219,id,name)==0);
    IUIAutomationElement *close=find(uia,root,502);
    if (!close) return 0;
    int closed_dialog=invoke(close,1); IUIAutomationElement_Release(close); CHECK(closed_dialog);
    ULONGLONG end=GetTickCount64()+15000;
    do {
        close=find(uia,root,502);
        if (!close) {
            IUIAutomationElement *item=find(uia,root,id); BOOL enabled=FALSE;
            HRESULT hr=item ? IUIAutomationElement_get_CurrentIsEnabled(item,&enabled) : E_FAIL;
            if (item) IUIAutomationElement_Release(item);
            if (SUCCEEDED(hr) && enabled) return 0;
        } else IUIAutomationElement_Release(close);
        Sleep(20);
    } while (GetTickCount64()<end);
    fputs("NexaCloud UIA details dialog did not release the file interaction scope\n",stderr); return 1;
}
static int cloud_probe(HWND hwnd) {
    OK(CoInitializeEx(NULL,COINIT_MULTITHREADED)); IUIAutomation *uia=NULL; IUIAutomationElement *root=NULL;
    OK(CoCreateInstance(&CLSID_CUIAutomation,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAutomation,(void **)&uia));
    OK(IUIAutomation_ElementFromHandle(uia,hwnd,&root)); CHECK(root);
    CHECK(cloud_select(uia,root,11,0)==0);
    CHECK(cloud_wait(uia,root,37,11,NULL)==0 && cloud_preview(uia,root,1000,L"Bergsee.jpg")==0);
    CHECK(cloud_select(uia,root,1001,0)==0 && cloud_preview(uia,root,1001,L"Wald.png")==0);
    IUIAutomationElement *old_file=find(uia,root,1001); CHECK(old_file);
    CHECK(cloud_select(uia,root,1001,1)==0 && cloud_wait(uia,root,219,0,NULL)==0);
    CHECK(cloud_select(uia,root,10,0)==0 && cloud_wait(uia,root,37,10,NULL)==0);
    ULONGLONG end=GetTickCount64()+15000;
    while (!unavailable(old_file) && GetTickCount64()<end) Sleep(20);
    CHECK(unavailable(old_file)); IUIAutomationElement_Release(old_file);
    CHECK(cloud_select(uia,root,11,0)==0 && cloud_wait(uia,root,37,11,NULL)==0);
    CHECK(cloud_select(uia,root,1001,0)==0 && cloud_preview(uia,root,1001,L"Wald.png")==0);
    IUIAutomationElement_Release(root); IUIAutomation_Release(uia); CoUninitialize();
    puts("Independent native NexaCloud UIA navigation/file/clear/reopen workflow passed"); return 0;
}
int wmain(int argc,WCHAR **argv) {
    if (argc==3 && wcscmp(argv[1],L"--cloud")==0) {
        setvbuf(stdout,NULL,_IONBF,0); /* Keep progress when a parent deadline fires. */
        return cloud_probe((HWND)(UINT_PTR)_wcstoui64(argv[2],NULL,10));
    }
    if (argc!=5&&argc!=6) return 2;
    HWND hwnd=(HWND)(UINT_PTR)_wcstoui64(argv[1],NULL,10);
    commands=(HANDLE)(UINT_PTR)_wcstoui64(argv[2],NULL,10); acks=(HANDLE)(UINT_PTR)_wcstoui64(argv[3],NULL,10);
    read_complete=(HANDLE)(UINT_PTR)_wcstoui64(argv[4],NULL,10);
    int result;
    if(argc==6&&wcscmp(argv[5],L"--design")==0) {
        result=1;IUIAutomation *uia=NULL;IUIAutomationElement *root=NULL;
        HRESULT init=CoInitializeEx(NULL,COINIT_MULTITHREADED);
        if(SUCCEEDED(init)) {
            if(SUCCEEDED(CoCreateInstance(&CLSID_CUIAutomation,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAutomation,(void **)&uia))&&
               SUCCEEDED(IUIAutomation_ElementFromHandle(uia,hwnd,&root))) {
                result=design_probe(uia,root,hwnd);unsigned reply;
                if(!result)result=host(UIA_TEST_DESTROY_WINDOW,&reply);
            }
            if(root)IUIAutomationElement_Release(root);if(uia)IUIAutomation_Release(uia);CoUninitialize();
        }
    } else result=run(hwnd);
    CloseHandle(commands); CloseHandle(acks); CloseHandle(read_complete); return result;
}
