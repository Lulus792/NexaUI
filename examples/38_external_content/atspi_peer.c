#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"content peer line %d: %s\n",__LINE__,#x); goto done; } } while (0)
static void pump(void) {
    while (g_main_context_iteration(NULL,FALSE)) {}
    const struct timespec delay={0,10000000}; nanosleep(&delay,NULL);
}
static int state(AtspiAccessible *node, AtspiStateType flag) {
    atspi_accessible_clear_cache(node); AtspiStateSet *states=atspi_accessible_get_state_set(node);
    int yes=states&&atspi_state_set_contains(states,flag); if(states)g_object_unref(states);return yes;
}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node||depth>8)return NULL;
    atspi_accessible_clear_cache(node);char *text=atspi_accessible_get_name(node,NULL);
    int match=text&&!strcmp(text,name);g_free(text);if(match)return g_object_ref(node);
    int count=atspi_accessible_get_child_count(node,NULL);if(count>32)return NULL;
    for(int i=0;i<count;++i){
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,NULL),*found=find(child,name,depth+1);
        if(child)g_object_unref(child);if(found)return found;
    }
    return NULL;
}
static AtspiAccessible *application(AtspiAccessible *desktop,pid_t child) {
    for(unsigned attempt=0;attempt<200;++attempt){
        atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,NULL);
        if(count>16)return NULL;
        for(int i=0;i<count;++i){
            AtspiAccessible *node=atspi_accessible_get_child_at_index(desktop,i,NULL);
            if(!node)continue;
            if(atspi_accessible_get_process_id(node,NULL)==(guint)child)return node;
            g_object_unref(node);
        }
        pump();
    }
    return NULL;
}
static int observe(AtspiAccessible *app,const char *name,int focused) {
    for(unsigned i=0;i<200;++i){
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
    AtspiAccessible *node=find(app,name,0);AtspiAction *action=node?atspi_accessible_get_action_iface(node):NULL;
    GError *error=NULL;int accepted=0,count=action?atspi_action_get_n_actions(action,&error):0;
    for(int i=0;!error&&i<count&&i<16;++i){
        char *text=atspi_action_get_action_name(action,i,&error);int match=text&&!strcmp(text,"click");g_free(text);
        if(match&&!error){accepted=atspi_action_do_action(action,i,&error)&&!error;break;}
    }
    if(error)g_error_free(error);if(action)g_object_unref(action);if(node)g_object_unref(node);return accepted;
}
static int stale_focus(AtspiComponent *component) {
    GError *error=NULL;int accepted=atspi_component_grab_focus(component,&error);
    int rejected=!accepted||error!=NULL;if(error)g_error_free(error);return rejected;
}
static int dimensions(AtspiAccessible *node) {
    AtspiComponent *component=atspi_accessible_get_component_iface(node);GError *error=NULL;
    AtspiRect *rect=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int yes=rect&&!error&&rect->x==16&&rect->y==136&&rect->width==448&&rect->height==208;
    g_free(rect);if(error)g_error_free(error);if(component)g_object_unref(component);return yes;
}
static int outer_window(char id[32],Window *window) {
    const char *path=getenv("NEXAUI_TEST_WESTON_LOG");FILE *file=path?fopen(path,"r"):NULL;if(!file)return 0;
    char line[1024];unsigned count=0;
    while(fgets(line,sizeof(line),file)){
        const char *label=strstr(line,"x11 output "),*start=label?strstr(label,"window id "):NULL;
        if(!start)continue;start+=strlen("window id ");size_t size=strspn(start,"0123456789");
        if(!size||size>=32){count=0;break;}memcpy(id,start,size);id[size]=0;++count;
    }
    fclose(file);if(count!=1)return 0;
    errno=0;char *end=NULL;unsigned long value=strtoul(id,&end,10);
    if(errno||!end||*end||!value)return 0;*window=(Window)value;return 1;
}
static int drive(const char *command,const char *value) {
    const char *driver=getenv("NEXAUI_CONTENT_TEST_XDOTOOL");if(!driver)return 0;
    pid_t child=fork();if(!child){
        if(!strcmp(command,"park"))execl(driver,driver,"mousemove","1","1",(char *)NULL);
        else execl(driver,driver,command,value,(char *)NULL);
        _exit(127);
    }
    if(child<0)return 0;int status=0;pid_t ended;
    do{ended=waitpid(child,&status,0);}while(ended<0&&errno==EINTR);
    return ended==child&&WIFEXITED(status)&&!WEXITSTATUS(status);
}
static int content_left=0,content_top=0;
static int point_at(const char *window,int dx,int dy) {
    const char *driver=getenv("NEXAUI_CONTENT_TEST_XDOTOOL");if(!driver)return 0;
    char x[32],y[32];snprintf(x,sizeof(x),"%d",content_left+dx);snprintf(y,sizeof(y),"%d",content_top+dy);
    pid_t child=fork();if(!child){execl(driver,driver,"mousemove","--window",window,x,y,(char *)NULL);_exit(127);}
    if(child<0)return 0;int status=0;pid_t ended;
    do{ended=waitpid(child,&status,0);}while(ended<0&&errno==EINTR);
    return ended==child&&WIFEXITED(status)&&!WEXITSTATUS(status);
}
static int point_content(const char *window) { return point_at(window,60,60); }
static int save_ppm(XImage *image,int left,int top,int width,int height,const char *name) {
    const char *prefix=getenv("NEXAUI_CONTENT_CAPTURE_PREFIX");if(!prefix)return 0;
    char path[4096];int size=snprintf(path,sizeof(path),"%s%s.ppm",prefix,name);
    if(size<1||(size_t)size>=sizeof(path))return 0;
    FILE *file=fopen(path,"wbx");if(!file)return 0;
    int okay=fprintf(file,"P6\n%d %d\n255\n",width,height)>0;
    for(int y=top;okay&&y<top+height;++y)for(int x=left;okay&&x<left+width;++x){
        unsigned long pixel=XGetPixel(image,x,y);
        uint8_t bytes[3]={(uint8_t)((pixel>>16)&255u),(uint8_t)((pixel>>8)&255u),(uint8_t)(pixel&255u)};
        okay=fwrite(bytes,1,sizeof(bytes),file)==sizeof(bytes);
    }
    if(fclose(file))okay=0;return okay;
}
static int capture(Display *display,Window window,unsigned long rgb,const char *name) {
    for(unsigned attempt=0;attempt<80;++attempt){
        XWindowAttributes info;if(!XGetWindowAttributes(display,window,&info)||info.width<1||info.height<1||info.width>2048||info.height>2048)return 0;
        XImage *image=XGetImage(display,window,0,0,(unsigned)info.width,(unsigned)info.height,AllPlanes,ZPixmap);
        if(!image)return 0;
        if(image->red_mask!=0xff0000u||image->green_mask!=0xff00u||image->blue_mask!=0xffu){XDestroyImage(image);return 0;}
        int min_x=info.width,min_y=info.height,max_x=-1,max_y=-1;size_t count=0;
        for(int y=0;y<info.height;++y)for(int x=0;x<info.width;++x)if((XGetPixel(image,x,y)&0xffffffu)==rgb){
            ++count;if(x<min_x)min_x=x;if(x>max_x)max_x=x;if(y<min_y)min_y=y;if(y>max_y)max_y=y;
        }
        int match=count==448u*208u&&max_x-min_x+1==448&&max_y-min_y+1==208;
        if(match){
            content_left=min_x;content_top=min_y;
            int saved=save_ppm(image,min_x,min_y,448,208,name);
            if(saved&&!strcmp(name,"fresh"))saved=save_ppm(image,0,0,info.width,info.height,"fresh-full");
            if(saved&&!strcmp(name,"fresh")) {
                saved=min_x>=16&&min_y>=136&&min_x-16+480<=info.width&&min_y-136+360<=info.height&&
                    save_ppm(image,min_x-16,min_y-136,480,360,"fresh-window");
            }
            XDestroyImage(image);
            if(saved)printf("CONTENT_PIXELS %s width=448 height=208 pixels=93184 rgb=%06lx\n",name,rgb);
            return saved;
        }
        XDestroyImage(image);pump();
    }
    fprintf(stderr,"Content screen rectangle mismatch: %s\n",name);return 0;
}
static int host_marker(FILE *log,const char *marker);
static int input_chrome(Display *display,const char *window,const char *name) {
    XWindowAttributes info;Window id=(Window)strtoul(window,NULL,10);
    if(!XGetWindowAttributes(display,id,&info)||content_left<16||content_top<136||
        content_left-16+480>info.width||content_top-136+360>info.height) return 0;
    XImage *image=XGetImage(display,id,0,0,(unsigned)info.width,(unsigned)info.height,AllPlanes,ZPixmap);
    if(!image)return 0;
    int saved=save_ppm(image,content_left-16,content_top-136,480,360,name);
    XDestroyImage(image);return saved;
}
#define INPUT_VERIFY(x) do { if(!(x)) { fprintf(stderr,"input peer line %d: %s\n",__LINE__,#x); return 0; } } while(0)
static int input_workflow(AtspiAccessible *app,Display *display,const char *window,FILE *log) {
    INPUT_VERIFY(observe(app,"Physical input fixture; exit: Ctrl+Shift+F12",0));
    if(!focus(app,"Enter content")||!drive("key","Return")||
        !observe(app,"Input active: Ctrl+Shift+F12 exits.",0)||!observe(app,"Fixture frame",1)||
        !input_chrome(display,window,"input-active-window")||
        !point_content(window)||!drive("keydown","a")||!drive("keydown","Tab")||!drive("mousedown","1")||
        !host_marker(log,"keys=2 buttons=1 down=2 up=0 pdown=1 pup=0 enter=1 exit=0 text=0 x=120 y=120")||
        !drive("keydown","ctrl")||!drive("keydown","shift")||!drive("key","F12")||
        !observe(app,"Input released.",0)||!observe(app,"Close",1)||
        !host_marker(log,"keys=0 buttons=0 down=4 up=4 pdown=1 pup=1 enter=1 exit=1 text=0")) return 0;
    if(!drive("keyup","a")||!drive("keyup","Tab")||!drive("keyup","ctrl")||!drive("keyup","shift")||!drive("mouseup","1")) return 0;
    if(!focus(app,"Enter content")||!drive("key","Return")||!observe(app,"Input active: Ctrl+Shift+F12 exits.",0)||
        !point_content(window)||!drive("keydown","b")||!drive("mousedown","1")||
        !host_marker(log,"keys=1 buttons=1 down=5 up=4 pdown=2 pup=1 enter=2 exit=1 text=0 x=120 y=120")) return 0;
    char root[32];snprintf(root,sizeof(root),"%lu",(unsigned long)DefaultRootWindow(display));
    if(!drive("windowfocus",root)||!observe(app,"Input focus lost.",0)||
        !host_marker(log,"keys=0 buttons=0 down=5 up=5 pdown=2 pup=2 enter=2 exit=2 text=0")||
        !drive("keyup","b")||!drive("mouseup","1")||!drive("windowfocus",window)||
        !observe(app,"Input focus lost.",0)) { fprintf(stderr,"input peer native focus-loss stage failed\n");return 0; }
    /* Restoring X carrier focus does not activate a Wayland surface. A real
       click on the host Enter button reactivates it through Weston policy. */
    INPUT_VERIFY(point_at(window,40,-72)&&drive("click","1"));
    INPUT_VERIFY(observe(app,"Input active: Ctrl+Shift+F12 exits.",0));
    INPUT_VERIFY(point_content(window)&&drive("keydown","c")&&drive("mousedown","1"));
    INPUT_VERIFY(host_marker(log,"keys=1 buttons=1 down=6 up=5 pdown=3 pup=2 enter=3 exit=2 text=0 x=120 y=120"));
    INPUT_VERIFY(press(app,"Disconnect")&&observe(app,"Input disconnected; pixels retained.",0));
    INPUT_VERIFY(host_marker(log,"keys=0 buttons=0 down=6 up=6 pdown=3 pup=3 enter=3 exit=3 text=0"));
    INPUT_VERIFY(drive("keyup","c")&&drive("mouseup","1"));
    AtspiAccessible *enter=find(app,"Enter content",0);
    int disabled=enter&&!state(enter,ATSPI_STATE_ENABLED)&&!state(enter,ATSPI_STATE_SENSITIVE);
    if(enter)g_object_unref(enter);
    if(!disabled||press(app,"Enter content")||!observe(app,"Input disconnected; pixels retained.",0)) return 0;
    INPUT_VERIFY(input_chrome(display,window,"input-disconnected-window"));
    printf("INPUT_PEER physical keys, 2x absolute pointer, host escape, native focus loss and disconnect passed\n");
    return 1;
}
static int host_marker(FILE *log,const char *marker) {
    char text[16384];
    for(unsigned i=0;i<200;++i){
        ssize_t bytes=pread(fileno(log),text,sizeof(text)-1u,0);
        if(bytes<0||(size_t)bytes==sizeof(text)-1u)return 0;text[bytes]=0;
        if(strstr(text,marker))return 1;pump();
    }
    return 0;
}

int main(int argc,char **argv) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*wayland=getenv("WAYLAND_DISPLAY"),*test_case=getenv("NEXAUI_CONTENT_TEST_CASE");
    if(argc!=2||!isolated||strcmp(isolated,"1")||!wayland||strcmp(wayland,"nexaui-test")||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!test_case||
        (strcmp(test_case,"full")&&strcmp(test_case,"input")&&strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close")))return 2;
    int result=1,initialized=0;pid_t child=-1;
    AtspiAccessible *desktop=NULL,*app=NULL,*retired=NULL,*fresh=NULL;
    AtspiComponent *old_component=NULL;Display *display=NULL;FILE *host_log=NULL;
    char id[32];Window outer=0;
    CHECK(outer_window(id,&outer)&&(display=XOpenDisplay(NULL))&&(host_log=tmpfile()));
    child=fork();CHECK(child>=0);
    if(!child){
        if(dup2(fileno(host_log),STDOUT_FILENO)<0||dup2(fileno(host_log),STDERR_FILENO)<0)_exit(126);
        execl(argv[1],argv[1],(char *)NULL);_exit(127);
    }
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop&&(app=application(desktop,child)));
    CHECK(drive("windowfocus",id)&&drive("park",NULL));
    CHECK(observe(app,"Waiting for frame.",0)&&observe(app,"Replace tab",1));
    CHECK(capture(display,outer,0xe3e7efu,"pending"));
    CHECK(drive("key","Tab")&&observe(app,"Close",1));
    if(!strcmp(test_case,"pending-close")){CHECK(drive("key","Return"));goto closed;}
    CHECK(drive("key","shift+Tab")&&observe(app,"Replace tab",1)&&drive("key","Return"));
    CHECK(observe(app,"Replacement waiting.",0)&&focus(app,"Release old")&&drive("key","Return"));
    CHECK(observe(app,"Old frame rejected.",0)&&capture(display,outer,0xe3e7efu,"rejected"));
    if(!strcmp(test_case,"rejected-close")){CHECK(focus(app,"Close")&&drive("key","Return"));goto closed;}
    CHECK(press(app,"Publish frame")&&observe(app,"Frame ready.",0));
    CHECK((retired=find(app,"Fixture frame",0))&&dimensions(retired));
    CHECK((old_component=atspi_accessible_get_component_iface(retired)));
    CHECK(capture(display,outer,0x1753c5u,"fresh"));
    CHECK(press(app,"Bad frame")&&observe(app,"Frame format unavailable.",0));
    CHECK(state(retired,ATSPI_STATE_DEFUNCT)&&stale_focus(old_component));
    CHECK(capture(display,outer,0xe3e7efu,"bad"));
    CHECK(press(app,"Publish frame")&&observe(app,"Frame recovered.",0));
    CHECK(state(retired,ATSPI_STATE_DEFUNCT)&&stale_focus(old_component));
    CHECK((fresh=find(app,"Fixture frame",0))&&dimensions(fresh));
    CHECK(capture(display,outer,0x47a31fu,"recovered"));
    CHECK(focus(app,"Fixture frame")&&drive("key","Tab")&&observe(app,"Scale 2x",1)&&drive("key","Return"));
    CHECK(observe(app,"Waiting for scaled frame.",0)&&state(fresh,ATSPI_STATE_DEFUNCT));
    CHECK(focus(app,"Publish frame")&&drive("key","Return")&&observe(app,"Scaled frame ready.",0));
    CHECK(host_marker(host_log,"CONTENT_STATE generation=2 resize=2 width=896 height=416 sequence=1 copied=3"));
    CHECK(capture(display,outer,0xbf3f89u,"scaled"));
    if(!strcmp(test_case,"input")) CHECK(input_workflow(app,display,id,host_log));
    CHECK(focus(app,"Close")&&drive("key","Return"));
closed: ;
    int status=0;pid_t ended=0;
    for(unsigned i=0;i<300&&!ended;++i){ended=waitpid(child,&status,WNOHANG);if(!ended)pump();}
    CHECK(ended==child);child=-1;CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
    printf("Native external content: %s passed; OS pixels, host keys and retired objects\n",test_case);result=0;
done:
    if(child>0){kill(child,SIGKILL);waitpid(child,NULL,0);}
    if(old_component)g_object_unref(old_component);if(fresh)g_object_unref(fresh);if(retired)g_object_unref(retired);
    if(app)g_object_unref(app);if(desktop)g_object_unref(desktop);if(initialized)atspi_exit();if(display)XCloseDisplay(display);
    if(host_log){
        char *text=malloc(128u*1024u);
        if(!text)result=1;
        else{ssize_t count=pread(fileno(host_log),text,128u*1024u,0);
            if(count<0||count==128*1024)result=1;else if(fwrite(text,1,(size_t)count,stdout)!=(size_t)count)result=1;free(text);}
        fclose(host_log);
    }
    return result;
}
