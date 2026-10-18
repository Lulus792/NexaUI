#define NOMINMAX
#include "window_internal.h"
#include "uia_protocol.h"
#include "design_a_fallback_scene.h"
#include <objbase.h>
#include <oleauto.h>
#include <uiautomation.h>
#include "uia_ids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct allocation_state { unsigned live; int fail_next; } allocation_state;
static void *allocate(void *user,size_t bytes) {
    allocation_state *s=user;
    if (s->fail_next) { s->fail_next=0; return NULL; }
    void *p=malloc(bytes); if (p) ++s->live; return p;
}
static void deallocate(void *user,void *p,size_t bytes) {
    (void)bytes; if (p) { --((allocation_state *)user)->live; free(p); }
}
typedef struct scene {
    nx_context *ctx;
    int checked, removed, recreated, renamed, large_extra;
    unsigned clicks, fill_clicks, total_clicks;
    uint64_t required_choice, optional_choice, radio_choice, other_choice;
    int selection_moved;
    int design_mode;
    design_a_scene design;
} scene;
static nx_control_desc control(uint64_t id,uint64_t parent,const char *name,nx_rect bounds) {
    nx_control_desc d={0}; d.id=id; d.parent_id=parent; d.name=(nx_utf8_span){name,strlen(name)}; d.bounds=bounds; return d;
}
static int frame(scene *s) {
    if(s->design_mode) return design_a_frame(s->ctx,&s->design,1);
    CHECK(nx_begin_frame(s->ctx,640,480,0)==NX_OK);
    nx_semantic_desc root={0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"NexaCloud",9}; root.bounds=(nx_rect){0,0,640,480};
    CHECK(nx_semantics_add(s->ctx,&root,NULL)==NX_OK);
    nx_semantic_desc group=root; group.id=10; group.parent_id=1; group.role=NX_ROLE_GROUP;
    group.name=(nx_utf8_span){"Dateien",7}; CHECK(nx_semantics_add(s->ctx,&group,NULL)==NX_OK);
    char name[64]; strcpy_s(name,sizeof(name),s->renamed ? "Changed" : "\xc3\x96" "ffnen \xf0\x9f\x93\x81");
    nx_control_desc d=control(2,10,name,(nx_rect){10,20,100,30});
    nx_interaction_state state; CHECK(nx_button(s->ctx,&d,&state)==NX_OK); s->clicks=state.clicked; s->total_clicks+=state.clicked;
    memset(name,'X',strlen(name)); /* Both core and native snapshots must own text. */
    d=control(3,10,"Sync",(nx_rect){10,60,100,30});
    CHECK(nx_checkbox(s->ctx,&d,&s->checked,&state)==NX_OK);
    d=control(4,10,"Disabled",(nx_rect){10,100,100,30}); d.disabled=1;
    CHECK(nx_button(s->ctx,&d,&state)==NX_OK);
    nx_widget_desc secret={0}; secret.semantic.id=5; secret.semantic.parent_id=1;
    secret.semantic.role=NX_ROLE_TEXT_FIELD; secret.semantic.bounds=(nx_rect){120,20,100,30};
    secret.semantic.name=(nx_utf8_span){"Password",8}; secret.semantic.value=(nx_utf8_span){"do-not-expose",13};
    secret.semantic.state=NX_SEM_PROTECTED; secret.semantic.actions=NX_ACTION_FOCUS;
    secret.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
    CHECK(nx_widget_register(s->ctx,&secret,&state)==NX_OK);
    d=control(7,1,"Outside",(nx_rect){2000,2000,100,30}); CHECK(nx_button(s->ctx,&d,&state)==NX_OK);
    if (!s->removed) {
        d=control(8,10,"Dynamic",(nx_rect){10,140,100,30});
        if (s->recreated) { int value=0; CHECK(nx_checkbox(s->ctx,&d,&value,&state)==NX_OK); }
        else CHECK(nx_button(s->ctx,&d,&state)==NX_OK);
    }
    if (s->large_extra) {
        char label[32768]; memset(label,'Z',sizeof(label));
        nx_semantic_desc extra={0}; extra.id=20; extra.parent_id=1; extra.role=NX_ROLE_TEXT;
        extra.bounds=(nx_rect){200,200,100,30}; extra.name=(nx_utf8_span){label,sizeof(label)};
        CHECK(nx_semantics_add(s->ctx,&extra,NULL)==NX_OK);
    }
    for (unsigned g=0;g<4;++g) {
        nx_semantic_desc container={0}; container.id=50u+g*10u; container.parent_id=1;
        container.role=g==0 ? NX_ROLE_TAB_LIST : g==2 ? NX_ROLE_GROUP : NX_ROLE_LIST;
        container.name=(nx_utf8_span){"Selection",9}; container.bounds=(nx_rect){250,20+(float)g*80,180,70};
        uint64_t *choice=g==0 ? &s->required_choice : g==1 ? &s->optional_choice : g==2 ? &s->radio_choice : &s->other_choice;
        CHECK(nx_selection_group(s->ctx,&container,g==0 || g==2,choice)==NX_OK);
        uint64_t parent=container.id;
        if (g==1) {
            nx_semantic_desc decoration=container; decoration.id=64; decoration.parent_id=60; decoration.role=NX_ROLE_PANEL;
            CHECK(nx_semantics_add(s->ctx,&decoration,NULL)==NX_OK); parent=64;
        }
        for (unsigned item=1;item<=3;++item) {
            if (g==3 && (!s->selection_moved || item!=1)) continue;
            if (g==1 && s->selection_moved && item==1) continue;
            nx_widget_desc member={0}; member.semantic.id=g==3 ? 61 : container.id+item; member.semantic.parent_id=parent;
            member.semantic.role=g==2 ? NX_ROLE_RADIO : g==0 ? NX_ROLE_TAB : NX_ROLE_LIST_ITEM;
            member.semantic.bounds=(nx_rect){250+(float)(item-1u)*50,20+(float)g*80,45,30};
            member.semantic.name=(nx_utf8_span){"Choice",6}; member.semantic.actions=NX_ACTION_SELECT|NX_ACTION_FOCUS;
            member.semantic.state=(*choice==member.semantic.id ? (g==2 ? NX_SEM_CHECKED : NX_SEM_SELECTED) : 0u) | (item==3 ? NX_SEM_DISABLED : 0u);
            member.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
            if (g==2) {
                nx_control_desc radio=control(member.semantic.id,parent,"Choice",member.semantic.bounds);radio.disabled=item==3;
                CHECK(nx_radio(s->ctx,&radio,member.semantic.id,choice,&state)==NX_OK && !state.clicked);
            } else {
                nx_result registered=nx_widget_register(s->ctx,&member,&state);
                if (registered!=NX_OK || state.clicked)
                    fprintf(stderr,"Selection member %llu: result=%d clicked=%d focus=%llu\n",
                        (unsigned long long)member.semantic.id,registered,state.clicked,
                        (unsigned long long)nx_focus_id(s->ctx));
                CHECK(registered==NX_OK && !state.clicked);
            }
        }
    }
    s->fill_clicks=0;
    for (uint64_t id=100;id<=164;++id) {
        d=control(id,1,"Queued",(nx_rect){120,60,100,30});
        CHECK(nx_button(s->ctx,&d,&state)==NX_OK); s->fill_clicks+=state.clicked;
    }
    CHECK(nx_end_frame(s->ctx)==NX_OK); return 0;
}
static int run(const WCHAR *client,int design_only) {
    CHECK(NX_UIA_AutomationFocusChangedEventId==UIA_AutomationFocusChangedEventId);
    CHECK(NX_UIA_AutomationIdPropertyId==UIA_AutomationIdPropertyId);
    CHECK(NX_UIA_ButtonControlTypeId==UIA_ButtonControlTypeId);
    CHECK(NX_UIA_CheckBoxControlTypeId==UIA_CheckBoxControlTypeId);
    CHECK(NX_UIA_ControlTypePropertyId==UIA_ControlTypePropertyId);
    CHECK(NX_UIA_CustomControlTypeId==UIA_CustomControlTypeId);
    CHECK(NX_UIA_DataItemControlTypeId==UIA_DataItemControlTypeId);
    CHECK(NX_UIA_EditControlTypeId==UIA_EditControlTypeId);
    CHECK(NX_UIA_FrameworkIdPropertyId==UIA_FrameworkIdPropertyId);
    CHECK(NX_UIA_GroupControlTypeId==UIA_GroupControlTypeId);
    CHECK(NX_UIA_HasKeyboardFocusPropertyId==UIA_HasKeyboardFocusPropertyId);
    CHECK(NX_UIA_HeaderItemControlTypeId==UIA_HeaderItemControlTypeId);
    CHECK(NX_UIA_HelpTextPropertyId==UIA_HelpTextPropertyId);
    CHECK(NX_UIA_ImageControlTypeId==UIA_ImageControlTypeId);
    CHECK(NX_UIA_InvokePatternId==UIA_InvokePatternId);
    CHECK(NX_UIA_IsContentElementPropertyId==UIA_IsContentElementPropertyId);
    CHECK(NX_UIA_IsControlElementPropertyId==UIA_IsControlElementPropertyId);
    CHECK(NX_UIA_IsDataValidForFormPropertyId==UIA_IsDataValidForFormPropertyId);
    CHECK(NX_UIA_IsEnabledPropertyId==UIA_IsEnabledPropertyId);
    CHECK(NX_UIA_IsKeyboardFocusablePropertyId==UIA_IsKeyboardFocusablePropertyId);
    CHECK(NX_UIA_IsOffscreenPropertyId==UIA_IsOffscreenPropertyId);
    CHECK(NX_UIA_IsPasswordPropertyId==UIA_IsPasswordPropertyId);
    CHECK(NX_UIA_IsRequiredForFormPropertyId==UIA_IsRequiredForFormPropertyId);
    CHECK(NX_UIA_ListControlTypeId==UIA_ListControlTypeId);
    CHECK(NX_UIA_ListItemControlTypeId==UIA_ListItemControlTypeId);
    CHECK(NX_UIA_MenuBarControlTypeId==UIA_MenuBarControlTypeId);
    CHECK(NX_UIA_MenuControlTypeId==UIA_MenuControlTypeId);
    CHECK(NX_UIA_MenuItemControlTypeId==UIA_MenuItemControlTypeId);
    CHECK(NX_UIA_NamePropertyId==UIA_NamePropertyId);
    CHECK(NX_UIA_NativeWindowHandlePropertyId==UIA_NativeWindowHandlePropertyId);
    CHECK(NX_UIA_PaneControlTypeId==UIA_PaneControlTypeId);
    CHECK(NX_UIA_ProgressBarControlTypeId==UIA_ProgressBarControlTypeId);
    CHECK(NX_UIA_RadioButtonControlTypeId==UIA_RadioButtonControlTypeId);
    CHECK(NX_UIA_SelectionItemIsSelectedPropertyId==UIA_SelectionItemIsSelectedPropertyId);
    CHECK(NX_UIA_SeparatorControlTypeId==UIA_SeparatorControlTypeId);
    CHECK(NX_UIA_SliderControlTypeId==UIA_SliderControlTypeId);
    CHECK(NX_UIA_StatusBarControlTypeId==UIA_StatusBarControlTypeId);
    CHECK(NX_UIA_TabItemControlTypeId==UIA_TabItemControlTypeId);
    CHECK(NX_UIA_TableControlTypeId==UIA_TableControlTypeId);
    CHECK(NX_UIA_TextControlTypeId==UIA_TextControlTypeId);
    CHECK(NX_UIA_TogglePatternId==UIA_TogglePatternId);
    CHECK(NX_UIA_ToggleToggleStatePropertyId==UIA_ToggleToggleStatePropertyId);
    CHECK(NX_UIA_ToolBarControlTypeId==UIA_ToolBarControlTypeId);
    CHECK(NX_UIA_ToolTipControlTypeId==UIA_ToolTipControlTypeId);
    CHECK(NX_UIA_TreeControlTypeId==UIA_TreeControlTypeId);
    CHECK(NX_UIA_TreeItemControlTypeId==UIA_TreeItemControlTypeId);
    CHECK(NX_UIA_ValueValuePropertyId==UIA_ValueValuePropertyId);
    CHECK(NX_UIA_ValuePatternId==UIA_ValuePatternId);
    CHECK(NX_UIA_WindowControlTypeId==UIA_WindowControlTypeId);
    CHECK(NX_UIA_TabControlTypeId==UIA_TabControlTypeId);
    CHECK(NX_UIA_SelectionPatternId==UIA_SelectionPatternId);
    CHECK(NX_UIA_SelectionItemPatternId==UIA_SelectionItemPatternId);
    CHECK(NX_UIA_SelectionSelectionPropertyId==UIA_SelectionSelectionPropertyId);
    CHECK(NX_UIA_SelectionCanSelectMultiplePropertyId==UIA_SelectionCanSelectMultiplePropertyId);
    CHECK(NX_UIA_SelectionIsSelectionRequiredPropertyId==UIA_SelectionIsSelectionRequiredPropertyId);
    CHECK(NX_UIA_SelectionItemSelectionContainerPropertyId==UIA_SelectionItemSelectionContainerPropertyId);
    CHECK(NX_UIA_Selection_InvalidatedEventId==UIA_Selection_InvalidatedEventId);
    CHECK(NX_UIA_SelectionItem_ElementSelectedEventId==UIA_SelectionItem_ElementSelectedEventId);
    CHECK(NX_UIA_SelectionItem_ElementRemovedFromSelectionEventId==UIA_SelectionItem_ElementRemovedFromSelectionEventId);
    allocation_state allocations={0};
    char title[1601]; memset(title,'A',1600); title[1600]=0;
    nx_window_desc desc={0}; desc.width=640; desc.height=480; desc.title_utf8=title;
    desc.flags=NX_WINDOW_HIDDEN; desc.allocator=(nx_allocator){&allocations,allocate,deallocate};
    nx_window *window=NULL; CHECK(nx_window_create(&desc,&window)==NX_OK);
    HWND hwnd=nx_window_native_hwnd(window); CHECK(hwnd);
    scene s={0}; s.required_choice=51; s.radio_choice=71; CHECK(nx_context_create(NULL,&s.ctx)==NX_OK); CHECK(frame(&s)==0);
    /* First publication must be transactional too. */
    allocations.fail_next=1;
    CHECK(nx_window_publish_semantics(window,s.ctx)==NX_ERROR_MEMORY && !allocations.fail_next);
    CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
    SECURITY_ATTRIBUTES security={sizeof(security),NULL,TRUE};
    HANDLE command_read,command_write,ack_read,ack_write;
    CHECK(CreatePipe(&command_read,&command_write,&security,0));
    CHECK(CreatePipe(&ack_read,&ack_write,&security,0));
    CHECK(SetHandleInformation(command_read,HANDLE_FLAG_INHERIT,0));
    CHECK(SetHandleInformation(ack_write,HANDLE_FLAG_INHERIT,0));
    WCHAR command[4096];
    CHECK(swprintf_s(command,4096,L"\"%s\" %llu %llu %llu%s",client,
        (unsigned long long)(UINT_PTR)hwnd,(unsigned long long)(UINT_PTR)command_write,
        (unsigned long long)(UINT_PTR)ack_read,design_only?L" --design":L"")>0);
    HANDLE log_read,log_write;
    CHECK(CreatePipe(&log_read,&log_write,&security,0));
    CHECK(SetHandleInformation(log_read,HANDLE_FLAG_INHERIT,0));
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
    CHECK(input!=INVALID_HANDLE_VALUE);
    STARTUPINFOW startup={0}; startup.cb=sizeof(startup); PROCESS_INFORMATION process={0};
    startup.dwFlags=STARTF_USESTDHANDLES; startup.hStdInput=input;
    startup.hStdOutput=log_write; startup.hStdError=log_write;
    CHECK(CreateProcessW(client,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process));
    CloseHandle(process.hThread); CloseHandle(command_write); CloseHandle(ack_read);
    CloseHandle(log_write); CloseHandle(input);
    ULONGLONG deadline=GetTickCount64()+90000;
    nx_context *old_context=NULL;
    int destroyed=0,auto_frames=0; ULONGLONG next_frame=0;
    while (WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) {
        CHECK(GetTickCount64()<deadline);
        if (window) CHECK(nx_window_poll(window,0)==NX_OK);
        else {
            /* COM and UIA host proxies can still call the UI apartment after
               HWND destruction. Keep pumping until the independent client exits. */
            MSG message;
            while (PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        DWORD logs=0;
        if (PeekNamedPipe(log_read,NULL,0,NULL,&logs,NULL) && logs) {
            char buffer[4096]; DWORD read=0;
            CHECK(ReadFile(log_read,buffer,logs<sizeof(buffer) ? logs : (DWORD)sizeof(buffer),&read,NULL));
            fwrite(buffer,1,read,stderr);
        }
        DWORD available=0;
        if (!PeekNamedPipe(command_read,NULL,0,NULL,&available,NULL)) {
            CHECK(GetLastError()==ERROR_BROKEN_PIPE); available=0; /* Child can close pipes before process exit. */
        }
        if (available>=sizeof(unsigned)) {
            unsigned request,reply=0; DWORD bytes=0;
            CHECK(ReadFile(command_read,&request,sizeof(request),&bytes,NULL) && bytes==sizeof(request));
            if (request==UIA_TEST_MOVE_UNPUBLISHED || request==UIA_TEST_MOVE_SELECTION || request==UIA_TEST_RESTORE_SELECTION) {
                s.selection_moved=request!=UIA_TEST_RESTORE_SELECTION; s.other_choice=0;
                CHECK(frame(&s)==0);
                if (request!=UIA_TEST_MOVE_UNPUBLISHED) CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
            } else if (request==UIA_TEST_ROUTE_SELECTION) {
                size_t actions=0; CHECK(nx_window_route_events(window,s.ctx,&actions)==NX_OK);
                CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK); reply=(unsigned)actions;
            } else if (request==UIA_TEST_SELECTION_APPLY || request==UIA_TEST_SELECTION_FOCUS) {
                size_t actions=0; CHECK(nx_window_route_events(window,s.ctx,&actions)==NX_OK);
                CHECK(frame(&s)==0); CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
                reply=request==UIA_TEST_SELECTION_FOCUS ? (unsigned)nx_focus_id(s.ctx) :
                    (unsigned)s.required_choice|((unsigned)s.optional_choice<<8u)|((unsigned)s.radio_choice<<16u);
            } else if (request==UIA_TEST_APPLY) {
                size_t actions=0; CHECK(nx_window_route_events(window,s.ctx,&actions)==NX_OK);
                CHECK(frame(&s)==0); CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
                reply=(s.clicks ? 1u : 0u)|(s.checked ? 2u : 0u)|(nx_focus_id(s.ctx)==2 ? 4u : 0u)|(s.fill_clicks<<8u)|((GetFocus()==hwnd && GetForegroundWindow()==hwnd) ? 8u : 0u);
            } else if (request==UIA_TEST_FILL_CORE) {
                for (uint64_t id=100;id<164;++id) CHECK(nx_semantics_request_action(s.ctx,id,NX_ACTION_PRESS)==NX_OK);
            } else if (request==UIA_TEST_FAIL_PUBLISH) {
                s.renamed=1; s.large_extra=1; CHECK(frame(&s)==0); allocations.fail_next=1;
                CHECK(nx_window_publish_semantics(window,s.ctx)==NX_ERROR_MEMORY && !allocations.fail_next);
            } else if (request==UIA_TEST_REPUBLISH) {
                CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
            } else if (request==UIA_TEST_REMOVE || request==UIA_TEST_RECREATE) {
                s.removed=request==UIA_TEST_REMOVE; s.recreated=request==UIA_TEST_RECREATE;
                CHECK(frame(&s)==0); CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
            } else if (request==UIA_TEST_REPLACE_CONTEXT) {
                old_context=s.ctx; CHECK(nx_context_create(NULL,&s.ctx)==NX_OK);
                CHECK(frame(&s)==0); CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
            } else if (request==UIA_TEST_WRONG_CONTEXT) {
                size_t actions=0; CHECK(nx_window_route_events(window,old_context,&actions)==NX_ERROR_STATE && !actions);
                nx_context_destroy(old_context); old_context=NULL;
            } else if (request==UIA_TEST_DESTROY_CONTEXT) {
                nx_context_destroy(s.ctx); s.ctx=NULL;
            } else if (request==UIA_TEST_DETACH) {
                CHECK(nx_window_publish_semantics(window,NULL)==NX_OK);
            } else if (request==UIA_TEST_DISABLE_HOST || request==UIA_TEST_ENABLE_HOST) {
                EnableWindow(hwnd,request==UIA_TEST_ENABLE_HOST);
                CHECK(!!IsWindowEnabled(hwnd)==(request==UIA_TEST_ENABLE_HOST));
            } else if (request==UIA_TEST_REATTACH) {
                CHECK(nx_context_create(NULL,&s.ctx)==NX_OK); CHECK(frame(&s)==0);
                CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
            } else if (request>=UIA_TEST_DESIGN_START && request<=UIA_TEST_DESIGN_UNGUARD) {
                if(request==UIA_TEST_DESIGN_START) {
                    CHECK(!s.design_mode&&!design_a_prepare(&s.design,1));s.design_mode=1;
                }
                size_t actions=0;CHECK(nx_window_route_events(window,s.ctx,&actions)==NX_OK);
                if(request==UIA_TEST_DESIGN_PROTECT)s.design.value_guard=1;
                if(request==UIA_TEST_DESIGN_EDITABLE)s.design.value_guard=2;
                if(request==UIA_TEST_DESIGN_UNGUARD)s.design.value_guard=0;
                if(request==UIA_TEST_DESIGN_END) {
                    s.design_mode=0;nx_theme theme;CHECK(nx_theme_preset(NX_THEME_DARK,&theme)==NX_OK);
                    CHECK(nx_context_set_theme(s.ctx,&theme)==NX_OK);
                }
                CHECK(frame(&s)==0&&nx_window_publish_semantics(window,s.ctx)==NX_OK);
                fprintf(stderr,"Design host: routed=%zu focus=%llu cursor=%g valid=%d\n",actions,
                    (unsigned long long)nx_focus_id(s.ctx),s.design.plot.cursor_x,s.design.plot.cursor_valid);
                reply=(s.design.reduced_effects?1u:0u)|(s.design.high_contrast?2u:0u)|
                    (s.design.applied_contrast?4u:0u)|(nx_focus_id(s.ctx)==DESIGN_PLOT?8u:0u)|
                    ((GetFocus()==hwnd&&GetForegroundWindow()==hwnd)?16u:0u)|(s.design.rgba_commands<<8u);
                if(request==UIA_TEST_DESIGN_END) design_a_free(&s.design);
            } else if (request==UIA_TEST_DESTROY_WINDOW) {
                nx_window_destroy(window); window=NULL; destroyed=1;
            } else if (request==UIA_TEST_STRESS_START) {
                s.total_clicks=0; auto_frames=1;
            } else if (request==UIA_TEST_STRESS_STOP) {
                auto_frames=0;
                for (unsigned i=0;i<3;++i) {
                    size_t actions=0; CHECK(nx_window_route_events(window,s.ctx,&actions)==NX_OK);
                    CHECK(frame(&s)==0); CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
                }
                reply=s.total_clicks;
            } else if (request==UIA_TEST_SHOW) {
                ShowWindow(hwnd,SW_SHOWNOACTIVATE);
            } else if (request==UIA_TEST_MOVE) {
                RECT r; CHECK(GetWindowRect(hwnd,&r));
                CHECK(SetWindowPos(hwnd,NULL,r.left+40,r.top+30,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE));
            } else CHECK(0);
            CHECK(WriteFile(ack_write,&reply,sizeof(reply),&bytes,NULL) && bytes==sizeof(reply));
        }
        if (auto_frames && GetTickCount64()>=next_frame) {
            size_t actions=0; CHECK(nx_window_route_events(window,s.ctx,&actions)==NX_OK);
            CHECK(frame(&s)==0); CHECK(nx_window_publish_semantics(window,s.ctx)==NX_OK);
            next_frame=GetTickCount64()+10;
        }
        Sleep(1);
    }
    DWORD logs=0;
    while (PeekNamedPipe(log_read,NULL,0,NULL,&logs,NULL) && logs) {
        char buffer[4096]; DWORD read=0;
        CHECK(ReadFile(log_read,buffer,logs<sizeof(buffer) ? logs : (DWORD)sizeof(buffer),&read,NULL));
        fwrite(buffer,1,read,stderr);
    }
    CloseHandle(log_read);
    DWORD status=1; CHECK(GetExitCodeProcess(process.hProcess,&status));
    CloseHandle(process.hProcess); CloseHandle(command_read); CloseHandle(ack_write);
    if (s.ctx) nx_context_destroy(s.ctx); if (old_context) nx_context_destroy(old_context);
    design_a_free(&s.design);
    if (window) nx_window_destroy(window);
    CHECK(!allocations.live); if (status) fprintf(stderr,"Client exit %lu\n",(unsigned long)status);
    CHECK(status==0 && destroyed);
    puts("Windows UIA external-client tests passed"); return 0;
}
int wmain(int argc,WCHAR **argv) {
    if (argc<2 || argc>3) return 2;
    if (argc==3 && wcscmp(argv[2],L"owned")==0) return run(argv[1],0);
    HRESULT hr=CoInitializeEx(NULL,argc==3 ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 2;
    int result=run(argv[1],argc==3&&wcscmp(argv[2],L"design")==0); CoUninitialize(); return result;
}
