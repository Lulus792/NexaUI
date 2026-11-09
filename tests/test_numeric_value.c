#include "nexaui/nexaui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x);return 1;} } while(0)

static nx_value_control_desc descriptor(void) {
    nx_value_control_desc d={0};d.control.id=1;d.control.parent_id=9;
    d.control.bounds=(nx_rect){0,0,80,20};d.control.name=(nx_utf8_span){"S",1};
    d.maximum=100;d.step=1;d.units_per_pixel=1;return d;
}
static int frame(nx_context *ctx,nx_value_control_desc d,double *value,int mode,int disabled) {
    CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK);
    if(mode==4) {
        nx_overlay_desc overlay={0};overlay.panel.id=d.control.parent_id;overlay.backdrop_id=80;
        overlay.panel.name=(nx_utf8_span){"Dialog",6};overlay.panel.bounds=(nx_rect){0,0,100,40};
        int dismissed=0;CHECK(nx_modal_begin(ctx,&overlay,&dismissed)==NX_OK && !dismissed);
    } else {
        nx_semantic_desc p={0};p.id=d.control.parent_id;p.role=NX_ROLE_GROUP;
        p.bounds=(nx_rect){0,0,100,40};p.state=disabled?NX_SEM_DISABLED:0;
        CHECK(nx_semantics_add(ctx,&p,NULL)==NX_OK);
    }
    nx_interaction_state state;
    if(mode==2) {
        nx_widget_desc w={0};w.semantic.id=d.control.id;w.semantic.parent_id=d.control.parent_id;
        w.semantic.role=NX_ROLE_SLIDER;w.semantic.name=d.control.name;w.semantic.bounds=d.control.bounds;
        w.semantic.actions=NX_ACTION_SET_VALUE|NX_ACTION_INCREMENT;w.flags=NX_WIDGET_INTERACTIVE;
        CHECK(nx_widget_register(ctx,&w,&state)==NX_OK);
    } else if(mode!=3) {
        CHECK((mode==1?nx_drag_value(ctx,&d,value,&state):nx_slider(ctx,&d,value,&state))==NX_OK);
        CHECK(!state.clicked);
    }
    if(mode==4)CHECK(nx_overlay_end(ctx)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int test_snapshot_and_order(void) {
    nx_context *ctx=NULL,*other=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_context_create(NULL,&other)==NX_OK);nx_value_control_desc d=descriptor();
    double value=2,other_value=2;nx_numeric_value_snapshot s={0};s.value=99;
    CHECK(nx_semantics_get_numeric_value(ctx,1,&s)==NX_ERROR_STATE && s.value==0);
    CHECK(!frame(ctx,d,&value,0,0) && !frame(other,d,&other_value,0,0));
    CHECK(nx_semantics_get_numeric_value(ctx,1,&s)==NX_OK && s.minimum==0 &&
        s.maximum==100 && s.increment==1 && s.value==2 && s.writable && s.context_identity && s.frame);
    CHECK(nx_semantics_request_numeric_value(other,1,&s,7)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,NAN)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,INFINITY)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,-1)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,101)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_action(ctx,1,NX_ACTION_INCREMENT)==NX_OK);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,7.4)==NX_OK);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,8)==NX_ERROR_LIMIT);
    CHECK(nx_semantics_request_action(ctx,1,NX_ACTION_DECREMENT)==NX_OK);
    CHECK(value==2 && !frame(ctx,d,&value,0,0) && value==6);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,10)==NX_ERROR_STATE);
    CHECK(!frame(ctx,d,&value,0,0) && value==6);
    nx_context_destroy(other);nx_context_destroy(ctx);return 0;
}
static int test_stale_submissions(void) {
    for(int test=0;test<10;++test) {
        nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
        nx_value_control_desc d=descriptor();double value=5;nx_numeric_value_snapshot s;
        int first_mode=test==8?4:0;
        CHECK(!frame(ctx,d,&value,first_mode,0));
        CHECK(nx_semantics_get_numeric_value(ctx,1,&s)==NX_OK);
        CHECK(nx_semantics_request_numeric_value(ctx,1,&s,9)==NX_OK);
        int mode=0,disabled=0;
        switch(test) {
        case 0:d.minimum=-1;break;
        case 1:d.maximum=99;break;
        case 2:d.step=2;break;
        case 3:value=4;break;
        case 4:d.control.disabled=1;break;
        case 5:disabled=1;break;
        case 6:d.control.parent_id=8;break;
        case 7:mode=1;break;
        case 8:break; /* Same parent ID, different overlay scope. */
        case 9:mode=2;break;
        }
        CHECK(!frame(ctx,d,&value,mode,disabled) && value!=9);
        if(mode==2)CHECK(nx_semantics_get_numeric_value(ctx,1,&s)==NX_ERROR_UNSUPPORTED && !s.context_identity);
        else {
            CHECK(nx_semantics_get_numeric_value(ctx,1,&s)==NX_OK);
            if(disabled || d.control.disabled)CHECK(!s.writable && nx_semantics_request_numeric_value(ctx,1,&s,9)==NX_ERROR_STATE);
        }
        d=descriptor();value=5;
        CHECK(!frame(ctx,d,&value,0,0) && value==5);nx_context_destroy(ctx);
    }
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    nx_value_control_desc d=descriptor();double value=5;nx_numeric_value_snapshot s;
    CHECK(!frame(ctx,d,&value,1,0) && nx_semantics_get_numeric_value(ctx,1,&s)==NX_OK);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&s,9)==NX_OK);
    CHECK(!frame(ctx,d,&value,1,0) && value==9);
    CHECK(nx_semantics_get_numeric_value(ctx,1,&s)==NX_OK && nx_semantics_request_numeric_value(ctx,1,&s,10)==NX_OK);
    CHECK(!frame(ctx,d,&value,3,0) && !frame(ctx,d,&value,1,0) && value==9);
    nx_context_destroy(ctx);return 0;
}
static int test_shared_capacity_and_input_order(void) {
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    nx_value_control_desc d=descriptor();d.control.parent_id=0;double values[65];
    nx_numeric_value_snapshot snapshots[65];nx_interaction_state state;
    CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK);
    for(unsigned i=0;i<65;++i){values[i]=5;d.control.id=i+1u;CHECK(nx_slider(ctx,&d,&values[i],&state)==NX_OK);}
    CHECK(nx_end_frame(ctx)==NX_OK);
    for(unsigned i=0;i<65;++i)CHECK(nx_semantics_get_numeric_value(ctx,i+1u,&snapshots[i])==NX_OK);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&snapshots[0],10)==NX_OK);
    for(unsigned i=1;i<32;++i)CHECK(nx_semantics_request_numeric_value(ctx,i+1u,&snapshots[i],10)==NX_OK);
    for(unsigned i=32;i<64;++i)CHECK(nx_semantics_request_action(ctx,i+1u,NX_ACTION_INCREMENT)==NX_OK);
    CHECK(nx_semantics_request_numeric_value(ctx,65,&snapshots[64],10)==NX_ERROR_LIMIT);
    CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK);
    for(unsigned i=0;i<65;++i){d.control.id=i+1u;CHECK(nx_slider(ctx,&d,&values[i],&state)==NX_OK && values[i]==(i<32?10:i<64?6:5));}
    CHECK(nx_end_frame(ctx)==NX_OK);
    CHECK(nx_semantics_get_numeric_value(ctx,1,&snapshots[0])==NX_OK);
    CHECK(nx_semantics_request_numeric_value(ctx,1,&snapshots[0],7)==NX_OK);
    CHECK(nx_focus_set(ctx,1)==NX_OK);
    nx_event key={0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_HOME;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);
    CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK);d.control.id=1;
    CHECK(nx_slider(ctx,&d,&values[0],&state)==NX_OK && values[0]==7 && !state.clicked && state.focused);
    CHECK(nx_end_frame(ctx)==NX_OK);nx_context_destroy(ctx);return 0;
}
int main(void) {
    if(test_snapshot_and_order() || test_stale_submissions() || test_shared_capacity_and_input_order())return 1;
    puts("numeric snapshot/order/stale/shared-capacity/input checks passed");return 0;
}
