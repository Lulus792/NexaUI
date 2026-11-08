#define _POSIX_C_SOURCE 200809L
#include <nexaui/nexaui.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include AE_FILES_GENERATED_VIEW
#define VERIFY(x) do{if(!(x)){fprintf(stderr,"Files paste %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned worker_calls;
static bool reject_worker(void *p,struct ae_files_session *s){(void)p;(void)s;++worker_calls;return false;}
static void reject_join(void *p){(void)p;VERIFY(false);}
struct source {const char *first,*second;unsigned reads;};
static nx_result read_clip(void *data,char *bytes,size_t capacity,size_t *length) {
    struct source *s=data;const char *value=s->second && s->reads?s->second:s->first;*length=strlen(value);
    if(!bytes)return NX_OK;
    if(capacity<=*length)return NX_ERROR_LIMIT;
    memcpy(bytes,value,*length+1);++s->reads;return NX_OK;
}
static void tick(struct ae_files_view *v) {
    nx_window_status status={0};bool changed=false;
    for(unsigned i=0;i<8;++i){VERIFY(ae_files_view_frame(v,1000,680,&status,&changed));if(!changed)return;}
    VERIFY(false);
}
static void has(nx_text_field *f,const char *expected) {
    nx_utf8_span text;VERIFY(nx_text_field_text(f,&text)==NX_OK && text.length==strlen(expected) && !memcmp(text.data,expected,text.length));
}
static void selected(nx_text_field *f,size_t anchor,size_t cursor) {
    size_t a=0,c=0;VERIFY(nx_text_field_selection(f,&a,&c)==NX_OK && a==anchor && c==cursor);
}
static void command(nx_context *c,nx_text_command command) {
    nx_event e={.type=NX_EVENT_TEXT_COMMAND,.text_command=command};VERIFY(nx_context_queue_event(c,&e)==NX_OK);
}
static void paste(struct ae_files_view *v,unsigned route) {
    if(route){command(v->ui,NX_TEXT_COMMAND_PASTE);return;}
    nx_event e={.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_V,.modifiers=NX_MOD_CONTROL};VERIFY(ae_files_view_event(v,&e));
    e.type=NX_EVENT_KEY_UP;VERIFY(ae_files_view_event(v,&e));
}
static void probe_frame(nx_context *ctx,nx_text_field *f,nx_text_field_desc *desc) {
    nx_interaction_state state;int valid=0,submitted=0;
    VERIFY(nx_begin_frame(ctx,1000,100,0)==NX_OK && nx_text_field_widget(ctx,desc,f,&state,&valid,&submitted)==NX_OK && nx_end_frame(ctx)==NX_OK);
}
/* Inspect history in the exact retained field through a separate public widget.
   This is a component probe, not a user bypass of Files' rejection barrier. */
static void probe_history(struct ae_files_view *v,nx_text_field *f,const char *undo,const char *redo,bool redo_first) {
    nx_context *ctx=NULL;VERIFY(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_set_read_only(f,0)==NX_OK);
    nx_text_field_desc desc={.id=1,.bounds={0,0,900,32},.name={"History probe",13},.fonts=v->fonts,.pixels_per_em=14,.glyph_cache_bytes=1024u*1024u};
    probe_frame(ctx,f,&desc);VERIFY(nx_focus_set(ctx,1)==NX_OK);
    if(redo_first){command(ctx,NX_TEXT_COMMAND_REDO);probe_frame(ctx,f,&desc);has(f,redo);command(ctx,NX_TEXT_COMMAND_UNDO);probe_frame(ctx,f,&desc);has(f,undo);}
    else {command(ctx,NX_TEXT_COMMAND_UNDO);probe_frame(ctx,f,&desc);has(f,undo);command(ctx,NX_TEXT_COMMAND_REDO);probe_frame(ctx,f,&desc);has(f,redo);}
    nx_context_destroy(ctx);
}
static void run_case(unsigned test,bool name,unsigned route,const void *font,size_t length,int root) {
    struct ae_files_session s;ae_files_session_init(&s,reject_worker,reject_join,NULL);enum ae_files_result result;
    s.files=ae_files_create(root,&result);VERIFY(s.files);memcpy(s.root_label,"Documents",10);
    struct ae_files_view *v=NULL;VERIFY(ae_files_view_create(&s,font,length,&v));struct source source={"abcde",NULL,0};
    v->clipboard=(nx_text_clipboard){&source,read_clip,NULL};
    if(name)ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);else s.query_focus=true;
    tick(v);enum ae_files_control control_id=name?AE_FV_NAME:AE_FV_SEARCH;
    VERIFY(nx_focus_set(v->ui,ae_files_view_control_id(v,control_id))==NX_OK);tick(v);
    char long_seed[255],oversized[257];memset(long_seed,'x',254);long_seed[254]=0;memset(oversized,'x',256);oversized[256]=0;
    if(test==2 || test==5)source.first=long_seed;
    paste(v,route);tick(v);nx_text_field *f=v->edits[name?1:0].field;VERIFY(f);has(f,source.first);
    const char *seed=source.first;size_t anchor=strlen(seed)==254?254:4,cursor=strlen(seed)==254?254:1;
    if(test==5)cursor=253;
    if(test==6){anchor=0;cursor=5;}
    VERIFY(nx_text_field_set_selection(f,anchor,cursor)==NX_OK);tick(v);
    if(test==4){source.first="Z";source.reads=0;paste(v,route);tick(v);has(f,"aZe");command(v->ui,NX_TEXT_COMMAND_UNDO);tick(v);has(f,"abcde");selected(f,4,1);}
    source.first=test==1?oversized:test==2 || test==5?"\xc3\xa9":test==6?".":name?"bad/name":"bad\tquery";
    source.second=test==3?"good":NULL;source.reads=0;paste(v,route);if(test==3)paste(v,route);tick(v);
    if(test==5){char expected[256];memset(expected,'x',253);memcpy(expected+253,"\xc3\xa9",3);has(f,expected);VERIFY(!(name?s.name_bad:s.query_bad));probe_history(v,f,long_seed,expected,false);}
    else if(test==6){has(f,".");VERIFY(!(name?s.name_bad:s.query_bad));if(name){nx_event enter={.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ENTER};VERIFY(ae_files_view_event(v,&enter));tick(v);VERIFY(s.dialog && !worker_calls);}probe_history(v,f,seed,".",false);}
    else {
        VERIFY(name?s.name_bad:s.query_bad);has(f,seed);selected(f,anchor,cursor);VERIFY(v->edits[name?1:0].field==f);
        if(test==3)VERIFY(source.reads==2);
        const char *model=name?s.name:ae_files_query(s.files);VERIFY(!strcmp(model,seed));
        if(name){nx_event enter={.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ENTER};VERIFY(ae_files_view_event(v,&enter));tick(v);VERIFY(s.dialog && !worker_calls);}
        probe_history(v,f,test==4?seed:"",test==4?"aZe":seed,test==4);
        VERIFY(name?s.name_bad:s.query_bad);
    }
    VERIFY(!worker_calls);ae_files_view_destroy(v);ae_files_destroy(s.files);
    printf("Files paste case%u name=%u route=%u passed\n",test,(unsigned)name,route);
}
int main(int argc,char **argv) {
    VERIFY(argc==2 && getuid()!=0);FILE *file=fopen(argv[1],"rb");VERIFY(file && !fseek(file,0,SEEK_END));long n=ftell(file);VERIFY(n>0 && !fseek(file,0,SEEK_SET));
    void *font=malloc((size_t)n);VERIFY(font && fread(font,1,(size_t)n,file)==(size_t)n && !fclose(file));
    char path[]="files-paste-XXXXXX";VERIFY(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);VERIFY(root>=0);
    for(unsigned test=0;test<7;++test)for(unsigned name=0;name<2;++name)for(unsigned route=0;route<2;++route)run_case(test,name!=0,route,font,(size_t)n,root);
    VERIFY(!worker_calls && !close(root) && !rmdir(path));free(font);
    puts("Files paste admission:28 original-view name/query keyboard/logical cases and retained-history probes passed");return 0;
}
