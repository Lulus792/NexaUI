/* External C17 COM consumer. No NexaUI headers, private provider calls or SDK linkage. */
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
#include "uia_value_protocol.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x);return 1;}}while(0)
#define OK(x) do {HRESULT hr_=(x);if(FAILED(hr_)){fprintf(stderr,"%s:%d: %s HRESULT=%08lx\n",__FILE__,__LINE__,#x,(unsigned long)hr_);return 1;}}while(0)
static HANDLE commands,acks,done;
static HRESULT set_value(IUIAutomationValuePattern *pattern,const WCHAR *text) {
    /* The client contract takes BSTR, unlike the provider's LPCWSTR method.
       A client allocation failure must not masquerade as an expected denial. */
    BSTR value=SysAllocString(text);
    if(!value){fputs("UIA client BSTR allocation failed\n",stderr);exit(EXIT_FAILURE);}
    HRESULT hr=IUIAutomationValuePattern_SetValue(pattern,value);SysFreeString(value);return hr;
}
static int host(unsigned command,unsigned expected) {
    DWORD n=0;unsigned reply=~0u;
    CHECK(WriteFile(commands,&command,sizeof(command),&n,NULL)&&n==sizeof(command));
    CHECK(ReadFile(acks,&reply,sizeof(reply),&n,NULL)&&n==sizeof(reply));
    fprintf(stderr,"Value host command %u reply %u\n",command,reply);
    CHECK(expected==~0u||reply==expected);return 0;
}
static IUIAutomationElement *find(IUIAutomation *uia,IUIAutomationElement *root,unsigned id) {
    WCHAR name[48];swprintf_s(name,48,L"nexa-%016llx",(unsigned long long)id);
    VARIANT v;VariantInit(&v);V_VT(&v)=VT_BSTR;V_BSTR(&v)=SysAllocString(name);if(!V_BSTR(&v))return NULL;
    IUIAutomationCondition *condition=NULL;IUIAutomationElement *e=NULL;
    HRESULT hr=IUIAutomation_CreatePropertyCondition(uia,UIA_AutomationIdPropertyId,v,&condition);VariantClear(&v);
    if(SUCCEEDED(hr)){IUIAutomationElement_FindFirst(root,TreeScope_Descendants,condition,&e);IUIAutomationCondition_Release(condition);}return e;
}
static int current(IUIAutomationValuePattern *p,const WCHAR *expected) {
    BSTR s=NULL;OK(IUIAutomationValuePattern_get_CurrentValue(p,&s));
    int equal=s&&SysStringLen(s)==wcslen(expected)&&!wcscmp(s,expected);SysFreeString(s);CHECK(equal);return 0;
}
static int property(IUIAutomation *uia,IUIAutomationElement *e,const WCHAR *expected) {
    VARIANT v;VariantInit(&v);OK(IUIAutomationElement_GetCurrentPropertyValueEx(e,UIA_ValueValuePropertyId,TRUE,&v));
    BOOL unsupported=TRUE;HRESULT hr=IUIAutomation_CheckNotSupported(uia,v,&unsupported);
    int equal=SUCCEEDED(hr)&&!unsupported&&V_VT(&v)==VT_BSTR&&V_BSTR(&v)&&!wcscmp(V_BSTR(&v),expected);
    VariantClear(&v);CHECK(equal);return 0;
}
typedef struct observer {
    IUIAutomationPropertyChangedEventHandler iface;LONG refs;
    volatile LONG values,readonly,enabled,bad,last_readonly,last_enabled;
    volatile LONG guard_password,guard_available,last_guard_password,last_guard_available;
    SRWLOCK lock;BSTR last;
} observer;
static ULONG STDMETHODCALLTYPE add(IUIAutomationPropertyChangedEventHandler *self) {return (ULONG)InterlockedIncrement(&((observer *)self)->refs);}
static ULONG STDMETHODCALLTYPE release(IUIAutomationPropertyChangedEventHandler *self) {
    observer *o=(observer *)self;LONG n=InterlockedDecrement(&o->refs);
    if(!n){SysFreeString(o->last);free(o);}return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE query(IUIAutomationPropertyChangedEventHandler *self,REFIID iid,void **out) {
    if(!out)return E_POINTER;*out=NULL;
    if(!IsEqualIID(iid,&IID_IUnknown)&&!IsEqualIID(iid,&IID_IUIAutomationPropertyChangedEventHandler))return E_NOINTERFACE;
    *out=self;add(self);return S_OK;
}
static HRESULT STDMETHODCALLTYPE changed(IUIAutomationPropertyChangedEventHandler *self,IUIAutomationElement *sender,PROPERTYID id,VARIANT value) {
    observer *o=(observer *)self;BSTR name=NULL;
    if(FAILED(IUIAutomationElement_get_CurrentAutomationId(sender,&name)))return S_OK;
    int primary=name&&!wcscmp(name,L"nexa-00000000000004b0"),protected_field=name&&!wcscmp(name,L"nexa-00000000000004b5"),guard=name&&!wcscmp(name,L"nexa-00000000000004b8");SysFreeString(name);
    if((protected_field||guard)&&id==UIA_ValueValuePropertyId){InterlockedIncrement(&o->bad);return S_OK;}
    if(guard&&(id==UIA_IsPasswordPropertyId||id==UIA_IsValuePatternAvailablePropertyId)) {
        if(V_VT(&value)!=VT_BOOL){InterlockedIncrement(&o->bad);return S_OK;}
        if(id==UIA_IsPasswordPropertyId){InterlockedExchange(&o->last_guard_password,!!V_BOOL(&value));InterlockedIncrement(&o->guard_password);}
        else {InterlockedExchange(&o->last_guard_available,!!V_BOOL(&value));InterlockedIncrement(&o->guard_available);}
        return S_OK;
    }
    if(!primary)return S_OK;
    if(id==UIA_ValueValuePropertyId) {
        if(V_VT(&value)!=VT_BSTR||!V_BSTR(&value)){InterlockedIncrement(&o->bad);return S_OK;}
        BSTR copy=SysAllocStringLen(V_BSTR(&value),SysStringLen(V_BSTR(&value)));
        if(!copy){InterlockedIncrement(&o->bad);return S_OK;}
        AcquireSRWLockExclusive(&o->lock);BSTR old=o->last;o->last=copy;ReleaseSRWLockExclusive(&o->lock);SysFreeString(old);
        InterlockedIncrement(&o->values);
    } else if(id==UIA_ValueIsReadOnlyPropertyId||id==UIA_IsEnabledPropertyId) {
        if(V_VT(&value)!=VT_BOOL){InterlockedIncrement(&o->bad);return S_OK;}
        if(id==UIA_ValueIsReadOnlyPropertyId){InterlockedExchange(&o->last_readonly,!!V_BOOL(&value));InterlockedIncrement(&o->readonly);}
        else {InterlockedExchange(&o->last_enabled,!!V_BOOL(&value));InterlockedIncrement(&o->enabled);}
    }
    return S_OK;
}
static IUIAutomationPropertyChangedEventHandlerVtbl observer_vtable={query,add,release,changed};
static LONG read_counter(volatile LONG *p) {return InterlockedCompareExchange(p,0,0);}
static int wait_counter(volatile LONG *p,LONG before) {
    ULONGLONG end=GetTickCount64()+3000;
    while(read_counter(p)<=before&&GetTickCount64()<end)Sleep(10);
    CHECK(read_counter(p)>before);return 0;
}
static int value_event(observer *o,LONG before,const WCHAR *expected) {
    CHECK(wait_counter(&o->values,before)==0);
    AcquireSRWLockShared(&o->lock);int equal=o->last&&!wcscmp(o->last,expected);ReleaseSRWLockShared(&o->lock);
    CHECK(equal&&read_counter(&o->bad)==0);return 0;
}
static int pattern(IUIAutomationElement *e,IUIAutomationValuePattern **out) {
    *out=NULL;OK(IUIAutomationElement_GetCurrentPatternAs(e,UIA_ValuePatternId,&IID_IUIAutomationValuePattern,(void **)out));CHECK(*out);return 0;
}
static int refresh(IUIAutomation *uia,IUIAutomationElement *root,IUIAutomationElement **e,IUIAutomationValuePattern **p) {
    if(*p)IUIAutomationValuePattern_Release(*p);if(*e)IUIAutomationElement_Release(*e);
    *e=find(uia,root,1200);CHECK(*e&&pattern(*e,p)==0&&current(*p,L"replacement")==0);return 0;
}
static int closed(IUIAutomationElement *element,IUIAutomationValuePattern *pattern) {
    BSTR name=NULL,value=NULL;HRESULT name_hr=IUIAutomationElement_get_CurrentName(element,&name);
    SysFreeString(name);HRESULT value_hr=IUIAutomationValuePattern_get_CurrentValue(pattern,&value);
    /* UIAutomation can return an empty default for a stale pattern's property.
       An independent element-lifetime error AND refused write distinguish it
       from the genuinely empty, supported live field tested above. */
    int hidden=FAILED(value_hr)||!value||!*value;SysFreeString(value);
    HRESULT write_hr=set_value(pattern,L"stale-probe");
    fprintf(stderr,"Value stale name=%08lx value=%08lx write=%08lx hidden=%d\n",(unsigned long)name_hr,(unsigned long)value_hr,(unsigned long)write_hr,hidden);
    return (name_hr==(HRESULT)UIA_E_ELEMENTNOTAVAILABLE||name_hr==RPC_E_DISCONNECTED||name_hr==RPC_E_SERVER_DIED_DNE) && hidden && FAILED(write_hr);
}
int wmain(int argc,WCHAR **argv) {
    if(argc!=5)return 2;HWND hwnd=(HWND)(UINT_PTR)_wcstoui64(argv[1],NULL,10);
    commands=(HANDLE)(UINT_PTR)_wcstoui64(argv[2],NULL,10);acks=(HANDLE)(UINT_PTR)_wcstoui64(argv[3],NULL,10);done=(HANDLE)(UINT_PTR)_wcstoui64(argv[4],NULL,10);
    OK(CoInitializeEx(NULL,COINIT_MULTITHREADED));IUIAutomation *uia=NULL;IUIAutomationElement *root=NULL;
    OK(CoCreateInstance(&CLSID_CUIAutomation,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAutomation,(void **)&uia));
    OK(IUIAutomation_ElementFromHandle(uia,hwnd,&root));CHECK(root);
    IUIAutomationElement *e[7]={0};IUIAutomationValuePattern *p[7]={0};
    const WCHAR *initial[]={L"10.42.0.1",L"7443",L"",L"read-only",L"disabled",NULL,L"one\ntwo"};
    for(unsigned i=0;i<7;++i) {
        e[i]=find(uia,root,1200+i);CHECK(e[i]);VARIANT v;VariantInit(&v);
        OK(IUIAutomationElement_GetCurrentPropertyValueEx(e[i],UIA_IsValuePatternAvailablePropertyId,TRUE,&v));
        if(i==5) {
            BOOL unsupported=FALSE;OK(IUIAutomation_CheckNotSupported(uia,v,&unsupported));
            CHECK((V_VT(&v)==VT_BOOL&&!V_BOOL(&v))||(V_VT(&v)==VT_UNKNOWN&&unsupported));
            VariantClear(&v);OK(IUIAutomationElement_GetCurrentPropertyValue(e[i],UIA_IsValuePatternAvailablePropertyId,&v));
            CHECK(V_VT(&v)==VT_BOOL&&!V_BOOL(&v));
        } else CHECK(V_VT(&v)==VT_BOOL&&V_BOOL(&v));
        VariantClear(&v);
        if(i==5) {
            BOOL password=FALSE;OK(IUIAutomationElement_get_CurrentIsPassword(e[i],&password));CHECK(password);
            HRESULT hr=IUIAutomationElement_GetCurrentPropertyValueEx(e[i],UIA_ValueValuePropertyId,TRUE,&v);
            if(SUCCEEDED(hr)){BOOL unsupported=FALSE;OK(IUIAutomation_CheckNotSupported(uia,v,&unsupported));CHECK(unsupported);}
            CHECK(V_VT(&v)!=VT_BSTR);VariantClear(&v);
            IUnknown *absent=NULL;hr=IUIAutomationElement_GetCurrentPattern(e[i],UIA_ValuePatternId,&absent);CHECK(FAILED(hr)||!absent);if(absent)IUnknown_Release(absent);
        } else {
            CHECK(pattern(e[i],&p[i])==0&&current(p[i],initial[i])==0&&property(uia,e[i],initial[i])==0);
            BOOL readonly=FALSE;OK(IUIAutomationValuePattern_get_CurrentIsReadOnly(p[i],&readonly));CHECK(!!readonly==(i==3));
        }
    }
    CHECK(FAILED(set_value(p[3],L"no"))&&FAILED(set_value(p[4],L"no")));
    /* Each fresh context starts with a small semantic buffer. Invoke native
       disable/re-enable or detach from the USER allocator, both while routing
       the copied payload and while reserving semantic storage before applying. */
    WCHAR probe_mode[32]={0};GetEnvironmentVariableW(L"NEXAUI_VALUE_CALLOUT",probe_mode,32);
    unsigned only=probe_mode[0]?(unsigned)wcstoul(probe_mode,NULL,10):0;
    for(unsigned command=VALUE_ROUTE_DISABLE_ALLOC;command<=VALUE_RESERVE_DETACH_FREE;++command) {
        if(only&&only!=command)continue;
        CHECK(host(VALUE_PREPARE_CALLOUT,0)==0);
        IUIAutomationValuePattern_Release(p[0]);IUIAutomationElement_Release(e[0]);
        e[0]=find(uia,root,1200);CHECK(e[0]&&pattern(e[0],&p[0])==0&&current(p[0],initial[0])==0);
        WCHAR payload[4097];for(size_t i=0;i<4096;++i)payload[i]=L'x';payload[4096]=0;
        OK(set_value(p[0],command<=VALUE_ROUTE_DETACH_ALLOC?L"oom-retry":payload));
        CHECK(host(command,1)==0);
        IUIAutomationValuePattern_Release(p[0]);IUIAutomationElement_Release(e[0]);
        IUIAutomationElement_Release(root);root=NULL;OK(IUIAutomation_ElementFromHandle(uia,hwnd,&root));
        e[0]=find(uia,root,1200);CHECK(e[0]&&pattern(e[0],&p[0])==0&&current(p[0],initial[0])==0);
    }
    /* Rebind peer elements after context publications invalidated old nodes. */
    for(unsigned i=1;i<7;++i) {
        if(p[i])IUIAutomationValuePattern_Release(p[i]);IUIAutomationElement_Release(e[i]);
        e[i]=find(uia,root,1200+i);CHECK(e[i]);p[i]=NULL;if(i!=5)CHECK(pattern(e[i],&p[i])==0);
    }
    observer *o=calloc(1,sizeof(*o));CHECK(o);o->iface.lpVtbl=&observer_vtable;o->refs=1;InitializeSRWLock(&o->lock);
    PROPERTYID properties[]={UIA_ValueValuePropertyId,UIA_ValueIsReadOnlyPropertyId,UIA_IsEnabledPropertyId,UIA_IsValuePatternAvailablePropertyId,UIA_IsPasswordPropertyId};
    OK(IUIAutomation_AddPropertyChangedEventHandlerNativeArray(uia,root,TreeScope_Subtree,NULL,&o->iface,properties,5));
    IUIAutomationElement *guard=find(uia,root,1208);IUIAutomationValuePattern *guard_pattern=NULL;
    CHECK(guard&&pattern(guard,&guard_pattern)==0&&current(guard_pattern,L"public-value")==0);
    LONG guard_password=read_counter(&o->guard_password),guard_available=read_counter(&o->guard_available);
    CHECK(host(VALUE_PROTECT_CUSTOM,0)==0&&wait_counter(&o->guard_password,guard_password)==0&&wait_counter(&o->guard_available,guard_available)==0);
    CHECK(read_counter(&o->last_guard_password)==1&&read_counter(&o->last_guard_available)==0&&read_counter(&o->bad)==0);
    BSTR hidden=NULL;HRESULT guarded_hr=IUIAutomationValuePattern_get_CurrentValue(guard_pattern,&hidden);
    CHECK(FAILED(guarded_hr)||!hidden||!*hidden);SysFreeString(hidden);
    CHECK(FAILED(set_value(guard_pattern,L"forbidden")));
    VARIANT guarded_property;VariantInit(&guarded_property);guarded_hr=IUIAutomationElement_GetCurrentPropertyValueEx(guard,UIA_ValueValuePropertyId,TRUE,&guarded_property);
    if(SUCCEEDED(guarded_hr)){BOOL unsupported=FALSE;OK(IUIAutomation_CheckNotSupported(uia,guarded_property,&unsupported));CHECK(unsupported);}
    CHECK(V_VT(&guarded_property)!=VT_BSTR);VariantClear(&guarded_property);
    guard_password=read_counter(&o->guard_password);guard_available=read_counter(&o->guard_available);
    CHECK(host(VALUE_UNPROTECT_CUSTOM,0)==0&&wait_counter(&o->guard_password,guard_password)==0&&wait_counter(&o->guard_available,guard_available)==0);
    CHECK(read_counter(&o->last_guard_password)==0&&read_counter(&o->last_guard_available)==1&&read_counter(&o->bad)==0&&current(guard_pattern,L"public-value")==0);
    IUIAutomationValuePattern_Release(guard_pattern);IUIAutomationElement_Release(guard);
    const WCHAR *unicode=L"\x6771\x4eac\xd83c\xdf0d";LONG previous=read_counter(&o->values);
    CHECK(host(VALUE_BLOCK_OWNER,0)==0);OK(set_value(p[0],unicode));CHECK(SetEvent(done));
    CHECK(current(p[0],L"10.42.0.1")==0&&host(VALUE_ROUTE,1)==0&&current(p[0],L"10.42.0.1")==0);
    CHECK(FAILED(set_value(p[0],L"busy")));
    CHECK(host(VALUE_FRAME,0)==0&&current(p[0],unicode)==0&&property(uia,e[0],unicode)==0&&value_event(o,previous,unicode)==0);
    CHECK(current(p[1],L"7443")==0&&current(p[2],L"")==0);
    previous=read_counter(&o->values);OK(set_value(p[0],L""));
    CHECK(host(VALUE_FRAME,0)==0&&current(p[0],L"")==0&&property(uia,e[0],L"")==0&&value_event(o,previous,L"")==0);
    previous=read_counter(&o->values);CHECK(host(VALUE_UNDO,0)==0&&current(p[0],unicode)==0&&value_event(o,previous,unicode)==0);
    previous=read_counter(&o->values);CHECK(host(VALUE_REDO,0)==0&&current(p[0],L"")==0&&value_event(o,previous,L"")==0);
    OK(set_value(p[0],L"oom-retry"));CHECK(host(VALUE_OOM_ROUTE,0)==0&&current(p[0],L"")==0);
    previous=read_counter(&o->values);CHECK(host(VALUE_FRAME,0)==0&&current(p[0],L"oom-retry")==0&&value_event(o,previous,L"oom-retry")==0);
    WCHAR invalid[]={0xd800,0};CHECK(FAILED(set_value(p[0],invalid)));
    CHECK(FAILED(set_value(p[0],L"a\nb")));
    WCHAR *large=malloc(65538u*sizeof(WCHAR));CHECK(large);
    for(size_t i=0;i<65537;++i)large[i]=L'1';large[65537]=0;
    CHECK(FAILED(set_value(p[0],large)));
    for(size_t i=0;i<32769;++i)large[i]=0xe9;large[32769]=0;
    CHECK(FAILED(set_value(p[0],large)));
    for(size_t i=0;i<65536;++i)large[i]=L'1';large[65536]=0;
    OK(set_value(p[0],large));free(large);
    previous=read_counter(&o->readonly);CHECK(host(VALUE_READONLY,1)==0&&wait_counter(&o->readonly,previous)==0&&read_counter(&o->last_readonly)==1);
    CHECK(current(p[0],L"oom-retry")==0&&FAILED(set_value(p[0],L"denied")));
    previous=read_counter(&o->readonly);CHECK(host(VALUE_EDITABLE,0)==0&&wait_counter(&o->readonly,previous)==0&&read_counter(&o->last_readonly)==0);
    OK(set_value(p[0],L"disabled-pending"));previous=read_counter(&o->enabled);
    CHECK(host(VALUE_DISABLE,1)==0&&wait_counter(&o->enabled,previous)==0&&read_counter(&o->last_enabled)==0);
    CHECK(current(p[0],L"oom-retry")==0&&FAILED(set_value(p[0],L"denied")));
    CHECK(host(VALUE_ENABLE,0)==0&&current(p[0],L"oom-retry")==0);
    OK(set_value(p[0],L"modal-pending"));CHECK(host(VALUE_MODAL,1)==0&&current(p[0],L"oom-retry")==0);
    CHECK(FAILED(set_value(p[0],L"modal-denied")));
    CHECK(host(VALUE_MODAL_CLOSE,0)==0&&current(p[0],L"oom-retry")==0);
    OK(set_value(p[6],L"multi\nline"));CHECK(host(VALUE_FRAME,0)==0&&current(p[6],L"multi\nline")==0&&current(p[1],L"7443")==0);
    OK(IUIAutomation_RemoveAllEventHandlers(uia));CHECK(read_counter(&o->bad)==0&&read_counter(&o->values)>=5&&read_counter(&o->readonly)>=2&&read_counter(&o->enabled)>=2);release(&o->iface);
    OK(set_value(p[0],L"stale-pending"));CHECK(host(VALUE_RECREATE,1)==0&&closed(e[0],p[0])&&refresh(uia,root,&e[0],&p[0])==0);
    OK(set_value(p[0],L"context-pending"));CHECK(host(VALUE_CONTEXT_ROUNDTRIP,1)==0&&closed(e[0],p[0])&&refresh(uia,root,&e[0],&p[0])==0);
    OK(set_value(p[0],L"destroy-context"));CHECK(host(VALUE_DESTROY_CONTEXT,1)==0&&closed(e[0],p[0])&&refresh(uia,root,&e[0],&p[0])==0);
    OK(set_value(p[0],L"disable-replay"));CHECK(host(VALUE_DISABLE_ROUNDTRIP,1)==0&&current(p[0],L"replacement")==0);
    OK(set_value(p[0],L"detach-replay"));CHECK(host(VALUE_DETACH_ROUNDTRIP,1)==0&&closed(e[0],p[0]));
    IUIAutomationElement_Release(root);root=NULL;OK(IUIAutomation_ElementFromHandle(uia,hwnd,&root));CHECK(refresh(uia,root,&e[0],&p[0])==0);
    OK(set_value(p[0],L"destroy-replay"));CHECK(host(VALUE_DESTROY_WINDOW,1)==0&&closed(e[0],p[0]));
    for(unsigned i=0;i<7;++i){if(p[i])IUIAutomationValuePattern_Release(p[i]);IUIAutomationElement_Release(e[i]);}
    IUIAutomationElement_Release(root);IUIAutomation_Release(uia);CloseHandle(commands);CloseHandle(acks);CloseHandle(done);CoUninitialize();
    puts("External C17 edit values, events, bounds, owner delivery and revocation passed");return 0;
}
