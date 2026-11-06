#include "fixture.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"removal-filter:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static nx_semantic_view node(removal_review *view,uint64_t id){
    uint32_t index;nx_semantic_view result;nx_context *ctx=removal_review_context(view);
    CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&result)==NX_OK);return result;
}
static int equals(nx_utf8_span value,const char *text){return value.length==strlen(text) && !memcmp(value.data,text,value.length);}
static removal_action frame(removal_review *view,float width,float height){
    removal_action action;nx_result result=removal_review_frame(view,width,height,&action);
    if(result!=NX_OK)fprintf(stderr,"frame %.0fx%.0f returned %d\n",width,height,result);
    CHECK(result==NX_OK);return action;
}
static void press(removal_review *view,uint64_t id){CHECK(nx_semantics_request_action(removal_review_context(view),id,NX_ACTION_PRESS)==NX_OK);}
static void key(removal_review *view,nx_key key_code){
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=key_code;
    CHECK(removal_review_queue_event(view,&event)==NX_OK);event.type=NX_EVENT_KEY_UP;
    CHECK(removal_review_queue_event(view,&event)==NX_OK);
}
static void capture(removal_review *view,unsigned width,unsigned height,const char *directory,const char *name){
    unsigned char *pixels=malloc((size_t)width*height*4);CHECK(pixels);const nx_command *commands;size_t count;nx_color background;
    nx_context *ctx=removal_review_context(view);CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK &&
        nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK && nx_headless_render(commands,count,pixels,width,height,width*4,background)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);CHECK(n>0 && (size_t)n<sizeof(path));
    FILE *file=fopen(path,"wb");CHECK(file && fprintf(file,"P6\n%u %u\n255\n",width,height)>0);
    for(size_t i=0;i<(size_t)width*height;++i)CHECK(fwrite(pixels+4*i,1,3,file)==3);
    CHECK(!fclose(file));free(pixels);
}
int main(int argc,char **argv){
    CHECK(argc==3);FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long length=ftell(file);
    CHECK(length>0 && length<64000000 && !fseek(file,0,SEEK_SET));unsigned char *bytes=malloc((size_t)length);CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)length,file)==(size_t)length && !fclose(file));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK &&
        nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    removal_entry seed[8];removal_plan plan=removal_fixture(seed);removal_entry *entries=removal_large_fixture(REMOVAL_MAX_ENTRIES);CHECK(entries);
    plan.entries=entries;plan.count=REMOVAL_MAX_ENTRIES;removal_owner *owner=NULL;removal_review *view=NULL;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    CHECK(frame(view,640,480).kind==REMOVAL_NONE);
    press(view,200);CHECK(frame(view,640,480).kind==REMOVAL_NONE && !(node(view,202).state&NX_SEM_DISABLED));
    CHECK(removal_review_filter(view,(nx_utf8_span){"MANAGED-OBJECT-099999",21})==NX_OK);
    frame(view,640,480);CHECK(node(view,202).state&NX_SEM_DISABLED);
    CHECK(equals(node(view,104).value,"1 of 20000 objects shown") && equals(node(view,100).value,"0 of 20000 objects shown"));
    CHECK(nx_focus_set(removal_review_context(view),21)==NX_OK);key(view,NX_KEY_END);frame(view,640,480);
    CHECK(equals(node(view,100999).name,"managed-object-099999"));
    CHECK(equals(node(view,31).value,"00000000000000000000000000000001 / revision 42"));
    char oversized[129];memset(oversized,'x',sizeof(oversized));
    CHECK(removal_review_filter(view,(nx_utf8_span){oversized,sizeof(oversized)})==NX_ERROR_LIMIT);
    CHECK(removal_review_filter(view,(nx_utf8_span){"bad\0query",9})==NX_ERROR_ARGUMENT);
    CHECK(removal_review_filter(view,(nx_utf8_span){"\xc0\xaf",2})==NX_ERROR_ARGUMENT);
    frame(view,640,480);CHECK(equals(node(view,104).value,"1 of 20000 objects shown"));
    for(unsigned theme_id=0;theme_id<2;++theme_id)for(unsigned doubled=0;doubled<2;++doubled){
        nx_theme theme;CHECK(nx_theme_preset(theme_id?NX_THEME_LIGHT:NX_THEME_DARK,&theme)==NX_OK);
        if(doubled)CHECK(nx_theme_set_metric(&theme,NX_METRIC_FONT_SIZE,2*theme.metrics[NX_METRIC_FONT_SIZE])==NX_OK);
        CHECK(nx_context_set_theme(removal_review_context(view),&theme)==NX_OK);
        for(unsigned narrow=0;narrow<2;++narrow){
            unsigned width=narrow?420:640,height=narrow?320:480;frame(view,(float)width,(float)height);
            CHECK(nx_focus_set(removal_review_context(view),250)==NX_OK);key(view,NX_KEY_ENTER);frame(view,(float)width,(float)height);
            CHECK(removal_review_filter_open(view) && nx_focus_id(removal_review_context(view))==301);
            CHECK(nx_semantics_request_action(removal_review_context(view),200,NX_ACTION_PRESS)!=NX_OK &&
                  nx_semantics_request_action(removal_review_context(view),202,NX_ACTION_PRESS)!=NX_OK);
            char name[80];snprintf(name,sizeof(name),"filter-%s-%u-%u",theme_id?"light":"dark",doubled?200:100,width);
            capture(view,width,height,argv[2],name);
            CHECK(nx_semantics_request_text_value(removal_review_context(view),301,(nx_utf8_span){"discarded",9})==NX_OK);
            frame(view,(float)width,(float)height);key(view,NX_KEY_ESCAPE);frame(view,(float)width,(float)height);
            CHECK(!removal_review_filter_open(view) && nx_focus_id(removal_review_context(view))==250);
            CHECK(equals(node(view,104).value,"1 of 20000 objects shown"));
        }
    }
    nx_theme normal;CHECK(nx_theme_preset(NX_THEME_DARK,&normal)==NX_OK && nx_context_set_theme(removal_review_context(view),&normal)==NX_OK);
    frame(view,640,480);press(view,250);frame(view,640,480);
    CHECK(nx_semantics_request_text_value(removal_review_context(view),301,(nx_utf8_span){oversized,sizeof(oversized)})==NX_OK);
    frame(view,640,480);CHECK(node(view,301).state&NX_SEM_INVALID);CHECK(node(view,302).state&NX_SEM_DISABLED);
    CHECK(nx_semantics_request_action(removal_review_context(view),302,NX_ACTION_PRESS)!=NX_OK);
    CHECK(nx_semantics_request_text_value(removal_review_context(view),301,(nx_utf8_span){"missing-object",14})==NX_OK);
    frame(view,640,480);press(view,302);frame(view,640,480);
    CHECK(!removal_review_filter_open(view) && equals(node(view,10).name,"No matching objects; request covers the entire plan"));
    CHECK(equals(node(view,100).value,"0 of 20000 objects shown"));capture(view,640,480,argv[2],"filter-empty");
    press(view,250);frame(view,640,480);
    CHECK(nx_semantics_request_text_value(removal_review_context(view),301,(nx_utf8_span){0})==NX_OK);frame(view,640,480);
    key(view,NX_KEY_ENTER);frame(view,640,480);CHECK(!removal_review_filter_open(view) && equals(node(view,100).value,"20000 objects"));
    CHECK(removal_review_filter(view,(nx_utf8_span){"managed-object-099999",21})==NX_OK);frame(view,640,480);
    press(view,200);frame(view,640,480);press(view,202);removal_action action=frame(view,640,480);
    CHECK(action.kind==REMOVAL_REQUEST && action.request.revision==42 && !memcmp(action.request.plan,plan.plan,16) &&
          !memcmp(action.request.scope,plan.scope,16) && !memcmp(action.request.installation,plan.installation,16));
    CHECK(removal_review_filter(view,(nx_utf8_span){0})==NX_OK);frame(view,640,480);CHECK(node(view,202).state&NX_SEM_DISABLED);
    removal_review_destroy(view);removal_owner_destroy(owner);
    entries[REMOVAL_MAX_ENTRIES-1].disposition=REMOVAL_UNKNOWN;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    CHECK(removal_review_filter(view,(nx_utf8_span){"managed-object-000000",21})==NX_OK);frame(view,640,480);
    CHECK(node(view,200).state&NX_SEM_DISABLED);CHECK(equals(node(view,105).value,"0 of 1 objects shown"));
    CHECK(!removal_owner_requests(owner));removal_review_destroy(view);removal_owner_destroy(owner);free(entries);
    plan=removal_fixture(seed);strcpy(seed[0].label,"Gr\xc3\xbc\xc3\x9f" "e");
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);
    CHECK(removal_review_filter(view,(nx_utf8_span){"Gr\xc3\xbc",4})==NX_OK);frame(view,420,320);
    CHECK(equals(node(view,1000).name,"Gr\xc3\xbc\xc3\x9f" "e") && equals(node(view,100).value,"1 of 4 objects shown"));
    CHECK(nx_focus_set(removal_review_context(view),250)==NX_OK);
    key(view,NX_KEY_ENTER);key(view,NX_KEY_ESCAPE);
    CHECK(frame(view,420,320).kind==REMOVAL_NONE && !removal_review_filter_open(view));
    for(unsigned i=0;i<3;++i){
        press(view,250);frame(view,420,320);CHECK(removal_review_filter_open(view));
        press(view,303);frame(view,420,320);
        CHECK(!removal_review_filter_open(view) && equals(node(view,100).value,"1 of 4 objects shown"));
    }
    key(view,NX_KEY_ESCAPE);CHECK(frame(view,420,320).kind==REMOVAL_CLOSE && !removal_owner_requests(owner));
    removal_review_destroy(view);removal_owner_destroy(owner);nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("Name filtering, full-scope requests, hidden unknowns, modal editing/cancel/limits, native admission and 200% reflow passed");return 0;
}
