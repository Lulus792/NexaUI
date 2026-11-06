#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static void pause_briefly(void){struct timespec delay={0,20000000};nanosleep(&delay,NULL);}
static AtspiAccessible *find(AtspiAccessible *root,const char *name,unsigned depth){
    if(!root || depth>12)return NULL;
    atspi_accessible_clear_cache(root);
    GError *error=NULL;char *text=atspi_accessible_get_name(root,&error);
    int match=!error && text && !strcmp(text,name);g_free(text);
    if(error){g_error_free(error);error=NULL;}if(match)return g_object_ref(root);
    int count=atspi_accessible_get_child_count(root,&error);if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *child=atspi_accessible_get_child_at_index(root,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *result=find(child,name,depth+1);if(child)g_object_unref(child);if(result)return result;
    }
    return NULL;
}
static AtspiAccessible *by_pid(AtspiAccessible *desktop,pid_t pid){
    GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
        guint actual=atspi_accessible_get_process_id(app,&error);
        if(!error && actual==(guint)pid)return app;
        if(error){g_error_free(error);error=NULL;}g_object_unref(app);
    }
    return NULL;
}
static int state(AtspiAccessible *node,AtspiStateType wanted){
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    int result=states && atspi_state_set_contains(states,wanted);if(states)g_object_unref(states);return result;
}
static int readable_text(AtspiAccessible *node,const char *expected){
    GError *error=NULL;atspi_accessible_clear_cache(node);
    AtspiText *text=atspi_accessible_get_text_iface(node);
    if(!text){fprintf(stderr,"Missing AT-SPI Text for a read-only value\n");return 0;}
    char *actual=atspi_text_get_text(text,0,-1,&error);
    int result=!error && actual && !strcmp(actual,expected);g_free(actual);
    if(!error)result=result && atspi_text_get_character_count(text,&error)==g_utf8_strlen(expected,-1);
    if(!error)result=result && atspi_text_get_caret_offset(text,&error)==-1;
    if(!error)result=result && atspi_text_get_n_selections(text,&error)==0;
    if(error){g_error_free(error);error=NULL;result=0;}
    /* Read-only data cannot acquire a caret or an app-owned selection. */
    int moved=atspi_text_set_caret_offset(text,0,&error);
    result=result && !moved;if(error){g_error_free(error);error=NULL;}
    int selected=atspi_text_add_selection(text,0,1,&error);
    result=result && !selected;if(error){g_error_free(error);error=NULL;}
    actual=atspi_text_get_text(text,0,-1,&error);
    result=result && !error && actual && !strcmp(actual,expected);g_free(actual);
    if(!error)result=result && atspi_text_get_caret_offset(text,&error)==-1;
    if(!error)result=result && atspi_text_get_n_selections(text,&error)==0;
    if(error){g_error_free(error);result=0;}
    AtspiEditableText *editable=atspi_accessible_get_editable_text_iface(node);
    result=result && !editable;if(editable)g_object_unref(editable);
    g_object_unref(text);return result;
}
static int retired_text(AtspiText *text){
    GError *error=NULL;char *value=atspi_text_get_text(text,0,-1,&error);
    int result=error && (!value || !value[0]);g_free(value);
    if(error)g_error_free(error);
    return result;
}
static int value(AtspiAccessible *node,const char *expected){
    GError *error=NULL;atspi_accessible_clear_cache(node);GHashTable *attrs=atspi_accessible_get_attributes(node,&error);
    int result=!error && attrs && !g_strcmp0(g_hash_table_lookup(attrs,"nexaui-value"),expected);
    if(attrs)g_hash_table_unref(attrs);
    if(error)g_error_free(error);
    return result && (!expected || readable_text(node,expected));
}
static int activate(AtspiAccessible *node){
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(node);
    int result=action && atspi_action_do_action(action,0,&error) && !error;
    if(action)g_object_unref(action);
    if(error)g_error_free(error);
    return result;
}
static int invoke(AtspiAccessible *node,const char *wanted){
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(node);int okay=0;
    int count=action?atspi_action_get_n_actions(action,&error):0;
    for(int i=0;i<count && !error;++i){
        char *name=atspi_action_get_name(action,i,&error);int match=name && !strcmp(name,wanted);g_free(name);
        if(match && !error){okay=atspi_action_do_action(action,i,&error) && !error;break;}
    }
    if(action)g_object_unref(action);
    if(error)g_error_free(error);
    return okay;
}
static int response(FILE *stream,const char *expected){
    struct pollfd fd={fileno(stream),POLLIN,0};char line[160];
    if(poll(&fd,1,10000)<=0 || !fgets(line,sizeof(line),stream))return 0;
    printf("HOST %s",line);return !strcmp(line,expected);
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"removal-atspi:%d: %s\n",__LINE__,#x);goto cleanup;}}while(0)
static int edit(AtspiAccessible *node,const char *value){
    GError *error=NULL;AtspiEditableText *text=atspi_accessible_get_editable_text_iface(node);
    int result=text && atspi_editable_text_set_text_contents(text,value,&error) && !error;
    if(text)g_object_unref(text);
    if(error)g_error_free(error);
    return result;
}
static int text_is(AtspiAccessible *node,const char *expected){
    GError *error=NULL;atspi_accessible_clear_cache(node);AtspiText *text=atspi_accessible_get_text_iface(node);
    char *value=text?atspi_text_get_text(text,0,-1,&error):NULL;
    int result=!error && value && !strcmp(value,expected);g_free(value);
    if(text)g_object_unref(text);
    if(error)g_error_free(error);
    return result;
}
static int focus(AtspiAccessible *node){
    GError *error=NULL;AtspiComponent *component=atspi_accessible_get_component_iface(node);
    int result=component && atspi_component_grab_focus(component,&error) && !error;
    if(component)g_object_unref(component);
    if(error)g_error_free(error);
    return result;
}
static int contained(AtspiAccessible *child,AtspiAccessible *parent){
    GError *error=NULL;AtspiComponent *a=atspi_accessible_get_component_iface(child),*b=atspi_accessible_get_component_iface(parent);
    AtspiRect *inner=a?atspi_component_get_extents(a,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    AtspiRect *outer=b && !error?atspi_component_get_extents(b,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int result=!error && inner && outer && inner->width>0 && inner->height>0 && inner->x>=outer->x && inner->y>=outer->y &&
        inner->x+inner->width<=outer->x+outer->width && inner->y+inner->height<=outer->y+outer->height;
    if(!result && inner && outer && getenv("NEXAUI_REMOVAL_BOUNDS_TRACE"))
        fprintf(stderr,"FILTER_BOUNDS child=%d,%d,%d,%d viewport=%d,%d,%d,%d\n",inner->x,inner->y,inner->width,inner->height,outer->x,outer->y,outer->width,outer->height);
    g_free(inner);g_free(outer);if(a)g_object_unref(a);if(b)g_object_unref(b);if(error)g_error_free(error);
    return result;
}
static int filter_preview(AtspiAccessible *app,AtspiAccessible *reviewed,int large,int incomplete,int ime,FILE *responses){
    int okay=0;AtspiAccessible *button=NULL,*field=NULL,*old_field=NULL,*apply=NULL,*cancel=NULL,*object=NULL,*scroll=NULL,*group=NULL,*preview=NULL;
    button=find(app,"Filter preview",0);CHECK(button && focus(button));
    for(unsigned i=0;i<100 && !state(button,ATSPI_STATE_FOCUSED);++i)pause_briefly();
    CHECK(state(button,ATSPI_STATE_FOCUSED) && activate(button));
    for(unsigned i=0;i<150 && !field;++i){field=find(app,"Object name filter",0);if(!field)pause_briefly();}
    CHECK(field && state(field,ATSPI_STATE_FOCUSED) && edit(field,"discarded draft"));
    cancel=find(app,"Cancel",0);CHECK(cancel && activate(cancel));old_field=field;field=NULL;
    for(unsigned i=0;i<100 && !state(old_field,ATSPI_STATE_DEFUNCT);++i)pause_briefly();
    CHECK(state(old_field,ATSPI_STATE_DEFUNCT) && state(button,ATSPI_STATE_FOCUSED) && activate(button));
    g_object_unref(cancel);cancel=NULL;
    for(unsigned i=0;i<150 && !field;++i){field=find(app,"Object name filter",0);if(!field)pause_briefly();}
    CHECK(field && !edit(old_field,"late old edit") && text_is(field,"") && state(field,ATSPI_STATE_FOCUSED));
    apply=find(app,"Apply filter",0);CHECK(apply && !activate(reviewed));
    group=find(app,"Remove in this scope",0);CHECK(group && value(group,large?"20000 objects":"4 objects"));
    if(ime){
        puts("REMOVAL_FILTER_PREEDIT");CHECK(response(responses,"REMOVAL_PREEDIT 7\n") && text_is(field,""));
        puts("REMOVAL_FILTER_CANCEL_PREEDIT");CHECK(response(responses,"REMOVAL_PREEDIT 0\n") && text_is(field,""));
        puts("REMOVAL_FILTER_REFOCUS");
        for(unsigned i=0;i<100 && !state(field,ATSPI_STATE_FOCUSED);++i)pause_briefly();
        CHECK(state(field,ATSPI_STATE_FOCUSED));puts("REMOVAL_FILTER_COMMIT");
        for(unsigned i=0;i<100 && !text_is(field,"\xc3\xa9");++i)pause_briefly();
        CHECK(text_is(field,"\xc3\xa9"));
    } else {
        char oversized[130];memset(oversized,'x',129);oversized[129]=0;CHECK(edit(field,oversized));
        for(unsigned i=0;i<100 && !state(field,ATSPI_STATE_INVALID_ENTRY);++i)pause_briefly();
        CHECK(state(field,ATSPI_STATE_INVALID_ENTRY) && !state(apply,ATSPI_STATE_ENABLED) && !activate(apply));
        const char *query=large?(incomplete?"managed-object-000000":"MANAGED-OBJECT-099999"):"Application code";
        CHECK(edit(field,query));
        for(unsigned i=0;i<100 && !state(apply,ATSPI_STATE_ENABLED);++i)pause_briefly();
        CHECK(text_is(field,query) && state(apply,ATSPI_STATE_ENABLED));
    }
    CHECK(value(group,large?"20000 objects":"4 objects"));g_object_unref(group);group=NULL;
    CHECK(activate(apply));
    for(unsigned i=0;i<100 && !state(field,ATSPI_STATE_DEFUNCT);++i)pause_briefly();
    CHECK(state(field,ATSPI_STATE_DEFUNCT) && !edit(field,"late closed edit"));
    const char *name=ime?"\xc3\xa9":large?(incomplete?"managed-object-000000":"managed-object-099999"):"Application code";
    preview=find(app,"Complete removal preview",0);CHECK(preview);scroll=find(app,"Vertical scrollbar",0);
    for(unsigned i=0;i<32;++i){
        if(!object)object=find(app,name,0);
        if(object && contained(object,preview))break;
        CHECK(scroll && invoke(scroll,"increment"));pause_briefly();
    }
    CHECK(object && state(object,ATSPI_STATE_SHOWING) && contained(object,preview));
    group=find(app,incomplete?"Unknown or unmanaged":large?"Backups or remote copies":"Remove in this scope",0);CHECK(group);
    CHECK(value(group,incomplete?"0 of 1 objects shown":large?"1 of 20000 objects shown":"1 of 4 objects shown"));
    if(!incomplete){
        CHECK(focus(reviewed));for(unsigned i=0;i<100 && !state(reviewed,ATSPI_STATE_FOCUSED);++i)pause_briefly();
        CHECK(state(reviewed,ATSPI_STATE_FOCUSED));
    }
    okay=1;
cleanup:
    if(preview)g_object_unref(preview);
    if(group)g_object_unref(group);
    if(scroll)g_object_unref(scroll);
    if(object)g_object_unref(object);
    if(cancel)g_object_unref(cancel);
    if(apply)g_object_unref(apply);
    if(old_field)g_object_unref(old_field);
    if(field)g_object_unref(field);
    if(button)g_object_unref(button);
    return okay;
}
static int refresh_refs(AtspiAccessible *desktop,pid_t pid,unsigned revision,AtspiAccessible **app,
                        AtspiAccessible **reviewed,AtspiAccessible **request,AtspiAccessible **close_view){
    char expected[96];snprintf(expected,sizeof(expected),"00000000000000000000000000000001 / revision %u",revision);
    for(unsigned i=0;i<150;++i){
        AtspiAccessible *next=by_pid(desktop,pid),*plan=next?find(next,"Plan",0):NULL;
        AtspiAccessible *a=NULL,*b=NULL,*c=NULL;
        if(plan && value(plan,expected)){a=find(next,"Entire plan reviewed",0);b=find(next,"Request removal",0);c=find(next,"Close view",0);}
        if(plan)g_object_unref(plan);
        if(a && b && c){
            if(*app)g_object_unref(*app);
            if(*reviewed)g_object_unref(*reviewed);
            if(*request)g_object_unref(*request);
            if(*close_view)g_object_unref(*close_view);
            *app=next;*reviewed=a;*request=b;*close_view=c;return 1;
        }
        if(a)g_object_unref(a);
        if(b)g_object_unref(b);
        if(c)g_object_unref(c);
        if(next)g_object_unref(next);
        pause_briefly();
    }
    return 0;
}
static int selection_is(AtspiAccessible *node){
    GError *error=NULL;AtspiText *text=atspi_accessible_get_text_iface(node);
    AtspiRange *range=text?atspi_text_get_selection(text,0,&error):NULL;
    int okay=!error && range && range->start_offset==0 && range->end_offset==1;
    g_free(range);if(text)g_object_unref(text);if(error)g_error_free(error);return okay;
}
static int refresh_before(AtspiAccessible *desktop,pid_t pid,FILE *commands,FILE *responses,int draft,int large,int filtered,
                          AtspiAccessible **app,AtspiAccessible **reviewed,AtspiAccessible **request,AtspiAccessible **close_view,AtspiAccessible *old_plan){
    int okay=0;AtspiAccessible *field=NULL,*next_field=NULL,*retired_request=g_object_ref(*request),*refresh=NULL,*cancel=NULL,*object=NULL,*preview=NULL;
    AtspiText *text=NULL;GError *error=NULL;
    if(draft){
        AtspiAccessible *button=find(*app,"Filter preview",0);CHECK(button && focus(button));
        for(unsigned i=0;i<100 && !state(button,ATSPI_STATE_FOCUSED);++i)pause_briefly();
        int opened=activate(button);g_object_unref(button);CHECK(opened);
        for(unsigned i=0;i<100 && !field;++i){field=find(*app,"Object name filter",0);if(!field)pause_briefly();}CHECK(field && edit(field,"before"));
        for(unsigned i=0;i<100 && !text_is(field,"before");++i)pause_briefly();
        CHECK(text_is(field,"before") && edit(field,"draft"));
        for(unsigned i=0;i<100 && !text_is(field,"draft");++i)pause_briefly();
        CHECK(text_is(field,"draft"));
        text=atspi_accessible_get_text_iface(field);CHECK(text && atspi_text_add_selection(text,0,1,&error) && !error);
        for(unsigned i=0;i<100 && !selection_is(field);++i)pause_briefly();
        CHECK(selection_is(field));
        puts("REMOVAL_REFRESH_PREEDIT");CHECK(response(responses,"REMOVAL_PREEDIT 7\n") && text_is(field,"draft"));
        CHECK(fputs("refresh\n",commands)>=0 && !fflush(commands) && response(responses,"REMOVAL_REFRESHED revision=43\n") &&
              response(responses,"REMOVAL_PREEDIT 0\n"));
    } else {
        CHECK(fputs("revise\n",commands)>=0 && !fflush(commands) && response(responses,"REMOVAL_REVISION_READY revision=43\n"));
        CHECK(value(old_plan,"00000000000000000000000000000001 / revision 42") && state(retired_request,ATSPI_STATE_DEFUNCT) && !activate(retired_request));
        refresh=find(*app,"Refresh preview",0);CHECK(refresh && activate(refresh) && response(responses,"REMOVAL_REFRESHED revision=43\n"));
    }
    CHECK(refresh_refs(desktop,pid,43,app,reviewed,request,close_view));
    CHECK(state(retired_request,ATSPI_STATE_DEFUNCT) && !activate(retired_request) && state(old_plan,ATSPI_STATE_DEFUNCT) &&
          !state(*reviewed,ATSPI_STATE_CHECKED) && !state(*request,ATSPI_STATE_ENABLED));
    if(draft){
        next_field=find(*app,"Object name filter",0);CHECK(next_field && text_is(next_field,"draft") && selection_is(next_field) &&
            state(next_field,ATSPI_STATE_FOCUSED) && state(field,ATSPI_STATE_DEFUNCT) && !edit(field,"late edit") && retired_text(text));
        puts("REMOVAL_REFRESH_UNDO");for(unsigned i=0;i<100 && !text_is(next_field,"before");++i)pause_briefly();CHECK(text_is(next_field,"before"));
        cancel=find(*app,"Cancel",0);CHECK(cancel && activate(cancel));
        for(unsigned i=0;i<100 && !state(next_field,ATSPI_STATE_DEFUNCT);++i)pause_briefly();
        CHECK(state(next_field,ATSPI_STATE_DEFUNCT));
    }
    if(large && filtered){
        object=find(*app,"managed-object-099999",0);preview=find(*app,"Complete removal preview",0);
        CHECK(object && preview && contained(object,preview) && value(object,"Backups or remote copies. This request does not remove backup or remote copies\nObject: 000000000000000000000000000186a0\nLogical bytes: 65536"));
    }
    CHECK(focus(*reviewed));for(unsigned i=0;i<100 && !state(*reviewed,ATSPI_STATE_FOCUSED);++i)pause_briefly();CHECK(state(*reviewed,ATSPI_STATE_FOCUSED));
    okay=1;
cleanup:
    if(error)g_error_free(error);
    if(text)g_object_unref(text);
    if(preview)g_object_unref(preview);
    if(object)g_object_unref(object);
    if(cancel)g_object_unref(cancel);
    if(refresh)g_object_unref(refresh);
    if(retired_request)g_object_unref(retired_request);
    if(next_field)g_object_unref(next_field);
    if(field)g_object_unref(field);
    return okay;
}
static int refresh_issued(AtspiAccessible *desktop,pid_t pid,FILE *commands,FILE *responses,AtspiAccessible **app,
                          AtspiAccessible **reviewed,AtspiAccessible **request,AtspiAccessible **close_view){
    int okay=0;AtspiAccessible *old_request=g_object_ref(*request),*plan=NULL,*refresh=NULL,*status=NULL;AtspiText *text=NULL;
    CHECK(fputs("refresh\n",commands)>=0 && !fflush(commands) && response(responses,"REMOVAL_REFRESHED revision=43\n") &&
          refresh_refs(desktop,pid,43,app,reviewed,request,close_view));
    CHECK(state(old_request,ATSPI_STATE_DEFUNCT) && !activate(old_request) && !state(*reviewed,ATSPI_STATE_ENABLED) && !activate(*request));
    plan=find(*app,"Plan",0);CHECK(plan);text=atspi_accessible_get_text_iface(plan);CHECK(text);
    CHECK(fputs("revise\n",commands)>=0 && !fflush(commands) && response(responses,"REMOVAL_REVISION_READY revision=44\n") &&
        value(plan,"00000000000000000000000000000001 / revision 43"));
    refresh=find(*app,"Refresh preview",0);CHECK(refresh && activate(refresh) && response(responses,"REMOVAL_REFRESHED revision=44\n") &&
        refresh_refs(desktop,pid,44,app,reviewed,request,close_view));
    CHECK(state(plan,ATSPI_STATE_DEFUNCT) && retired_text(text) && !activate(*reviewed) && !activate(*request));
    status=find(*app,"Request revision 42 handed to owner; outcome unknown",0);CHECK(status && !state(status,ATSPI_STATE_BUSY));
    GError *error=NULL;char *description=atspi_accessible_get_description(status,&error);
    int described=!error && description && strstr(description,"Original request revision 42;");g_free(description);if(error)g_error_free(error);CHECK(described);
    CHECK(focus(*close_view));for(unsigned i=0;i<100 && !state(*close_view,ATSPI_STATE_FOCUSED);++i)pause_briefly();CHECK(state(*close_view,ATSPI_STATE_FOCUSED));
    okay=1;
cleanup:
    if(status)g_object_unref(status);
    if(text)g_object_unref(text);
    if(refresh)g_object_unref(refresh);
    if(plan)g_object_unref(plan);
    if(old_request)g_object_unref(old_request);
    return okay;
}
int main(int argc,char **argv){
    int okay=0,input[2]={-1,-1},output[2]={-1,-1};pid_t child=-1;FILE *commands=NULL,*responses=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*reviewed=NULL,*request=NULL,*close_view=NULL,*old_plan=NULL,*status=NULL,*old_request=NULL;
    AtspiText *held_text=NULL;
    const char *font=getenv("NEXAUI_REMOVAL_FONT");int incomplete=getenv("NEXAUI_REMOVAL_INCOMPLETE")!=NULL;
    int keyboard=getenv("NEXAUI_REMOVAL_KEYBOARD")!=NULL;setvbuf(stdout,NULL,_IONBF,0);
    int large=getenv("NEXAUI_REMOVAL_LARGE")!=NULL;
    int filtering=getenv("NEXAUI_REMOVAL_FILTER")!=NULL,ime=getenv("NEXAUI_REMOVAL_FILTER_IME")!=NULL;
    const char *refresh_mode=getenv("NEXAUI_REMOVAL_REFRESH_MODE");unsigned requested_revision=42;
    signal(SIGPIPE,SIG_IGN);
    CHECK(argc==2 && font && !pipe(input) && !pipe(output));child=fork();CHECK(child>=0);
    if(!child){
        if(dup2(input[0],STDIN_FILENO)<0 || dup2(output[1],STDOUT_FILENO)<0)_exit(125);
        close(input[0]);close(input[1]);close(output[0]);close(output[1]);
        execl(argv[1],argv[1],font,(char *)NULL);_exit(126);
    }
    close(input[0]);input[0]=-1;close(output[1]);output[1]=-1;
    commands=fdopen(input[1],"w");CHECK(commands);input[1]=-1;
    responses=fdopen(output[0],"r");CHECK(responses);output[0]=-1;setvbuf(responses,NULL,_IONBF,0);
    CHECK(response(responses,"REMOVAL_READY\n") && atspi_init()==0);desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned i=0;i<150 && !reviewed;++i){
        if(app){g_object_unref(app);app=NULL;}app=by_pid(desktop,child);
        if(app)reviewed=find(app,"Entire plan reviewed",0);
        if(!reviewed)pause_briefly();
    }
    CHECK(reviewed);request=find(app,"Request removal",0);close_view=find(app,"Close view",0);old_plan=find(app,"Plan",0);
    CHECK(request && close_view && old_plan && value(old_plan,"00000000000000000000000000000001 / revision 42"));
    held_text=atspi_accessible_get_text_iface(old_plan);CHECK(held_text);
    AtspiAccessible *installation=find(app,"Installation",0),*scope=find(app,"Scope",0);
    CHECK(installation && scope && value(installation,"00000000000000000000000000000002") &&
        value(scope,"Selected user's managed installation (00000000000000000000000000000003)"));
    g_object_unref(installation);g_object_unref(scope);
    CHECK(!state(request,ATSPI_STATE_ENABLED) && !activate(request));
    const char *groups[]={"Remove in this scope","Retained: shared dependency","Retained: explicit choice",
        "Independent documents","Backups or remote copies","Unknown or unmanaged"};
    for(unsigned i=0;i<6;++i){AtspiAccessible *group=find(app,groups[i],0);CHECK(group);
        const char *expected=large?(i==5?(incomplete?"1 object":"0 objects"):i==4 && incomplete?"19999 objects":"20000 objects"):
            i==0?"4 objects":i==5?"0 objects":"1 object";
        CHECK(value(group,expected));g_object_unref(group);}
    const char *objects[]={ime?"\xc3\xa9":"Application code","Private settings and cache","Credential registration","Owned background service",
        "Shared support library","Profile deliberately kept","Independent project document","Backup outside this action"};
    const char *reasons[]={"Owned by this installation","Managed private state in the selected user scope",
        "Revoke the app-owned credential registration","Stop and unregister only this app's background work",
        "Another installed application still requires this component","The user explicitly chose to retain this profile",
        "User-created document outside automatic app-state removal","This request does not remove backup or remote copies"};
    if(!large)for(unsigned i=0;i<8;++i){
        AtspiAccessible *object=find(app,objects[i],0);char expected[512];CHECK(object);
        snprintf(expected,sizeof(expected),"%s. %s\nObject: %032x\nLogical bytes: %u",
            groups[i<4?0:i-3],reasons[i],10+i,4096*(i+1));
        CHECK(value(object,expected));g_object_unref(object);
    }
    if(large && !incomplete){
        AtspiAccessible *scroll=find(app,"Vertical scrollbar",0),*object=NULL;CHECK(scroll);
        if(keyboard){
            CHECK(state(reviewed,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_LARGE_FOCUS_SCROLL");
            for(unsigned i=0;i<100 && !state(scroll,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(scroll,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_LARGE_END");
            for(unsigned i=0;i<100 && !object;++i){object=find(app,"managed-object-099999",0);if(!object)pause_briefly();}
            CHECK(object && value(object,"Backups or remote copies. This request does not remove backup or remote copies\nObject: 000000000000000000000000000186a0\nLogical bytes: 32768"));
            puts("REMOVAL_KEY_LARGE_FOCUS_ACK");
            for(unsigned i=0;i<100 && !state(reviewed,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(reviewed,ATSPI_STATE_FOCUSED));
        } else {
            CHECK(invoke(scroll,"increment"));
            for(unsigned i=0;i<100 && !object;++i){object=find(app,"managed-object-000000",0);if(!object)pause_briefly();}
            CHECK(object);
        }
        AtspiAccessible *preview=find(app,"Complete removal preview",0);GError *error=NULL;CHECK(preview);
        int children=atspi_accessible_get_child_count(preview,&error);CHECK(!error && children>0 && children<300);
        g_object_unref(preview);g_object_unref(object);g_object_unref(scroll);
    }
    if(filtering)CHECK(filter_preview(app,reviewed,large,incomplete,ime,responses));
    if(refresh_mode && strcmp(refresh_mode,"issued")){
        CHECK(refresh_before(desktop,child,commands,responses,!strcmp(refresh_mode,"draft"),large,filtering,&app,&reviewed,&request,&close_view,old_plan));requested_revision=43;
    }
    if(incomplete){
        status=find(app,"Inventory incomplete; request unavailable",0);CHECK(status && !state(reviewed,ATSPI_STATE_ENABLED));
        CHECK(activate(close_view) && response(responses,"REMOVAL_CLOSED requests=0 reopened=0\n"));
    } else {
        if(keyboard){CHECK(state(reviewed,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_CHECK");}
        else CHECK(activate(reviewed));
        for(unsigned i=0;i<100 && !state(reviewed,ATSPI_STATE_CHECKED);++i)pause_briefly();
        CHECK(state(reviewed,ATSPI_STATE_CHECKED));
        for(unsigned i=0;i<100 && !state(request,ATSPI_STATE_ENABLED);++i)pause_briefly();
        CHECK(state(request,ATSPI_STATE_ENABLED));
        if(keyboard){
            puts("REMOVAL_KEY_FOCUS_CLOSE");for(unsigned i=0;i<100 && !state(close_view,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(close_view,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_FOCUS_REQUEST");
            for(unsigned i=0;i<100 && !state(request,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(request,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_REQUEST");
        } else CHECK(activate(request));
        char requested[96];snprintf(requested,sizeof(requested),"REMOVAL_REQUESTED revision=%u\n",requested_revision);CHECK(response(responses,requested));
        CHECK(fputs("reopen\n",commands)>=0 && !fflush(commands) && response(responses,"REMOVAL_REOPENED\n"));
        g_object_unref(app);app=NULL;old_request=request;request=NULL;g_object_unref(close_view);close_view=NULL;
        for(unsigned i=0;i<150 && !request;++i){
            if(app){g_object_unref(app);app=NULL;}app=by_pid(desktop,child);
            if(app)request=find(app,"Request removal",0);
            if(!request)pause_briefly();
        }
        CHECK(request && !state(request,ATSPI_STATE_ENABLED) && state(old_plan,ATSPI_STATE_DEFUNCT) && retired_text(held_text));
        CHECK(!activate(request) && state(old_request,ATSPI_STATE_DEFUNCT) && !activate(old_request));
        status=find(app,"Request handed to owner; outcome unknown",0);CHECK(status && !state(status,ATSPI_STATE_BUSY));
        close_view=find(app,"Close view",0);CHECK(close_view);
        if(refresh_mode && !strcmp(refresh_mode,"issued"))CHECK(refresh_issued(desktop,child,commands,responses,&app,&reviewed,&request,&close_view));
        if(keyboard){CHECK(state(close_view,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_CLOSE");}
        else CHECK(activate(close_view));
        CHECK(response(responses,"REMOVAL_CLOSED requests=1 reopened=1\n"));
    }
    {int status_code=0;CHECK(waitpid(child,&status_code,0)==child && WIFEXITED(status_code) && WEXITSTATUS(status_code)==0);child=-1;}
    okay=1;puts("Independent removal preview, exact plan, disabled action, view replacement and clean close passed");
cleanup:
    if(child>0){
        if(commands){fputs("close\n",commands);fflush(commands);}
        int status_code=0;pid_t ended=0;
        for(unsigned i=0;i<100 && !(ended=waitpid(child,&status_code,WNOHANG));++i)pause_briefly();
        if(!ended){kill(child,SIGKILL);waitpid(child,&status_code,0);}
    }
    if(held_text)g_object_unref(held_text);
    if(status)g_object_unref(status);
    if(old_plan)g_object_unref(old_plan);
    if(old_request)g_object_unref(old_request);
    if(close_view)g_object_unref(close_view);
    if(request)g_object_unref(request);
    if(reviewed)g_object_unref(reviewed);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    atspi_exit();
    if(commands)fclose(commands);
    if(responses)fclose(responses);
    for(unsigned i=0;i<2;++i){if(input[i]>=0)close(input[i]);if(output[i]>=0)close(output[i]);}
    return okay?0:1;
}
