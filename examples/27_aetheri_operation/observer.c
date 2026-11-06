#include "observer.h"
#include "core/fixture.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct operation_observer {
    struct ae_catalogue *catalogue;
    struct ae_fixture_request request;
    const uint8_t *manifest;size_t length;
    nx_font_collection *fonts;nx_context *ctx;
    struct ae_fixture_progress observed;
    char component[129];
    int queried,query_result;float scroll_x,scroll_y;
};
static nx_utf8_span span(const char *s){return (nx_utf8_span){s,strlen(s)};}
static void hex(const uint8_t *bytes,size_t length,char *out){
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<length;++i){out[2*i]=digits[bytes[i]>>4];out[2*i+1]=digits[bytes[i]&15];}
    out[2*length]=0;
}
nx_result observer_create(struct ae_catalogue *catalogue,const struct ae_fixture_request *request,
    const uint8_t *manifest,size_t length,nx_font_collection *fonts,operation_observer **out){
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    if(!catalogue || !request || !manifest || !length || length>AE_FIXTURE_MAX_BYTES || !fonts)return NX_ERROR_ARGUMENT;
    struct ae_fixture fixture;
    if(ae_fixture_verify_manifest(manifest,length,request->artifact,&fixture)!=0 || fixture.component.size>128)return NX_ERROR_ARGUMENT;
    operation_observer *view=calloc(1,sizeof(*view));if(!view)return NX_ERROR_MEMORY;
    nx_result result=nx_context_create(NULL,&view->ctx);
    if(result!=NX_OK){free(view);return result;}
    view->catalogue=catalogue;view->request=*request;view->manifest=manifest;
    view->length=length;view->fonts=fonts;view->query_result=-EAGAIN;
    memcpy(view->component,fixture.component.data,fixture.component.size);
    *out=view;return NX_OK;
}
void observer_destroy(operation_observer *view){if(view){nx_context_destroy(view->ctx);free(view);}}
nx_context *observer_context(operation_observer *view){return view?view->ctx:NULL;}
int observer_refresh(operation_observer *view){
    if(!view)return -EINVAL;
    struct ae_fixture_progress next={0};
    view->queried=1;
    view->query_result=ae_catalogue_fixture_get(view->catalogue,&view->request,view->manifest,view->length,&next);
    if(!view->query_result)view->observed=next;
    return view->query_result;
}
static const char *status(const operation_observer *view,int *busy){
    *busy=0;
    if(!view->queried)return "Status not yet queried";
    if(view->query_result)return "Original operation unavailable; query its owner";
    const struct ae_fixture_progress *p=&view->observed;
    if(p->activation_present)return "Installation completion recorded";
    if(p->step==AE_FIXTURE_UNCERTAIN || (p->publication_present && p->publication_step==AE_FIXTURE_UNCERTAIN))
        return "Outcome uncertain; retain the original operation";
    *busy=1;
    if(p->step==AE_FIXTURE_PLANNED)return "Accepted; validation pending";
    if(p->step==AE_FIXTURE_EXECUTING)return "Staging in progress; no completion estimate";
    if(p->publication_present && p->publication_step==AE_FIXTURE_READY)return "Publication recorded; activation pending";
    return "Staging recorded; publication pending";
}
nx_result observer_frame(operation_observer *view,float width,float height,observer_action *action){
    if(!view || !action)return NX_ERROR_ARGUMENT;
    *action=OBSERVER_NONE;
    if(width<420 || height<320)return NX_ERROR_LIMIT;
    char values[9][129];
    hex(view->request.operation,16,values[0]);hex(view->request.install,16,values[1]);
    hex(view->request.scope,16,values[2]);hex(view->request.generation,16,values[3]);
    hex(view->request.slot,16,values[4]);hex(view->request.artifact,32,values[5]);
    if(view->query_result){strcpy(values[6],view->queried?"Unavailable":"Not queried");strcpy(values[7],"Not established by this observation");}
    else {
        snprintf(values[6],sizeof(values[6]),"%lld",(long long)view->observed.revision);
        strcpy(values[7],view->observed.installation_active?"Active installation observed":
            view->observed.activation_present?"Recorded installation is no longer active":"Not activated");
    }
    strcpy(values[8],view->component);
    const char *names[]={"Operation","Install","Scope","Generation","Storage slot","Artifact","Observed revision","Current installation","Component"};
    int busy=0;const char *message=status(view,&busy);
    const char *description="Local catalogue observation; not a process, grant or disk-action receipt";
    nx_text_layout *rows[9]={0},*heading=NULL,*title=NULL,*buttons[2]={0};
    float heights[9],total=0,em=0;nx_result result=nx_theme_metric(view->ctx,NX_METRIC_FONT_SIZE,&em);
    if(result==NX_OK)result=nx_text_layout_create(view->fonts,message,strlen(message),NX_BIDI_LTR,em,width-32,&heading);
    const char *title_text="Test catalogue operation";
    if(result==NX_OK)result=nx_text_layout_create(view->fonts,title_text,strlen(title_text),NX_BIDI_LTR,em,width-32,&title);
    for(unsigned i=0;i<9 && result==NX_OK;++i){
        char visual[256];
        /* A digest is shown in fixed eight-byte groups so even narrow views can
           wrap without eliding identity. The semantic value stays exact hex. */
        if(i<=5){size_t used=(size_t)snprintf(visual,sizeof(visual),"%s:",names[i]);
            unsigned groups=i==5?4:2;
            for(unsigned group=0;group<groups;++group){visual[used++]=' ';memcpy(visual+used,values[i]+16*group,16);used+=16;}visual[used]=0;}
        else snprintf(visual,sizeof(visual),"%s: %.128s",names[i],values[i]);
        result=nx_text_layout_create(view->fonts,visual,strlen(visual),NX_BIDI_LTR,em,width-56,&rows[i]);
        if(result==NX_OK){heights[i]=(float)nx_text_layout_height(rows[i])+12;total+=heights[i];}
    }
    const char *labels[]={"Refresh status","Close view"};
    for(unsigned i=0;i<2 && result==NX_OK;++i)
        result=nx_text_layout_create(view->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,em,width-48,&buttons[i]);
    if(result!=NX_OK)goto done;
    float button_height=em+20,half=(width-44)/2;
    int stacked=nx_text_layout_width(buttons[0])+16>half || nx_text_layout_width(buttons[1])+16>half;
    float footer=(stacked?2*button_height+12:button_height)+24;
    float title_height=(float)nx_text_layout_height(title),heading_height=(float)nx_text_layout_height(heading);
    float heading_y=20+title_height,top=heading_y+heading_height+32;
    if(height-top-footer<32){result=NX_ERROR_LIMIT;goto done;}
    result=nx_begin_frame(view->ctx,width,height,0);if(result!=NX_OK)goto done;
    nx_color text={0};result=nx_theme_color(view->ctx,NX_COLOR_TEXT,&text);
    nx_semantic_desc root={1,0,NX_ROLE_WINDOW,{0,0,width,height},span("Original operation observer"),{0},{0},0,0};
    if(result==NX_OK)result=nx_semantics_add(view->ctx,&root,NULL);
    nx_semantic_desc title_node={2,1,NX_ROLE_TEXT,{16,12,width-32,title_height},span(title_text),{0},{0},NX_SEM_READONLY,0};
    if(result==NX_OK)result=nx_semantics_add(view->ctx,&title_node,NULL);
    if(result==NX_OK)result=nx_draw_text_layout(view->ctx,title,(nx_point){16,12},text,1024*1024);
    nx_rect status_bounds={16,heading_y,width-32,heading_height};
    if(result==NX_OK && busy){
        nx_activity_desc activity={10,1,{16,heading_y+heading_height+8,width-32,8},span(message),span(description),.5};
        result=nx_activity(view->ctx,&activity);
    } else if(result==NX_OK){
        nx_semantic_desc node={10,1,NX_ROLE_STATUS,status_bounds,span(message),span(description),{0},NX_SEM_READONLY,0};
        result=nx_semantics_add(view->ctx,&node,NULL);
    }
    if(result==NX_OK)result=nx_draw_text_layout(view->ctx,heading,(nx_point){16,heading_y},text,1024*1024);
    nx_scroll_area_desc area={0};area.id=20;area.parent_id=1;area.name=span("Exact operation target");
    area.viewport=(nx_rect){16,top,width-32,height-top-footer};area.content_size=(nx_size){width-56,total};
    area.thumb_y_id=21;area.axes=NX_SCROLL_Y;nx_scroll_info scroll;int opened=0;
    if(result==NX_OK){result=nx_scroll_area_begin(view->ctx,&area,&view->scroll_x,&view->scroll_y,&scroll);opened=result==NX_OK;}
    float y=top-view->scroll_y;
    for(unsigned i=0;i<9 && result==NX_OK;++i){
        nx_semantic_desc node={100+i,20,NX_ROLE_TEXT,{16,y,width-56,heights[i]},span(names[i]),{0},span(values[i]),NX_SEM_READONLY,0};
        result=nx_semantics_add_readonly_text_snapshot_without_caret(view->ctx,&node,node.id,NULL);
        if(result==NX_OK)result=nx_draw_text_layout(view->ctx,rows[i],(nx_point){16,y},text,1024*1024);
        y+=heights[i];
    }
    if(opened){nx_result end=nx_scroll_area_end(view->ctx);if(result==NX_OK)result=end;}
    for(unsigned i=0;i<2 && result==NX_OK;++i){
        nx_control_desc button={0};button.id=200+i;button.parent_id=1;
        button.bounds=stacked?(nx_rect){16,height-footer+12+(button_height+12)*(float)i,width-32,button_height}:
            (nx_rect){16+(half+12)*(float)i,height-footer+12,half,button_height};
        button.name=span(labels[i]);button.label_layout=buttons[i];button.label_origin=(nx_point){button.bounds.x+8,button.bounds.y+10};
        button.glyph_cache_bytes=1024*1024;nx_interaction_state state;
        result=nx_button(view->ctx,&button,&state);
        if(result==NX_OK && state.clicked)*action=i?OBSERVER_CLOSE:OBSERVER_REFRESH;
    }
    {nx_result end=nx_end_frame(view->ctx);if(result==NX_OK)result=end;}
done:
    if(heading)nx_text_layout_destroy(heading);
    if(title)nx_text_layout_destroy(title);
    for(unsigned i=0;i<9;++i)if(rows[i])nx_text_layout_destroy(rows[i]);
    for(unsigned i=0;i<2;++i)if(buttons[i])nx_text_layout_destroy(buttons[i]);
    return result;
}
