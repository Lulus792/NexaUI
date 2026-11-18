#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"content peer line %d: %s\n",__LINE__,#x); goto done; } } while (0)
#define CONTENT_HOST_LOG_LIMIT (128u*1024u)
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

static int portrait_semantics(AtspiAccessible *app,const char *name,int zoomed) {
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

static int jpeg_quadrants(Display *display,Window window,int x,int y,int spread,const char *name) {
    static const unsigned long expected[]={0x193cdcu,0xe61e28u,0xe1cd23u,0x23d237u};
    int extent=2*spread+1;
    for (unsigned attempt=0;attempt<100;++attempt) {
        XImage *image=XGetImage(display,window,x-spread,y-spread,(unsigned)extent,(unsigned)extent,AllPlanes,ZPixmap);
        if (!image) return 0;
        int ok=1;
        for (int i=0;ok&&i<4;++i) {
            unsigned long pixel=XGetPixel(image,i%2?extent-1:0,i/2?extent-1:0);
            for (unsigned shift=0;shift<24;shift+=8)
                if (abs((int)((pixel>>shift)&255u)-(int)((expected[i]>>shift)&255u))>4) ok=0;
        }
        if (ok) ok=save_ppm(image,0,0,extent,extent,name);
        XDestroyImage(image);
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
static int browser_pixel(Display *display,Window window,int x,int y,unsigned long color) {
    unsigned long actual=0;
    for (unsigned i=0;i<150;++i) {
        XImage *image=XGetImage(display,window,content_left+x,content_top+y,1,1,AllPlanes,ZPixmap);
        if (!image) return 0;
        actual=XGetPixel(image,0,0)&0xffffffu;
        XDestroyImage(image);
        if (actual==color) return 1;
        pump();
    }
    fprintf(stderr,"Browser pixel %d,%d got %06lx expected %06lx\n",x,y,actual,color);
    return 0;
}
static AtspiAccessible *await_preview(AtspiAccessible *app,const char *name,const char *status) {
    for (unsigned i=0;i<200;++i) {
        AtspiAccessible *node=find(app,name,0);
        char *description=node?atspi_accessible_get_description(node,NULL):NULL;
        int ready=description&&!strcmp(description,status);
        g_free(description);
        if (ready) return node;
        if (node) g_object_unref(node);
        pump();
    }
    return NULL;
}
static AtspiAccessible *pdf_ready_preview(AtspiAccessible *app,unsigned page) {
    char name[24];
    snprintf(name,sizeof(name),"Page %u",page);
    return await_preview(app,name,"Preview ready");
}
static int gallery_preview(AtspiAccessible *app,const char *name,const char *status,int selected) {
    AtspiAccessible *node=await_preview(app,name,status);
    if (!node) return 0;
    int ok=atspi_accessible_get_role(node,NULL)==ATSPI_ROLE_PUSH_BUTTON&&state(node,ATSPI_STATE_SELECTED)==selected;
    g_object_unref(node);
    return ok;
}
static int jpeg_gallery_preview(AtspiAccessible *app,Display *display,Window window,const char *name) {
    AtspiAccessible *node=await_preview(app,name,"Preview ready");
    AtspiComponent *component=node?atspi_accessible_get_component_iface(node):NULL;
    AtspiRect *rect=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    int ok=rect && jpeg_quadrants(display,window,content_left-16+rect->x+rect->width/2,
        content_top-136+rect->y+48,10,"jpeg-preview");
    g_free(rect);
    if (component) g_object_unref(component);
    if (node) g_object_unref(node);
    return ok;
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
static int host_markers(FILE *log,const char *marker,unsigned required) {
    char *text=malloc(CONTENT_HOST_LOG_LIMIT);
    if (!text) return 0;
    int found=0;
    for(unsigned i=0;i<200;++i){
        ssize_t bytes=pread(fileno(log),text,CONTENT_HOST_LOG_LIMIT-1u,0);
        if(bytes<0||(size_t)bytes==CONTENT_HOST_LOG_LIMIT-1u) break;
        text[bytes]=0;
        unsigned seen=0;
        for (const char *p=text;(p=strstr(p,marker));p+=strlen(marker)) {
            if (++seen==required) {
                found=1;
                break;
            }
        }
        if (found) break;
        pump();
    }
    free(text);
    return found;
}
static int host_marker(FILE *log,const char *marker) { return host_markers(log,marker,1); }

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

static int browser_ax_parent(AtspiAccessible *node,const char *name) {
    AtspiAccessible *parent=atspi_accessible_get_parent(node,NULL);
    gchar *actual=parent?atspi_accessible_get_name(parent,NULL):NULL;
    int ok=actual&&!strcmp(actual,name);
    g_free(actual);
    if (parent) g_object_unref(parent);
    return ok;
}

static int browser_ax_document(AtspiAccessible *node,const char *title) {
    AtspiAccessible *group=atspi_accessible_get_parent(node,NULL);
    AtspiAccessible *document=group?atspi_accessible_get_parent(group,NULL):NULL;
    int ok=group&&document&&browser_ax_parent(node,"Guide content")&&browser_ax_parent(group,title)&&
        browser_ax_parent(document,"Browser content");
    if (document) g_object_unref(document);
    if (group) g_object_unref(group);
    return ok;
}

static int browser_ax_image(AtspiAccessible *app) {
    AtspiAccessible *node=find(app,"Guide illustration",0);
    AtspiComponent *component=node?atspi_accessible_get_component_iface(node):NULL;
    AtspiRect *bounds=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    int ok=bounds&&atspi_accessible_get_role(node,NULL)==ATSPI_ROLE_IMAGE&&browser_ax_parent(node,"Guide content")&&
        bounds->x==452&&bounds->y==206&&bounds->width==12&&bounds->height==24;
    if (bounds&&!ok) fprintf(stderr,"Browser AX image bounds %d,%d %dx%d role=%d\n",
        bounds->x,bounds->y,bounds->width,bounds->height,(int)atspi_accessible_get_role(node,NULL));
    if (bounds) g_free(bounds);
    if (component) g_object_unref(component);
    if (node) g_object_unref(node);
    return ok;
}

static int guest_video(AtspiAccessible *app, int desktop) {
    AtspiAccessible *node=find(app,desktop?"Guest video, 800 × 600 pixels":"Guest video, 640 × 400 pixels",0);
    AtspiComponent *component=node?atspi_accessible_get_component_iface(node):NULL;
    AtspiRect *rect=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    int ok=rect&&atspi_accessible_get_role(node,NULL)==ATSPI_ROLE_IMAGE&&browser_ax_parent(node,"Guest display")&&
        abs(rect->x-(desktop?101:74))<=1&&rect->y==136&&abs(rect->width-(desktop?277:332))<=1&&rect->height==208;
    if (rect&&!ok) fprintf(stderr,"Guest image geometry %d,%d %dx%d\n",rect->x,rect->y,rect->width,rect->height);
    g_free(rect);
    if (component) g_object_unref(component);
    if (node) g_object_unref(node);
    return ok;
}

static int guest_inspector_text(AtspiAccessible *app,const char *dialog,const char *prefix,char *value,size_t size) {
    AtspiAccessible *panel=find(app,dialog,0);
    if (!panel) return 0;
    int count=atspi_accessible_get_child_count(panel,NULL), found=0;
    size_t length=strlen(prefix);
    for (int i=0;i<count&&!found;++i) {
        AtspiAccessible *row=atspi_accessible_get_child_at_index(panel,i,NULL);
        char *name=row?atspi_accessible_get_name(row,NULL):NULL;
        if (name&&!strncmp(name,prefix,length)&&strlen(name+length)<size) {
            strcpy(value,name+length);
            found=1;
        }
        g_free(name);
        if (row) g_object_unref(row);
    }
    g_object_unref(panel);
    return found;
}

static int guest_resource_counter(AtspiAccessible *app,const char *prefix,unsigned long long *value) {
    char line[40],suffix[7]={0},end=0;
    return guest_inspector_text(app,"Guest resources",prefix,line,sizeof(line))&&
        sscanf(line,"%llu %6s%c",value,suffix,&end)==2&&!strcmp(suffix,"bytes");
}

typedef struct guest_reset_scope { char environment[33], checkpoint[65]; } guest_reset_scope;
static int read_guest_reset_scope(AtspiAccessible *app,guest_reset_scope *scope,const char *directory) {
    const char *dialog="Reset preview", *hex="0123456789abcdef";
    char grant[128];
    snprintf(grant,sizeof(grant),"Host files stay: %s",directory?directory:"");
    return observe(app,"Checkpoint SHA-256",0)&&observe(app,"Work disk: 16777216 bytes",0)&&
        observe(app,"Work disk changes would be replaced.",0)&&observe(app,directory?grant:"No external write grants.",0)&&
        guest_inspector_text(app,dialog,"Environment ",scope->environment,sizeof(scope->environment))&&
        guest_inspector_text(app,dialog,"Checkpoint 1 of 2 ",scope->checkpoint,33)&&
        guest_inspector_text(app,dialog,"Checkpoint 2 of 2 ",scope->checkpoint+32,33)&&
        strlen(scope->environment)==32&&strspn(scope->environment,hex)==32&&
        strlen(scope->checkpoint)==64&&strspn(scope->checkpoint,hex)==64;
}

static int read_guest_sharing(AtspiAccessible *app,const char *directory,const char *status,const char *network,
                              const char *microphone) {
    char row[128];
    snprintf(row,sizeof(row),"Host directory: %s",directory?directory:"");
    const char *clipboard=getenv("NEXAUI_CONTENT_TEST_GUEST_CLIPBOARD")?
        "Clipboard: one-time text transfers":"Clipboard: unavailable";
    return observe(app,"Guest sharing",0)&&observe(app,clipboard,0)&&
        observe(app,status?status:directory?"Files: read/write":"Files: unavailable",0)&&
        observe(app,network?network:"Network: unavailable",0)&&
        observe(app,microphone?microphone:"Microphone: unavailable",0)&&
        (getenv("NEXAUI_CONTENT_TEST_GUEST_USB")||observe(app,"Devices: unavailable",0))&&
        (!directory||observe(app,row,0));
}

static int guest_command(const char *command) {
    const struct timespec input_interval={0,45000000};
    for (const char *key=command;*key;++key) {
        char name[2]={*key,0};
        if (!drive("key",name)) return 0;
        nanosleep(&input_interval,NULL);
    }
    return drive("key","Return");
}

static int release_guest_keys(AtspiAccessible *app) {
    return drive("keydown","ctrl")&&drive("keydown","shift")&&drive("key","F12")&&
        observe(app,"Guest input released.",0)&&observe(app,"Close",1)&&
        drive("keyup","shift")&&drive("keyup","ctrl");
}

static int offer_host_clipboard(const char *source,const char *text) {
    FILE *input=tmpfile();
    if (!input) return 0;
    int ok=fputs(text,input)>=0&&!fflush(input)&&!fseek(input,0,SEEK_SET);
    pid_t child=ok?fork():-1;
    if (!child) {
        if (dup2(fileno(input),STDIN_FILENO)<0) _exit(125);
        execlp("timeout","timeout","4",source,"--type","text/plain;charset=utf-8",(char *)NULL);
        _exit(125);
    }
    fclose(input);
    if (child<0) return 0;
    int status;
    pid_t joined;
    do { joined=waitpid(child,&status,0); } while (joined<0&&errno==EINTR);
    return joined==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
}

static int await_guest_marker(const char *expected,unsigned attempts) {
    const char *socket=getenv("NEXAUI_CONTENT_TEST_GUEST_SOCKET");
    const char *end=socket?strrchr(socket,'/'):NULL;
    if (!end||end-socket>=80) return 0;
    char pattern[128];
    snprintf(pattern,sizeof(pattern),"%.*s/qemu-guest-*/guest.trace",(int)(end-socket),socket);
    glob_t paths={0};
    int found=glob(pattern,0,NULL,&paths)==0&&paths.gl_pathc==1;
    FILE *trace=found?fopen(paths.gl_pathv[0],"rb"):NULL;
    globfree(&paths);
    if (!trace) return 0;
    int ok=0;
    for (unsigned i=0;i<attempts;++i) {
        char line[512];
        while (fgets(line,sizeof(line),trace)) {
            line[strcspn(line,"\r\n")]=0;
            if (!strcmp(line,expected)) ok=1;
        }
        if (ok) break;
        rewind(trace);
        pump();
    }
    fclose(trace);
    return ok;
}

static int guest_console_marker(AtspiAccessible *app,const char *window,const char *command,const char *marker) {
    return focus(app,"Enter guest")&&drive("key","Return")&&
        observe(app,"Guest input active: Ctrl+Shift+F12 exits.",0)&&
        point_at(window,155,100)&&drive("click","1")&&guest_command(command)&&
        await_guest_marker(marker,600)&&release_guest_keys(app);
}

static int guest_ax_key(AtspiAccessible *app,const char *key,const char *marker) {
    return focus(app,"Enter guest")&&drive("key","Return")&&
        observe(app,"Guest input active: Ctrl+Shift+F12 exits.",0)&&
        drive("key",key)&&await_guest_marker(marker,600)&&release_guest_keys(app);
}

static int guest_ax_tree(AtspiAccessible *app,AtspiAccessible *label) {
    AtspiAccessible *view=find(app,"Guest display",0);
    AtspiAccessible *root=view?find(view,"Guest accessibility check",0):NULL;
    AtspiAccessible *button=root?find(root,"Inspect guest job",0):NULL;
    AtspiAccessible *disabled=root?find(root,"Disabled guest command",0):NULL;
    AtspiAccessible *private_entry=root?find(root,"private-example-entry",0):NULL;
    AtspiComponent *component=button?atspi_accessible_get_component_iface(button):NULL;
    AtspiRect *bounds=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,NULL):NULL;
    AtspiAction *actions=button?atspi_accessible_get_action_iface(button):NULL;
    int ok=root&&atspi_accessible_get_child_count(root,NULL)==3&&browser_ax_parent(root,"Guest display")&&
        browser_ax_parent(label,"Guest accessibility check")&&
        bounds&&atspi_accessible_get_role(button,NULL)==ATSPI_ROLE_PUSH_BUTTON&&
        abs(bounds->x-129)<=1&&abs(bounds->y-195)<=1&&abs(bounds->width-222)<=1&&abs(bounds->height-39)<=1&&
        !state(button,ATSPI_STATE_FOCUSABLE)&&(!actions||atspi_action_get_n_actions(actions,NULL)==0)&&
        disabled&&atspi_accessible_get_role(disabled,NULL)==ATSPI_ROLE_PUSH_BUTTON&&
        !state(disabled,ATSPI_STATE_ENABLED)&&!private_entry;
    if (!ok&&bounds) fprintf(stderr,"Guest AX button bounds %d,%d %dx%d\n",
        bounds->x,bounds->y,bounds->width,bounds->height);
    if (actions) g_object_unref(actions);
    g_free(bounds);
    if (component) g_object_unref(component);
    if (private_entry) g_object_unref(private_entry);
    if (disabled) g_object_unref(disabled);
    if (button) g_object_unref(button);
    if (root) g_object_unref(root);
    if (view) g_object_unref(view);
    return ok;
}

static int guest_ax_retired(AtspiAccessible *app,AtspiAccessible *old) {
    for (unsigned i=0;i<200;++i) {
        AtspiAccessible *root=find(app,"Guest accessibility check",0);
        if (root) g_object_unref(root);
        else if (state(old,ATSPI_STATE_DEFUNCT)) return guest_video(app,1);
        pump();
    }
    return 0;
}

static int guest_ax_flow(AtspiAccessible *app,Display *display,const char *window) {
    const char *ready="Gaststatus – bereit",*updated="Gaststatus – fertig";
    AtspiAccessible *label=NULL,*second=NULL;
    int ok=0;
    CHECK(guest_console_marker(app,window,"axopen","NEXAUI_GUEST_AX_OPEN"));
    CHECK(await_guest_marker("NEXAUI_GUEST_AX_APP_STARTED",2400));
    CHECK(observe(app,ready,0));
    CHECK((label=find(app,ready,0))&&pdf_text_is(label,ready)&&guest_ax_tree(app,label)&&
        guest_video(app,1)&&input_chrome(display,window,"guest-ax-ready"));
    CHECK(guest_ax_key(app,"F8","NEXAUI_GUEST_AX_UPDATED")&&observe(app,updated,0)&&
        !state(label,ATSPI_STATE_DEFUNCT)&&pdf_text_is(label,updated)&&input_chrome(display,window,"guest-ax-updated"));
    CHECK(guest_ax_key(app,"F9","NEXAUI_GUEST_AX_APP_EXITED")&&guest_ax_retired(app,label));
    CHECK(guest_console_marker(app,window,"axopen","NEXAUI_GUEST_AX_OPEN"));
    for (unsigned attempt=0;attempt<12&&!second;++attempt) {
        if (observe(app,ready,0)) second=find(app,ready,0);
    }
    CHECK(second&&second!=label&&pdf_text_is(second,ready));
    CHECK(press(app,"Disconnect")&&observe(app,"Console disconnected. Guest retained.",0)&&
        guest_ax_retired(app,second)&&input_chrome(display,window,"guest-ax-disconnected"));
    puts("GUEST_AX_NATIVE verified: scoped read-only text, roles and bounds, update, guest exit, new tree and disconnect retirement");
    ok=1;
done:
    if (second) g_object_unref(second);
    if (label) g_object_unref(label);
    return ok;
}

static int guest_microphone_review(AtspiAccessible *app,const char *window,const char *command,
                                   const char *started,const char *sampled) {
    return guest_console_marker(app,window,command,started)&&
        focus(app,"Sharing")&&drive("key","Return")&&observe(app,"Guest sharing",0)&&await_guest_marker(sampled,1600);
}

static int read_guest_export(const char *directory, struct stat *info) {
    char path[128];
    snprintf(path,sizeof(path),"%s/guest-export.txt",directory);
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if (fd<0) return 0;
    static const char expected[]="NexaUI guest export\n";
    char data[sizeof(expected)];
    int ok=!fstat(fd,info)&&S_ISREG(info->st_mode)&&info->st_size==(off_t)(sizeof(expected)-1)&&
        read(fd,data,sizeof(data))==(ssize_t)(sizeof(expected)-1)&&!memcmp(expected,data,sizeof(expected)-1);
    close(fd);
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
        strcmp(test_case,"browser")&&strcmp(test_case,"guest")&&strcmp(test_case,"guest-desktop")&&
        strcmp(test_case,"guest-storage")&&strcmp(test_case,"guest-reset")&&
        strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close")))return 2;
    const char *uri_receipt=getenv("NEXAUI_CONTENT_TEST_URI_RECEIPT");
    if (uri_receipt && !strncmp(argv[1],"http",4)) return uri_received(uri_receipt,argv[1]);
    int browser_case=!strcmp(test_case,"browser");
    int guest_reset=!strcmp(test_case,"guest-reset");
    int guest_storage=guest_reset||!strcmp(test_case,"guest-storage");
    int guest_desktop=guest_storage||!strcmp(test_case,"guest-desktop");
    int guest_case=guest_desktop||!strcmp(test_case,"guest");
    int guest_ax=guest_desktop&&getenv("NEXAUI_CONTENT_TEST_GUEST_AX");
    int browser_input=browser_case&&getenv("NEXAUI_CONTENT_TEST_BROWSER_INPUT");
    int browser_pointer=browser_case&&getenv("NEXAUI_CONTENT_TEST_BROWSER_POINTER");
    int browser_ax=browser_case&&getenv("NEXAUI_CONTENT_TEST_BROWSER_AX");
    int png_case=browser_case||!strcmp(test_case,"png");
    int png_viewer=png_case&&getenv("NEXAUI_CONTENT_TEST_PNG_VIEWER");
    int gallery=png_case&&getenv("NEXAUI_CONTENT_TEST_PNG_GALLERY");
    int jpeg_gallery=gallery&&getenv("NEXAUI_CONTENT_TEST_JPEG_GALLERY");
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
    const char *viewport=pdf_case?"PDF page image":browser_case?"Browser content":guest_case?"Guest display":
        png_case?"PNG image":"Fixture frame";
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
        if(browser_input) {
            const char *wire=getenv("NEXAUI_CONTENT_TEST_BROWSER_WIRE");
            int fd=wire?open(wire,O_WRONLY|O_CREAT|O_EXCL,0600):-1;
            if(fd<0||dup2(fd,STDERR_FILENO)<0||close(fd)||setenv("WAYLAND_DEBUG","client",1))_exit(126);
        }
        execl(argv[1],argv[1],(char *)NULL);_exit(127);
    }
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop&&(app=application(desktop,child)));
    CHECK(drive("windowfocus",id)&&drive("park",NULL));
    if (gallery) {
        const char *broken=jpeg_gallery?"Defekt.jpg":"Defekt.png";
        CHECK(observe(app,"Image 1 of 7.",0)&&observe(app,"Gallery",1)&&capture(display,outer,blue,"png-gallery-first"));
        CHECK(drive("key","Return")&&observe(app,"Image gallery",0));
        CHECK(gallery_preview(app,"Meer.png","Preview ready",1)&&gallery_preview(app,broken,"Preview unavailable",0)&&
            gallery_preview(app,"Wald.png","Preview ready",0)&&input_chrome(display,id,"png-gallery-previews"));
        CHECK(drive("key","Escape")&&observe(app,"Gallery",1)&&drive("key","Return"));
        CHECK(gallery_preview(app,"Wald.png","Preview ready",0)&&press(app,broken));
        CHECK(observe(app,"Image decoding failed; image retained.",0)&&capture(display,outer,blue,"png-gallery-retained"));
        CHECK(press(app,"Scale 2x")&&observe(app,"Image 1 of 7.",0)&&press(app,"Gallery"));
        CHECK(gallery_preview(app,"Meer.png","Preview ready",1)&&gallery_preview(app,"Wald.png","Preview ready",0));
        CHECK(input_chrome(display,id,"png-gallery-previews-scaled")&&press(app,"Next previews"));
        CHECK(gallery_preview(app,"Abend.png","Preview ready",0)&&gallery_preview(app,"Morgen.png","Preview ready",0)&&
            gallery_preview(app,"Himmel.png","Preview ready",0)&&press(app,"Next previews"));
        const char *last=jpeg_gallery?"Küste.jpg":"Küste.png";
        CHECK(gallery_preview(app,last,"Preview ready",0)&&!press(app,"Next previews"));
        if (jpeg_gallery) CHECK(jpeg_gallery_preview(app,display,outer,last));
        CHECK(focus(app,last)&&drive("key","Return")&&observe(app,"Image 7 of 7.",0));
        if (jpeg_gallery) {
            const char *name="JPEG image, 2048 × 4096 pixels";
            CHECK(portrait_semantics(app,name,0)&&jpeg_quadrants(display,outer,content_left+224,content_top+104,30,"jpeg-fit"));
            CHECK(press(app,"Zoom in")&&portrait_semantics(app,name,1)&&
                jpeg_quadrants(display,outer,content_left+224,content_top+104,60,"jpeg-zoom"));
            CHECK(press(app,"Gallery")&&press(app,"Previous previews")&&press(app,"Previous previews")&&
                focus(app,"Meer.png")&&drive("key","Return")&&observe(app,"Image 1 of 7.",0)&&
                observe(app,"PNG image, 112 × 52 pixels",0)&&capture(display,outer,blue,"jpeg-return-to-png"));
            puts("JPEG_GALLERY verified: oriented preview, fit, zoom, original dimensions and return to PNG");
        } else {
            CHECK(observe(app,"PNG image, 112 × 52 pixels",0)&&capture(display,outer,0xd34f1fu,"png-gallery-last"));
        }
        CHECK(focus(app,"Close")&&drive("key","Return"));
        puts("PNG_GALLERY verified: named files, pixel previews, failed image, keyboard, retained cache and backing change");
        goto closed;
    }
    CHECK(observe(app,"Waiting for frame.",0)&&observe(app,"Replace tab",1));
    CHECK(capture(display,outer,0xe3e7efu,"pending"));
    CHECK(drive("key","Tab")&&observe(app,"Close",1));
    if(!strcmp(test_case,"pending-close")){CHECK(drive("key","Return"));goto closed;}
    CHECK(drive("key","shift+Tab")&&observe(app,"Replace tab",1)&&drive("key","Return"));
    CHECK(observe(app,"Replacement waiting.",0)&&focus(app,"Release old")&&drive("key","Return"));
    CHECK(observe(app,"Old frame rejected.",0)&&capture(display,outer,0xe3e7efu,"rejected"));
    if (browser_ax) CHECK(!find(app,"Retired guide",0));
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
    if (browser_ax) {
        const char *value="Grün, café and 👋.";
        CHECK(observe(app,"Browser guide",0)&&(first_text=find(app,value,0)));
        CHECK(pdf_text_is(first_text,value)&&browser_ax_document(first_text,"Browser guide"));
        CHECK(browser_ax_image(app)&&!find(app,"Hidden fixture marker",0));
        CHECK(browser_pixel(display,outer,410,82,blue)&&input_chrome(display,id,"browser-ax-fresh"));
    } else if (guest_case) {
        CHECK(guest_video(app,guest_desktop)&&
            browser_pixel(display,outer,guest_desktop?156:224,guest_desktop?14:104,
                guest_desktop?0xffffffu:0x0000ffu)&&input_chrome(display,id,"guest-fresh"));
    } else CHECK(capture(display,outer,blue,"fresh"));
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
    CHECK(press(app,browser_case?"Crash browser":guest_case?"Pause guest":"Bad frame"));
    if(png_case||pdf_case||guest_case) {
        const char *failure=pdf_case?"PDF rendering failed; page retained.":
            browser_case?"Browser engine unavailable; view retained.":
            guest_case?"Guest paused. Last display retained.":"PNG decoding failed; image retained.";
        CHECK(observe(app,failure,0));
        CHECK(!state(retired,ATSPI_STATE_DEFUNCT)&&dimensions(retired));
        if (browser_ax) {
            CHECK(!state(first_text,ATSPI_STATE_DEFUNCT)&&pdf_text_is(first_text,"Grün, café and 👋.")&&
                browser_pixel(display,outer,410,82,blue)&&input_chrome(display,id,"browser-ax-retained"));
        } else if (guest_case) {
            CHECK(guest_video(app,guest_desktop)&&
                browser_pixel(display,outer,guest_desktop?156:224,guest_desktop?14:104,
                    guest_desktop?0xffffffu:0x0000ffu)&&input_chrome(display,id,"guest-paused"));
        } else CHECK(capture(display,outer,blue,"bad-retained"));
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
    CHECK(press(app,pdf_case?"Next page":guest_case?"Resume guest":"Publish frame")&&observe(app,
        thumbnails?"PDF page 2 of 7.":pdf_case?"PDF page 2 of 2.":"Frame recovered.",0));
    if(!png_case&&!pdf_case&&!guest_case)CHECK(state(retired,ATSPI_STATE_DEFUNCT)&&stale_focus(old_component));
    CHECK((fresh=find(app,viewport,0))&&dimensions(fresh));
    CHECK(host_marker(host_log,(pdf_case||png_viewer||browser_case||guest_case)?
        "CONTENT_STATE generation=2 resize=1 width=448 height=208 sequence=3 copied=2 copy_bytes=372736":
        "CONTENT_STATE generation=2 resize=1 width=448 height=208 sequence=3 copied=2 copy_bytes=23296"));
    if (png_viewer) {
        CHECK(portrait_semantics(app,"PNG image, 208 × 416 pixels",1)&&png_portrait_pixels(display,outer,1,0,"png-portrait-center"));
        CHECK(focus(app,viewport)&&drive("key","Right")&&png_portrait_pixels(display,outer,1,0,"png-portrait-axis-locked"));
        CHECK(drive("key","Down")&&png_portrait_pixels(display,outer,1,48,"png-portrait-panned"));
        CHECK(press(app,"Scale 2x")&&observe(app,"Waiting for scaled frame.",0));
        CHECK(press(app,"Publish frame")&&observe(app,"Scaled frame ready.",0));
        CHECK(portrait_semantics(app,"PNG image, 208 × 416 pixels",1)&&png_portrait_pixels(display,outer,1,48,"png-portrait-scaled"));
        CHECK(focus(app,viewport)&&drive("key","ctrl+0")&&portrait_semantics(app,"PNG image, 208 × 416 pixels",0));
        CHECK(png_portrait_pixels(display,outer,0,0,"png-portrait-fit")&&focus(app,"Close")&&drive("key","Return"));
        puts("PNG_VIEWER verified: original samples, fitted image bounds, locked axis, pan, metadata and backing scale");
        goto closed;
    }
    if (browser_ax) {
        CHECK(state(first_text,ATSPI_STATE_DEFUNCT)&&observe(app,"Recovered guide",0)&&
            (second_text=find(app,"Grün, café and 👋.",0))&&pdf_text_is(second_text,"Grün, café and 👋.")&&
            browser_ax_document(second_text,"Recovered guide")&&
            browser_ax_image(app));
    } else if (guest_case) {
        CHECK(guest_video(app,guest_desktop)&&(guest_desktop||browser_pixel(display,outer,224,104,0x0000ffu)));
    } else if (detail_case) CHECK(pdf_detail_capture(display,outer,0,"pdf-detail-fit-before"));
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
    CHECK(focus(app,"Publish frame")&&drive("key","Return")&&observe(app,pdf_case?"PDF page 2 of 2 at 2x.":
        guest_case?"Guest running. Keyboard and three-button mouse.":"Scaled frame ready.",0));
    CHECK(host_marker(host_log,"CONTENT_STATE generation=2 resize=2 width=896 height=416 sequence=1 copied=3"));
    if (browser_ax) {
        CHECK(state(second_text,ATSPI_STATE_DEFUNCT)&&browser_ax_image(app)&&browser_pixel(display,outer,410,82,0xbf3f89u));
        CHECK((scaled_text=find(app,"Notes closed.",0))&&pdf_text_is(scaled_text,"Notes closed.")&&
            focus(app,"Show notes")&&observe(app,"Show notes",1)&&press(app,"Show notes"));
        CHECK(observe(app,"Notes opened ✓.",0)&&!state(scaled_text,ATSPI_STATE_DEFUNCT)&&pdf_text_is(scaled_text,"Notes opened ✓."));
        CHECK(browser_pixel(display,outer,32,104,0x21bb71u)&&input_chrome(display,id,"browser-ax-activated"));
        puts("BROWSER_ATSPI verified: document hierarchy, Unicode text, clipped image, retirement and native action");
    } else if (guest_desktop) {
        CHECK(guest_video(app,1)&&input_chrome(display,id,"guest-desktop-scaled"));
        CHECK(observe(app,"Review host access in Sharing.",0));
        const char *directory=NULL;
        char shared[100];
        if (getenv("NEXAUI_CONTENT_TEST_FILE_SHARE")) {
            const char *address=getenv("NEXAUI_CONTENT_TEST_GUEST_SOCKET");
            const char *end=address?strrchr(address,'/'):NULL;
            CHECK(end&&end-address<80);
            snprintf(shared,sizeof(shared),"%.*s/host-export",(int)(end-address),address);
            directory=shared;
        }
        int revoke=getenv("NEXAUI_CONTENT_TEST_FILE_REVOKE")!=NULL;
        int grant=getenv("NEXAUI_CONTENT_TEST_FILE_GRANT")!=NULL;
        int cutoff=getenv("NEXAUI_CONTENT_TEST_FILE_BINDFS")!=NULL;
        const char *network=getenv("NEXAUI_CONTENT_TEST_GUEST_NETWORK")?"Network: 10.0.2.100:8080 access off":NULL;
        const char *microphone=getenv("NEXAUI_CONTENT_TEST_GUEST_MICROPHONE")?"Microphone: test tone off":NULL;
        CHECK((!revoke&&!grant)||directory);
        CHECK(!cutoff||(grant&&revoke));
        CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,grant?"Files: access off":NULL,network,microphone)&&
            input_chrome(display,id,"guest-sharing")&&drive("key","Escape")&&observe(app,"Sharing",1));
        const char *clipboard_source=getenv("NEXAUI_CONTENT_TEST_HOST_WL_COPY");
        if (clipboard_source) {
            const char *payload="Grüße aus NexaUI\nZweite Zeile\n";
            char excess[194];
            memset(excess,'a',sizeof(excess)-1u);
            excess[sizeof(excess)-1u]=0;
            CHECK(offer_host_clipboard(clipboard_source,excess)&&drive("windowfocus",id)&&
                focus(app,"Sharing")&&drive("key","Return")&&focus(app,"Review clipboard text")&&drive("key","Return")&&
                observe(app,"Clipboard text unavailable or cannot be reviewed.",0)&&
                drive("key","Escape")&&observe(app,"Sharing",1));
            CHECK(offer_host_clipboard(clipboard_source,payload)&&drive("windowfocus",id)&&
                focus(app,"Sharing")&&drive("key","Return")&&focus(app,"Review clipboard text")&&drive("key","Return")&&
                observe(app,payload,0)&&input_chrome(display,id,"guest-clipboard-review")&&
                drive("key","Escape")&&observe(app,"Sharing",1));
            CHECK(drive("key","Return")&&focus(app,"Review clipboard text")&&drive("key","Return")&&
                observe(app,payload,0)&&focus(app,"Copy text to guest")&&drive("key","Return")&&
                observe(app,"Guest clipboard confirmed.",0)&&input_chrome(display,id,"guest-clipboard-confirmed")&&
                drive("key","Escape")&&observe(app,"Sharing",1));
            CHECK(guest_console_marker(app,id,"clipcheck","NEXAUI_CLIPBOARD_INDEPENDENT_OK")&&
                input_chrome(display,id,"guest-clipboard-read"));
            puts("GUEST_CLIPBOARD_NATIVE verified: size rejection, cancel, review, keyboard send, and independent guest paste");
            const char *received="Grüße aus dem Gast\nZweite Gastzeile\n";
            CHECK(guest_console_marker(app,id,"clipbad","NEXAUI_CLIPBOARD_GUEST_BAD")&&
                focus(app,"Sharing")&&drive("key","Return")&&focus(app,"Review guest text")&&drive("key","Return")&&
                observe(app,"Clipboard text unavailable or cannot be reviewed.",0)&&
                drive("key","Escape")&&observe(app,"Sharing",1)&&clipboard_reads(payload));
            CHECK(drive("windowfocus",id)&&guest_console_marker(app,id,"clipoffer","NEXAUI_CLIPBOARD_GUEST_OFFER")&&
                focus(app,"Sharing")&&drive("key","Return")&&focus(app,"Review guest text")&&drive("key","Return")&&
                observe(app,received,0)&&input_chrome(display,id,"guest-clipboard-import-review")&&
                drive("key","Escape")&&observe(app,"Sharing",1)&&clipboard_reads(payload));
            CHECK(drive("windowfocus",id)&&focus(app,"Sharing")&&drive("key","Return")&&
                focus(app,"Review guest text")&&drive("key","Return")&&observe(app,received,0)&&
                focus(app,"Copy text to host")&&drive("key","Return")&&observe(app,"Host clipboard offered.",0)&&
                input_chrome(display,id,"guest-clipboard-import-offered")&&drive("key","Escape")&&observe(app,"Sharing",1)&&
                clipboard_reads(received)&&drive("windowfocus",id));
            puts("GUEST_CLIPBOARD_IMPORT_NATIVE verified: guest data review, host selection retained until approval, and independent host paste");
        }
        unsigned long long reads=0, before=0, after=0;
        guest_reset_scope baseline={0}, preview={0};
        CHECK(focus(app,"Resources")&&drive("key","Return")&&observe(app,"Guest resources",0)&&
            observe(app,"1 vCPU, 768 MiB RAM assigned",0)&&observe(app,"Guest running",0));
        if (guest_storage) {
            CHECK(observe(app,"Work disk: 16 MiB, read/write",0)&&
                guest_resource_counter(app,"Device read ",&reads)&&reads>0&&
                guest_resource_counter(app,"Device wrote ",&before));
        } else CHECK(observe(app,"No persistent disk attached",0));
        CHECK(input_chrome(display,id,"guest-resources-first")&&drive("key","Escape")&&observe(app,"Resources",1));
        if (guest_storage) {
            CHECK(focus(app,"Review reset")&&drive("key","Return")&&read_guest_reset_scope(app,&baseline,grant?NULL:directory)&&
                input_chrome(display,id,"guest-reset-first"));
            CHECK(drive("key","Escape")&&observe(app,"Review reset",1));
        }
        CHECK(focus(app,"Enter guest")&&drive("key","Return")&&
            observe(app,"Guest input active: Ctrl+Shift+F12 exits.",0));
        CHECK(point_at(id,140,90)&&host_marker(host_log,"x=157 y=259")&&
            point_at(id,155,100)&&drive("mousedown","1"));
        if (grant) {
            char path[128];
            snprintf(path,sizeof(path),"%s/guest-export.txt",directory);
            CHECK(guest_command("probe")&&browser_pixel(display,outer,156,43,0x00a800u)&&
                release_guest_keys(app)&&drive("mouseup","1"));
            CHECK(access(path,F_OK)!=0&&errno==ENOENT);
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,"Files: access off",network,microphone)&&
                focus(app,"Grant file access")&&drive("key","Return")&&
                read_guest_sharing(app,directory,"Files: activation pending",network,microphone)&&input_chrome(display,id,"guest-grant-pending"));
            CHECK(focus(app,"Check sharing status")&&drive("key","Return")&&read_guest_sharing(app,directory,NULL,network,microphone)&&
                input_chrome(display,id,"guest-granted")&&drive("key","Escape")&&observe(app,"Sharing",1));
            CHECK(focus(app,"Enter guest")&&drive("key","Return")&&
                observe(app,"Guest input active: Ctrl+Shift+F12 exits.",0)&&point_at(id,150,100)&&guest_command("attach")&&
                browser_pixel(display,outer,156,43,0x0000a8u));
            puts("GUEST_GRANT_NATIVE verified: denied before consent, scoped activation and guest mount");
        }
        CHECK(guest_command("nexa")&&await_guest_marker("NEXAUI_DESKTOP_INPUT",600));
        CHECK(browser_pixel(display,outer,156,43,0x00a800u)&&input_chrome(display,id,"guest-desktop-typed"));
        if (cutoff) {
            CHECK(guest_command("hold")&&browser_pixel(display,outer,156,43,0x0000a8u)&&
                input_chrome(display,id,"guest-export-held"));
        }
        CHECK(drive("keydown","ctrl")&&drive("keydown","shift")&&drive("key","F12")&&
            observe(app,"Guest input released.",0)&&observe(app,"Close",1));
        CHECK(drive("mouseup","1")&&drive("keyup","shift")&&drive("keyup","ctrl"));
        if (revoke) {
            struct stat prior={0}, after_probe={0};
            const char *pending="Files: revocation pending", *revoked="Files: revoked";
            CHECK(read_guest_export(directory,&prior));
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,NULL,network,microphone)&&
                focus(app,"Revoke file access")&&drive("key","Return")&&read_guest_sharing(app,directory,pending,network,microphone)&&
                input_chrome(display,id,"guest-revoke-pending"));
            CHECK(focus(app,"Check sharing status")&&drive("key","Return")&&
                read_guest_sharing(app,directory,cutoff?revoked:pending,network,microphone)&&drive("key","Escape")&&observe(app,"Sharing",1));
            if (!cutoff) {
                CHECK(focus(app,"Enter guest")&&drive("key","Return")&&
                    observe(app,"Guest input active: Ctrl+Shift+F12 exits.",0)&&point_at(id,150,100)&&guest_command("detach")&&
                    browser_pixel(display,outer,156,43,0x0000a8u)&&release_guest_keys(app));
            }
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,revoked,network,microphone)&&
                input_chrome(display,id,"guest-revoked")&&drive("key","Escape")&&observe(app,"Sharing",1));
            CHECK(focus(app,"Enter guest")&&drive("key","Return")&&
                observe(app,"Guest input active: Ctrl+Shift+F12 exits.",0)&&point_at(id,150,100)&&guest_command(cutoff?"check":"probe")&&
                browser_pixel(display,outer,156,43,0x00a800u)&&release_guest_keys(app));
            CHECK(read_guest_export(directory,&after_probe)&&prior.st_dev==after_probe.st_dev&&prior.st_ino==after_probe.st_ino);
            if (cutoff) {
                char late[128];
                snprintf(late,sizeof(late),"%s/late.txt",directory);
                CHECK(access(late,F_OK)!=0&&errno==ENOENT&&input_chrome(display,id,"guest-export-cutoff"));
                puts("GUEST_CUTOFF_NATIVE verified: retained mount and handle, denied writes and unchanged host file");
            } else puts("GUEST_REVOKE_NATIVE verified: pending, confirmed removal, guest denied and prior file retained");
        }
        if (network) {
            const char *file=revoke?"Files: revoked":NULL;
            const char *active="Network: 10.0.2.100:8080 private TCP active";
            CHECK(guest_console_marker(app,id,"netoff","NEXAUI_NETWORK_OFF"));
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,file,network,microphone)&&
                focus(app,"Grant network access")&&drive("key","Return")&&
                read_guest_sharing(app,directory,file,"Network: 10.0.2.100:8080 activation pending",microphone)&&
                input_chrome(display,id,"guest-network-pending"));
            CHECK(focus(app,"Check network status")&&drive("key","Return")&&read_guest_sharing(app,directory,file,active,microphone)&&
                input_chrome(display,id,"guest-network-active")&&drive("key","Escape")&&observe(app,"Sharing",1)&&
                guest_console_marker(app,id,"netconnect","NEXAUI_NETWORK_CONNECTED"));
            network="Network: 10.0.2.100:8080 revoked";
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,file,active,microphone)&&
                focus(app,"Revoke network access")&&drive("key","Return")&&read_guest_sharing(app,directory,file,network,microphone)&&
                input_chrome(display,id,"guest-network-revoked")&&drive("key","Escape")&&observe(app,"Sharing",1)&&
                guest_console_marker(app,id,"netcheck","NEXAUI_NETWORK_CUTOFF"));
            puts("GUEST_NETWORK_NATIVE verified: private TCP grant and live cutoff for held and new connections");
        }
        if (microphone) {
            const char *file=revoke?"Files: revoked":NULL;
            const char *active="Microphone: test tone on";
            CHECK(guest_microphone_review(app,id,"micoff","NEXAUI_MICROPHONE_RECORD_START","NEXAUI_MICROPHONE_OFF"));
            CHECK(read_guest_sharing(app,directory,file,network,microphone)&&
                focus(app,"Close sharing")&&drive("key","Tab")&&observe(app,"Enable test tone",1)&&drive("key","Return")&&
                read_guest_sharing(app,directory,file,network,active)&&input_chrome(display,id,"guest-microphone-active")&&
                drive("key","Escape")&&observe(app,"Sharing",1)&&
                guest_microphone_review(app,id,"micon","NEXAUI_MICROPHONE_RECORD_ACTIVE","NEXAUI_MICROPHONE_HELD_ACTIVE"));
            microphone="Microphone: revoked";
            CHECK(read_guest_sharing(app,directory,file,network,active)&&
                focus(app,"Close sharing")&&drive("key","Tab")&&observe(app,"Revoke test tone",1)&&drive("key","Return")&&
                read_guest_sharing(app,directory,file,network,microphone)&&input_chrome(display,id,"guest-microphone-revoked")&&
                drive("key","Escape")&&observe(app,"Sharing",1)&&
                guest_microphone_review(app,id,"miccheck","NEXAUI_MICROPHONE_RECORD_CUTOFF","NEXAUI_MICROPHONE_CUTOFF")&&
                drive("key","Escape")&&observe(app,"Sharing",1));
            puts("GUEST_MICROPHONE_NATIVE verified: held and fresh ALSA capture silent after tone cutoff");
        }
        if (getenv("NEXAUI_CONTENT_TEST_GUEST_USB")) {
            const char *file=revoke?"Files: revoked":NULL;
            CHECK(guest_console_marker(app,id,"usboff","NEXAUI_USB_OFF"));
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,file,network,microphone)&&
                observe(app,"Devices: read-only USB off",0)&&focus(app,"Close sharing")&&drive("key","Tab")&&
                observe(app,"Attach test disk",1)&&drive("key","Return")&&observe(app,"Devices: USB attach pending",0));
            CHECK(focus(app,"Check device status")&&drive("key","Return")&&observe(app,"Devices: USB disk read-only",0)&&
                input_chrome(display,id,"guest-usb-active")&&drive("key","Escape")&&observe(app,"Sharing",1)&&
                guest_console_marker(app,id,"usbopen","NEXAUI_USB_ACTIVE_RO"));
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&observe(app,"Devices: USB disk read-only",0)&&
                focus(app,"Close sharing")&&drive("key","Tab")&&observe(app,"Detach test disk",1)&&drive("key","Return")&&
                observe(app,"Devices: USB detach pending",0)&&input_chrome(display,id,"guest-usb-pending")&&
                drive("key","Escape")&&observe(app,"Sharing",1)&&
                guest_console_marker(app,id,"usbcheck","NEXAUI_USB_REVOKED_HELD_AND_FRESH"));
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,file,network,microphone)&&
                observe(app,"Devices: USB disk detached",0)&&input_chrome(display,id,"guest-usb-revoked")&&
                drive("key","Escape")&&observe(app,"Sharing",1));
            puts("GUEST_USB_NATIVE verified: read-only disk and detached for held and fresh reads");
        }
        if (guest_ax) CHECK(guest_ax_flow(app,display,id));
        else CHECK(press(app,"Disconnect")&&observe(app,"Console disconnected. Guest retained.",0)&&
            browser_pixel(display,outer,156,43,0x00a800u)&&input_chrome(display,id,"guest-desktop-disconnected"));
        if (directory) {
            CHECK(focus(app,"Sharing")&&drive("key","Return")&&read_guest_sharing(app,directory,revoke?"Files: revoked":NULL,network,microphone)&&
                focus(app,"Close sharing")&&drive("key","Return")&&observe(app,"Sharing",1));
            puts("GUEST_SHARING_NATIVE verified: exact file grant and independent channel states across disconnect");
        }
        puts("GUEST_DESKTOP_NATIVE verified: fitted live Linux desktop, guest terminal typing, host escape and retained disconnect");
        if (guest_storage) {
            CHECK(focus(app,"Review reset")&&drive("key","Return")&&read_guest_reset_scope(app,&preview,directory)&&
                !strcmp(baseline.environment,preview.environment)&&!strcmp(baseline.checkpoint,preview.checkpoint)&&
                input_chrome(display,id,"guest-reset-after-save"));
            CHECK(focus(app,"Close reset preview")&&drive("key","Return")&&observe(app,"Review reset",1)&&
                browser_pixel(display,outer,156,43,0x00a800u));
            printf("GUEST_RESET_NATIVE environment=%s checkpoint=%s\n",preview.environment,preview.checkpoint);
            CHECK(focus(app,"Resources")&&drive("key","Return")&&observe(app,"Guest running",0)&&
                observe(app,"Work disk: 16 MiB, read/write",0)&&
                guest_resource_counter(app,"Device wrote ",&after)&&after>before&&
                input_chrome(display,id,"guest-resources-written"));
            printf("GUEST_STORAGE_NATIVE before=%llu after=%llu\n",before,after);
            CHECK(drive("key","Escape")&&observe(app,"Resources",1)&&browser_pixel(display,outer,156,43,0x00a800u));
            if (guest_reset) {
                struct stat before_reset={0}, after_reset={0};
                if (directory) CHECK(read_guest_export(directory,&before_reset));
                CHECK(focus(app,"Review reset")&&drive("key","Return")&&read_guest_reset_scope(app,&preview,directory)&&
                    !strcmp(baseline.environment,preview.environment)&&!strcmp(baseline.checkpoint,preview.checkpoint)&&
                    observe(app,"Stops the guest. The previous disk is kept.",0));
                CHECK(focus(app,"Restore checkpoint")&&drive("key","Return")&&
                    observe(app,"Checkpoint restored. Guest stopped.",0)&&input_chrome(display,id,"guest-reset-applied"));
                if (directory) {
                    CHECK(read_guest_export(directory,&after_reset)&&before_reset.st_dev==after_reset.st_dev&&
                        before_reset.st_ino==after_reset.st_ino);
                    puts("GUEST_SHARING_NATIVE verified: host file bytes and identity preserved by reset");
                }
                CHECK(focus(app,"Close reset preview")&&drive("key","Return")&&observe(app,"Review reset",1));
                CHECK(drive("key","Return")&&observe(app,"Checkpoint restored. Guest stopped.",0)&&
                    focus(app,"Close reset preview")&&drive("key","Return")&&observe(app,"Review reset",1));
                puts("GUEST_RESET_NATIVE verified: keyboard confirmation, status recovery and retained result");
            }
        }
        CHECK(point_at(id,274,-38)&&drive("click","1"));
        goto closed;
    } else if (guest_case) {
        CHECK(guest_video(app,0)&&browser_pixel(display,outer,224,104,0x0000ffu));
        CHECK(observe(app,"Host sharing unavailable: clipboard, files, network, microphone, devices.",0));
        const char *active="Guest input active: Ctrl+Shift+F12 exits.";
        const char *released="Guest input released.";
        CHECK(focus(app,"Enter guest")&&drive("key","Return")&&observe(app,active,0)&&observe(app,viewport,1));
        CHECK(browser_pixel(display,outer,90,33,0xffffffu));
        CHECK(point_at(id,120,55)&&host_marker(host_log,"x=120 y=105")&&point_at(id,150,75));
        CHECK(browser_pixel(display,outer,90,33,0x0000ffu)&&browser_pixel(display,outer,150,72,0xffffffu)&&
            input_chrome(display,id,"guest-pointer-moved"));
        const char *mouse_buttons[]={"3","2"};
        for (size_t i=0;i<sizeof(mouse_buttons)/sizeof(*mouse_buttons);++i) {
            CHECK(drive("mousedown",mouse_buttons[i])&&browser_pixel(display,outer,150,72,0xffff00u));
            CHECK(drive("mouseup",mouse_buttons[i])&&browser_pixel(display,outer,150,72,0xffffffu));
        }
        CHECK(drive("mousedown","1")&&browser_pixel(display,outer,150,72,0xffff00u)&&
            input_chrome(display,id,"guest-button-down"));
        CHECK(drive("keydown","a")&&browser_pixel(display,outer,224,104,0x00ff00u)&&input_chrome(display,id,"guest-key-down"));
        CHECK(drive("keydown","ctrl")&&drive("keydown","shift")&&drive("key","F12")&&
            observe(app,released,0)&&observe(app,"Close",1));
        CHECK(browser_pixel(display,outer,224,104,0xff00ffu)&&browser_pixel(display,outer,150,72,0xffffffu)&&
            input_chrome(display,id,"guest-key-released"));
        CHECK(drive("mouseup","1")&&drive("keyup","a")&&drive("keyup","shift")&&drive("keyup","ctrl")&&
            drive("key","a")&&observe(app,"Close",1));
        CHECK(focus(app,"Enter guest")&&drive("key","Return")&&observe(app,active,0));
        CHECK(drive("mousedown","1")&&browser_pixel(display,outer,150,72,0xffff00u));
        CHECK(point_at(id,164,82)&&browser_pixel(display,outer,179,87,0xffff00u)&&
            browser_pixel(display,outer,150,72,0xff00ffu)&&input_chrome(display,id,"guest-button-drag"));
        CHECK(point_at(id,150,75)&&browser_pixel(display,outer,150,72,0xffff00u));
        CHECK(drive("keydown","a")&&browser_pixel(display,outer,224,104,0x00ff00u));
        char desktop_id[32];
        snprintf(desktop_id,sizeof(desktop_id),"%lu",(unsigned long)DefaultRootWindow(display));
        CHECK(drive("windowfocus",desktop_id)&&observe(app,"Guest input focus lost.",0)&&
            browser_pixel(display,outer,224,104,0xff00ffu)&&browser_pixel(display,outer,150,72,0xffffffu)&&
            input_chrome(display,id,"guest-focus-release"));
        CHECK(drive("mouseup","1")&&drive("keyup","a")&&drive("windowfocus",id)&&
            point_at(id,420,-119)&&drive("click","1"));
        CHECK(focus(app,"Enter guest")&&drive("key","Return")&&observe(app,active,0));
        CHECK(point_at(id,150,75)&&drive("mousedown","1")&&browser_pixel(display,outer,150,72,0xffff00u));
        CHECK(point_at(id,30,75)&&observe(app,released,0)&&browser_pixel(display,outer,150,72,0xffffffu)&&
            input_chrome(display,id,"guest-letterbox-release")&&drive("mouseup","1"));
        CHECK(focus(app,"Enter guest")&&drive("key","Return")&&observe(app,active,0));
        CHECK(point_at(id,150,75)&&drive("mousedown","1")&&browser_pixel(display,outer,150,72,0xffff00u));
        CHECK(press(app,"Disconnect")&&observe(app,"Console disconnected. Guest retained.",0)&&
            browser_pixel(display,outer,150,72,0xffffffu)&&input_chrome(display,id,"guest-disconnect-release")&&
            drive("mouseup","1"));
        puts("GUEST_NATIVE verified: VGA pixels, pause/resume, keyboard, three mouse buttons, host escape, focus loss, letterbox exit and disconnect");
        /* The X carrier alone does not restore native Wayland keyboard focus. */
        CHECK(point_at(id,274,-38)&&drive("click","1"));
        goto closed;
    } else CHECK(capture(display,outer,pdf_case?blue:0xbf3f89u,pdf_case?"pdf-page2-scaled":"scaled"));
    if (browser_case) puts("BROWSER_NATIVE verified: retired tab, engine failure, host recovery, 2x backing and native controls");
    if (browser_input) {
        CHECK(focus(app,"Enter page")&&drive("key","Return")&&
            observe(app,"Page input active: Ctrl+Shift+F12 exits.",0)&&observe(app,viewport,1));
        CHECK(drive("key","a")&&host_marker(host_log,"BROWSER_IME kind=7 revision=2"));
        CHECK(drive("key","108")&&drive("keydown","Shift_L")&&drive("key","apostrophe")&&drive("keyup","Shift_L"));
        CHECK(host_marker(host_log,"BROWSER_IME kind=6 revision=2"));
        CHECK(input_chrome(display,id,"browser-preedit")&&drive("key","u")&&
            host_marker(host_log,"BROWSER_IME kind=7 revision=3"));
        CHECK(input_chrome(display,id,"browser-commit")&&drive("keydown","ctrl")&&drive("keydown","shift")&&drive("key","F12"));
        CHECK(observe(app,"Page input released.",0)&&observe(app,"Close",1));
        CHECK(drive("keyup","shift")&&drive("keyup","ctrl"));
        CHECK(focus(app,"Enter page")&&drive("key","Return")&&
            observe(app,"Page input active: Ctrl+Shift+F12 exits.",0)&&observe(app,viewport,1));
        CHECK(drive("key","108")&&drive("keydown","Shift_L")&&drive("key","apostrophe")&&drive("keyup","Shift_L"));
        CHECK(host_marker(host_log,"BROWSER_IME kind=6 revision=4")&&press(app,"Release page")&&
            observe(app,"Page input released.",0)&&observe(app,"Close",1));
        CHECK(drive("key","b")&&observe(app,"Close",1));
        puts("BROWSER_INPUT verified: native keys, preedit, commit, candidate geometry and host escape");
    }
    if (browser_pointer) {
        CHECK(focus(app,"Enter page")&&drive("key","Return")&&
            observe(app,"Page input active: Ctrl+Shift+F12 exits.",0)&&observe(app,viewport,1));
        CHECK(browser_pixel(display,outer,344,42,0xeeeeeeu)&&browser_pixel(display,outer,382,110,0x3667d1u));
        CHECK(point_at(id,365,48)&&drive("click","1")&&browser_pixel(display,outer,344,42,0x21bb71u));
        CHECK(point_at(id,345,110)&&drive("click","5")&&browser_pixel(display,outer,382,110,0xffaa00u));
        CHECK(input_chrome(display,id,"browser-pointer-changed"));
        CHECK(point_at(id,449,112)&&host_marker(host_log,"BROWSER_POINTER kind=13 adopted")&&drive("click","5"));
        CHECK(point_at(id,365,48)&&drive("mousedown","1")&&host_markers(host_log,"BROWSER_POINTER kind=10 adopted",2));
        CHECK(press(app,"Release page")&&observe(app,"Page input released.",0)&&observe(app,"Close",1));
        CHECK(point_at(id,274,-38)&&drive("mouseup","1")&&observe(app,"Close",1));
        CHECK(point_at(id,345,110)&&drive("click","5")&&browser_pixel(display,outer,382,110,0xffaa00u));
        CHECK(focus(app,"Enter page")&&drive("key","Return")&&observe(app,"Page input active: Ctrl+Shift+F12 exits.",0));
        CHECK(point_at(id,365,48)&&drive("click","1")&&browser_pixel(display,outer,344,42,0xeeeeeeu));
        CHECK(press(app,"Release page")&&observe(app,"Page input released.",0)&&observe(app,"Close",1));
        puts("BROWSER_POINTER verified: native click, wheel pixels, viewport exit, held-button release and fresh click");
    }
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
        char *text=malloc(CONTENT_HOST_LOG_LIMIT);
        if(!text)result=1;
        else{ssize_t count=pread(fileno(host_log),text,CONTENT_HOST_LOG_LIMIT,0);
            if(count<0||count==CONTENT_HOST_LOG_LIMIT)result=1;else if(fwrite(text,1,(size_t)count,stdout)!=(size_t)count)result=1;free(text);}
        fclose(host_log);
    }
    return result;
}
