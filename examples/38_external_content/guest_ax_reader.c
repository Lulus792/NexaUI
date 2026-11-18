#define _POSIX_C_SOURCE 200809L
#include "browser_ax.h"
#include <atspi/atspi.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Coordinates are local to the selected window. A presenter must establish
   that window's placement before using these bounds on a guest framebuffer. */
enum { VISIT_LIMIT=64, DEPTH_LIMIT=8, WINDOW_LIMIT=2048 };
typedef struct guest_ax_snapshot {
    content_ax_node nodes[CONTENT_AX_NODE_LIMIT];
    unsigned count, visited;
    int width, height;
} guest_ax_snapshot;

static int number(const char *text, unsigned long limit, unsigned *out) {
    char *end;
    if (!*text || strspn(text,"0123456789")!=strlen(text)) return 0;
    errno=0;
    unsigned long value=strtoul(text,&end,10);
    if (errno || *end || !value || value>limit) return 0;
    *out=(unsigned)value;
    return 1;
}

static int showing(AtspiAccessible *object, int *enabled) {
    atspi_accessible_clear_cache(object);
    AtspiStateSet *states=atspi_accessible_get_state_set(object);
    if (!states) return 0;
    int visible=!atspi_state_set_contains(states,ATSPI_STATE_DEFUNCT)&&
        atspi_state_set_contains(states,ATSPI_STATE_VISIBLE)&&atspi_state_set_contains(states,ATSPI_STATE_SHOWING);
    if (enabled) *enabled=atspi_state_set_contains(states,ATSPI_STATE_ENABLED)&&
        atspi_state_set_contains(states,ATSPI_STATE_SENSITIVE);
    g_object_unref(states);
    return visible;
}

static AtspiAccessible *find_window(unsigned pid, GError **error) {
    AtspiAccessible *desktop=atspi_get_desktop(0),*result=NULL;
    if (!desktop) return NULL;
    int count=atspi_accessible_get_child_count(desktop,error);
    for (int i=0;!*error&&i<count&&count<=VISIT_LIMIT;++i) {
        AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,error);
        if (!app) break;
        if (atspi_accessible_get_process_id(app,error)==pid && !*error) {
            int windows=atspi_accessible_get_child_count(app,error);
            for (int j=0;!*error&&j<windows&&windows<=VISIT_LIMIT;++j) {
                AtspiAccessible *window=atspi_accessible_get_child_at_index(app,j,error);
                if (!window) {
                    g_clear_object(&result);
                    break;
                }
                if (atspi_accessible_get_role(window,error)==ATSPI_ROLE_FRAME&&showing(window,NULL)) {
                    if (result) {
                        g_clear_object(&result);
                        g_object_unref(window);
                        break;
                    }
                    result=window;
                } else g_object_unref(window);
            }
            g_object_unref(app);
            break;
        }
        g_object_unref(app);
    }
    g_object_unref(desktop);
    return result;
}

static AtspiRect *local_bounds(AtspiAccessible *object, GError **error) {
    AtspiComponent *component=atspi_accessible_get_component_iface(object);
    if (!component) return NULL;
    AtspiRect *bounds=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,error);
    g_object_unref(component);
    return bounds;
}

static int name_into(AtspiAccessible *object, content_ax_node *node, GError **error) {
    char *name=atspi_accessible_get_name(object,error);
    size_t bytes=name?strnlen(name,sizeof(node->name)+1u):0;
    int valid=name&&bytes&&bytes<=sizeof(node->name)&&g_utf8_validate(name,(gssize)bytes,NULL);
    for (const char *p=name;valid&&*p;p=g_utf8_next_char(p)) {
        if (g_unichar_iscntrl(g_utf8_get_char(p))) valid=0;
    }
    if (valid) {
        node->name_bytes=(uint16_t)bytes;
        memcpy(node->name,name,bytes);
    }
    g_free(name);
    return valid&&!*error;
}

static int collect(guest_ax_snapshot *snapshot, AtspiAccessible *object, unsigned depth, GError **error) {
    if (++snapshot->visited>VISIT_LIMIT || depth>DEPTH_LIMIT) return 0;
    int enabled;
    if (!showing(object,&enabled)) return 1;
    AtspiRole role=atspi_accessible_get_role(object,error);
    if (*error) return 0;
    if (role==ATSPI_ROLE_LABEL || role==ATSPI_ROLE_STATIC || role==ATSPI_ROLE_PUSH_BUTTON) {
        AtspiRect *bounds=local_bounds(object,error);
        if (!bounds || *error) {
            g_free(bounds);
            return 0;
        }
        int inside=bounds->x>=0 && bounds->y>=0 && bounds->width>0 && bounds->height>0 &&
            bounds->width<=snapshot->width-bounds->x && bounds->height<=snapshot->height-bounds->y;
        if (inside) {
            content_ax_node node={.id=snapshot->count+1u,.parent=1,
                .x=(uint16_t)bounds->x,.y=(uint16_t)bounds->y,
                .width=(uint16_t)bounds->width,.height=(uint16_t)bounds->height,
                .role=role==ATSPI_ROLE_PUSH_BUTTON?CONTENT_AX_BUTTON:CONTENT_AX_TEXT,
                .state=enabled?0:CONTENT_AX_DISABLED};
            if (snapshot->count==CONTENT_AX_NODE_LIMIT || !name_into(object,&node,error)) {
                g_free(bounds);
                return 0;
            }
            snapshot->nodes[snapshot->count++]=node;
        }
        g_free(bounds);
        return 1;
    }
    /* Other controls and text values are outside this read-only profile. */
    if (depth && role!=ATSPI_ROLE_PANEL && role!=ATSPI_ROLE_FILLER && role!=ATSPI_ROLE_SECTION)
        return 1;
    int children=atspi_accessible_get_child_count(object,error);
    if (*error || children<0 || children>VISIT_LIMIT) return 0;
    for (int i=0;i<children;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(object,i,error);
        if (!child) return 0;
        int ok=!*error&&collect(snapshot,child,depth+1u,error);
        g_object_unref(child);
        if (!ok) return 0;
    }
    return 1;
}

static void put_u16(uint8_t *bytes, uint32_t value) {
    bytes[0]=(uint8_t)(value>>8);
    bytes[1]=(uint8_t)value;
}

static void put_u32(uint8_t *bytes, uint32_t value) {
    put_u16(bytes,value>>16);
    put_u16(bytes+2,value);
}

static int write_snapshot(const guest_ax_snapshot *snapshot, unsigned epoch) {
    uint8_t packet[4u+CONTENT_AX_NODE_LIMIT*(22u+CONTENT_AX_NAME_LIMIT)];
    put_u16(packet,epoch);
    put_u16(packet+2,snapshot->count);
    size_t end=4;
    for (unsigned i=0;i<snapshot->count;++i) {
        const content_ax_node *node=&snapshot->nodes[i];
        uint8_t *row=packet+end;
        put_u32(row,node->id);
        put_u32(row+4,node->parent);
        put_u16(row+8,node->role);
        put_u16(row+10,node->state);
        put_u16(row+12,node->x);
        put_u16(row+14,node->y);
        put_u16(row+16,node->width);
        put_u16(row+18,node->height);
        put_u16(row+20,node->name_bytes);
        memcpy(row+22,node->name,node->name_bytes);
        end+=22u+node->name_bytes;
    }
    return fwrite(packet,1,end,stdout)==end && fflush(stdout)==0;
}

int main(int argc, char **argv) {
    unsigned pid,epoch,width,height;
    if (argc!=5 || !number(argv[1],INT_MAX,&pid) || !number(argv[2],UINT16_MAX,&epoch) ||
        !number(argv[3],WINDOW_LIMIT,&width) || !number(argv[4],WINDOW_LIMIT,&height)) return 2;
    alarm(4);
    atspi_set_timeout(500,500);
    if (atspi_init()!=0) return 1;
    GError *error=NULL;
    AtspiAccessible *window=find_window(pid,&error);
    AtspiRect *bounds=window&&!error?local_bounds(window,&error):NULL;
    guest_ax_snapshot snapshot={.count=1,.width=(int)width,.height=(int)height};
    snapshot.nodes[0]=(content_ax_node){.id=1,.role=CONTENT_AX_GROUP,.width=(uint16_t)width,.height=(uint16_t)height};
    int ready=bounds&&!error&&bounds->x==0&&bounds->y==0&&bounds->width==(int)width&&bounds->height==(int)height&&
        name_into(window,&snapshot.nodes[0],&error)&&collect(&snapshot,window,0,&error)&&snapshot.count>1&&
        showing(window,NULL)&&atspi_accessible_get_process_id(window,&error)==pid&&!error;
    g_free(bounds);
    g_clear_object(&window);
    g_clear_error(&error);
    atspi_exit();
    return ready&&write_snapshot(&snapshot,epoch)?0:1;
}
