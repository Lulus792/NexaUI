#define NOMINMAX
#include "window_internal.h"
#include "uia_multi_protocol.h"
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x);return 1;}}while(0)
typedef struct allocation {
 size_t live,failed;int fail,callout,ready,fired;nx_window *window;HWND hwnd;
} allocation;
static void callout(allocation *a) {
 int command=a->callout;a->callout=0;++a->fired;
 fprintf(stderr,"Multi allocator callout %d\n",command);
 if(command==MULTI_CALLOUT_DETACH_ALLOC||command==MULTI_CALLOUT_DETACH_FREE) {
  if(nx_window_publish_semantics(a->window,NULL)!=NX_OK)abort();
 } else {EnableWindow(a->hwnd,FALSE);EnableWindow(a->hwnd,TRUE);}
}
static void *allocate(void *user,size_t bytes) {
 allocation *a=user;
 if(a->fail){a->fail=0;++a->failed;return NULL;}
 if(a->callout) {
  if(a->callout==MULTI_CALLOUT_DISABLE_FREE||a->callout==MULTI_CALLOUT_DETACH_FREE)a->ready=1;
  else callout(a);
 }
 void *p=malloc(bytes);if(p)++a->live;return p;
}
static void deallocate(void *user,void *p,size_t bytes) {
 allocation *a=user;(void)bytes;
 if(p){if(a->ready&&a->callout){a->ready=0;callout(a);}--a->live;free(p);}
}
typedef struct scene {
 nx_context *ctx;nx_selection_model *list,*table;nx_context_desc cd;
 int required,disabled,modal,alternate,focus_once;size_t extra;
} scene;
static int catalog(nx_selection_model *m,unsigned first,size_t count) {
 nx_selection_item items[128];CHECK(count<=128);
 for(size_t i=0;i<count;++i)items[i]=(nx_selection_item){first+i,0};
 CHECK(nx_selection_model_set_items(m,items,count)==NX_OK);return 0;
}
static int create_model(nx_selection_model **m,unsigned first) {
 CHECK(nx_selection_model_create(128,NULL,m)==NX_OK && catalog(*m,first,3)==0);
 CHECK(nx_selection_model_set_active(*m,first+2)==NX_OK);return 0;
}
static nx_semantic_desc node(uint64_t id,uint64_t parent,nx_semantic_role role,const char *name,nx_rect bounds) {
 nx_semantic_desc d={0};d.id=id;d.parent_id=parent;d.role=role;
 d.name=(nx_utf8_span){name,strlen(name)};d.bounds=bounds;return d;
}
static int build(scene *s) {
 CHECK(nx_begin_frame(s->ctx,600,360,0)==NX_OK);
 nx_semantic_desc d=node(1,0,NX_ROLE_WINDOW,"Multi selection",(nx_rect){0,0,600,360});
 CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
 d=node(10,1,NX_ROLE_LIST,"Files",(nx_rect){0,0,220,250});d.state=s->disabled?NX_SEM_DISABLED:0;
 CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(s->ctx,10,s->list,s->required)==NX_OK);
 uint64_t parent=s->alternate?101:100;
 d=node(parent,10,NX_ROLE_PANEL,"Rows",(nx_rect){0,0,220,250});CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
 nx_selection_view v;CHECK(nx_selection_model_get(s->list,&v)==NX_OK);
 for(size_t i=0;i<v.item_count;++i) {
  d=node(v.items[i].id,parent,NX_ROLE_LIST_ITEM,"File",(nx_rect){10,30,200,24});
  if(i==0)d.actions=NX_ACTION_FOCUS;
  CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
  if(i==0){nx_interaction_state state;CHECK(nx_interact_region(s->ctx,d.id,d.bounds,NX_REGION_FOCUSABLE,&state)==NX_OK);}
  CHECK(nx_selection_bind_item(s->ctx,10,d.id,i!=0)==NX_OK);
 }
 nx_table_column_desc column={0};column.id=21;column.name=(nx_utf8_span){"Name",4};column.width=220;
 column.minimum_width=100;column.maximum_width=240;column.sortable=1;
 nx_table_desc table={0};table.id=20;table.parent_id=1;table.name=(nx_utf8_span){"Transfers",9};
 table.bounds=(nx_rect){250,0,220,200};table.header_height=24;table.columns=&column;table.column_count=1;
 static nx_table_sort sort={0};
 CHECK(nx_table_headers(s->ctx,&table,&sort)==NX_OK && nx_selection_bind_owner(s->ctx,20,s->table,0)==NX_OK);
 for(unsigned i=0;i<3;++i) {
  nx_container_desc row={0};row.id=20001+i;row.parent_id=20;row.name=(nx_utf8_span){"Transfer",8};
  row.bounds=(nx_rect){250,30+(float)i*24,220,24};
  CHECK(nx_table_row(s->ctx,&row)==NX_OK && nx_selection_bind_item(s->ctx,20,row.id,i!=0)==NX_OK);
  nx_container_desc cell=row;cell.id=21001+i;cell.parent_id=row.id;cell.name=(nx_utf8_span){"Cell",4};
  CHECK(nx_table_cell(s->ctx,&cell)==NX_OK);
 }
 if(s->modal) {
  nx_overlay_desc modal={0};modal.panel.id=500;modal.backdrop_id=501;
  modal.panel.bounds=(nx_rect){300,220,200,100};modal.panel.name=(nx_utf8_span){"Modal",5};
  int dismissed;CHECK(nx_modal_begin(s->ctx,&modal,&dismissed)==NX_OK && nx_overlay_end(s->ctx)==NX_OK);
 }
 if(s->focus_once){CHECK(nx_focus_set(s->ctx,10001)==NX_OK);s->focus_once=0;}
 return 0;
}
static int frame(scene *s){CHECK(build(s)==0 && nx_end_frame(s->ctx)==NX_OK);return 0;}
static int read_model(nx_selection_model *m,unsigned first,unsigned *mask,unsigned *count) {
 nx_selection_view v;CHECK(nx_selection_model_get(m,&v)==NX_OK);*mask=0;*count=(unsigned)v.selected_count;
 for(size_t i=0;i<v.selected_count;++i)if(v.selected_ids[i]>=first&&v.selected_ids[i]<first+3)*mask|=1u<<(unsigned)(v.selected_ids[i]-first);
 return 0;
}
static int reply_model(scene *s,multi_reply *r) {
 CHECK(read_model(s->list,10001,&r->list_mask,&r->list_count)==0);
 CHECK(read_model(s->table,20001,&r->table_mask,&r->table_count)==0);
 nx_selection_view v;CHECK(nx_selection_model_get(s->list,&v)==NX_OK);
 r->active=(unsigned)v.active_id;r->focus=(unsigned)nx_focus_id(s->ctx);return 0;
}
static int route(nx_window *window,scene *s,size_t *count) {
 CHECK(nx_window_route_events(window,s->ctx,count)==NX_OK);return 0;
}
static nx_result extra(scene *s) {
 char *text=malloc(s->extra);if(!text)return NX_ERROR_MEMORY;memset(text,'X',s->extra);
 nx_semantic_desc d=node(3000,1,NX_ROLE_TEXT,"",(nx_rect){0});d.name=(nx_utf8_span){text,s->extra};
 nx_result result=nx_semantics_add(s->ctx,&d,NULL);free(text);return result;
}
static int reset(scene *s) {
 s->required=s->disabled=s->alternate=0;
 if(s->modal){CHECK(nx_overlay_close(s->ctx)==NX_OK);s->modal=0;}
 uint64_t id=10001;CHECK(catalog(s->list,10001,3)==0);
 CHECK(nx_selection_model_set_selected(s->list,&id,1)==NX_OK && nx_selection_model_set_selected(s->table,NULL,0)==NX_OK);
 CHECK(nx_selection_model_set_active(s->list,10003)==NX_OK);s->focus_once=1;return 0;
}
static int run(const WCHAR *client) {
 allocation a={0};nx_allocator allocator={&a,allocate,deallocate};scene s={0};s.extra=8192;s.focus_once=1;s.cd.allocator=allocator;
 CHECK(create_model(&s.list,10001)==0 && create_model(&s.table,20001)==0 && nx_context_create(&s.cd,&s.ctx)==NX_OK);
 nx_window_desc wd={0};wd.width=600;wd.height=360;wd.title_utf8="NexaUI native multi selection";wd.flags=NX_WINDOW_HIDDEN;wd.allocator=allocator;
 nx_window *window=NULL;CHECK(nx_window_create(&wd,&window)==NX_OK);
 HWND hwnd=nx_window_native_hwnd(window);CHECK(hwnd&&!IsWindowVisible(hwnd));a.hwnd=hwnd;a.window=window;
 ShowWindow(hwnd,SW_SHOWNOACTIVATE);
 CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
 SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};HANDLE commands,child_commands,acks,child_acks,logs,child_logs;
 CHECK(CreatePipe(&commands,&child_commands,&sa,0)&&CreatePipe(&child_acks,&acks,&sa,0)&&CreatePipe(&logs,&child_logs,&sa,0));
 CHECK(SetHandleInformation(commands,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(acks,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(logs,HANDLE_FLAG_INHERIT,0));
 HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);CHECK(input!=INVALID_HANDLE_VALUE);
 WCHAR commandline[4096];CHECK(swprintf_s(commandline,4096,L"\"%ls\" %llu %llu %llu",client,
  (unsigned long long)(UINT_PTR)hwnd,(unsigned long long)(UINT_PTR)child_commands,(unsigned long long)(UINT_PTR)child_acks)>0);
 STARTUPINFOW startup={0};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
 startup.hStdInput=input;startup.hStdOutput=child_logs;startup.hStdError=child_logs;PROCESS_INFORMATION process={0};
 CHECK(CreateProcessW(client,commandline,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process));
 CloseHandle(process.hThread);CloseHandle(input);CloseHandle(child_commands);CloseHandle(child_acks);CloseHandle(child_logs);
 ULONGLONG end=GetTickCount64()+90000;int destroyed=0;
 while(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) {
  if(GetTickCount64()>=end){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);fputs("multi client deadline\n",stderr);return 1;}
  if(window)CHECK(nx_window_poll(window,0)==NX_OK);
  else {MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
  DWORD available=0,n=0;
  if(PeekNamedPipe(logs,NULL,0,NULL,&available,NULL)&&available){char buffer[4096];CHECK(ReadFile(logs,buffer,available<sizeof(buffer)?available:(DWORD)sizeof(buffer),&n,NULL));fwrite(buffer,1,n,stderr);}
  available=0;if(!PeekNamedPipe(commands,NULL,0,NULL,&available,NULL)){CHECK(GetLastError()==ERROR_BROKEN_PIPE);available=0;}
  if(available>=sizeof(unsigned)) {
   unsigned command;multi_reply reply={0};size_t routed=0;CHECK(ReadFile(commands,&command,sizeof(command),&n,NULL)&&n==sizeof(command));
   if(command==MULTI_STEP||command==MULTI_ROUTE) {
    CHECK(route(window,&s,&routed)==0);
    if(command==MULTI_STEP)CHECK(frame(&s)==0&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
   } else if(command==MULTI_CHECK) {
   } else if(command==MULTI_SAME_PUBLISH||command==MULTI_PUBLISH_RETRY) {
    CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
   } else if(command==MULTI_FILL_CORE) {
    for(unsigned i=0;i<64;++i)CHECK(nx_semantics_request_action(s.ctx,10001,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(nx_semantics_request_action(s.ctx,10001,NX_ACTION_ADD_SELECTION)==NX_ERROR_LIMIT);
   } else if(command==MULTI_NATIVE_OOM) {
    CHECK(catalog(s.list,10001,64)==0);uint64_t ids[64];for(unsigned i=0;i<64;++i)ids[i]=10001+i;
    CHECK(nx_selection_model_set_selected(s.list,ids,64)==NX_OK&&frame(&s)==0);
    a.fail=1;a.failed=0;CHECK(nx_window_publish_semantics(window,s.ctx)==NX_ERROR_MEMORY&&a.failed==1&&!a.fail);
    reply.detail=1;
   } else if(command==MULTI_OOM_FRAME||command>=MULTI_CALLOUT_DISABLE_ALLOC&&command<=MULTI_CALLOUT_DETACH_FREE) {
    CHECK(route(window,&s,&routed)==0);multi_reply before={0};CHECK(reply_model(&s,&before)==0&&build(&s)==0);
    if(command==MULTI_OOM_FRAME) {
     a.fail=1;a.failed=0;CHECK(extra(&s)==NX_ERROR_MEMORY&&a.failed==1&&!a.fail);
     multi_reply failed={0};CHECK(reply_model(&s,&failed)==0&&failed.list_mask==before.list_mask&&failed.list_count==before.list_count);
     fprintf(stderr,"Multi OOM before end model unchanged=1\n");CHECK(extra(&s)==NX_OK);
    } else {
     s.extra*=2;a.callout=(int)command;a.ready=a.fired=0;CHECK(extra(&s)==NX_OK&&a.fired==1&&!a.callout);
    }
    CHECK(nx_end_frame(s.ctx)==NX_OK);
    multi_reply after={0};CHECK(reply_model(&s,&after)==0);
    if(command!=MULTI_OOM_FRAME)CHECK(after.list_mask==before.list_mask&&after.list_count==before.list_count);
    fprintf(stderr,"Multi allocator outcome command=%u routed=%zu unchanged=%d fired=%d\n",command,routed,after.list_mask==before.list_mask,a.fired);
    CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
   } else if(command==MULTI_DESTROY_WINDOW) {
    nx_window_destroy(window);window=NULL;destroyed=1;CHECK(frame(&s)==0);
   } else {
    if(command==MULTI_RESET)CHECK(reset(&s)==0);
    else if(command==MULTI_REQUIRED){uint64_t id=10001;CHECK(nx_selection_model_set_selected(s.list,&id,1)==NX_OK);s.required=1;}
    else if(command==MULTI_OPTIONAL)s.required=0;
    else if(command==MULTI_REVISION_CHANGE){uint64_t id=10001;CHECK(nx_selection_model_set_selected(s.list,&id,1)==NX_OK);}
    else if(command==MULTI_DISABLE_ROUNDTRIP){EnableWindow(hwnd,FALSE);EnableWindow(hwnd,TRUE);}
    else if(command==MULTI_DETACH_ROUNDTRIP)CHECK(nx_window_publish_semantics(window,NULL)==NX_OK&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
    else if(command==MULTI_CONTEXT_ROUNDTRIP) {
     nx_context *original=s.ctx;CHECK(nx_context_create(&s.cd,&s.ctx)==NX_OK&&frame(&s)==0&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
     nx_context *other=s.ctx;s.ctx=original;CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);nx_context_destroy(other);
    } else if(command==MULTI_REPLACE_MODEL){nx_selection_model_destroy(s.list);CHECK(create_model(&s.list,10001)==0);}
    else if(command==MULTI_REUSE_UNPUBLISHED) {
     nx_selection_item items[]={{10001,0},{10003,0}};CHECK(nx_selection_model_set_items(s.list,items,2)==NX_OK&&catalog(s.list,10001,3)==0);
    } else if(command==MULTI_REPARENT)s.alternate=!s.alternate;
    else if(command==MULTI_MODAL)s.modal=1;
    else if(command==MULTI_MODAL_CLOSE){CHECK(nx_overlay_close(s.ctx)==NX_OK);s.modal=0;}
    else if(command==MULTI_DISABLE_OWNER)s.disabled=1;
    else if(command==MULTI_ENABLE_OWNER)s.disabled=0;
    else if(command==MULTI_DESTROY_CONTEXT){nx_context_destroy(s.ctx);s.ctx=NULL;s.focus_once=1;CHECK(nx_context_create(&s.cd,&s.ctx)==NX_OK);}
    else {fprintf(stderr,"unknown multi command %u\n",command);return 1;}
    CHECK(frame(&s)==0&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
   }
   reply.routed=(unsigned)routed;CHECK(reply_model(&s,&reply)==0);
   fprintf(stderr,"Multi host command=%u list=%u/%u table=%u/%u active=%u focus=%u routed=%u detail=%u\n",command,reply.list_mask,reply.list_count,reply.table_mask,reply.table_count,reply.active,reply.focus,reply.routed,reply.detail);
   CHECK(WriteFile(acks,&reply,sizeof(reply),&n,NULL)&&n==sizeof(reply));
  }
  CHECK(MsgWaitForMultipleObjectsEx(1,&process.hProcess,1,QS_ALLINPUT,MWMO_INPUTAVAILABLE)!=WAIT_FAILED);
 }
 DWORD available=0,n=0;while(PeekNamedPipe(logs,NULL,0,NULL,&available,NULL)&&available){char buffer[4096];CHECK(ReadFile(logs,buffer,available<sizeof(buffer)?available:(DWORD)sizeof(buffer),&n,NULL));fwrite(buffer,1,n,stderr);}
 DWORD status=1;CHECK(GetExitCodeProcess(process.hProcess,&status));CloseHandle(process.hProcess);CloseHandle(commands);CloseHandle(acks);CloseHandle(logs);
 CHECK(destroyed&&status==0);nx_context_destroy(s.ctx);nx_selection_model_destroy(s.list);nx_selection_model_destroy(s.table);
 CHECK(a.live==0);puts("native multi selection host and lifetime guards passed");return 0;
}
int wmain(int argc,WCHAR **argv) {
 if(argc<2||argc>3)return 2;int owned=argc==3&&!wcscmp(argv[2],L"owned");HRESULT hr=S_OK;
 if(!owned){hr=CoInitializeEx(NULL,argc==3?COINIT_MULTITHREADED:COINIT_APARTMENTTHREADED);if(FAILED(hr))return 2;}
 int result=run(argv[1]);if(!owned)CoUninitialize();return result;
}
