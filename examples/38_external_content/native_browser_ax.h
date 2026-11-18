#ifndef CONTENT_NATIVE_BROWSER_AX_H
#define CONTENT_NATIVE_BROWSER_AX_H

static uint64_t browser_ax_id(uint32_t epoch,uint32_t node) {
    return (UINT64_C(1)<<56)|((uint64_t)epoch<<32)|node;
}

static nx_result browser_ax_semantics(nx_context *context,content_surface *surface,
    const nx_viewport_desc *viewport,int enable_actions,content_ax_action *action) {
    const content_ax_node *nodes;
    size_t count;
    uint32_t epoch;
    nx_result result=content_surface_get_ax(surface,&nodes,&count,&epoch);
    if (result!=NX_OK || !count) return result;
    content_surface_status status;
    result=content_surface_get_status(surface,&status);
    if (result!=NX_OK) return result;
    nx_rect view=viewport->bounds;
    float sx=view.width/(float)status.request.width, sy=view.height/(float)status.request.height;
    static const nx_semantic_role roles[]={0,NX_ROLE_GROUP,NX_ROLE_TEXT,NX_ROLE_IMAGE,NX_ROLE_BUTTON};
    for (size_t i=0;i<count;++i) {
        const content_ax_node *node=&nodes[i];
        nx_rect bounds={view.x+node->x*sx,view.y+node->y*sy,node->width*sx,node->height*sy};
        nx_semantic_desc desc={
            .id=browser_ax_id(epoch,node->id),
            .parent_id=node->parent?browser_ax_id(epoch,node->parent):viewport->id,
            .role=roles[node->role],.bounds=bounds,.name={node->name,node->name_bytes},
            .state=(node->state&CONTENT_AX_DISABLED)?NX_SEM_DISABLED:0};
        if (node->role==CONTENT_AX_TEXT) {
            desc.state|=NX_SEM_READONLY;
            desc.value=desc.name;
            result=nx_semantics_add_readonly_text_snapshot_without_caret(context,&desc,desc.id,NULL);
        } else if (node->role==CONTENT_AX_BUTTON && enable_actions && !desc.state) {
            desc.actions=NX_ACTION_FOCUS|NX_ACTION_PRESS;
            nx_widget_desc widget={.semantic=desc,.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
            nx_interaction_state state;
            result=nx_widget_register(context,&widget,&state);
            if (result==NX_OK && state.clicked) *action=(content_ax_action){epoch,node->id};
            if (result==NX_OK && state.focused)
                result=nx_stroke_rect(context,bounds,2,(nx_color){0,0,0,1});
        } else result=nx_semantics_add(context,&desc,NULL);
        if (result!=NX_OK) return result;
    }
    return NX_OK;
}

#endif
