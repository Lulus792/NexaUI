/* Drive the private test host with a socketpair, never an actual app owner. */
#define main aetheri_desktop_layer_program_main
#include "main_layer_test.c"
#undef main

int main(int argc,char **argv) {
    CHECK(argc==2);struct app a={.launcher=-1,.toggle_output=-1};
    a.font=load_font(argv[1],&a.font_size);CHECK(a.font);
    int sockets[2];CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET,0,sockets));a.launcher=sockets[0];
    for(unsigned i=0;i<3;++i) {
        struct view *v=&a.views[i];v->app=&a;v->index=i;v->width=640;v->height=84;v->revision=1;
        ae_desktop_model_init(&v->model);
    }
    struct view *v=&a.views[0];CHECK(aetheri_desktop_view_create(&v->model,a.font,a.font_size,&v->view));
    advance(v);nx_context *ctx=aetheri_desktop_view_context(v->view);
    for(unsigned attempt=0;attempt<2;++attempt) {
        CHECK(nx_semantics_request_action(ctx,13,NX_ACTION_PRESS)==NX_OK);advance(v);
        char packet[3];CHECK(recv(sockets[1],packet,sizeof(packet),0)==3 && !memcmp(packet,"L2F",3));
        for(unsigned i=0;i<3;++i)CHECK(a.views[i].model.pending[AE_DESKTOP_FILES]);
        CHECK(nx_semantics_request_action(ctx,13,NX_ACTION_PRESS)==NX_ERROR_STATE);
        const char *reply=attempt?"E2F":"A2F";CHECK(send(sockets[1],reply,3,0)==3);launcher_reply(&a);
        for(unsigned i=0;i<3;++i)CHECK(!a.views[i].model.pending[AE_DESKTOP_FILES] &&
            a.views[i].model.process[AE_DESKTOP_FILES]==!attempt && a.views[i].model.launch_error[AE_DESKTOP_FILES]==!!attempt);
        advance(v);
        if(!attempt){CHECK(send(sockets[1],"X2F",3,0)==3);launcher_reply(&a);
            for(unsigned i=0;i<3;++i)CHECK(!a.views[i].model.process[AE_DESKTOP_FILES]);advance(v);}
    }
    aetheri_desktop_view_destroy(v->view);free(a.font);close(sockets[0]);close(sockets[1]);
    puts("Files dock launch packet, pending, accepted, exit and error replies passed");return 0;
}
