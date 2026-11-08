#include "close_request.h"
#include <stdio.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"close contract %d: %s\n",__LINE__,#e);return 1;}}while(0)
int main(void){
    nx_close_state s={0};int legacy=0;
    CHECK(!nx_close_take(&s,&legacy)&&!legacy);
    CHECK(nx_close_resolve(&s,&legacy,1,NX_WINDOW_CLOSE_CANCEL)==NX_ERROR_STATE);
    nx_close_notify(&s,&legacy);CHECK(legacy==1);
    uint64_t first=nx_close_take(&s,&legacy);CHECK(first&&!legacy&&!nx_close_take(&s,&legacy));
    CHECK(nx_close_resolve(&s,&legacy,first,(nx_window_close_action)99)==NX_ERROR_ARGUMENT);
    nx_close_notify(&s,&legacy);CHECK(legacy);
    CHECK(nx_close_resolve(&s,&legacy,first,NX_WINDOW_CLOSE_CANCEL)==NX_ERROR_STATE&&legacy);
    uint64_t second=nx_close_take(&s,&legacy);CHECK(second>first&&!legacy);
    CHECK(nx_close_resolve(&s,&legacy,first,NX_WINDOW_CLOSE_ACCEPT)==NX_ERROR_STATE);
    CHECK(nx_close_resolve(&s,&legacy,second,NX_WINDOW_CLOSE_CANCEL)==NX_OK&&!legacy);
    CHECK(nx_close_resolve(&s,&legacy,second,NX_WINDOW_CLOSE_CANCEL)==NX_ERROR_STATE);
    nx_close_notify(&s,&legacy);nx_close_notify(&s,&legacy);
    uint64_t latest=nx_close_take(&s,&legacy);CHECK(latest>second+1&&!legacy);
    CHECK(nx_close_resolve(&s,&legacy,latest,NX_WINDOW_CLOSE_ACCEPT)==NX_OK&&legacy);
    CHECK(!nx_close_take(&s,&legacy)&&legacy);
    nx_close_notify(&s,&legacy);CHECK(!nx_close_take(&s,&legacy)&&legacy);
    CHECK(nx_close_resolve(&s,&legacy,latest,NX_WINDOW_CLOSE_CANCEL)==NX_ERROR_STATE&&legacy);
    s=(nx_close_state){0};legacy=0;nx_close_notify(&s,&legacy);first=nx_close_take(&s,&legacy);
    nx_close_force(&s,&legacy);CHECK(legacy&&!nx_close_take(&s,&legacy));
    CHECK(nx_close_resolve(&s,&legacy,first,NX_WINDOW_CLOSE_CANCEL)==NX_ERROR_STATE&&legacy);
    s=(nx_close_state){0};s.sequence=UINT64_MAX-1;legacy=0;nx_close_notify(&s,&legacy);
    latest=nx_close_take(&s,&legacy);CHECK(latest==UINT64_MAX&&!legacy);
    CHECK(nx_close_resolve(&s,&legacy,latest,NX_WINDOW_CLOSE_CANCEL)==NX_OK);
    nx_close_notify(&s,&legacy);CHECK(legacy&&!nx_close_take(&s,&legacy));
    CHECK(nx_close_resolve(&s,&legacy,latest,NX_WINDOW_CLOSE_CANCEL)==NX_ERROR_STATE);
    puts("Close requests: once-only delivery, supersession, cancellation, terminal acceptance/force and non-wrapping exhaustion passed.");return 0;
}
