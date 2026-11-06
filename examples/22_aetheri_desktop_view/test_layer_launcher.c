/* Drive the private test host with a socketpair, never an actual app owner. */
#define main aetheri_desktop_layer_program_main
#include "main_layer_test.c"
#undef main
#include <sys/wait.h>

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
        char packet[3];CHECK(recv(sockets[1],packet,sizeof(packet),0)==3 && !memcmp(packet,"L3F",3));
        for(unsigned i=0;i<3;++i)CHECK(a.views[i].model.pending[AE_DESKTOP_FILES]);
        CHECK(nx_semantics_request_action(ctx,13,NX_ACTION_PRESS)==NX_ERROR_STATE);
        const char *reply=attempt?"E3F":"A3F";CHECK(send(sockets[1],reply,3,0)==3);launcher_reply(&a);
        for(unsigned i=0;i<3;++i)CHECK(!a.views[i].model.pending[AE_DESKTOP_FILES] &&
            a.views[i].model.process[AE_DESKTOP_FILES]==!attempt && a.views[i].model.launch_error[AE_DESKTOP_FILES]==!!attempt);
        advance(v);
        if(!attempt){CHECK(send(sockets[1],"X3F",3,0)==3);launcher_reply(&a);
            for(unsigned i=0;i<3;++i)CHECK(!a.views[i].model.process[AE_DESKTOP_FILES]);advance(v);}
    }
    /* Old protocol replies must fail closed, without mutating the parent. */
    CHECK(send(sockets[1],"A2F",3,0)==3);
    pid_t child=fork();CHECK(child>=0);
    if(!child){launcher_reply(&a);_exit(0);}
    int status=0;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==1);
    CHECK(!a.views[0].model.pending[AE_DESKTOP_FILES] && !a.views[0].model.process[AE_DESKTOP_FILES]);
    /* Ctrl+Q sends the owner's shutdown fence once, without retiring the
       desktop or submitting later launch actions while children drain. */
    a.owner_fixture=true;a.focused=v;v->configured=v->expanded=true;
    a.xkb=xkb_context_new(XKB_CONTEXT_NO_FLAGS);CHECK(a.xkb);
    a.keymap=xkb_keymap_new_from_names(a.xkb,NULL,XKB_KEYMAP_COMPILE_NO_FLAGS);CHECK(a.keymap);
    a.key_state=xkb_state_new(a.keymap);CHECK(a.key_state);
    xkb_mod_index_t control=xkb_keymap_mod_get_index(a.keymap,XKB_MOD_NAME_CTRL);CHECK(control<32);
    modifiers(&a,NULL,0,1u<<control,0,0,0);
    key(&a,NULL,0,0,KEY_Q,WL_KEYBOARD_KEY_STATE_PRESSED);
    char packet[3];CHECK(recv(sockets[1],packet,sizeof(packet),0)==3 && !memcmp(packet,"Q30",3));
    key(&a,NULL,0,0,KEY_Q,WL_KEYBOARD_KEY_STATE_PRESSED);
    CHECK(nx_semantics_request_action(ctx,13,NX_ACTION_PRESS)==NX_OK);advance(v);
    CHECK(recv(sockets[1],packet,sizeof(packet),MSG_DONTWAIT)<0 && (errno==EAGAIN || errno==EWOULDBLOCK));
    CHECK(a.closing && !a.retired && !a.views[0].model.pending[AE_DESKTOP_FILES]);
    xkb_state_unref(a.key_state);xkb_keymap_unref(a.keymap);xkb_context_unref(a.xkb);
    aetheri_desktop_view_destroy(v->view);free(a.font);close(sockets[0]);close(sockets[1]);
    puts("Files dock launch packet, pending, accepted, exit and error replies passed");return 0;
}
