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
static int component_top(AtspiAccessible *app,const char *name) {
    AtspiAccessible *node=find(app,name,0);
    AtspiComponent *component=node?atspi_accessible_get_component_iface(node):NULL;
    AtspiRect *rect=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    int y=rect?rect->y:-1;
    g_free(rect);
    if (component) g_object_unref(component);
    if (node) g_object_unref(node);
    return y;
}
static int pdf_link_bounds_at(AtspiAccessible *node,int x,int width) {
    if (!node || atspi_accessible_get_role(node,NULL)!=ATSPI_ROLE_PUSH_BUTTON) return 0;
    AtspiComponent *component=atspi_accessible_get_component_iface(node);
    AtspiRect *rect=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    int ok=rect&&rect->x==x&&rect->y==224&&rect->width==width&&rect->height==48;
    g_free(rect);
    if (component) g_object_unref(component);
    return ok;
}
static int pdf_link_bounds(AtspiAccessible *node) { return pdf_link_bounds_at(node,352,84); }

static int pdf_zoom_link(AtspiAccessible *node, int x, int width) {
    AtspiComponent *component=node?atspi_accessible_get_component_iface(node):NULL;
    if (!component) return 0;
    int ok=0;
    for (unsigned i=0;i<100&&!ok;++i) {
        AtspiRect *rect=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL);
        /* Fractional float edges may straddle an integer by one pixel. */
        ok=rect&&abs(rect->x-x)<=1&&rect->y==216&&abs(rect->width-width)<=1&&rect->height==72;
        g_free(rect);
        if (!ok) pump();
    }
    g_object_unref(component);
    return ok;
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
    int pdf_patch = !strcmp(name,"pdf-page2") || !strcmp(name,"pdf-page2-scaled");
    int recovered = !strcmp(name,"recovered") || pdf_patch;
    for(unsigned attempt=0;attempt<80;++attempt){
        XWindowAttributes info;if(!XGetWindowAttributes(display,window,&info)||info.width<1||info.height<1||info.width>2048||info.height>2048)return 0;
        XImage *image=XGetImage(display,window,0,0,(unsigned)info.width,(unsigned)info.height,AllPlanes,ZPixmap);
        if(!image)return 0;
        if(image->red_mask!=0xff0000u||image->green_mask!=0xff00u||image->blue_mask!=0xffu){XDestroyImage(image);return 0;}
        int min_x=info.width,min_y=info.height,max_x=-1,max_y=-1;size_t count=0;
        for(int y=0;y<info.height;++y)for(int x=0;x<info.width;++x)if((XGetPixel(image,x,y)&0xffffffu)==rgb){
            ++count;if(x<min_x)min_x=x;if(x>max_x)max_x=x;if(y<min_y)min_y=y;if(y>max_y)max_y=y;
        }
        size_t patch = recovered ? 112u * 52u : 0;
        int match=count==448u*208u-patch&&max_x-min_x+1==448&&max_y-min_y+1==208;
        if (match && recovered) {
            for (int y=0;match && y<208;++y) {
                for (int x=0;x<448;++x) {
                    unsigned long expected = x>=224 && x<336 && y>=104 && y<156 ?
                        (pdf_patch?0x00ff00u:0x47a31fu) : rgb;
                    if ((XGetPixel(image,min_x+x,min_y+y)&0xffffffu)!=expected) {
                        match=0;
                        break;
                    }
                }
            }
        }
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
static int pdf_zoom_pixels(Display *display,Window window,int patch_x,const char *name) {
    for (unsigned i=0;i<100;++i) {
        XImage *image=XGetImage(display,window,content_left-1,content_top,450,208,AllPlanes,ZPixmap);
        if (!image) return 0;
        unsigned long outside=XGetPixel(image,0,124)&0xffffffu;
        int ok=outside!=0x0000ffu&&outside!=0x00ff00u;
        for (int y=2;ok&&y<206;++y)
            for (int x=2;ok&&x<446;++x) {
                unsigned long expected=x>=patch_x&&x<patch_x+168&&y>=104&&y<182?0x00ff00u:0x0000ffu;
                ok=(XGetPixel(image,x+1,y)&0xffffffu)==expected;
            }
        if (ok) ok=save_ppm(image,1,0,448,208,name);
        XDestroyImage(image);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int png_portrait_pixels(Display *display,Window window,int zoomed,int pan,const char *name) {
    int left=zoomed?146:172,right=448-left;
    for (unsigned attempt=0;attempt<100;++attempt) {
        XImage *image=XGetImage(display,window,content_left,content_top,448,208,AllPlanes,ZPixmap);
        if (!image) return 0;
        unsigned long margin=XGetPixel(image,left-1,170)&0xffffffu;
        int ok=margin!=0x47a31fu&&(XGetPixel(image,right,170)&0xffffffu)==margin&&
            (XGetPixel(image,left,170)&0xffffffu)==0x47a31fu&&(XGetPixel(image,right-1,170)&0xffffffu)==0x47a31fu;
        for (int y=2;ok&&y<206;++y) {
            int source=zoomed?(4*(y+52+pan)+2)/3:2*y;
            if (!zoomed&&source>=192&&source<224) continue;
            unsigned long expected=source<192?0x1753c5u:source>=224?0x47a31fu:
                source%2?0xffce56u:0xdf2437u;
            ok=(XGetPixel(image,224,y)&0xffffffu)==expected;
        }
        if (ok) ok=save_ppm(image,0,0,448,208,name);
        XDestroyImage(image);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int png_portrait_semantics(AtspiAccessible *app,int zoomed) {
    const char *name="PNG image, 208 × 416 pixels";
    for (unsigned i=0;i<100;++i) {
        AtspiAccessible *node=find(app,name,0);
        AtspiComponent *component=node?atspi_accessible_get_component_iface(node):NULL;
        AtspiRect *rect=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
        int ok=rect&&atspi_accessible_get_role(node,NULL)==ATSPI_ROLE_IMAGE&&
            abs(rect->x-(zoomed?162:188))<=1&&rect->y==136&&abs(rect->width-(zoomed?156:104))<=1&&rect->height==208;
        g_free(rect);
        if (component) g_object_unref(component);
        if (node) g_object_unref(node);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int pdf_detail_capture(Display *display,Window window,int mode,const char *name) {
    for (unsigned attempt=0;attempt<100;++attempt) {
        XImage *image=XGetImage(display,window,content_left,content_top,448,208,AllPlanes,ZPixmap);
        if (!image) return 0;
        int ok=(XGetPixel(image,mode?200:250,mode?170:125)&0xffffffu)==0x00ff00u;
        if (!mode) ok=ok&&(XGetPixel(image,200,170)&0xffffffu)==0x0000ffu;
        else if (mode==1) {
            for (int x=48;ok&&x<208;x+=2) ok=XGetPixel(image,x,72)==XGetPixel(image,x+1,72);
        } else if (mode==2) {
            for (int x=48;ok&&x<208;++x)
                ok=(XGetPixel(image,x,72)&0xffffffu)==(x%2?0x2000dfu:0xff0000u);
        } else if (mode==3) {
            int transitions=0;
            for (int x=8;x<200;++x) transitions+=XGetPixel(image,x,72)!=XGetPixel(image,x+1,72);
            ok=ok&&transitions>110;
        }
        if (ok) ok=save_ppm(image,0,0,448,208,name);
        XDestroyImage(image);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int pdf_detail_gate(unsigned edge,int release) {
    const char *directory=getenv("NEXAUI_CONTENT_TEST_ENCODED");
    if (!directory) return 0;
    char path[4096];
    int length=snprintf(path,sizeof(path),"%s/detail-%u-%s",directory,edge,release?"release":"started");
    if (length<0 || (size_t)length>=sizeof(path)) return 0;
    if (release) {
        FILE *marker=fopen(path,"wx");
        return marker&&fclose(marker)==0;
    }
    for (unsigned i=0;i<100;++i) {
        if (access(path,F_OK)==0) return 1;
        pump();
    }
    return 0;
}

static int export_file(const char *path,uint64_t *fingerprint) {
    static const uint8_t signature[]={137,80,78,71,13,10,26,10};
    static const uint8_t dimensions[]={0,0,3,34,0,0,1,196}; /* 802 × 452 */
    FILE *file=fopen(path,"rb");
    if (!file) return 0;
    uint8_t buffer[4096];
    size_t read=fread(buffer,1,sizeof(buffer),file), total=0;
    int ok=read>=24&&!memcmp(buffer,signature,sizeof(signature))&&!memcmp(buffer+12,"IHDR",4)&&
        !memcmp(buffer+16,dimensions,sizeof(dimensions));
    uint64_t hash=UINT64_C(14695981039346656037);
    while (ok&&read) {
        total+=read;
        if (total>8u*1024u*1024u) { ok=0;break; }
        for (size_t i=0;i<read;++i) hash=(hash^buffer[i])*UINT64_C(1099511628211);
        read=fread(buffer,1,sizeof(buffer),file);
    }
    ok=ok&&!ferror(file);
    if (fclose(file)) ok=0;
    if (ok) *fingerprint=hash;
    return ok;
}

static AtspiRect *pdf_detail_word(AtspiAccessible *node) {
    AtspiText *text=atspi_accessible_get_text_iface(node);
    AtspiRect *rect=text?atspi_text_get_character_extents(text,10,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    if (text) g_object_unref(text);
    return rect;
}

static int pdf_detail_link(AtspiAccessible *node) {
    AtspiComponent *component=atspi_accessible_get_component_iface(node);
    if (!component) return 0;
    AtspiRect *rect=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL);
    int ok=rect&&abs(rect->x-416)<=1&&rect->y==208&&abs(rect->width-48)<=1&&rect->height==96;
    g_free(rect);
    g_object_unref(component);
    return ok;
}

static int pdf_zoom_highlight(Display *display,Window window,const char *name) {
    for (unsigned i=0;i<100;++i) {
        XImage *image=XGetImage(display,window,content_left,content_top,448,208,AllPlanes,ZPixmap);
        if (!image) return 0;
        int ok=(XGetPixel(image,74,35)&0xffffffu)!=0x0000ffu &&
            (XGetPixel(image,38,35)&0xffffffu)==0x0000ffu;
        if (ok) ok=save_ppm(image,0,0,448,208,name);
        XDestroyImage(image);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int pdf_zoom_word(AtspiAccessible *node,int offset,int expected_x,int width) {
    AtspiText *text=node?atspi_accessible_get_text_iface(node):NULL;
    if (!text) return 0;
    int ok=0;
    for (unsigned i=0;i<100&&!ok;++i) {
        GError *error=NULL;
        AtspiRect *rect=atspi_text_get_character_extents(text,offset,ATSPI_COORD_TYPE_WINDOW,&error);
        ok=expected_x<0 ? error!=NULL :
            !error&&rect&&abs(rect->x-expected_x)<=1&&rect->y==151&&abs(rect->width-width)<=1&&rect->height==45;
        g_free(rect);
        g_clear_error(&error);
        if (!ok) pump();
    }
    g_object_unref(text);
    return ok;
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
static AtspiAccessible *pdf_ready_preview(AtspiAccessible *app,unsigned page) {
    char name[24];
    snprintf(name,sizeof(name),"Page %u",page);
    for (unsigned i=0;i<200;++i) {
        AtspiAccessible *node=find(app,name,0);
        char *description=node?atspi_accessible_get_description(node,NULL):NULL;
        int ready=description&&!strcmp(description,"Preview ready");
        g_free(description);
        if (ready) return node;
        if (node) g_object_unref(node);
        pump();
    }
    return NULL;
}
static int pdf_preview_ready(AtspiAccessible *app,unsigned page) {
    AtspiAccessible *node=pdf_ready_preview(app,page);
    if (!node) return 0;
    g_object_unref(node);
    return 1;
}
static int pdf_preview_pixels(Display *display,Window window) {
    for (unsigned i=0;i<100;++i) {
        XImage *view=XGetImage(display,window,content_left-16,content_top-136,480,360,AllPlanes,ZPixmap);
        if (!view) return 0;
        int ok=(XGetPixel(view,200,155)&0xffffffu)==0x0000ffu &&
            (XGetPixel(view,247,173)&0xffffffu)==0x00ff00u &&
            (XGetPixel(view,275,173)&0xffffffu)==0x0000ffu;
        XDestroyImage(view);
        if (ok) return 1;
        pump();
    }
    return 0;
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

static int pdf_text_is(AtspiAccessible *node, const char *expected) {
    AtspiText *text=node?atspi_accessible_get_text_iface(node):NULL;
    if(!text)return 0;
    GError *error=NULL;
    char *value=atspi_text_get_text(text,0,-1,&error);
    int ok=!error&&value&&!strcmp(value,expected);
    if(ok)ok=atspi_text_get_character_count(text,&error)==g_utf8_strlen(expected,-1)&&!error;
    if(ok)ok=atspi_text_get_caret_offset(text,&error)==-1&&!error;
    if(ok)ok=atspi_text_get_n_selections(text,&error)==0&&!error&&!state(node,ATSPI_STATE_EDITABLE);
    g_free(value);
    if(error)g_error_free(error);
    g_object_unref(text);
    return ok;
}

static AtspiAccessible *pdf_text(AtspiAccessible *app, const char *expected) {
    for(unsigned i=0;i<200;++i) {
        AtspiAccessible *node=find(app,"PDF page text",0);
        if(pdf_text_is(node,expected))return node;
        if(node)g_object_unref(node);
        pump();
    }
    return NULL;
}

static int pdf_word_geometry(AtspiAccessible *node, int first_width, int space, int next_x) {
    AtspiText *text = atspi_accessible_get_text_iface(node);
    if (!text) return 0;
    GError *error = NULL;
    AtspiRect *first = atspi_text_get_character_extents(text, 1, ATSPI_COORD_TYPE_WINDOW, &error);
    int ok = !error && first && first->x == 48 && first->y == 181 &&
        first->width == first_width && first->height == 30;
    g_free(first);
    if (ok) ok = atspi_text_get_offset_at_point(text,60,190,ATSPI_COORD_TYPE_WINDOW,&error)==0 && !error;
    if (ok) ok = atspi_text_get_offset_at_point(text,next_x,190,ATSPI_COORD_TYPE_WINDOW,&error)==space+1 && !error;
    if (ok) ok = atspi_text_get_offset_at_point(text,30,190,ATSPI_COORD_TYPE_WINDOW,&error)==-1 && !error;
    if (ok) {
        AtspiRect *unmapped = atspi_text_get_character_extents(text,space,ATSPI_COORD_TYPE_WINDOW,&error);
        ok = error != NULL;
        g_free(unmapped);
    }
    g_clear_error(&error);
    g_object_unref(text);
    return ok;
}

static int pdf_find_text(AtspiAccessible *app, const char *value) {
    AtspiAccessible *field = find(app,"Find in PDF",0);
    AtspiEditableText *editor = field ? atspi_accessible_get_editable_text_iface(field) : NULL;
    GError *error = NULL;
    int ok = editor && atspi_editable_text_set_text_contents(editor,value,&error) && !error;
    g_clear_error(&error);
    if (editor) g_object_unref(editor);
    if (field) g_object_unref(field);
    return ok;
}

static int pdf_find_value(AtspiAccessible *app, const char *expected) {
    AtspiAccessible *field = find(app,"Find in PDF",0);
    AtspiText *text = field ? atspi_accessible_get_text_iface(field) : NULL;
    GError *error = NULL;
    char *value = text ? atspi_text_get_text(text,0,-1,&error) : NULL;
    int ok = !error && value && !strcmp(value,expected);
    g_free(value);
    g_clear_error(&error);
    if (text) g_object_unref(text);
    if (field) g_object_unref(field);
    return ok;
}

static int pdf_find_pixels(Display *display, Window window, int second_page, const char *capture_name) {
    for (unsigned attempt = 0; attempt < 80; ++attempt) {
        XImage *image = XGetImage(display,window,content_left,content_top,448,208,AllPlanes,ZPixmap);
        if (!image) return 0;
        /* The only changed pixels are inside the first word's own rectangle. */
        int ok = (XGetPixel(image,50,52) & 0xffffffu) != 0x0000ffu;
        int word_end = second_page ? 104 : 152;
        for (int y = 0; ok && y < 208; ++y) {
            for (int x = 0; ok && x < 448; ++x) {
                if (x >= 32 && x < word_end && y >= 45 && y < 75) continue;
                unsigned long expected = second_page && x >= 224 && x < 336 && y >= 104 && y < 156 ?
                    0x00ff00u : 0x0000ffu;
                ok = (XGetPixel(image,x,y) & 0xffffffu) == expected;
            }
        }
        if (ok) ok = save_ppm(image,0,0,448,208,capture_name);
        XDestroyImage(image);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int pdf_selection_is(AtspiText *text, const char *expected, int start, int end) {
    for (unsigned i=0;i<200;++i) {
        GError *error=NULL;
        int count=atspi_text_get_n_selections(text,&error);
        AtspiRange *range=count==1&&!error?atspi_text_get_selection(text,0,&error):NULL;
        char *value=range&&!error?atspi_text_get_text(text,range->start_offset,range->end_offset,&error):NULL;
        int match=!error && (expected ?
            range&&range->start_offset==start&&range->end_offset==end&&value&&!strcmp(value,expected) :
            count==0);
        g_free(value);
        g_free(range);
        g_clear_error(&error);
        if (match) return 1;
        pump();
    }
    return 0;
}

static int pdf_selection_pixels(Display *display, Window window) {
    for (unsigned i=0;i<80;++i) {
        XImage *image=XGetImage(display,window,content_left,content_top,448,208,AllPlanes,ZPixmap);
        if (!image) return 0;
        int ok=(XGetPixel(image,55,52)&0xffffffu)!=0x0000ffu &&
            (XGetPixel(image,170,52)&0xffffffu)!=0x0000ffu &&
            (XGetPixel(image,244,52)&0xffffffu)==0x0000ffu;
        if (ok) ok=save_ppm(image,0,0,448,208,"pdf-selection");
        XDestroyImage(image);
        if (ok) return 1;
        pump();
    }
    return 0;
}

static int clipboard_reads(const char *expected) {
    const char *reader=getenv("NEXAUI_CONTENT_TEST_CLIPBOARD_READER");
    FILE *output=reader?tmpfile():NULL;
    if (!output) return 0;
    pid_t child=fork();
    if (!child) {
        if (dup2(fileno(output),STDOUT_FILENO)<0) _exit(126);
        execl(reader,reader,"--no-newline","--type","text/plain;charset=utf-8",(char *)NULL);
        _exit(127);
    }
    int status=0,ok=0;
    if (child>0) {
        pid_t ended=0;
        for (unsigned i=0;i<300&&!ended;++i) { ended=waitpid(child,&status,WNOHANG);if (!ended) pump(); }
        if (!ended) { kill(child,SIGKILL);waitpid(child,NULL,0); }
        char bytes[128];
        ssize_t length=pread(fileno(output),bytes,sizeof(bytes),0);
        ok=ended==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0&&length==(ssize_t)strlen(expected)&&
            !memcmp(bytes,expected,(size_t)length);
    }
    fclose(output);
    return ok;
}

static const char external_url[]="https://example.invalid/docs?x=1&y=two%20words#part";

static int uri_received(const char *receipt, const char *uri) {
    if (!receipt || strcmp(uri,external_url)) return 2;
    FILE *file=fopen(receipt,"ab");
    if (!file) return 1;
    int ok=fprintf(file,"%s\n",uri)>0;
    if (fclose(file)) ok=0;
    return ok?0:1;
}

static int uri_receipt_is(const char *receipt, int written) {
    if (!receipt) return 0;
    FILE *file=fopen(receipt,"rb");
    if (!written) {
        if (file) { fclose(file);return 0; }
        return errno==ENOENT;
    }
    if (!file) return 0;
    char output[128];
    size_t bytes=fread(output,1,sizeof(output),file);
    int ok=bytes==sizeof(external_url)&&!memcmp(output,external_url,bytes-1u)&&output[bytes-1u]=='\n';
    fclose(file);
    return ok;
}

int main(int argc,char **argv) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*wayland=getenv("WAYLAND_DISPLAY"),*test_case=getenv("NEXAUI_CONTENT_TEST_CASE");
    if(argc!=2||!isolated||strcmp(isolated,"1")||!wayland||strcmp(wayland,"nexaui-test")||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!test_case||
        (strcmp(test_case,"full")&&strcmp(test_case,"png")&&strcmp(test_case,"pdf")&&strcmp(test_case,"input")&&
        strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close")))return 2;
    const char *uri_receipt=getenv("NEXAUI_CONTENT_TEST_URI_RECEIPT");
    if (uri_receipt && !strncmp(argv[1],"http",4)) return uri_received(uri_receipt,argv[1]);
    int png_case=!strcmp(test_case,"png");
    int png_viewer=png_case&&getenv("NEXAUI_CONTENT_TEST_PNG_VIEWER");
    int pdf_case=!strcmp(test_case,"pdf");
    int compose=pdf_case&&getenv("NEXAUI_CONTENT_PDF_COMPOSE");
    int links=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_LINK_READER");
    int selection=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_SELECT");
    int url_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_URL");
    int zoom_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_ZOOM");
    int detail_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_DETAIL");
    const char *export_path=pdf_case?getenv("NEXAUI_CONTENT_TEST_EXPORT_PATH"):NULL;
    int thumbnails=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_THUMBNAILS");
    int properties=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_PROPERTIES");
    const char *viewport=pdf_case?"PDF page image":png_case?"PNG image":"Fixture frame";
    unsigned long blue=pdf_case?0x0000ffu:0x1753c5u;
    int result=1,initialized=0;pid_t child=-1;
    AtspiAccessible *desktop=NULL,*app=NULL,*retired=NULL,*fresh=NULL;
    AtspiAccessible *first_text=NULL,*second_text=NULL,*scaled_text=NULL,*returned_text=NULL,*hit_text=NULL;
    AtspiAccessible *old_link=NULL,*link=NULL,*back=NULL;
    AtspiAccessible *external=NULL;
    AtspiAccessible *selected=NULL,*new_selected=NULL;
    AtspiText *selection_text=NULL,*new_text=NULL;
    AtspiEditableText *selection_edit=NULL;
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
    if(pdf_case) {
        AtspiAccessible *old_text=find(app,"PDF page text",0);
        int absent=old_text==NULL;
        if(old_text)g_object_unref(old_text);
        CHECK(absent&&host_marker(host_log,"PDF_INDEX stale generation=1 current=2"));
    }
    if(!strcmp(test_case,"rejected-close")){CHECK(focus(app,"Close")&&drive("key","Return"));goto closed;}
    CHECK(press(app,"Publish frame")&&observe(app,thumbnails?"PDF page 1 of 7.":pdf_case?"PDF page 1 of 2.":"Frame ready.",0));
    CHECK((retired=find(app,viewport,0))&&dimensions(retired));
    CHECK((old_component=atspi_accessible_get_component_iface(retired)));
    CHECK(capture(display,outer,blue,"fresh"));
    if (png_viewer) CHECK(observe(app,"PNG image, 112 × 52 pixels",0)&&press(app,"Zoom in"));
    if(pdf_case)CHECK((first_text=pdf_text(app,"Résumé page one\n\n"))&&pdf_word_geometry(first_text,120,6,195));
    if(links) CHECK((old_link=find(app,"PDF link to page 2",0))&&pdf_link_bounds(old_link)&&
        atspi_accessible_get_child_count(retired,NULL)==3);
    if (url_case) {
        CHECK((external=find(app,"External PDF link",0))&&pdf_link_bounds_at(external,36,68));
        CHECK(focus(app,"External PDF link")&&drive("key","Return"));
        CHECK(observe(app,"External link address",0)&&observe(app,external_url,0)&&uri_receipt_is(uri_receipt,0));
        CHECK(!press(app,"Close"));
        CHECK(drive("key","Escape")&&observe(app,"External PDF link",1)&&uri_receipt_is(uri_receipt,0));
        CHECK(drive("key","Return")&&observe(app,"External link address",0)&&observe(app,external_url,0));
        CHECK(input_chrome(display,id,"pdf-url-preview"));
        CHECK(focus(app,"Copy address")&&drive("key","Return")&&observe(app,"Address copied",0));
        CHECK(clipboard_reads(external_url)&&uri_receipt_is(uri_receipt,0));
        CHECK(drive("windowfocus",id)&&press(app,"Open link")&&observe(app,"Link handed off",0));
        CHECK(uri_receipt_is(uri_receipt,1)&&press(app,"Close preview")&&observe(app,"External PDF link",1));
        puts("PDF_EXTERNAL_LINK verified: accessible URL, modal focus, clipboard, exact explicit handoff");
        CHECK(focus(app,"Close")&&drive("key","Return"));
        goto closed;
    }
    CHECK(press(app,"Bad frame"));
    if(png_case||pdf_case) {
        CHECK(observe(app,pdf_case?"PDF rendering failed; page retained.":"PNG decoding failed; image retained.",0));
        CHECK(!state(retired,ATSPI_STATE_DEFUNCT)&&dimensions(retired));
        CHECK(capture(display,outer,blue,"bad-retained"));
        if (png_viewer) CHECK(observe(app,"PNG image, 112 × 52 pixels",0));
        if(pdf_case)CHECK(!state(first_text,ATSPI_STATE_DEFUNCT)&&pdf_text_is(first_text,"Résumé page one\n\n")&&
            pdf_word_geometry(first_text,120,6,195));
        if(links) CHECK(!state(old_link,ATSPI_STATE_DEFUNCT)&&pdf_link_bounds(old_link));
        if (selection) {
            CHECK(press(app,"Select text")&&observe(app,"PDF text selection",1));
            CHECK((selected=find(app,"PDF text selection",0))&&state(first_text,ATSPI_STATE_DEFUNCT)&&
                !state(selected,ATSPI_STATE_EDITABLE)&&(selection_text=atspi_accessible_get_text_iface(selected)));
            if (links) CHECK((link=find(app,"PDF link to page 2",0))&&state(old_link,ATSPI_STATE_DEFUNCT)&&
                !state(link,ATSPI_STATE_ENABLED)&&!press(app,"PDF link to page 2"));
            CHECK(drive("key","Home")&&drive("key","shift+Right")&&drive("key","shift+Right")&&
                pdf_selection_is(selection_text,"Ré",0,2));
            CHECK(point_at(id,48,52)&&drive("mousedown","1")&&point_at(id,174,52)&&drive("mouseup","1")&&
                drive("park",NULL)&&pdf_selection_is(selection_text,"Résumé page",0,11));
            CHECK(pdf_selection_pixels(display,outer)&&drive("key","ctrl+c")&&
                observe(app,"Selection copied",0)&&clipboard_reads("Résumé page"));
            CHECK(drive("windowfocus",id)&&focus(app,"PDF text selection"));
            CHECK(atspi_text_set_selection(selection_text,0,7,11,NULL)&&pdf_selection_is(selection_text,"page",7,11));
            CHECK((selection_edit=atspi_accessible_get_editable_text_iface(selected))&&
                !atspi_editable_text_set_text_contents(selection_edit,"Edited",NULL)&&pdf_selection_is(selection_text,"page",7,11));
            /* A new native key press supplies the clipboard ownership serial
               after the independent client has returned keyboard focus. */
            CHECK(drive("key","End")&&pdf_selection_is(selection_text,NULL,0,0));
            CHECK(atspi_editable_text_copy_text(selection_edit,7,11,NULL)&&
                observe(app,"Selection copied",0)&&clipboard_reads("page"));
            CHECK(drive("windowfocus",id)&&focus(app,"PDF text selection"));
            CHECK(press(app,"Next page")&&observe(app,"PDF page 2 of 2.",0)&&state(selected,ATSPI_STATE_DEFUNCT));
            CHECK((new_selected=find(app,"PDF text selection",0))&&(new_text=atspi_accessible_get_text_iface(new_selected))&&
                pdf_selection_is(new_text,NULL,0,0)&&!atspi_text_set_selection(selection_text,0,0,1,NULL));
            CHECK(focus(app,"PDF text selection")&&drive("key","Escape")&&observe(app,viewport,1));
            CHECK((second_text=pdf_text(app,"Grün page two\n\n"))&&state(new_selected,ATSPI_STATE_DEFUNCT));
            puts("PDF_SELECTION verified: key and pointer selection, keyboard and AT-SPI clipboard bytes, read-only text and page reset");
            CHECK(focus(app,"Close")&&drive("key","Return"));
            goto closed;
        }
        if(pdf_case) {
            CHECK(drive("key","ctrl+f")&&observe(app,"Find in PDF",1));
            CHECK(pdf_find_value(app,""));
            CHECK(pdf_find_text(app,"Résumé")&&observe(app,"1 match on page",0));
            CHECK(pdf_find_pixels(display,outer,0,"pdf-find-resume"));
            /* simple-IME owns Escape while its editor is active. The
               ordinary PDF case checks the application's Escape path. */
            if (compose) CHECK(focus(app,viewport));
            else CHECK(drive("key","Escape")&&observe(app,viewport,1));
        }
    } else {
        CHECK(observe(app,"Frame format unavailable.",0));
        CHECK(state(retired,ATSPI_STATE_DEFUNCT)&&stale_focus(old_component));
        CHECK(capture(display,outer,0xe3e7efu,"bad"));
    }
    CHECK(press(app,pdf_case?"Next page":"Publish frame")&&observe(app,
        thumbnails?"PDF page 2 of 7.":pdf_case?"PDF page 2 of 2.":"Frame recovered.",0));
    if(!png_case&&!pdf_case)CHECK(state(retired,ATSPI_STATE_DEFUNCT)&&stale_focus(old_component));
    CHECK((fresh=find(app,viewport,0))&&dimensions(fresh));
    CHECK(host_marker(host_log,(pdf_case||png_viewer)?
        "CONTENT_STATE generation=2 resize=1 width=448 height=208 sequence=3 copied=2 copy_bytes=372736":
        "CONTENT_STATE generation=2 resize=1 width=448 height=208 sequence=3 copied=2 copy_bytes=23296"));
    if (png_viewer) {
        CHECK(png_portrait_semantics(app,1)&&png_portrait_pixels(display,outer,1,0,"png-portrait-center"));
        CHECK(focus(app,viewport)&&drive("key","Right")&&png_portrait_pixels(display,outer,1,0,"png-portrait-axis-locked"));
        CHECK(drive("key","Down")&&png_portrait_pixels(display,outer,1,48,"png-portrait-panned"));
        CHECK(press(app,"Scale 2x")&&observe(app,"Waiting for scaled frame.",0));
        CHECK(press(app,"Publish frame")&&observe(app,"Scaled frame ready.",0));
        CHECK(png_portrait_semantics(app,1)&&png_portrait_pixels(display,outer,1,48,"png-portrait-scaled"));
        CHECK(focus(app,viewport)&&drive("key","ctrl+0")&&png_portrait_semantics(app,0));
        CHECK(png_portrait_pixels(display,outer,0,0,"png-portrait-fit")&&focus(app,"Close")&&drive("key","Return"));
        puts("PNG_VIEWER verified: original samples, fitted image bounds, locked axis, pan, metadata and backing scale");
        goto closed;
    }
    if (detail_case) CHECK(pdf_detail_capture(display,outer,0,"pdf-detail-fit-before"));
    else CHECK(capture(display,outer,blue,pdf_case?"pdf-page2":"recovered"));
    if(pdf_case)CHECK((second_text=pdf_text(app,"Grün page two\n\n"))&&state(first_text,ATSPI_STATE_DEFUNCT)&&
        pdf_word_geometry(second_text,72,4,145)&&observe(app,"0 matches on page",0));
    if (export_path) {
        CHECK(press(app,"Zoom in")&&pdf_zoom_pixels(display,outer,224,"pdf-export-center"));
        CHECK(focus(app,viewport)&&drive("key","Right")&&pdf_zoom_pixels(display,outer,176,"pdf-export-panned"));
        CHECK(focus(app,"Pages")&&drive("key","Return")&&observe(app,"PDF pages",0));
        CHECK(focus(app,"Export PNG")&&drive("key","Return")&&observe(app,"Exporting PNG",0));
        CHECK(pdf_detail_gate(1024,0)&&access(export_path,F_OK)<0&&errno==ENOENT&&!press(app,"Export PNG"));
        CHECK(drive("key","Escape")&&observe(app,"Pages",1)&&press(app,"Fit page"));
        CHECK(pdf_detail_capture(display,outer,0,"pdf-export-live-fit"));
        CHECK(pdf_detail_gate(1024,1)&&host_marker(host_log,"PDF_EXPORT ticket=1 generation=2 page=2 result=0"));
        uint64_t first=0,after=0;
        CHECK(export_file(export_path,&first)&&press(app,"Pages")&&observe(app,"PNG saved",0));
        CHECK(press(app,"Export PNG")&&observe(app,"PNG already exists",0));
        CHECK(export_file(export_path,&after)&&after==first&&press(app,"Close pages"));
        CHECK(focus(app,"Close")&&drive("key","Return"));
        puts("PDF_EXPORT verified: frozen panned page, explicit size, accessible status and no replacement");
        goto closed;
    }
    if (detail_case) {
        CHECK(press(app,"Zoom in")&&pdf_detail_gate(672,0));
        CHECK(press(app,"Zoom in")&&focus(app,viewport)&&drive("key","Right"));
        CHECK(pdf_detail_capture(display,outer,1,"pdf-detail-fallback"));
        CHECK((back=find(app,"PDF link to page 1",0))&&pdf_detail_link(back));
        AtspiRect *rect=pdf_detail_word(second_text);
        int valid=rect&&rect->width>0&&rect->height>0;
        AtspiRect word=valid?*rect:(AtspiRect){0};
        g_free(rect);
        CHECK(valid&&pdf_detail_gate(672,1)&&host_marker(host_log,"edge=672 pixels=672x312 accepted=0 result=0"));
        CHECK(pdf_detail_gate(896,0)&&pdf_detail_gate(896,1)&&host_marker(host_log,"edge=896 pixels=896x416 accepted=1 result=0"));
        CHECK(pdf_detail_capture(display,outer,2,"pdf-detail-refined")&&pdf_detail_link(back)&&!state(back,ATSPI_STATE_DEFUNCT));
        rect=pdf_detail_word(second_text);
        valid=rect&&rect->x==word.x&&rect->y==word.y&&rect->width==word.width&&rect->height==word.height;
        g_free(rect);
        CHECK(valid);
        CHECK(press(app,"Zoom in")&&host_marker(host_log,"edge=1024 pixels=0x0 accepted=1 result="));
        CHECK(pdf_detail_capture(display,outer,3,"pdf-detail-retained"));
        CHECK(press(app,"Fit page")&&focus(app,"Close")&&pdf_detail_capture(display,outer,0,"pdf-detail-fit-after"));
        CHECK(drive("key","Return"));
        puts("PDF_DETAIL verified: native vector detail, delayed fallback, stale raster, stable geometry and retained failure");
        goto closed;
    }
    if (properties) {
        static const char expected[]="Title\nRésumé — Übersicht 2026\nAuthor\n"
            "Zoë Martín, Maja Søndergaard, Anaïs Laurent, Jens Østergård, Léonie Weber, Şirin Kaya, Béla Kovács, "
            "Łucja Nowak, Rui Ferreira, Élodie Dubois, Zoë N'Dour, Lejla Hadžić, Anika Meier, Tereza Nováková\n"
            "Subject\nValue unavailable\nPages\n2\nPDF version\n1.4\n";
        CHECK(host_marker(host_log,"PDF_PROPERTIES generation=1 accepted=0 result=0")&&
            host_marker(host_log,"PDF_PROPERTIES generation=2 accepted=1 result=0"));
        CHECK(focus(app,"Pages")&&drive("key","Return")&&observe(app,"PDF pages",0));
        CHECK(focus(app,"Properties")&&drive("key","Return")&&observe(app,"Document properties",0));
        CHECK((selected=find(app,"PDF properties text",0))&&pdf_text_is(selected,expected)&&
            state(selected,ATSPI_STATE_READ_ONLY));
        CHECK(!press(app,"Close pages")&&!press(app,"Close")&&input_chrome(display,id,"pdf-properties"));
        CHECK(observe(app,"Close properties",1)&&drive("key","Tab")&&observe(app,"Vertical scrollbar",1));
        int before=component_top(app,"Vertical scrollbar"),after=before;
        CHECK(before>0&&drive("key","End"));
        for (unsigned i=0;i<200&&after<=before;++i) { pump();after=component_top(app,"Vertical scrollbar"); }
        CHECK(after>before&&pdf_text_is(selected,expected)&&input_chrome(display,id,"pdf-properties-end"));
        CHECK(drive("key","Escape")&&observe(app,"Properties",1)&&state(selected,ATSPI_STATE_DEFUNCT));
        CHECK(drive("key","Return")&&observe(app,"PDF properties text",0)&&
            (new_selected=find(app,"PDF properties text",0))&&pdf_text_is(new_selected,expected));
        CHECK(press(app,"Close properties")&&observe(app,"Properties",1)&&state(new_selected,ATSPI_STATE_DEFUNCT));
        CHECK(drive("key","Escape")&&observe(app,"Pages",1)&&focus(app,"Close")&&drive("key","Return"));
        puts("PDF_PROPERTIES verified: pinned generation, Unicode fields, keyboard scroll, native read-only text and nested focus restoration");
        goto closed;
    }
    if (thumbnails) {
        CHECK(focus(app,"Pages")&&drive("key","Return")&&observe(app,"PDF pages",0));
        CHECK(pdf_preview_ready(app,1)&&(old_link=pdf_ready_preview(app,2))&&pdf_preview_ready(app,3));
        CHECK(atspi_accessible_get_role(old_link,NULL)==ATSPI_ROLE_PUSH_BUTTON&&state(old_link,ATSPI_STATE_SELECTED));
        CHECK(!press(app,"Close")&&pdf_preview_pixels(display,outer)&&input_chrome(display,id,"pdf-previews-1x"));
        CHECK(drive("key","Escape")&&observe(app,"Pages",1)&&state(old_link,ATSPI_STATE_DEFUNCT));
        CHECK(drive("key","Return")&&observe(app,"PDF pages",0)&&pdf_preview_ready(app,3));
        CHECK(press(app,"Close pages")&&observe(app,"Pages",1));
        CHECK(press(app,"Scale 2x")&&observe(app,"Waiting for scaled frame.",0));
        CHECK(press(app,"Publish frame")&&observe(app,"PDF page 2 of 7 at 2x.",0));
        CHECK(press(app,"Pages")&&observe(app,"PDF pages",0)&&pdf_preview_ready(app,3));
        CHECK(pdf_preview_pixels(display,outer)&&input_chrome(display,id,"pdf-previews-2x"));
        CHECK(press(app,"Next previews")&&pdf_preview_ready(app,4)&&pdf_preview_ready(app,5)&&pdf_preview_ready(app,6));
        CHECK(!find(app,"Page 3",0)&&press(app,"Next previews")&&pdf_preview_ready(app,7));
        CHECK(!find(app,"Page 6",0)&&!press(app,"Next previews")&&input_chrome(display,id,"pdf-previews-last"));
        CHECK(focus(app,"Page 7")&&drive("key","Return")&&observe(app,"PDF page 7 of 7 at 2x.",0));
        CHECK((hit_text=pdf_text(app,"Last preview page\n\n"))&&state(second_text,ATSPI_STATE_DEFUNCT));
        CHECK(focus(app,"Close")&&capture(display,outer,0x00ffffu,"pdf-page7")&&drive("key","Return"));
        puts("PDF_PREVIEWS verified: bounded pages, retained cache, native pixels, scale change, modal focus and page navigation");
        goto closed;
    }
    if (zoom_case) {
        CHECK((back=find(app,"PDF link to page 1",0))&&pdf_link_bounds(back));
        CHECK(press(app,"Zoom in")&&pdf_zoom_pixels(display,outer,224,"pdf-zoom-center"));
        CHECK(pdf_zoom_link(back,408,56)&&pdf_zoom_word(second_text,0,16,43));
        CHECK(focus(app,viewport)&&drive("key","Right")&&pdf_zoom_pixels(display,outer,176,"pdf-zoom-key-pan"));
        CHECK(pdf_zoom_link(back,360,104)&&pdf_zoom_word(second_text,0,-1,0));
        CHECK(point_at(id,252,120)&&drive("mousedown","2")&&point_at(id,292,120)&&drive("mouseup","2")&&
            drive("park",NULL)&&pdf_zoom_pixels(display,outer,216,"pdf-zoom-pointer-pan"));
        CHECK(pdf_zoom_link(back,400,64)&&pdf_zoom_word(second_text,5,64,107));
        CHECK(focus(app,"Find in PDF")&&pdf_find_text(app,"page")&&observe(app,"1 match on page",0)&&
            pdf_zoom_highlight(display,outer,"pdf-zoom-search"));
        CHECK(drive("key","ctrl+a")&&drive("key","BackSpace")&&pdf_zoom_pixels(display,outer,216,"pdf-zoom-clear"));
        CHECK(press(app,"Select text")&&observe(app,"PDF text selection",1));
        CHECK((selected=find(app,"PDF text selection",0))&&(selection_text=atspi_accessible_get_text_iface(selected)));
        CHECK(point_at(id,74,35)&&drive("click","1")&&pdf_selection_is(selection_text,"page",5,9)&&
            pdf_zoom_highlight(display,outer,"pdf-zoom-selection"));
        CHECK(drive("park",NULL)&&drive("key","Escape")&&observe(app,"PDF page text",0));
        CHECK(press(app,"Scale 2x")&&observe(app,"Waiting for scaled frame.",0)&&state(back,ATSPI_STATE_DEFUNCT));
        CHECK(press(app,"Publish frame")&&observe(app,"PDF page 2 of 2 at 2x.",0));
        CHECK(pdf_zoom_pixels(display,outer,216,"pdf-zoom-backing"));
        CHECK((scaled_text=pdf_text(app,"Grün page two\n\n"))&&pdf_zoom_word(scaled_text,5,64,107));
        CHECK(press(app,"Fit page")&&capture(display,outer,blue,"pdf-page2-scaled"));
        CHECK(pdf_word_geometry(scaled_text,72,4,145)&&focus(app,"Close")&&drive("key","Return"));
        puts("PDF_ZOOM verified: clipped bitmap and geometry, keyboard and mouse pan, search/selection and backing scale");
        goto closed;
    }
    if (compose) {
        CHECK(drive("key","ctrl+f")&&observe(app,"Find in PDF",1)&&pdf_find_text(app,""));
        for (unsigned i=0;i<200&&!pdf_find_value(app,"");++i) pump();
        CHECK(pdf_find_value(app,""));
        CHECK(drive("key","108")&&drive("keydown","Shift_L")&&drive("key","apostrophe")&&drive("keyup","Shift_L"));
        CHECK(host_marker(host_log,"PDF_FIND_PREEDIT")&&pdf_find_value(app,""));
        CHECK(drive("key","u")&&observe(app,"1 match on page",0)&&pdf_find_value(app,"ü"));
        CHECK(pdf_find_pixels(display,outer,1,"pdf-find-compose"));
        CHECK(pdf_find_text(app,"Résumé")&&observe(app,"0 matches on page",0));
        puts("PDF_FIND_IME verified: native preedit leaves the query empty and commit finds the matching word");
    }
    CHECK(focus(app,viewport)&&drive("key","Tab"));
    if(links) CHECK(observe(app,"PDF link to page 1",1)&&drive("key","Tab")&&state(old_link,ATSPI_STATE_DEFUNCT));
    if (pdf_case) {
        CHECK(observe(app,"Find in PDF",1));
        if (compose) CHECK(focus(app,"Scale 2x"));
        else CHECK(drive("key","Tab"));
    }
    CHECK(observe(app,"Scale 2x",1)&&drive("key","Return"));
    CHECK(observe(app,"Waiting for scaled frame.",0)&&state(fresh,ATSPI_STATE_DEFUNCT));
    if(pdf_case)CHECK(state(second_text,ATSPI_STATE_DEFUNCT));
    CHECK(focus(app,"Publish frame")&&drive("key","Return")&&observe(app,pdf_case?"PDF page 2 of 2 at 2x.":"Scaled frame ready.",0));
    CHECK(host_marker(host_log,"CONTENT_STATE generation=2 resize=2 width=896 height=416 sequence=1 copied=3"));
    CHECK(capture(display,outer,pdf_case?blue:0xbf3f89u,pdf_case?"pdf-page2-scaled":"scaled"));
    if(pdf_case) {
        CHECK((scaled_text=pdf_text(app,"Grün page two\n\n"))&&pdf_word_geometry(scaled_text,72,4,145)&&
            observe(app,"0 matches on page",0));
        CHECK(focus(app,"Previous page")&&drive("key","Return")&&observe(app,"PDF page 1 of 2 at 2x.",0));
        CHECK(host_marker(host_log,"CONTENT_STATE generation=2 resize=2 width=896 height=416 sequence=2 copied=4"));
        CHECK((returned_text=pdf_text(app,"Résumé page one\n\n"))&&state(scaled_text,ATSPI_STATE_DEFUNCT)&&
            pdf_word_geometry(returned_text,120,6,195));
        CHECK(observe(app,"1 match on page",0)&&pdf_find_pixels(display,outer,0,"pdf-find-resume-scaled"));
        CHECK(drive("key","ctrl+f")&&observe(app,"Find in PDF",1));
        if (compose) CHECK(pdf_find_text(app,""));
        else CHECK(drive("key","ctrl+a")&&drive("key","BackSpace"));
        CHECK(capture(display,outer,blue,"pdf-previous"));
        CHECK(pdf_find_text(app,"page")&&observe(app,"2 matches in document",0));
        CHECK(pdf_find_text(app,"Grün")&&observe(app,"0 matches on page",0)&&observe(app,"1 match in document",0));
        CHECK(focus(app,"Next hit")&&drive("key","Return")&&observe(app,"PDF page 2 of 2 at 2x.",0));
        CHECK((hit_text=pdf_text(app,"Grün page two\n\n"))&&state(returned_text,ATSPI_STATE_DEFUNCT));
        CHECK(observe(app,viewport,1)&&focus(app,"Previous page"));
        CHECK(observe(app,"1 match on page",0)&&pdf_find_pixels(display,outer,1,"pdf-document-hit-two"));
        CHECK(focus(app,"Find in PDF")&&pdf_find_text(app,"Résumé")&&observe(app,"0 matches on page",0));
        CHECK(press(app,"Next hit")&&observe(app,"PDF page 1 of 2 at 2x.",0)&&state(hit_text,ATSPI_STATE_DEFUNCT));
        CHECK(observe(app,"Find in PDF",1)&&focus(app,"Next page"));
        CHECK(observe(app,"1 match on page",0)&&pdf_find_pixels(display,outer,0,"pdf-document-hit-one"));
        puts("PDF_DOCUMENT_FIND verified: totals, keyboard jump, AT-SPI wrap and accepted page highlight");
        puts("PDF_FIND verified: keyboard focus, clear, accessible UTF-8 query, page rollover and logical highlight");
        puts("PDF_TEXT AT-SPI verified: UTF-8 pages, word bounds, point offsets, unmapped spaces, retained failure and resize");
    }
    if(links) {
        CHECK((link=find(app,"PDF link to page 2",0))&&pdf_link_bounds(link));
        if (compose) CHECK(point_at(id,350,100)&&drive("click","1")&&drive("park",NULL));
        else CHECK(focus(app,viewport)&&drive("key","Tab")&&observe(app,"PDF link to page 2",1)&&drive("key","Return"));
        CHECK(observe(app,"PDF page 2 of 2 at 2x.",0)&&state(link,ATSPI_STATE_DEFUNCT));
        CHECK((back=find(app,"PDF link to page 1",0))&&pdf_link_bounds(back));
        CHECK(press(app,"PDF link to page 1")&&observe(app,"PDF page 1 of 2 at 2x.",0)&&state(back,ATSPI_STATE_DEFUNCT));
        puts("PDF_LINK verified: both destination forms, accepted bounds and defunct prior links");
    }
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
    if(first_text)g_object_unref(first_text);if(second_text)g_object_unref(second_text);
    if(scaled_text)g_object_unref(scaled_text);if(returned_text)g_object_unref(returned_text);
    if(hit_text)g_object_unref(hit_text);
    if(old_link)g_object_unref(old_link);
    if(link)g_object_unref(link);
    if(back)g_object_unref(back);
    if(external)g_object_unref(external);
    if(selection_text)g_object_unref(selection_text);
    if(selection_edit)g_object_unref(selection_edit);
    if(new_text)g_object_unref(new_text);
    if(selected)g_object_unref(selected);
    if(new_selected)g_object_unref(new_selected);
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
