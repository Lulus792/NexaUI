#define NOMINMAX
#include "window_internal.h"
#include "uia_ids.h"
#include "uia_value_protocol.h"
#include <objbase.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x);return 1;}}while(0)
#include "fixture_font.h"

typedef struct allocation {
    size_t live,fail_size,failed_size;
    unsigned callout,fired;int reserve_ready;
    HWND hwnd;nx_window *window;
} allocation;
static void allocator_callout(allocation *a,size_t n) {
    unsigned command=a->callout;a->callout=0;++a->fired;
    fprintf(stderr,"Allocator callout enter command=%u bytes=%zu\n",command,n);fflush(stderr);
    if(command==VALUE_ROUTE_DETACH_ALLOC||command==VALUE_RESERVE_DETACH_ALLOC||command==VALUE_RESERVE_DETACH_FREE) {
        if(nx_window_publish_semantics(a->window,NULL)!=NX_OK)abort();
    } else {EnableWindow(a->hwnd,FALSE);EnableWindow(a->hwnd,TRUE);}
    fprintf(stderr,"Allocator callout exit command=%u\n",command);fflush(stderr);
}
static void *allocate(void *u,size_t n) {
    allocation *a=u;if(a->fail_size==n){a->failed_size=n;a->fail_size=0;return NULL;}
    if(a->callout==VALUE_ROUTE_DISABLE_ALLOC||a->callout==VALUE_ROUTE_DETACH_ALLOC) {
        if(n==10)allocator_callout(a,n);
    } else if(a->callout&&n>=4097) {
        if(a->callout==VALUE_RESERVE_DISABLE_FREE||a->callout==VALUE_RESERVE_DETACH_FREE)a->reserve_ready=1;
        else allocator_callout(a,n);
    }
    void *p=malloc(n);if(p)++a->live;return p;
}
static void deallocate(void *u,void *p,size_t n) {
    allocation *a=u;
    if(p){if(a->callout&&a->reserve_ready){a->reserve_ready=0;allocator_callout(a,n);}--a->live;free(p);}
}
typedef struct scene {
    nx_context *ctx;nx_font_collection *fonts;nx_text_field *fields[7];
    int disabled,modal,value_protected;
} scene;
static int matches(nx_text_field *field,const char *expected) {
    nx_utf8_span s;return nx_text_field_text(field,&s)==NX_OK && s.length==strlen(expected) &&
        (!s.length||memcmp(s.data,expected,s.length)==0);
}
static int frame(scene *s) {
    CHECK(nx_begin_frame(s->ctx,480,320,0)==NX_OK);
    nx_semantic_desc root={0};root.id=1000;root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"Edit values",11};root.bounds=(nx_rect){0,0,480,320};
    CHECK(nx_semantics_add(s->ctx,&root,NULL)==NX_OK);
    if(s->modal) {
        nx_overlay_desc modal={0};modal.panel.id=1400;modal.backdrop_id=1401;
        modal.panel.name=(nx_utf8_span){"Modal",5};modal.panel.bounds=(nx_rect){300,20,150,120};
        int dismissed;CHECK(nx_modal_begin(s->ctx,&modal,&dismissed)==NX_OK && nx_overlay_end(s->ctx)==NX_OK);
    }
    for(unsigned i=0;i<7;++i) {
        nx_text_field_desc d={0};d.id=1200+i;d.parent_id=1000;
        d.name=(nx_utf8_span){"Edit",4};d.bounds=(nx_rect){8,8+(float)i*40,250,30};
        d.fonts=s->fonts;d.pixels_per_em=16;d.glyph_cache_bytes=65536;
        d.disabled=i==4 || (i==0&&s->disabled);d.password=i==5;d.multiline=i==6;
        nx_interaction_state state;int valid,submitted;
        CHECK(nx_text_field_widget(s->ctx,&d,s->fields[i],&state,&valid,&submitted)==NX_OK);
    }
    /* Deliberately give protected custom semantics plaintext: native queries
       and event preparation must suppress it independently of built-in fields. */
    nx_semantic_desc custom={0};custom.id=1208;custom.parent_id=1000;custom.role=NX_ROLE_CUSTOM;
    custom.name=(nx_utf8_span){"Privacy transition",18};custom.bounds=(nx_rect){300,180,140,28};
    const char *value=s->value_protected?"private-custom-fixture":"public-value";
    custom.value=(nx_utf8_span){value,strlen(value)};custom.state=s->value_protected?NX_SEM_PROTECTED:0;
    CHECK(nx_semantics_add(s->ctx,&custom,NULL)==NX_OK);
    CHECK(nx_end_frame(s->ctx)==NX_OK);return 0;
}
static int route(nx_window *window,scene *s,size_t *count) {
    CHECK(nx_window_route_events(window,s->ctx,count)==NX_OK);return 0;
}
static int run(const WCHAR *client) {
    CHECK(NX_UIA_IsValuePatternAvailablePropertyId==UIA_IsValuePatternAvailablePropertyId);
    CHECK(NX_UIA_ValueIsReadOnlyPropertyId==UIA_ValueIsReadOnlyPropertyId);
    allocation a={0};nx_allocator allocator={&a,allocate,deallocate};
    nx_window_desc wd={0};wd.width=480;wd.height=320;wd.title_utf8="NexaUI owned edit values";
    wd.flags=NX_WINDOW_HIDDEN;wd.allocator=allocator;nx_window *window=NULL;
    CHECK(nx_window_create(&wd,&window)==NX_OK);HWND hwnd=nx_window_native_hwnd(window);CHECK(hwnd&&!IsWindowVisible(hwnd));
    a.hwnd=hwnd;a.window=window;
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;scene s={0};
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&s.fonts)==NX_OK);
    CHECK(nx_font_collection_add(s.fonts,font)==NX_OK);
    const char *initial[]={"10.42.0.1","7443","","read-only","disabled","private-test-value","one\ntwo"};
    for(unsigned i=0;i<7;++i) {
        nx_result r=i==5?nx_text_field_create_password(initial[i],strlen(initial[i]),&s.fields[i]):
            nx_text_field_create(initial[i],strlen(initial[i]),8192,&s.fields[i]);CHECK(r==NX_OK);
    }
    CHECK(nx_text_field_set_read_only(s.fields[3],1)==NX_OK);
    nx_context_desc cd={0};cd.allocator=allocator;CHECK(nx_context_create(&cd,&s.ctx)==NX_OK);
    CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
    size_t count=0;CHECK(route(window,&s,&count)==0 && frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
    CHECK(nx_focus_set(s.ctx,1200)==NX_OK);
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};HANDLE commands,child_commands,acks,child_acks,logs,child_logs;
    HANDLE done=CreateEventW(&sa,TRUE,FALSE,NULL);CHECK(done);
    CHECK(CreatePipe(&commands,&child_commands,&sa,0)&&CreatePipe(&child_acks,&acks,&sa,0)&&CreatePipe(&logs,&child_logs,&sa,0));
    CHECK(SetHandleInformation(commands,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(acks,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(logs,HANDLE_FLAG_INHERIT,0));
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);CHECK(input!=INVALID_HANDLE_VALUE);
    WCHAR cmd[4096];CHECK(swprintf_s(cmd,4096,L"\"%ls\" %llu %llu %llu %llu",client,
        (unsigned long long)(UINT_PTR)hwnd,(unsigned long long)(UINT_PTR)child_commands,
        (unsigned long long)(UINT_PTR)child_acks,(unsigned long long)(UINT_PTR)done)>0);
    STARTUPINFOW startup={0};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=input;startup.hStdOutput=child_logs;startup.hStdError=child_logs;PROCESS_INFORMATION process={0};
    CHECK(CreateProcessW(client,cmd,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process));
    CloseHandle(process.hThread);CloseHandle(input);CloseHandle(child_commands);CloseHandle(child_acks);CloseHandle(child_logs);
    ULONGLONG end=GetTickCount64()+90000;int destroyed=0;
    while(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) {
        if(GetTickCount64()>=end){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);fputs("value client deadline\n",stderr);return 1;}
        if(window)CHECK(nx_window_poll(window,0)==NX_OK);
        else {MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
        DWORD available=0,n=0;
        if(PeekNamedPipe(logs,NULL,0,NULL,&available,NULL)&&available){char buffer[4096];CHECK(ReadFile(logs,buffer,available<sizeof(buffer)?available:(DWORD)sizeof(buffer),&n,NULL));fwrite(buffer,1,n,stderr);}
        available=0;
        if(!PeekNamedPipe(commands,NULL,0,NULL,&available,NULL)){CHECK(GetLastError()==ERROR_BROKEN_PIPE);available=0;}
        if(available>=sizeof(unsigned)) {
            unsigned command,reply=0;CHECK(ReadFile(commands,&command,sizeof(command),&n,NULL)&&n==sizeof(command));
            int block=0;count=0;
            if(command==VALUE_PREPARE_CALLOUT) {
                nx_context_destroy(s.ctx);s.ctx=NULL;CHECK(nx_context_create(&cd,&s.ctx)==NX_OK);
                /* Warm both alternating interaction-region buffers, so the
                   armed reserve callback cannot fire in region registration. */
                CHECK(frame(&s)==0&&frame(&s)==0&&nx_window_publish_semantics(window,s.ctx)==NX_OK&&nx_focus_set(s.ctx,1200)==NX_OK);
            } else if(command>=VALUE_ROUTE_DISABLE_ALLOC&&command<=VALUE_RESERVE_DETACH_FREE) {
                size_t anchor=0,cursor=0;CHECK(nx_text_field_selection(s.fields[0],&anchor,&cursor)==NX_OK);
                a.fired=0;a.reserve_ready=0;
                if(command<=VALUE_ROUTE_DETACH_ALLOC)a.callout=command;
                CHECK(route(window,&s,&count)==0);
                if(command>=VALUE_RESERVE_DISABLE_ALLOC)a.callout=command;
                int frame_result=frame(&s);nx_utf8_span observed={0};
                CHECK(nx_text_field_text(s.fields[0],&observed)==NX_OK);
                fprintf(stderr,"Allocator outcome command=%u frame=%d fired=%u remaining=%u length=%zu unchanged=%d\n",
                    command,frame_result,a.fired,a.callout,observed.length,matches(s.fields[0],"10.42.0.1"));
                CHECK(frame_result==0&&a.fired==1&&a.callout==0&&matches(s.fields[0],"10.42.0.1"));
                size_t after_anchor=0,after_cursor=0;
                CHECK(nx_text_field_selection(s.fields[0],&after_anchor,&after_cursor)==NX_OK&&anchor==after_anchor&&cursor==after_cursor);
                for(unsigned i=0;i<2;++i) {
                    nx_event e={0};e.type=NX_EVENT_TEXT_COMMAND;e.text_command=i?NX_TEXT_COMMAND_REDO:NX_TEXT_COMMAND_UNDO;
                    CHECK(nx_context_queue_event(s.ctx,&e)==NX_OK&&frame(&s)==0&&matches(s.fields[0],"10.42.0.1"));
                }
                CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);reply=1;
                fprintf(stderr,"Allocator callout revoked text, selection and history unchanged command=%u\n",command);
            } else if(command==VALUE_BLOCK_OWNER){CHECK(ResetEvent(done));block=1;}
            else if(command==VALUE_OOM_ROUTE) {
                a.fail_size=10;CHECK(nx_window_route_events(window,s.ctx,&count)==NX_ERROR_MEMORY);
                CHECK(a.failed_size==10 && a.fail_size==0);reply=0; /* No text was admitted on OOM. */
            } else {
                CHECK(window&&route(window,&s,&count)==0);reply=0;
                fprintf(stderr,"Value route command=%u aggregate=%zu\n",command,count);
                if(command==VALUE_ROUTE||command==VALUE_READONLY||command==VALUE_DISABLE||command==VALUE_MODAL||
                   command==VALUE_RECREATE||command==VALUE_CONTEXT_ROUNDTRIP||command==VALUE_DISABLE_ROUNDTRIP||
                   command==VALUE_DETACH_ROUNDTRIP||command==VALUE_DESTROY_WINDOW||command==VALUE_DESTROY_CONTEXT) {
                    /* Route counts also include native events/focus. The public
                       duplicate contract proves THIS text payload reached the
                       owner queue, independently of that aggregate. */
                    CHECK(nx_semantics_request_text_value(s.ctx,1200,(nx_utf8_span){"probe",5})==NX_ERROR_LIMIT);
                    reply=1;
                }
                if(command==VALUE_ROUTE) {}
                else if(command==VALUE_FRAME||command==VALUE_UNDO||command==VALUE_REDO) {
                    if(command!=VALUE_FRAME){nx_event e={0};e.type=NX_EVENT_TEXT_COMMAND;e.text_command=command==VALUE_UNDO?NX_TEXT_COMMAND_UNDO:NX_TEXT_COMMAND_REDO;CHECK(nx_context_queue_event(s.ctx,&e)==NX_OK);}
                    CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_PROTECT_CUSTOM||command==VALUE_UNPROTECT_CUSTOM) {
                    s.value_protected=command==VALUE_PROTECT_CUSTOM;
                    CHECK(frame(&s)==0&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_READONLY||command==VALUE_EDITABLE) {
                    CHECK(nx_text_field_set_read_only(s.fields[0],command==VALUE_READONLY)==NX_OK);
                    CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_DISABLE||command==VALUE_ENABLE) {
                    s.disabled=command==VALUE_DISABLE;CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_MODAL||command==VALUE_MODAL_CLOSE) {
                    s.modal=command==VALUE_MODAL;if(!s.modal)CHECK(nx_overlay_close(s.ctx)==NX_OK);
                    CHECK(frame(&s)==0 && nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_RECREATE) {
                    nx_text_field_destroy(s.fields[0]);CHECK(nx_text_field_create("replacement",11,8192,&s.fields[0])==NX_OK);
                    CHECK(frame(&s)==0&&matches(s.fields[0],"replacement")&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_CONTEXT_ROUNDTRIP) {
                    nx_context *original=s.ctx,*other=NULL;CHECK(nx_context_create(&cd,&other)==NX_OK);
                    s.ctx=other;CHECK(frame(&s)==0&&nx_window_publish_semantics(window,other)==NX_OK);
                    s.ctx=original;CHECK(nx_window_publish_semantics(window,original)==NX_OK);nx_context_destroy(other);
                    CHECK(frame(&s)==0&&matches(s.fields[0],"replacement")&&nx_window_publish_semantics(window,original)==NX_OK);
                } else if(command==VALUE_DISABLE_ROUNDTRIP) {
                    EnableWindow(hwnd,FALSE);EnableWindow(hwnd,TRUE);
                    CHECK(IsWindowEnabled(hwnd)&&frame(&s)==0&&matches(s.fields[0],"replacement")&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_DESTROY_CONTEXT) {
                    nx_context_destroy(s.ctx);s.ctx=NULL;CHECK(nx_context_create(&cd,&s.ctx)==NX_OK);
                    CHECK(frame(&s)==0&&matches(s.fields[0],"replacement")&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_DETACH_ROUNDTRIP) {
                    CHECK(nx_window_publish_semantics(window,NULL)==NX_OK&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                    CHECK(frame(&s)==0&&matches(s.fields[0],"replacement")&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                } else if(command==VALUE_DESTROY_WINDOW) {
                    nx_window_destroy(window);window=NULL;destroyed=1;
                    CHECK(frame(&s)==0&&matches(s.fields[0],"replacement"));
                } else {fprintf(stderr,"unknown value command %u\n",command);return 1;}
            }
            CHECK(WriteFile(acks,&reply,sizeof(reply),&n,NULL)&&n==sizeof(reply));
            if(block) {
                DWORD waited=WaitForSingleObject(done,2000);
                /* UIA may mediate setters through the owner. This observation gate
                   does not require owner-independent action completion: it must
                   never edit while blocked, and routing/model reflection below
                   are checked separately after normal pumping resumes. */
                fprintf(stderr,"ValueSet owner hold wait=%lu model_unchanged=%d\n",waited,matches(s.fields[0],"10.42.0.1"));
                CHECK((waited==WAIT_OBJECT_0||waited==WAIT_TIMEOUT)&&matches(s.fields[0],"10.42.0.1"));
            }
        }
        CHECK(MsgWaitForMultipleObjectsEx(1,&process.hProcess,1,QS_ALLINPUT,MWMO_INPUTAVAILABLE)!=WAIT_FAILED);
    }
    DWORD available=0,n=0;
    while(PeekNamedPipe(logs,NULL,0,NULL,&available,NULL)&&available){char buffer[4096];CHECK(ReadFile(logs,buffer,available<sizeof(buffer)?available:(DWORD)sizeof(buffer),&n,NULL));fwrite(buffer,1,n,stderr);}
    DWORD status=1;CHECK(GetExitCodeProcess(process.hProcess,&status));CloseHandle(process.hProcess);
    CloseHandle(commands);CloseHandle(acks);CloseHandle(logs);CloseHandle(done);
    CHECK(destroyed);nx_context_destroy(s.ctx);for(unsigned i=0;i<7;++i)nx_text_field_destroy(s.fields[i]);
    nx_font_collection_destroy(s.fonts);nx_font_close(font);CHECK(a.live==0&&status==0);
    puts("owned native edit values and lifetime guards passed");return 0;
}
int wmain(int argc,WCHAR **argv) {
    if(argc<2||argc>3)return 2;
    int owned=argc==3&&!wcscmp(argv[2],L"owned");HRESULT hr=S_OK;
    if(!owned){hr=CoInitializeEx(NULL,argc==3?COINIT_MULTITHREADED:COINIT_APARTMENTTHREADED);if(FAILED(hr))return 2;}
    int result=run(argv[1]);if(!owned)CoUninitialize();return result;
}
