#include "fixture.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"removal-allocation:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void __real_free(void *);
static size_t ordinal,fail_at,live;
static int fail(void){return fail_at && ++ordinal==fail_at;}
void *__wrap_malloc(size_t bytes){if(fail())return NULL;void *p=__real_malloc(bytes);if(p)++live;return p;}
void *__wrap_calloc(size_t count,size_t bytes){if(fail())return NULL;void *p=__real_calloc(count,bytes);if(p)++live;return p;}
void __wrap_free(void *p){if(p){CHECK(live>0);--live;}__real_free(p);}
int main(void){
    removal_entry entries[33],original[8];removal_plan plan=removal_fixture(entries);memcpy(original,entries,sizeof(original));
    removal_owner *owner=NULL;
    unsigned rejected=0;
    for(size_t i=1;i<=7;++i){
        ordinal=0;fail_at=i;nx_result result=removal_owner_create(&plan,&owner);fail_at=0;
        CHECK(!memcmp(entries,original,sizeof(original)));
        if(i<=6){CHECK(result==NX_ERROR_MEMORY && !owner && !live);++rejected;}
        else {CHECK(result==NX_OK && owner);removal_owner_destroy(owner);owner=NULL;CHECK(!live);}
    }
    for(size_t i=8;i<33;++i){entries[i]=entries[0];entries[i].semantic_id=1000+i;entries[i].object[15]=(unsigned char)(10+i);}
    plan.count=33;CHECK(removal_owner_create(&plan,&owner)==NX_OK);
    nx_font_collection *fonts=NULL;CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK);size_t baseline=live;
    int succeeded=0;
    for(size_t i=1;i<128;++i){
        removal_review *view=NULL;ordinal=0;fail_at=i;
        nx_result result=removal_review_create(owner,fonts,&view);fail_at=0;
        if(result==NX_OK){CHECK(view);removal_review_destroy(view);CHECK(live==baseline);succeeded=1;break;}
        CHECK(result==NX_ERROR_MEMORY && !view && live==baseline && !removal_owner_requests(owner));++rejected;
    }
    CHECK(succeeded);removal_review *filtered=NULL;CHECK(removal_review_create(owner,fonts,&filtered)==NX_OK);
    size_t filter_baseline=live;succeeded=0;
    for(size_t i=1;i<128;++i){
        ordinal=0;fail_at=i;nx_result result=removal_review_filter(filtered,(nx_utf8_span){"Application",11});fail_at=0;
        if(result==NX_OK){succeeded=1;break;}
        CHECK(result==NX_ERROR_MEMORY && live==filter_baseline && !removal_owner_requests(owner));++rejected;
        /* Empty was the previous applied filter and must still be a no-op. */
        ordinal=0;fail_at=1;result=removal_review_filter(filtered,(nx_utf8_span){0});fail_at=0;
        CHECK(result==NX_OK && !ordinal && live==filter_baseline);
    }
    CHECK(succeeded);removal_review_destroy(filtered);CHECK(live==baseline);
    removal_review *old_view=NULL,*other=NULL;
    CHECK(removal_review_create(owner,fonts,&old_view)==NX_OK && removal_review_create(owner,fonts,&other)==NX_OK);
    size_t old_live=live;plan.revision=43;succeeded=0;
    for(size_t i=1;i<128;++i){
        ordinal=0;fail_at=i;nx_result result=removal_owner_refresh(owner,&plan);fail_at=0;
        if(result==NX_OK){succeeded=1;break;}
        CHECK(result==NX_ERROR_MEMORY && removal_owner_revision(owner)==42 && removal_review_revision(old_view)==42 && live==old_live);++rejected;
    }
    CHECK(succeeded && removal_owner_revision(owner)==43 && removal_review_revision(other)==42);
    size_t staged_live=live;nx_context *original_context=removal_review_context(old_view);succeeded=0;
    for(size_t i=1;i<128;++i){
        ordinal=0;fail_at=i;nx_result result=removal_review_refresh(old_view);fail_at=0;
        if(result==NX_OK){succeeded=1;break;}
        CHECK(result==NX_ERROR_MEMORY && removal_review_revision(old_view)==42 && removal_review_context(old_view)==original_context && live==staged_live);++rejected;
    }
    CHECK(succeeded && removal_review_revision(old_view)==43 && removal_review_revision(other)==42);
    ordinal=0;fail_at=1;nx_result unchanged=removal_review_refresh(old_view);fail_at=0;CHECK(unchanged==NX_OK && !ordinal);
    removal_review_destroy(old_view);removal_review_destroy(other);
    nx_font_collection_destroy(fonts);removal_owner_destroy(owner);CHECK(!live);
    printf("Rejected %u allocation failures; caller data, owner state and live allocation balance preserved\n",rejected);return 0;
}
