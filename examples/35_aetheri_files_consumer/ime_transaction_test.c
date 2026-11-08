#define _POSIX_C_SOURCE 200809L
#include <nexaui/nexaui.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Test the app parser with genuine public host-IME tokens. This guard is not a
   native-window proof; run_ime.py separately exercises real Wayland/AT-SPI. */
static nx_result test_guard(nx_window *,uint64_t,uint64_t,char *,size_t,size_t *,int32_t *,int32_t *);
#define nx_window_get_ime_preedit_for test_guard
#include AE_FILES_GENERATED_VIEW
#undef nx_window_get_ime_preedit_for

#define VERIFY(x) do {if(!(x)){fprintf(stderr,"IME controller %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static struct ae_files_view *guard_view;
static unsigned work_attempts;
static bool reject_worker(void *user,struct ae_files_session *session) {(void)user;(void)session;++work_attempts;return false;}
static void unexpected_join(void *user) {(void)user;VERIFY(false);}
static nx_result test_guard(nx_window *window,uint64_t id,uint64_t revision,char *bytes,size_t capacity,
    size_t *length,int32_t *begin,int32_t *end) {
    (void)bytes;(void)capacity;VERIFY(!window && guard_view);
    struct edit *edit=&guard_view->edits[guard_view->s->dialog?1:0];uint64_t current=0;
    nx_result result=nx_text_field_host_ime_revision(guard_view->ui,id,edit->field,&current);
    if(result!=NX_OK || current!=revision) return NX_ERROR_STATE;
    *length=0;*begin=*end=-1;return NX_OK;
}
static void tick(struct ae_files_view *view) {
    nx_window_status status={0};bool changed=false;
    for(unsigned i=0;i<8;++i) {
        VERIFY(ae_files_view_frame(view,1000,680,&status,&changed));
        if(!changed) return;
    }
    VERIFY(false);
}
static void deliver(struct ae_files_view *view,nx_event event) {
    bool handled=false;VERIFY(sync_scope(view) && native_transaction(view,&event,&handled));
    if(!handled) VERIFY(ae_files_view_event(view,&event));
}
static void keypress(struct ae_files_view *view,uint32_t key) {
    deliver(view,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=key,.modifiers=NX_MOD_CONTROL});
    deliver(view,(nx_event){.type=NX_EVENT_KEY_UP,.key=key,.modifiers=NX_MOD_CONTROL});tick(view);
}
static nx_event boundary(struct ae_files_view *view,nx_event_type type) {
    uint64_t id=ae_files_view_control_id(view,AE_FV_NAME),revision=0;
    nx_result result=nx_text_field_host_ime_revision(view->ui,id,view->edits[1].field,&revision);
    if(result!=NX_OK) fprintf(stderr,"host token result=%d id=%llu focus=%llu dialog=%d bad=%d composing=%d name=%s\n",
        (int)result,(unsigned long long)id,(unsigned long long)nx_focus_id(view->ui),view->s->dialog,view->s->name_bad,view->s->composing,view->s->name);
    VERIFY(result==NX_OK);
    return (nx_event){.type=type,.ime_target=id,.ime_revision=revision,.ime_before=NX_IME_TRANSACTION_COMMIT};
}
static nx_event character(nx_event begin,uint32_t scalar,uint32_t flags) {
    return (nx_event){.type=NX_EVENT_TEXT,.scalar=scalar,.ime_target=begin.ime_target,
        .ime_revision=begin.ime_revision,.ime_before=flags};
}
static void complete(struct ae_files_view *view,uint32_t scalar) {
    nx_event e=boundary(view,NX_EVENT_IME_TRANSACTION_BEGIN);deliver(view,e);
    deliver(view,character(e,scalar,NX_IME_COMMIT_START|NX_IME_COMMIT_END));
    e.type=NX_EVENT_IME_TRANSACTION_END;deliver(view,e);tick(view);
}
static void name_is(struct ae_files_view *view,const char *expected,size_t anchor,size_t cursor) {
    nx_utf8_span value;size_t a=0,c=0;
    VERIFY(!strcmp(view->s->name,expected));
    VERIFY(nx_text_field_text(view->edits[1].field,&value)==NX_OK && value.length==strlen(expected) && !memcmp(value.data,expected,value.length));
    VERIFY(nx_text_field_selection(view->edits[1].field,&a,&c)==NX_OK && a==anchor && c==cursor);
}
static void seed(struct ae_files_view *view,const char *value) {
    keypress(view,NX_KEY_U);if(*value) ae_files_session_name_input(view->s,value);tick(view);
    VERIFY(nx_focus_set(view->ui,control(view,AE_FV_NAME))==NX_OK);tick(view);
}
static void run_case(unsigned test,const void *font,size_t font_size,int root) {
    struct ae_files_session session;ae_files_session_init(&session,reject_worker,unexpected_join,NULL);
    enum ae_files_result result;session.files=ae_files_create(root,&result);VERIFY(session.files);
    memcpy(session.root_label,"Documents",10);struct ae_files_view *view=NULL;
    VERIFY(ae_files_view_create(&session,font,font_size,&view));guard_view=view;
    ae_files_session_dialog_open(&session,AE_FILES_CREATE_FILE);
    nx_event focus={.type=NX_EVENT_FOCUS,.focused=1};VERIFY(nx_context_queue_event(view->ui,&focus)==NX_OK);tick(view);
    VERIFY(nx_text_field_set_host_ime(view->edits[1].field,1)==NX_OK);tick(view);seed(view,"ABCDE");
    nx_event begin=boundary(view,NX_EVENT_IME_TRANSACTION_BEGIN),end=begin;end.type=NX_EVENT_IME_TRANSACTION_END;
    if(test==0) {
        VERIFY(nx_text_field_set_selection(view->edits[1].field,4,1)==NX_OK);tick(view);
        begin=boundary(view,NX_EVENT_IME_TRANSACTION_BEGIN);begin.ime_before|=NX_IME_TRANSACTION_DELETE;end=begin;end.type=NX_EVENT_IME_TRANSACTION_END;
        deliver(view,begin);deliver(view,(nx_event){.type=NX_EVENT_IME_DELETE_SURROUNDING,.ime_target=begin.ime_target,.ime_revision=begin.ime_revision,.ime_before=1,.ime_after=1});
        deliver(view,character(begin,'x',NX_IME_COMMIT_START));deliver(view,character(begin,0xe9,NX_IME_COMMIT_END));deliver(view,end);tick(view);
        name_is(view,"x\xc3\xa9",3,3);keypress(view,NX_KEY_Z);name_is(view,"ABCDE",4,1);
    } else if(test==1) {
        deliver(view,begin);deliver(view,character(begin,'x',NX_IME_COMMIT_START));tick(view);name_is(view,"ABCDE",5,5);
        deliver(view,character(begin,'y',NX_IME_COMMIT_END));deliver(view,end);tick(view);name_is(view,"ABCDExy",7,7);
        keypress(view,NX_KEY_Z);name_is(view,"ABCDE",5,5);
    } else if(test==2 || test==3 || test==8) {
        deliver(view,begin);
        if(test==2) deliver(view,begin);
        if(test==8) for(unsigned i=0;i<300;++i) deliver(view,character(begin,'x',i==0?NX_IME_COMMIT_START:i==299?NX_IME_COMMIT_END:0));
        else deliver(view,character(begin,'x',NX_IME_COMMIT_START|NX_IME_COMMIT_END));
        if(test==3) {nx_event wrong=end;++wrong.ime_revision;deliver(view,wrong);tick(view);name_is(view,"ABCDE",5,5);}
        deliver(view,end);if(test==2) deliver(view,end);tick(view);name_is(view,"ABCDE",5,5);VERIFY(session.name_bad);
        keypress(view,NX_KEY_ENTER);VERIFY(!work_attempts);seed(view,"");complete(view,'y');name_is(view,"y",1,1);
    } else if(test==4 || test==5 || test==6 || test==9) {
        deliver(view,begin);deliver(view,character(begin,'x',NX_IME_COMMIT_START|NX_IME_COMMIT_END));tick(view);name_is(view,"ABCDE",5,5);
        if(test==5) {VERIFY(ae_files_refresh(session.files)==AE_FILES_OK);tick(view);}
        else if(test==9) {VERIFY(nx_text_field_set_selection(view->edits[1].field,4,4)==NX_OK);tick(view);}
        else if(test==6) {
            focus.focused=0;deliver(view,focus);tick(view);focus.focused=1;deliver(view,focus);tick(view);
        }
        if(test!=5 && test!=9) seed(view,"");
        const char *expected=test==5?"ABCDEy":test==9?"ABCDyE":"y";size_t caret=test==5?6u:test==9?5u:1u;
        complete(view,'y');name_is(view,expected,caret,caret);
        deliver(view,end);tick(view);name_is(view,expected,caret,caret);
    } else if(test==7) {
        seed(view,"fresh");deliver(view,begin);deliver(view,character(begin,'x',NX_IME_COMMIT_START|NX_IME_COMMIT_END));deliver(view,end);tick(view);
        name_is(view,"fresh",5,5);VERIFY(!session.name_bad);complete(view,'y');name_is(view,"freshy",6,6);
    }
    VERIFY(!work_attempts);ae_files_session_dialog_close(&session);ae_files_view_destroy(view);guard_view=NULL;ae_files_destroy(session.files);
    printf("Controller IME case%u passed\n",test);fflush(stdout);
}
int main(int argc,char **argv) {
    VERIFY(argc==2 && getuid()!=0);FILE *file=fopen(argv[1],"rb");VERIFY(file && !fseek(file,0,SEEK_END));long size=ftell(file);VERIFY(size>0 && !fseek(file,0,SEEK_SET));
    void *font=malloc((size_t)size);VERIFY(font && fread(font,1,(size_t)size,file)==(size_t)size && !fclose(file));
    char path[]="files-ime-controller-XXXXXX";VERIFY(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);VERIFY(root>=0);
    for(unsigned i=0;i<10;++i) run_case(i,font,(size_t)size,root);
    VERIFY(!close(root) && !rmdir(path));free(font);puts("Files IME controller:10 public-host-token parser/model cases passed");return 0;
}
