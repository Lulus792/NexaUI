#ifndef EXAMPLE_CONTENT_BROWSER_AX_H
#define EXAMPLE_CONTENT_BROWSER_AX_H

#include <nexaui/nexaui.h>

#define CONTENT_AX_NODE_LIMIT 32u
#define CONTENT_AX_NAME_LIMIT 256u
enum content_ax_role { CONTENT_AX_GROUP=1, CONTENT_AX_TEXT, CONTENT_AX_IMAGE, CONTENT_AX_BUTTON };
#define CONTENT_AX_DISABLED 1u

/* Parent-first document records in physical frame pixels. */
typedef struct content_ax_node {
    uint32_t id,parent;
    uint16_t x,y,width,height;
    uint16_t role,state,name_bytes;
    char name[CONTENT_AX_NAME_LIMIT];
} content_ax_node;
typedef struct content_ax_action { uint32_t epoch,node; } content_ax_action;

#endif
