#include "fixture.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"removal-review:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static nx_semantic_view node(removal_review *view,uint64_t id){
    uint32_t index;nx_semantic_view result;nx_context *ctx=removal_review_context(view);
    CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&result)==NX_OK);return result;
}
static int equals(nx_utf8_span value,const char *text){return value.length==strlen(text) && !memcmp(value.data,text,value.length);}
static void key(removal_review *view,nx_key code){
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=code;
    CHECK(nx_context_queue_event(removal_review_context(view),&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(removal_review_context(view),&event)==NX_OK);
}
static removal_action frame(removal_review *view,float width,float height){
    removal_action action;CHECK(removal_review_frame(view,width,height,&action)==NX_OK);return action;
}
static void capture(removal_review *view,unsigned width,unsigned height,const char *directory,const char *name){
    unsigned char *pixels=malloc((size_t)width*height*4);CHECK(pixels);
    const nx_command *commands;size_t count;nx_color background;
    CHECK(nx_theme_color(removal_review_context(view),NX_COLOR_BACKGROUND,&background)==NX_OK &&
        nx_frame_commands(removal_review_context(view),&commands,&count)==NX_OK &&
        nx_headless_render(commands,count,pixels,width,height,width*4,background)==NX_OK);
    char path[4096];int length=snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    CHECK(length>0 && (size_t)length<sizeof(path));FILE *file=fopen(path,"wb");CHECK(file);
    CHECK(fprintf(file,"P6\n%u %u\n255\n",width,height)>0);
    for(size_t i=0;i<(size_t)width*height;++i)CHECK(fwrite(pixels+4*i,1,3,file)==3);
    CHECK(!fclose(file));free(pixels);
}
static void reject(const removal_plan *plan){
    removal_owner *owner=NULL;CHECK(removal_owner_create(plan,&owner)==NX_ERROR_ARGUMENT && !owner);
}
int main(int argc,char **argv){
    CHECK(argc==3);FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long size=ftell(file);
    CHECK(size>0 && size<64000000 && !fseek(file,0,SEEK_SET));unsigned char *bytes=malloc((size_t)size);CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)size,file)==(size_t)size && !fclose(file));nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,(size_t)size,NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK &&
        nx_font_collection_add(fonts,font)==NX_OK);
    removal_entry entries[32],bad_entries[8];removal_plan plan=removal_fixture(entries),bad=removal_fixture(bad_entries);
    bad.count=REMOVAL_MAX_ENTRIES+1;reject(&bad);
    bad=removal_fixture(bad_entries);bad.entries[1].semantic_id=bad.entries[0].semantic_id;reject(&bad);
    bad=removal_fixture(bad_entries);memcpy(bad.entries[1].object,bad.entries[0].object,16);reject(&bad);
    bad=removal_fixture(bad_entries);bad.entries[0].label[0]=(char)0xff;reject(&bad);
    bad=removal_fixture(bad_entries);memset(bad.entries[0].reason,'X',sizeof(bad.entries[0].reason));reject(&bad);
    bad=removal_fixture(bad_entries);bad.entries[0].disposition=(removal_disposition)6;reject(&bad);
    bad=removal_fixture(bad_entries);bad.revision=0;reject(&bad);
    bad=removal_fixture(bad_entries);memset(bad.scope,0,16);reject(&bad);
    removal_plan expected_plan=plan;removal_owner *owner=NULL;removal_review *view=NULL;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    strcpy(plan.entries[0].label,"Changed caller draft");plan.revision=99;
    CHECK(frame(view,640,480).kind==REMOVAL_NONE && equals(node(view,1000).name,"Application code"));
    CHECK(equals(node(view,31).value,"00000000000000000000000000000001 / revision 42"));
    for(unsigned i=0;i<6;++i)CHECK(node(view,100+i).state&NX_SEM_READONLY);
    CHECK(node(view,202).state&NX_SEM_DISABLED);
    CHECK(nx_semantics_request_action(removal_review_context(view),202,NX_ACTION_PRESS)!=NX_OK);
    CHECK(nx_focus_set(removal_review_context(view),200)==NX_OK);key(view,NX_KEY_SPACE);
    CHECK(frame(view,640,480).kind==REMOVAL_NONE && !(node(view,202).state&NX_SEM_DISABLED));
    capture(view,640,480,argv[2],"reviewed-dark");
    key(view,NX_KEY_TAB);CHECK(frame(view,640,480).kind==REMOVAL_NONE && nx_focus_id(removal_review_context(view))==201);
    key(view,NX_KEY_TAB);CHECK(frame(view,640,480).kind==REMOVAL_NONE && nx_focus_id(removal_review_context(view))==202);
    key(view,NX_KEY_ENTER);removal_action action=frame(view,640,480);
    CHECK(action.kind==REMOVAL_REQUEST && action.request.revision==42 && action.request.plan[15]==1 &&
        action.request.installation[15]==2 && action.request.scope[15]==3 && removal_owner_requests(owner)==1);
    CHECK(!memcmp(action.request.plan,expected_plan.plan,16) && !memcmp(action.request.installation,expected_plan.installation,16) &&
        !memcmp(action.request.scope,expected_plan.scope,16));
    CHECK(frame(view,640,480).kind==REMOVAL_NONE && node(view,202).state&NX_SEM_DISABLED);
    CHECK(equals(node(view,10).name,"Request handed to owner; outcome unknown"));
    removal_review_destroy(view);view=NULL;
    CHECK(removal_review_create(owner,fonts,&view)==NX_OK && frame(view,640,480).kind==REMOVAL_NONE);
    CHECK(node(view,202).state&NX_SEM_DISABLED && removal_owner_requests(owner)==1);
    CHECK(nx_semantics_request_action(removal_review_context(view),202,NX_ACTION_PRESS)!=NX_OK);
    removal_review_destroy(view);removal_owner_destroy(owner);view=NULL;owner=NULL;
    plan=removal_fixture(entries);CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    CHECK(frame(view,640,480).kind==REMOVAL_NONE);
    CHECK(nx_semantics_request_action(removal_review_context(view),200,NX_ACTION_PRESS)==NX_OK);
    CHECK(frame(view,640,480).kind==REMOVAL_NONE);
    CHECK(nx_semantics_request_action(removal_review_context(view),202,NX_ACTION_PRESS)==NX_OK);
    removal_owner_invalidate(owner);CHECK(frame(view,640,480).kind==REMOVAL_NONE && !removal_owner_requests(owner));
    CHECK(equals(node(view,10).name,"Plan no longer current; request unavailable"));
    removal_review_destroy(view);removal_owner_destroy(owner);view=NULL;owner=NULL;
    plan=removal_fixture(entries);plan.entries[plan.count]=plan.entries[7];plan.entries[plan.count].semantic_id=1100;
    plan.entries[plan.count].object[15]=99;plan.entries[plan.count++].disposition=REMOVAL_UNKNOWN;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    for(unsigned theme_index=0;theme_index<2;++theme_index)for(unsigned large=0;large<2;++large){
        nx_theme theme;CHECK(nx_theme_preset(theme_index?NX_THEME_LIGHT:NX_THEME_DARK,&theme)==NX_OK);
        if(large)CHECK(nx_theme_set_metric(&theme,NX_METRIC_FONT_SIZE,2*theme.metrics[NX_METRIC_FONT_SIZE])==NX_OK);
        CHECK(nx_context_set_theme(removal_review_context(view),&theme)==NX_OK);
        for(unsigned narrow=0;narrow<2;++narrow){
            unsigned width=narrow?420:640,height=narrow?320:480;
            CHECK(frame(view,(float)width,(float)height).kind==REMOVAL_NONE);
            CHECK(node(view,202).state&NX_SEM_DISABLED && equals(node(view,10).name,"Inventory incomplete; request unavailable"));
            CHECK(node(view,200).state&NX_SEM_DISABLED && nx_semantics_request_action(removal_review_context(view),200,NX_ACTION_PRESS)!=NX_OK);
            for(uint64_t id=200;id<=202;++id){nx_rect bounds=node(view,id).bounds;
                CHECK(bounds.x>=0 && bounds.y>=0 && bounds.x+bounds.width<=width && bounds.y+bounds.height<=height);}
            char name[80];snprintf(name,sizeof(name),"incomplete-%s-%s-%s",theme_index?"light":"dark",large?"large":"normal",narrow?"narrow":"ordinary");
            capture(view,width,height,argv[2],name);
        }
    }
    CHECK(nx_focus_set(removal_review_context(view),21)==NX_OK);float before=node(view,1100).bounds.y;
    key(view,NX_KEY_END);CHECK(frame(view,420,320).kind==REMOVAL_NONE && node(view,1100).bounds.y<before);
    capture(view,420,320,argv[2],"incomplete-light-large-end");
    CHECK(nx_semantics_request_action(removal_review_context(view),201,NX_ACTION_PRESS)==NX_OK);
    CHECK(frame(view,420,320).kind==REMOVAL_CLOSE && !removal_owner_requests(owner));
    removal_review_destroy(view);removal_owner_destroy(owner);
    for(unsigned empty=0;empty<2;++empty){
        plan=removal_fixture(entries);if(empty)plan.count=0;else plan.inventory_complete=0;
        CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
        CHECK(frame(view,640,480).kind==REMOVAL_NONE && node(view,202).state&NX_SEM_DISABLED);
        CHECK(equals(node(view,10).name,empty?"No removals listed; request unavailable":"Inventory incomplete; request unavailable"));
        removal_review_destroy(view);removal_owner_destroy(owner);
    }
    plan=removal_fixture(entries);
    memset(plan.entries[0].label,'X',sizeof(plan.entries[0].label)-1);plan.entries[0].label[159]=0;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    CHECK(frame(view,420,320).kind==REMOVAL_NONE && equals(node(view,1000).name,plan.entries[0].label));
    CHECK(nx_focus_set(removal_review_context(view),22)==NX_OK);float before_x=node(view,1000).bounds.x;
    key(view,NX_KEY_END);CHECK(frame(view,420,320).kind==REMOVAL_NONE && node(view,1000).bounds.x<before_x);
    capture(view,420,320,argv[2],"long-name-horizontal-end");
    removal_review_destroy(view);removal_owner_destroy(owner);
    plan=removal_fixture(entries);
    for(size_t i=plan.count;i<32;++i){
        plan.entries[i]=plan.entries[0];plan.entries[i].semantic_id=1000+i;
        plan.entries[i].object[15]=(unsigned char)(10+i);
        snprintf(plan.entries[i].label,sizeof(plan.entries[i].label),"Additional managed object %zu",i);
    }
    plan.count=32;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    CHECK(frame(view,420,320).kind==REMOVAL_NONE);
    for(size_t i=0;i<plan.count;++i)CHECK(equals(node(view,plan.entries[i].semantic_id).name,plan.entries[i].label));
    removal_action rejected={.kind=REMOVAL_REQUEST};
    CHECK(removal_review_frame(view,419,320,&rejected)==NX_ERROR_LIMIT && rejected.kind==REMOVAL_NONE && !removal_owner_requests(owner));
    removal_review_destroy(view);removal_owner_destroy(owner);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("Exact copied plan, six dispositions, keyboard, stale request, incomplete inventory and reopen without replay passed");return 0;
}
