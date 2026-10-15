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
static HANDLE commands,acks;
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
typedef struct observer {
    IUIAutomationStructureChangedEventHandler structure;
    IUIAutomationFocusChangedEventHandler focus;
    LONG refs, structures, focuses;
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
    else return E_NOINTERFACE;
    observer_add(o); return S_OK;
}
#define OBSERVER_INTERFACE(prefix,type,member) \
static HRESULT STDMETHODCALLTYPE prefix##_query(type *self,REFIID iid,void **out) { return observer_query(CONTAINING_RECORD(self,observer,member),iid,out); } \
static ULONG STDMETHODCALLTYPE prefix##_add(type *self) { return observer_add(CONTAINING_RECORD(self,observer,member)); } \
static ULONG STDMETHODCALLTYPE prefix##_release(type *self) { return observer_release(CONTAINING_RECORD(self,observer,member)); }
OBSERVER_INTERFACE(structure,IUIAutomationStructureChangedEventHandler,structure)
OBSERVER_INTERFACE(focus,IUIAutomationFocusChangedEventHandler,focus)
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
static IUIAutomationStructureChangedEventHandlerVtbl structure_vtable={structure_query,structure_add,structure_release,structure_event};
static IUIAutomationFocusChangedEventHandlerVtbl focus_vtable={focus_query,focus_add,focus_release,focus_event};
static int wait_event(LONG *counter,LONG previous) {
    ULONGLONG end=GetTickCount64()+3000;
    while (InterlockedCompareExchange(counter,0,0)<=previous && GetTickCount64()<end) Sleep(10);
    return InterlockedCompareExchange(counter,0,0)>previous;
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
    events->structure.lpVtbl=&structure_vtable; events->focus.lpVtbl=&focus_vtable; events->refs=1;
    DWORD process_id=0; CHECK(GetWindowThreadProcessId(hwnd,&process_id)); events->process_id=(int)process_id;
    OK(IUIAutomation_AddStructureChangedEventHandler(uia,root,TreeScope_Subtree,NULL,&events->structure));
    OK(IUIAutomation_AddFocusChangedEventHandler(uia,NULL,&events->focus));
    CHECK(name_is(button,L"\x00d6" L"ffnen \xd83d\xdcc1"));
    CONTROLTYPEID type=0; OK(IUIAutomationElement_get_CurrentControlType(button,&type)); CHECK(type==UIA_ButtonControlTypeId);
    BOOL value=FALSE; OK(IUIAutomationElement_get_CurrentIsEnabled(disabled,&value)); CHECK(!value);
    CHECK(invoke(disabled,0));
    OK(IUIAutomationElement_get_CurrentIsPassword(secret,&value)); CHECK(value);
    VARIANT property; VariantInit(&property);
    OK(IUIAutomationElement_GetCurrentPropertyValue(secret,UIA_ValueValuePropertyId,&property));
    CHECK(V_VT(&property)==VT_BSTR && SysStringLen(V_BSTR(&property))==0); VariantClear(&property);
    OK(IUIAutomationElement_get_CurrentIsOffscreen(outside,&value)); CHECK(value);
    RECT first_bounds,second_bounds; OK(IUIAutomationElement_get_CurrentBoundingRectangle(button,&first_bounds));
    CHECK(first_bounds.right>first_bounds.left && first_bounds.bottom>first_bounds.top);
    unsigned reply=0; CHECK(host(UIA_TEST_MOVE,&reply)==0);
    OK(IUIAutomationElement_get_CurrentBoundingRectangle(button,&second_bounds));
    CHECK(second_bounds.left-first_bounds.left==40 && second_bounds.top-first_bounds.top==30);
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
    CHECK(host(UIA_TEST_REPLACE_CONTEXT,&reply)==0 && unavailable(button) && unavailable(replacement));
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
    CHECK(host(UIA_TEST_DESTROY_WINDOW,&reply)==0 && closed(new_root) && closed(new_button));
    IUIAutomationElement_Release(new_button); IUIAutomationElement_Release(new_root);
    IUIAutomationElement_Release(replacement); IUIAutomationElement_Release(dynamic);
    IUIAutomationTogglePattern_Release(toggle_pattern);
    IUIAutomationElement_Release(button); IUIAutomationElement_Release(toggle); IUIAutomationElement_Release(disabled);
    IUIAutomationElement_Release(secret); IUIAutomationElement_Release(outside); IUIAutomationElement_Release(root);
    IUIAutomation_Release(uia); CoUninitialize();
    puts("Independent UIA client passed"); return 0;
}
int wmain(int argc,WCHAR **argv) {
    if (argc!=4) return 2;
    HWND hwnd=(HWND)(UINT_PTR)_wcstoui64(argv[1],NULL,10);
    commands=(HANDLE)(UINT_PTR)_wcstoui64(argv[2],NULL,10); acks=(HANDLE)(UINT_PTR)_wcstoui64(argv[3],NULL,10);
    int result=run(hwnd); CloseHandle(commands); CloseHandle(acks); return result;
}
