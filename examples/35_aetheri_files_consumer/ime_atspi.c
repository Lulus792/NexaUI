/* Reuse the original independent peer's bounded discovery and text checks.
 * This test peer links libatspi only; no NexaUI or Files model linkage. */
#define main original_files_peer_main
#include "files_view_atspi.c"
#undef main

int main(int argc,char **argv) {
    if(argc==3 && (!strcmp(argv[2],"clipboard-copy") || !strcmp(argv[2],"clipboard-cut") || !strcmp(argv[2],"clipboard-paste"))) {
        char *end=NULL;unsigned long pid=strtoul(argv[1],&end,10);
        if(!pid || !end || *end || atspi_init()) return 2;
        atspi_set_timeout(1000,1000);
        AtspiAccessible *desktop=atspi_get_desktop(0),*app=desktop?target(desktop,pid):NULL;
        AtspiAccessible *field=app?lookup(app,"Name",ATSPI_ROLE_ENTRY):NULL;
        AtspiEditableText *edit=field?atspi_accessible_get_editable_text_iface(field):NULL;
        AtspiText *text=field?atspi_accessible_get_text_iface(field):NULL;GError *error=NULL;
        int count=text?atspi_text_get_character_count(text,&error):-1,okay=0;
        if(edit && !error && count>=0) {
            if(!strcmp(argv[2],"clipboard-copy")) okay=atspi_editable_text_copy_text(edit,0,count,&error);
            else if(!strcmp(argv[2],"clipboard-cut")) okay=atspi_editable_text_cut_text(edit,0,count,&error);
            else okay=atspi_editable_text_paste_text(edit,0,&error);
        }
        if(!okay || error) fprintf(stderr,"Clipboard peer mode=%s count=%d edit=%d error=%s\n",argv[2],count,edit!=NULL,error?error->message:"none");
        if(error) {g_error_free(error);okay=0;}
        if(text) g_object_unref(text);
        if(edit) g_object_unref(edit);
        if(field) g_object_unref(field);
        if(app) g_object_unref(app);
        if(desktop) g_object_unref(desktop);
        atspi_exit();
        if(!okay) {fputs("Native clipboard AT-SPI operation failed\n",stderr);return 1;}
        puts("Native clipboard AT-SPI operation completed");return 0;
    }
    if(argc==3 && strcmp(argv[2],"preedit-no-placeholder")) return original_files_peer_main(argc,argv);
    if(argc==3) {
        char *end=NULL;unsigned long pid=strtoul(argv[1],&end,10);
        if(!pid || !end || *end || atspi_init()) return 2;
        atspi_set_timeout(1000,1000);
        AtspiAccessible *desktop=atspi_get_desktop(0),*app=desktop?target(desktop,pid):NULL;
        int okay=0;double deadline=now()+3;
        while(app && now()<deadline) {
            AtspiAccessible *field=lookup(app,"Search this folder",ATSPI_ROLE_ENTRY);
            AtspiAccessible *placeholder=lookup(app,"Search this folder",ATSPI_ROLE_LABEL);
            okay=field && !placeholder;
            if(field) g_object_unref(field);
            if(placeholder) g_object_unref(placeholder);
            if(okay) break;
            pump();
        }
        if(app) g_object_unref(app);
        if(desktop) g_object_unref(desktop);
        atspi_exit();
        if(!okay) {fputs("Native preedit placeholder remained visible\n",stderr);return 1;}
        puts("Native preedit suppresses original placeholder");return 0;
    }
    if(argc!=5 && argc!=7) return 2;
    char *end=NULL;unsigned long pid=strtoul(argv[1],&end,10);
    if(!pid || !end || *end) return 2;
    long cursor=strtol(argv[4],&end,10);
    glong count=g_utf8_strlen(argv[3],-1);
    if(!end || *end || cursor<0 || cursor>1024 || count<0 || count>1024) return 2;
    long first=-1,last=-1;
    if(argc==7) {
        first=strtol(argv[5],&end,10);if(!end || *end || first<0 || first>count) return 2;
        last=strtol(argv[6],&end,10);if(!end || *end || last<first || last>count) return 2;
    }
    if(atspi_init()) return 2;
    atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0);
    AtspiAccessible *app=desktop?target(desktop,pid):NULL;
    int okay=app && await_text(app,argv[2],argv[3],(int)count,(int)cursor,(int)first,(int)last);
    if(app) g_object_unref(app);
    if(desktop) g_object_unref(desktop);
    atspi_exit();
    if(!okay) {fputs("Native IME committed-text/caret expectation failed\n",stderr);return 1;}
    puts("Native IME independent text/caret check passed");return 0;
}
