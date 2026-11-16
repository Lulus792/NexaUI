#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <physim/experiment.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Physim peer line %d: %s\n",__LINE__,#x);goto done; } } while(0)
extern const ps_experiment_api *ps_get_experiment(void);
static void pump(void) { while(g_main_context_iteration(NULL,FALSE)){}const struct timespec t={0,10000000};nanosleep(&t,NULL); }
static int state(AtspiAccessible *node,AtspiStateType flag) {
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    int yes=states&&atspi_state_set_contains(states,flag);if(states)g_object_unref(states);return yes;
}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node||depth>8)return NULL;
    atspi_accessible_clear_cache(node);char *s=atspi_accessible_get_name(node,NULL);
    int field=!strcmp(name,"Length (m)")||!strcmp(name,"Initial angle (rad)");
    int match=s&&!strcmp(s,name)&&(!field||atspi_accessible_get_role(node,NULL)==ATSPI_ROLE_ENTRY);
    g_free(s);if(match)return g_object_ref(node);
    int count=atspi_accessible_get_child_count(node,NULL);if(count>32)return NULL;
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,NULL),*found=find(child,name,depth+1u);
        if(child)g_object_unref(child);if(found)return found;
    }
    return NULL;
}
static AtspiAccessible *application(AtspiAccessible *desktop,pid_t pid) {
    for(unsigned attempt=0;attempt<200;++attempt) {
        atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,NULL);
        if(count>16)return NULL;
        for(int i=0;i<count;++i) {
            AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,NULL);if(!app)continue;
            if(atspi_accessible_get_process_id(app,NULL)==(guint)pid)return app;g_object_unref(app);
        }
        pump();
    }
    return NULL;
}
static int observe(AtspiAccessible *app,const char *name,int focused) {
    for(unsigned i=0;i<200;++i) {
        AtspiAccessible *node=find(app,name,0);int yes=node&&(!focused||state(node,ATSPI_STATE_FOCUSED));
        if(node)g_object_unref(node);if(yes)return 1;pump();
    }
    return 0;
}
static int focus(AtspiAccessible *app,const char *name) {
    AtspiAccessible *node=find(app,name,0);AtspiComponent *c=node?atspi_accessible_get_component_iface(node):NULL;
    GError *error=NULL;int yes=c&&atspi_component_grab_focus(c,&error)&&!error;
    if(error)g_error_free(error);if(c)g_object_unref(c);if(node)g_object_unref(node);
    return yes&&observe(app,name,1);
}
static int press(AtspiAccessible *app,const char *name) {
    AtspiAccessible *node=find(app,name,0);AtspiAction *a=node?atspi_accessible_get_action_iface(node):NULL;
    GError *error=NULL;int yes=0,count=a?atspi_action_get_n_actions(a,&error):0;
    for(int i=0;!error&&i<count&&i<16;++i) {
        char *s=atspi_action_get_action_name(a,i,&error);int match=s&&!strcmp(s,"click");g_free(s);
        if(match&&!error) { yes=atspi_action_do_action(a,i,&error)&&!error;break; }
    }
    if(error)g_error_free(error);if(a)g_object_unref(a);if(node)g_object_unref(node);return yes;
}
static int edit(AtspiAccessible *node,const char *value) {
    AtspiEditableText *e=node?atspi_accessible_get_editable_text_iface(node):NULL;GError *error=NULL;
    int yes=e&&atspi_editable_text_set_text_contents(e,value,&error)&&!error;
    if(error)g_error_free(error);if(e)g_object_unref(e);return yes;
}
static int select_all(AtspiAccessible *node) {
    AtspiText *text=node?atspi_accessible_get_text_iface(node):NULL;GError *error=NULL;
    int length=text?atspi_text_get_character_count(text,&error):0;
    int count=text&&!error?atspi_text_get_n_selections(text,&error):-1,yes=0;
    if(!error&&length==3&&count==0)yes=atspi_text_add_selection(text,0,length,&error);
    else if(!error&&length==3&&count==1)yes=atspi_text_set_selection(text,0,0,length,&error);
    AtspiRange *range=yes&&!error?atspi_text_get_selection(text,0,&error):NULL;
    yes=range&&!error&&range->start_offset==0&&range->end_offset==length;
    if(range)g_boxed_free(ATSPI_TYPE_RANGE,range);if(error)g_error_free(error);if(text)g_object_unref(text);return yes;
}
static int text_is(AtspiAccessible *app,const char *name,const char *value) {
    for(unsigned i=0;i<200;++i) {
        AtspiAccessible *node=find(app,name,0);AtspiText *t=node?atspi_accessible_get_text_iface(node):NULL;
        GError *error=NULL;char *s=t?atspi_text_get_text(t,0,-1,&error):NULL;int yes=s&&!error&&!strcmp(s,value);
        g_free(s);if(error)g_error_free(error);if(t)g_object_unref(t);if(node)g_object_unref(node);
        if(yes)return 1;pump();
    }
    return 0;
}
static int marker(FILE *file,const char *wanted) {
    char buffer[65536];
    for(unsigned i=0;i<200;++i) {
        ssize_t n=pread(fileno(file),buffer,sizeof(buffer)-1u,0);
        if(n<0||(size_t)n==sizeof(buffer)-1u)return 0;buffer[n]=0;
        if(strstr(buffer,wanted))return 1;pump();
    }
    fprintf(stderr,"Missing Physim marker: %s\n",wanted);return 0;
}
static int outer_window(char id[32],Window *window) {
    const char *path=getenv("NEXAUI_TEST_WESTON_LOG");FILE *f=path?fopen(path,"r"):NULL;if(!f)return 0;
    char line[1024];unsigned count=0;
    while(fgets(line,sizeof(line),f)) {
        const char *label=strstr(line,"x11 output "),*start=label?strstr(label,"window id "):NULL;
        if(!start)continue;start+=strlen("window id ");size_t size=strspn(start,"0123456789");
        if(!size||size>=32) { count=0;break; }memcpy(id,start,size);id[size]=0;++count;
    }
    fclose(f);if(count!=1)return 0;*window=(Window)strtoul(id,NULL,10);return *window!=0;
}
static int drive(const char *command,const char *value) {
    const char *driver=getenv("NEXAUI_PHYSIM_XDOTOOL");if(!driver)return 0;
    pid_t pid=fork();if(!pid) {
        if(!strcmp(command,"park"))execl(driver,driver,"mousemove","1","1",(char *)NULL);
        else execl(driver,driver,command,value,(char *)NULL);_exit(127);
    }
    if(pid<0)return 0;int status=0;pid_t ended;
    do{ended=waitpid(pid,&status,0);}while(ended<0&&errno==EINTR);
    return ended==pid&&WIFEXITED(status)&&!WEXITSTATUS(status);
}
typedef struct screen_point { int x,y; } screen_point;
static int click_at(const char *window,screen_point point) {
    const char *driver=getenv("NEXAUI_PHYSIM_XDOTOOL");if(!driver)return 0;
    char x[16],y[16];snprintf(x,sizeof(x),"%d",point.x);snprintf(y,sizeof(y),"%d",point.y);
    pid_t pid=fork();
    if(!pid) { execl(driver,driver,"mousemove","--window",window,x,y,"click","1",(char *)NULL);_exit(127); }
    if(pid<0)return 0;int status=0;pid_t ended;
    do{ended=waitpid(pid,&status,0);}while(ended<0&&errno==EINTR);
    return ended==pid&&WIFEXITED(status)&&!WEXITSTATUS(status);
}
static int original_bob(double length,double *x,double *y,double *radius) {
    const ps_experiment_api *api=ps_get_experiment();ps_context *c=calloc(1,sizeof(*c));if(!c)return 0;
    c->struct_size=sizeof(*c);c->api_version=PS_API_VERSION;
    int yes=ps_parameter_override(c,"length",length)==PS_OK&&api->create(c)==PS_OK&&ps_parameter_finalize(c)==PS_OK;
    for(unsigned i=0;yes&&i<240;++i) { yes=api->step(c,1.0/120.0)==PS_OK;c->time_s+=1.0/120.0; }
    if(yes) {
        ps_scene scene={0};api->build_scene(c,&scene);yes=0;
        for(uint32_t i=0;i<scene.count;++i)if(scene.objects[i].id==3&&scene.objects[i].shape==PS_SPHERE) {
            *x=scene.objects[i].a.x;*y=scene.objects[i].a.y;*radius=scene.objects[i].radius;yes=1;break;
        }
    }
    if(c->user)api->destroy(c);free(c);return yes;
}
static int save_ppm(XImage *image,int x,int y,int width,int height,const char *tag) {
    const char *prefix=getenv("NEXAUI_PHYSIM_CAPTURE");if(!prefix)return 0;
    char path[4096];int n=snprintf(path,sizeof(path),"%s%s.ppm",prefix,tag);if(n<1||(size_t)n>=sizeof(path))return 0;
    FILE *f=fopen(path,"wbx");if(!f)return 0;int yes=fprintf(f,"P6\n%d %d\n255\n",width,height)>0;
    for(int row=y;yes&&row<y+height;++row)for(int col=x;yes&&col<x+width;++col) {
        unsigned long p=XGetPixel(image,col,row);uint8_t rgb[3]={(uint8_t)(p>>16),(uint8_t)(p>>8),(uint8_t)p};
        yes=fwrite(rgb,1,3,f)==3;
    }
    if(fclose(f))yes=0;return yes;
}
static int bob_color(XImage *image,int x,int y) {
    return (XGetPixel(image,x,y)&0xffffffu)==0x53dec2u;
}
static int capture_position(Display *display,Window window,double length,const char *tag,screen_point *target) {
    double bob_x=0,bob_y=0,radius=0;if(!original_bob(length,&bob_x,&bob_y,&radius))return 0;
    for(unsigned attempt=0;attempt<80;++attempt) {
        XWindowAttributes info;if(!XGetWindowAttributes(display,window,&info)||info.width!=1600||info.height!=1200)return 0;
        XImage *image=XGetImage(display,window,0,0,1600,1200,AllPlanes,ZPixmap);if(!image)return 0;
        if(image->red_mask!=0xff0000u||image->green_mask!=0xff00u||image->blue_mask!=0xffu) { XDestroyImage(image);return 0; }
        int left=1600,top=1200,right=-1,bottom=-1;
        for(int y=0;y<1200;++y)for(int x=0;x<1600;++x)if((XGetPixel(image,x,y)&0xffffffu)==0x090e17u) {
            if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
        }
        int yes=right-left+1==376&&bottom-top+1==384&&left>=408&&top>=160&&left-408+800<=1600&&top-160+560<=1200;
        unsigned colored=0;
        if(yes) {
            double projection=384.0/(length*1.8);
            int x=(int)((double)left+188+bob_x*projection),y=(int)((double)top+384*.16-bob_y*projection),r=(int)(radius*projection);
            yes=r>0&&x-r>=left&&x+r<=right&&y-r>=top&&y+r<=bottom;
            int found=0;
            if(yes)for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx) {
                int distance=dx*dx+dy*dy,px=x+dx,py=y+dy;
                if(distance>r*r||!bob_color(image,px,py))continue;
                ++colored;
                if(target&&!found&&distance<=r*r/4&&bob_color(image,px-1,py)&&bob_color(image,px+1,py)&&
                    bob_color(image,px,py-1)&&bob_color(image,px,py+1)) {
                    *target=(screen_point){px,py};found=1;
                }
            }
            yes=yes&&colored>(unsigned)(r*r)&&(!target||found);
        }
        if(yes) {
            yes=save_ppm(image,left-408,top-160,800,560,tag);XDestroyImage(image);
            if(yes)printf("PHYSIM_PIXELS %s length=%.1f disk_pixels=%u\n",tag,length,colored);return yes;
        }
        XDestroyImage(image);pump();
    }
    fprintf(stderr,"Physim native pixel mismatch: %s\n",tag);return 0;
}
static int capture(Display *display,Window window,double length,const char *tag) {
    return capture_position(display,window,length,tag,NULL);
}
static int stale(AtspiAccessible *node,AtspiComponent *component) {
    GError *error=NULL;int accepted=atspi_component_grab_focus(component,&error);int refused=!accepted||error!=NULL;
    if(error)g_error_free(error);
    int defunct=state(node,ATSPI_STATE_DEFUNCT),edited=edit(node,"9");
    printf("PHYSIM_OLD_OBJECT defunct=%d focus_refused=%d edit_accepted=%d\n",defunct,refused,edited);fflush(stdout);
    return defunct&&refused&&!edited;
}
int main(int argc,char **argv) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*test_case=getenv("NEXAUI_PHYSIM_CASE"),*trace=getenv("NEXAUI_PHYSIM_TRACE");
    if(argc!=2||!isolated||strcmp(isolated,"1")||!getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!test_case||!trace)return 2;
    int result=1,initialized=0;pid_t child=-1;Display *display=NULL;FILE *log=NULL,*error_log=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*old=NULL;AtspiComponent *old_component=NULL;char id[32];Window outer=0;
    CHECK(outer_window(id,&outer)&&(display=XOpenDisplay(NULL))&&(log=tmpfile())&&(error_log=fopen(trace,"wbx")));
    child=fork();CHECK(child>=0);
    if(!child) { if(dup2(fileno(log),STDOUT_FILENO)<0||dup2(fileno(error_log),STDERR_FILENO)<0)_exit(126);execl(argv[1],argv[1],(char *)NULL);_exit(127); }
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop&&(app=application(desktop,child)));
    CHECK(drive("windowfocus",id)&&drive("park",NULL)&&observe(app,"Model generation 1; length 1.5 m.",0)&&focus(app,"Length (m)"));
    screen_point bob={0};
    CHECK(text_is(app,"Length (m)","1.5")&&capture_position(display,outer,1.5,"initial",&bob));
    if(!strcmp(test_case,"full")) {
        CHECK(focus(app,"Original Physim pendulum scene")&&drive("key","Tab")&&observe(app,"Inspect next",1));
        CHECK(drive("key","Return")&&observe(app,"1/12 Line #1, parent #100",0));
        CHECK(drive("key","Return")&&observe(app,"2/12 Sphere #2, parent #100",0));
        CHECK(click_at(id,bob)&&observe(app,"3/12 Sphere #3, parent #101",0));
        CHECK(press(app,"Inspect next")&&observe(app,"4/12 Arrow #4, parent #3",0));
        CHECK(click_at(id,bob)&&observe(app,"3/12 Sphere #3, parent #101",0));
        CHECK(observe(app,"Original Physim pendulum scene",1)&&focus(app,"Inspect next"));
        CHECK(drive("park",NULL)&&observe(app,"Inspect next",1)&&capture(display,outer,1.5,"inspected"));
        CHECK(focus(app,"Length (m)"));
        CHECK(drive("key","ctrl+a")&&drive("key","0")&&drive("key","Return")&&text_is(app,"Length (m)","0"));
        AtspiAccessible *field=find(app,"Length (m)",0);CHECK(field);int invalid=state(field,ATSPI_STATE_INVALID_ENTRY);g_object_unref(field);CHECK(invalid);
        CHECK(press(app,"Apply model")&&observe(app,"Invalid parameters; model generation 1 retained.",0));
        CHECK(observe(app,"3/12 Sphere #3, parent #101",0));
        CHECK(marker(log,"PHYSIM_ACTION id=12 valid=0 generation=1 length=1.5"));
    } else {
        CHECK(drive("key","108")&&drive("key","apostrophe")&&marker(log,"PHYSIM_PREEDIT focus=10 length=1 generation=1"));
        if(!strcmp(test_case,"ime-close")) { CHECK(press(app,"Close"));goto closed; }
        if(!strcmp(test_case,"ime-replace")) {
            CHECK((old=find(app,"Length (m)",0))&&(old_component=atspi_accessible_get_component_iface(old)));
            CHECK(press(app,"Replace view")&&marker(log,"PHYSIM_REPLACED count=1 generation=1 length=1.5"));
            CHECK(stale(old,old_component)&&text_is(app,"Length (m)","1.5"));
        } else {
            CHECK(focus(app,"Apply model")&&marker(log,"PHYSIM_PREEDIT_END focus=12 generation=1"));
            CHECK(text_is(app,"Length (m)","1.5"));
        }
    }
    CHECK(focus(app,"Length (m)"));
    AtspiAccessible *field=find(app,"Length (m)",0);CHECK(field);
    int edited=!strcmp(test_case,"full")?edit(field,"2.5"):select_all(field);g_object_unref(field);CHECK(edited);
    if(strcmp(test_case,"full"))CHECK(drive("key","2")&&drive("key","period")&&drive("key","5"));
    CHECK(text_is(app,"Length (m)","2.5"));
    CHECK(observe(app,"Model generation 1; length 1.5 m.",0)||observe(app,"Invalid parameters; model generation 1 retained.",0));
    CHECK(focus(app,"Apply model")&&drive("key","Return")&&observe(app,"Model generation 2; length 2.5 m.",0));
    CHECK(observe(app,"Scene objects (12)",0));
    CHECK(marker(log,"PHYSIM_ACTION id=12 valid=1 generation=2 length=2.5")&&capture(display,outer,2.5,"applied"));
    if(!strcmp(test_case,"full")) {
        CHECK(drive("key","Tab")&&observe(app,"Zoom +",1)&&drive("key","Return"));
        CHECK(marker(log,"PHYSIM_ACTION id=13 valid=1 generation=2 length=2.5 low=0.25 high=0.75"));
        CHECK(press(app,"Pan right")&&marker(log,"PHYSIM_ACTION id=14 valid=1 generation=2 length=2.5 low=0.375 high=0.875"));
        CHECK((old=find(app,"Length (m)",0))&&(old_component=atspi_accessible_get_component_iface(old)));
        CHECK(drive("key","F6")&&marker(log,"PHYSIM_REPLACED count=1 generation=2 length=2.5 low=0.375 high=0.875"));
        CHECK(stale(old,old_component));
        CHECK(text_is(app,"Length (m)","2.5"));
        CHECK(focus(app,"Fit data")&&drive("key","Return")&&marker(log,"PHYSIM_ACTION id=15 valid=1 generation=2 length=2.5 low=0 high=1"));
        CHECK(drive("key","F7")&&marker(log,"PHYSIM_SURFACE generation=2 width=752 height=768 backing=2")&&capture_position(display,outer,2.5,"backing2",&bob));
        CHECK(click_at(id,bob)&&observe(app,"3/12 Sphere #3, parent #101",0));
        CHECK(focus(app,"Original angle channel (rad)")&&drive("key","Tab")&&observe(app,"Original Physim pendulum scene",1));
        CHECK(drive("key","Tab")&&observe(app,"Inspect next",1));
        CHECK(drive("key","Tab")&&observe(app,"Length (m)",1)&&drive("key","shift+Tab")&&observe(app,"Inspect next",1));
        CHECK(drive("key","shift+Tab")&&observe(app,"Original Physim pendulum scene",1));
    }
    CHECK(focus(app,"Close")&&drive("key","Return"));
closed: ;
    int status=0;pid_t ended=0;
    for(unsigned i=0;i<300&&!ended;++i) { ended=waitpid(child,&status,WNOHANG);if(!ended)pump(); }
    CHECK(ended==child);child=-1;CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
    printf("Native original Physim %s passed: OS input/AT-SPI/pixels/model/lifetime\n",test_case);result=0;
done:
    if(child>0) { kill(child,SIGKILL);waitpid(child,NULL,0); }
    if(old_component)g_object_unref(old_component);if(old)g_object_unref(old);if(app)g_object_unref(app);if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();if(display)XCloseDisplay(display);if(error_log)fclose(error_log);
    if(log) {
        char buffer[65536];ssize_t n=pread(fileno(log),buffer,sizeof(buffer),0);
        if(n<0||(size_t)n==sizeof(buffer))result=1;else if(fwrite(buffer,1,(size_t)n,stdout)!=(size_t)n)result=1;
        fclose(log);
    }
    return result;
}
