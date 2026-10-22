/* Independent external C17 UI Automation consumer: no NexaUI API or linkage. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include "uia_multi_protocol.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x);return 1;}}while(0)
#define OK(x) do {HRESULT hr_=(x);if(FAILED(hr_)){fprintf(stderr,"%s:%d: %s HRESULT=%08lx\n",__FILE__,__LINE__,#x,(unsigned long)hr_);return 1;}}while(0)
static HANDLE commands,acks;static HWND hwnd;
static int host(unsigned command,multi_reply *reply) {
 DWORD n=0;CHECK(WriteFile(commands,&command,sizeof(command),&n,NULL)&&n==sizeof(command));
 CHECK(ReadFile(acks,reply,sizeof(*reply),&n,NULL)&&n==sizeof(*reply));
 fprintf(stderr,"Multi client command=%u masks=%u/%u counts=%u/%u routed=%u\n",command,reply->list_mask,reply->table_mask,reply->list_count,reply->table_count,reply->routed);return 0;
}
static IUIAutomationElement *find(IUIAutomation *uia,IUIAutomationElement *root,unsigned id) {
 WCHAR name[48];swprintf_s(name,48,L"nexa-%016llx",(unsigned long long)id);
 VARIANT value;VariantInit(&value);V_VT(&value)=VT_BSTR;V_BSTR(&value)=SysAllocString(name);if(!V_BSTR(&value))return NULL;
 IUIAutomationCondition *condition=NULL;IUIAutomationElement *element=NULL;
 HRESULT hr=IUIAutomation_CreatePropertyCondition(uia,UIA_AutomationIdPropertyId,value,&condition);VariantClear(&value);
 if(SUCCEEDED(hr)){IUIAutomationElement_FindFirst(root,TreeScope_Descendants,condition,&element);IUIAutomationCondition_Release(condition);}return element;
}
static unsigned id_of(IUIAutomationElement *e) {
 BSTR id=NULL;unsigned long long n=0;HRESULT hr=IUIAutomationElement_get_CurrentAutomationId(e,&id);
 if(SUCCEEDED(hr)&&id)swscanf_s(id,L"nexa-%llx",&n);SysFreeString(id);return (unsigned)n;
}
static int boolean_property(IUIAutomation *uia,IUIAutomationElement *e,PROPERTYID property,BOOL expected) {
 VARIANT v;VariantInit(&v);OK(IUIAutomationElement_GetCurrentPropertyValueEx(e,property,TRUE,&v));
 BOOL unsupported=TRUE;HRESULT hr=IUIAutomation_CheckNotSupported(uia,v,&unsupported);
 int good=SUCCEEDED(hr)&&!unsupported&&V_VT(&v)==VT_BOOL&&V_BOOL(&v)==(expected?VARIANT_TRUE:VARIANT_FALSE);
 VariantClear(&v);CHECK(good);return 0;
}
static IUIAutomationSelectionItemPattern *item(IUIAutomation *uia,IUIAutomationElement *root,unsigned id) {
 IUIAutomationElement *e=find(uia,root,id);IUIAutomationSelectionItemPattern *p=NULL;
 if(e){IUIAutomationElement_GetCurrentPatternAs(e,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&p);IUIAutomationElement_Release(e);}return p;
}
static HRESULT act(IUIAutomation *uia,IUIAutomationElement *root,unsigned id,unsigned action) {
 IUIAutomationSelectionItemPattern *p=item(uia,root,id);if(!p)return E_NOINTERFACE;
 HRESULT hr=action==0?IUIAutomationSelectionItemPattern_Select(p):action==1?IUIAutomationSelectionItemPattern_AddToSelection(p):IUIAutomationSelectionItemPattern_RemoveFromSelection(p);
 IUIAutomationSelectionItemPattern_Release(p);return hr;
}
static int selection(IUIAutomation *uia,IUIAutomationElement *root,unsigned owner,unsigned first,uint64_t mask) {
 IUIAutomationElement *e=find(uia,root,owner);CHECK(e);
 IUIAutomationSelectionPattern *p=NULL;OK(IUIAutomationElement_GetCurrentPatternAs(e,UIA_SelectionPatternId,&IID_IUIAutomationSelectionPattern,(void **)&p));CHECK(p);
 BOOL multiple=FALSE;OK(IUIAutomationSelectionPattern_get_CurrentCanSelectMultiple(p,&multiple));CHECK(multiple);
 CHECK(boolean_property(uia,e,UIA_SelectionCanSelectMultiplePropertyId,TRUE)==0);
 IUIAutomationElementArray *a=NULL;OK(IUIAutomationSelectionPattern_GetCurrentSelection(p,&a));CHECK(a);
 int n=0;OK(IUIAutomationElementArray_get_Length(a,&n));uint64_t actual=0;
 for(int i=0;i<n;++i) {
  IUIAutomationElement *row=NULL;OK(IUIAutomationElementArray_GetElement(a,i,&row));CHECK(row);
  unsigned id=id_of(row);CHECK(id>=first&&id-first<64);
  uint64_t bit=UINT64_C(1)<<(id-first);CHECK(!(actual&bit));actual|=bit;
  CHECK(boolean_property(uia,row,UIA_SelectionItemIsSelectedPropertyId,TRUE)==0);
  IUIAutomationSelectionItemPattern *ip=NULL;OK(IUIAutomationElement_GetCurrentPatternAs(row,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&ip));CHECK(ip);
  IUIAutomationElement *container=NULL;OK(IUIAutomationSelectionItemPattern_get_CurrentSelectionContainer(ip,&container));CHECK(container&&id_of(container)==owner);
  IUIAutomationElement_Release(container);IUIAutomationSelectionItemPattern_Release(ip);IUIAutomationElement_Release(row);
 }
 CHECK(actual==mask);IUIAutomationElementArray_Release(a);IUIAutomationSelectionPattern_Release(p);IUIAutomationElement_Release(e);return 0;
}
typedef struct observer {
 IUIAutomationEventHandler iface;LONG refs;volatile LONG selected,added,removed,invalidated,bad,last_selected,last_added,last_removed;
} observer;
static ULONG STDMETHODCALLTYPE add(IUIAutomationEventHandler *self){return (ULONG)InterlockedIncrement(&((observer *)self)->refs);}
static ULONG STDMETHODCALLTYPE release(IUIAutomationEventHandler *self){LONG n=InterlockedDecrement(&((observer *)self)->refs);if(!n)free(self);return (ULONG)n;}
static HRESULT STDMETHODCALLTYPE query(IUIAutomationEventHandler *self,REFIID iid,void **out) {
 if(!out)return E_POINTER;*out=NULL;if(!IsEqualIID(iid,&IID_IUnknown)&&!IsEqualIID(iid,&IID_IUIAutomationEventHandler))return E_NOINTERFACE;
 *out=self;add(self);return S_OK;
}
static HRESULT STDMETHODCALLTYPE event(IUIAutomationEventHandler *self,IUIAutomationElement *sender,EVENTID event_id) {
 observer *o=(observer *)self;BSTR value=NULL;unsigned long long id=0;
 HRESULT hr=IUIAutomationElement_get_CachedAutomationId(sender,&value);
 if(SUCCEEDED(hr)&&value)swscanf_s(value,L"nexa-%llx",&id);SysFreeString(value);
 if(FAILED(hr)||!id){InterlockedIncrement(&o->bad);return S_OK;}
 if(event_id==UIA_Selection_InvalidatedEventId){if(id!=10&&id!=20)InterlockedIncrement(&o->bad);InterlockedIncrement(&o->invalidated);}
 else {
  if(!((id>=10001&&id<=10064)||(id>=20001&&id<=20003)))InterlockedIncrement(&o->bad);
  if(event_id==UIA_SelectionItem_ElementSelectedEventId){InterlockedExchange(&o->last_selected,(LONG)id);InterlockedIncrement(&o->selected);}
  if(event_id==UIA_SelectionItem_ElementAddedToSelectionEventId){InterlockedExchange(&o->last_added,(LONG)id);InterlockedIncrement(&o->added);}
  if(event_id==UIA_SelectionItem_ElementRemovedFromSelectionEventId){InterlockedExchange(&o->last_removed,(LONG)id);InterlockedIncrement(&o->removed);}
 }
 return S_OK;
}
static IUIAutomationEventHandlerVtbl event_vtable={query,add,release,event};
static int wait_event(observer *o,volatile LONG *counter,LONG minimum,volatile LONG *last,unsigned id) {
 ULONGLONG until=GetTickCount64()+3000;
 while(InterlockedCompareExchange(counter,0,0)<minimum&&GetTickCount64()<until)Sleep(10);
 fprintf(stderr,"Multi events selected=%ld added=%ld removed=%ld invalidated=%ld bad=%ld\n",o->selected,o->added,o->removed,o->invalidated,o->bad);
 CHECK(!o->bad&&InterlockedCompareExchange(counter,0,0)>=minimum&&(!last||(unsigned)InterlockedCompareExchange(last,0,0)==id));return 0;
}
static int refresh(IUIAutomation *uia,IUIAutomationElement **root) {
 if(*root)IUIAutomationElement_Release(*root);*root=NULL;OK(IUIAutomation_ElementFromHandle(uia,hwnd,root));CHECK(*root);return 0;
}
static int stale(IUIAutomationElement *old,IUIAutomationSelectionItemPattern *pattern) {
 BSTR name=NULL;HRESULT hr=IUIAutomationElement_get_CurrentName(old,&name);SysFreeString(name);
 CHECK(FAILED(hr));CHECK(FAILED(IUIAutomationSelectionItemPattern_Select(pattern)));return 0;
}
static int run(void) {
 IUIAutomation *uia=NULL;OK(CoCreateInstance(&CLSID_CUIAutomation,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAutomation,(void **)&uia));
 IUIAutomationElement *root=NULL;CHECK(refresh(uia,&root)==0);
 CHECK(selection(uia,root,10,10001,0)==0 && selection(uia,root,20,20001,0)==0);
 IUIAutomationElement *visible=find(uia,root,10001),*hidden=find(uia,root,10002);CHECK(visible&&hidden);
 CHECK(boolean_property(uia,visible,UIA_IsOffscreenPropertyId,FALSE)==0&&boolean_property(uia,hidden,UIA_IsOffscreenPropertyId,TRUE)==0);
 IUIAutomationElement_Release(visible);IUIAutomationElement_Release(hidden);
 observer *o=calloc(1,sizeof(*o));CHECK(o);o->iface.lpVtbl=&event_vtable;o->refs=1;
 IUIAutomationCacheRequest *cache=NULL;OK(IUIAutomation_CreateCacheRequest(uia,&cache));OK(IUIAutomationCacheRequest_AddProperty(cache,UIA_AutomationIdPropertyId));
 EVENTID events[]={UIA_SelectionItem_ElementSelectedEventId,UIA_SelectionItem_ElementAddedToSelectionEventId,UIA_SelectionItem_ElementRemovedFromSelectionEventId,UIA_Selection_InvalidatedEventId};
 for(size_t i=0;i<4;++i)OK(IUIAutomation_AddAutomationEventHandler(uia,events[i],root,TreeScope_Subtree,cache,&o->iface));
 IUIAutomationCacheRequest_Release(cache);multi_reply r;
 OK(act(uia,root,10001,0));CHECK(host(MULTI_CHECK,&r)==0&&r.list_mask==0&&r.list_count==0);
 Sleep(50);CHECK(o->selected==0&&selection(uia,root,10,10001,0)==0);
 CHECK(host(MULTI_SAME_PUBLISH,&r)==0&&r.list_count==0);
 CHECK(FAILED(act(uia,root,10002,1))); /* Busy until a different completed frame. */
 CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==1&&r.table_mask==0&&r.active==10003&&r.focus==10001);
 CHECK(selection(uia,root,10,10001,1)==0&&wait_event(o,&o->selected,1,&o->last_selected,10001)==0);
 LONG added=o->added;OK(act(uia,root,10002,1));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==3&&r.focus==10001);
 CHECK(selection(uia,root,10,10001,3)==0&&wait_event(o,&o->added,added+1,&o->last_added,10002)==0);
 LONG chosen=o->selected,removed=o->removed;OK(act(uia,root,10001,0));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==1);
 CHECK(selection(uia,root,10,10001,1)==0&&wait_event(o,&o->selected,chosen+1,&o->last_selected,10001)==0);
 CHECK(wait_event(o,&o->removed,removed+1,&o->last_removed,10002)==0);
 OK(act(uia,root,10002,1));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==3);
 chosen=o->selected;removed=o->removed;
 OK(act(uia,root,10001,2));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==2);
 CHECK(selection(uia,root,10,10001,2)==0&&wait_event(o,&o->selected,chosen+1,&o->last_selected,10002)==0);
 CHECK(wait_event(o,&o->removed,removed+1,&o->last_removed,10001)==0);
 OK(act(uia,root,10002,2));CHECK(host(MULTI_STEP,&r)==0&&r.list_count==0&&selection(uia,root,10,10001,0)==0);
 OK(act(uia,root,20001,0));CHECK(host(MULTI_STEP,&r)==0&&r.table_mask==1&&r.list_mask==0);
 OK(act(uia,root,20002,1));CHECK(host(MULTI_STEP,&r)==0&&r.table_mask==3&&selection(uia,root,20,20001,3)==0);
 OK(act(uia,root,20001,2));CHECK(host(MULTI_STEP,&r)==0&&r.table_mask==2&&selection(uia,root,20,20001,2)==0);
 IUIAutomationElement *header=find(uia,root,21);CHECK(header);IUIAutomationSelectionItemPattern *header_item=NULL;
 IUIAutomationElement_GetCurrentPatternAs(header,UIA_SelectionItemPatternId,&IID_IUIAutomationSelectionItemPattern,(void **)&header_item);CHECK(!header_item);IUIAutomationElement_Release(header);
 CHECK(host(MULTI_REQUIRED,&r)==0&&r.list_mask==1);
 IUIAutomationElement *owner=find(uia,root,10);CHECK(owner&&boolean_property(uia,owner,UIA_SelectionIsSelectionRequiredPropertyId,TRUE)==0);IUIAutomationElement_Release(owner);
 CHECK(FAILED(act(uia,root,10001,2)));
 OK(act(uia,root,10002,1));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==3);
 OK(act(uia,root,10001,2));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==2);
 CHECK(host(MULTI_RESET,&r)==0&&r.list_mask==1);
 CHECK(host(MULTI_NATIVE_OOM,&r)==0&&r.list_count==64&&r.detail==1);
 CHECK(selection(uia,root,10,10001,1)==0); /* Original publication survives OOM. */
 CHECK(host(MULTI_PUBLISH_RETRY,&r)==0&&selection(uia,root,10,10001,UINT64_MAX)==0);
 OK(IUIAutomation_RemoveAllEventHandlers(uia));CHECK(!o->bad);release(&o->iface);
 CHECK(host(MULTI_RESET,&r)==0&&r.list_mask==1);
 CHECK(host(MULTI_FILL_CORE,&r)==0);
 OK(act(uia,root,10002,1));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==1&&r.routed==0);
 CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==3&&r.routed>=1&&selection(uia,root,10,10001,3)==0);
 CHECK(host(MULTI_RESET,&r)==0);OK(act(uia,root,10002,0));
 CHECK(host(MULTI_OOM_FRAME,&r)==0&&r.list_mask==2&&r.routed>=1);
 CHECK(host(MULTI_RESET,&r)==0);
 OK(act(uia,root,10002,0));CHECK(host(MULTI_REVISION_CHANGE,&r)==0&&r.list_mask==1);
 CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==1&&r.routed==0);
 OK(act(uia,root,10002,0));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==2);
 CHECK(host(MULTI_RESET,&r)==0);
 unsigned mutations[]={MULTI_DISABLE_ROUNDTRIP,MULTI_CONTEXT_ROUNDTRIP,MULTI_DESTROY_CONTEXT,
  MULTI_DETACH_ROUNDTRIP,MULTI_REPARENT,MULTI_REUSE_UNPUBLISHED,MULTI_REPLACE_MODEL};
 for(unsigned forwarded=0;forwarded<2;++forwarded)for(size_t i=0;i<sizeof(mutations)/sizeof(mutations[0]);++i) {
  CHECK(host(MULTI_RESET,&r)==0&&refresh(uia,&root)==0);
  IUIAutomationElement *old=find(uia,root,10002);IUIAutomationSelectionItemPattern *p=item(uia,root,10002);CHECK(old&&p);
  OK(IUIAutomationSelectionItemPattern_Select(p));
  if(forwarded)CHECK(host(MULTI_ROUTE,&r)==0&&r.list_mask==1&&r.routed>=1);
  else CHECK(host(MULTI_CHECK,&r)==0&&r.list_mask==1);
  CHECK(host(mutations[i],&r)==0&&r.list_mask==(mutations[i]==MULTI_REPLACE_MODEL?0u:1u));
  if(mutations[i]!=MULTI_DISABLE_ROUNDTRIP)CHECK(stale(old,p)==0);
  else {OK(IUIAutomationSelectionItemPattern_AddToSelection(p));CHECK(host(MULTI_STEP,&r)==0&&r.list_mask==3);}
  IUIAutomationSelectionItemPattern_Release(p);IUIAutomationElement_Release(old);
  CHECK(refresh(uia,&root)==0);
 }
 CHECK(host(MULTI_RESET,&r)==0&&refresh(uia,&root)==0);
 OK(act(uia,root,10002,0));CHECK(host(MULTI_ROUTE,&r)==0&&r.routed>=1);
 CHECK(host(MULTI_MODAL,&r)==0&&r.list_mask==1);IUIAutomationElement *row=find(uia,root,10002);CHECK(row);
 CHECK(boolean_property(uia,row,UIA_IsEnabledPropertyId,FALSE)==0&&FAILED(act(uia,root,10002,0)));IUIAutomationElement_Release(row);
 CHECK(host(MULTI_MODAL_CLOSE,&r)==0);
 OK(act(uia,root,10002,0));CHECK(host(MULTI_ROUTE,&r)==0&&r.routed>=1);
 CHECK(host(MULTI_DISABLE_OWNER,&r)==0&&r.list_mask==1&&FAILED(act(uia,root,10002,0)));
 CHECK(host(MULTI_ENABLE_OWNER,&r)==0);
 for(unsigned command=MULTI_CALLOUT_DISABLE_ALLOC;command<=MULTI_CALLOUT_DETACH_FREE;++command) {
  CHECK(host(MULTI_RESET,&r)==0&&refresh(uia,&root)==0);
  OK(act(uia,root,10002,0));CHECK(host(command,&r)==0&&r.routed>=1&&r.list_mask==1);
  CHECK(refresh(uia,&root)==0&&selection(uia,root,10,10001,1)==0);
 }
 CHECK(host(MULTI_RESET,&r)==0&&refresh(uia,&root)==0);
 IUIAutomationSelectionItemPattern *retained=item(uia,root,10002);CHECK(retained);
 OK(IUIAutomationSelectionItemPattern_Select(retained));CHECK(host(MULTI_ROUTE,&r)==0&&r.routed>=1);
 CHECK(host(MULTI_DESTROY_WINDOW,&r)==0&&r.list_mask==1&&FAILED(IUIAutomationSelectionItemPattern_Select(retained)));
 IUIAutomationSelectionItemPattern_Release(retained);IUIAutomationElement_Release(root);IUIAutomation_Release(uia);
 puts("External C17 multi-selection sets, events, offscreen, model reflection, bounds and lifetime passed");return 0;
}
int wmain(int argc,WCHAR **argv) {
 if(argc!=4)return 2;hwnd=(HWND)(UINT_PTR)_wcstoui64(argv[1],NULL,10);
 commands=(HANDLE)(UINT_PTR)_wcstoui64(argv[2],NULL,10);acks=(HANDLE)(UINT_PTR)_wcstoui64(argv[3],NULL,10);
 if(FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))return 2;
 int result=run();CoUninitialize();return result;
}
