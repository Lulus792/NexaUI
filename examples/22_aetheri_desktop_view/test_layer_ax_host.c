/* Exercise the layer host's actual accessibility polling with a simulated
   configured view and a real external AT-SPI client/registry. */
#define main aetheri_desktop_layer_program_main
#include "main_layer_test.c"
#undef main
static volatile sig_atomic_t remove_requested;
static void remove_output(int sig){(void)sig;remove_requested=1;}
int main(int argc,char **argv) {
    CHECK(argc==3 && !strcmp(argv[1],"--diagnostics"));
    setvbuf(stdout,NULL,_IOLBF,0);
    struct app a={.launcher=-1,.toggle_output=-1};
    a.font=load_font(argv[2],&a.font_size);CHECK(a.font);
    struct view *v=&a.views[0];v->app=&a;v->width=640;v->height=480;
    v->configured=v->expanded=true;v->revision=1;
    ae_desktop_model_init(&v->model);ae_desktop_model_search(&v->model,0,true);
    CHECK(aetheri_desktop_view_create(&v->model,a.font,a.font_size,&v->view));
    CHECK(nx_linux_accessibility_host_create(&v->accessibility)==NX_OK);
    a.focused=v;a.keyboard=(struct wl_keyboard *)(uintptr_t)1; /* simulated native focus */
    advance(v);CHECK(nx_focus_set(aetheri_desktop_view_context(v->view),20)==NX_OK);
    CHECK(aetheri_desktop_view_surface_focus(v->view,1)==NX_OK);advance(v);
    accessibility_publish(v);v->dirty=false;
    if(getenv("NEXAUI_AX_LAYER_MULTI")) {
        for(unsigned i=1;i<3;++i) {
            struct view *other=&a.views[i];other->app=&a;other->index=i;
            other->width=640;other->height=480;other->configured=true;other->revision=1;
            ae_desktop_model_init(&other->model);
            if(i==2){other->model.german=true;other->expanded=true;ae_desktop_model_search(&other->model,i,true);}
            CHECK(aetheri_desktop_view_create(&other->model,a.font,a.font_size,&other->view));
            CHECK(nx_linux_accessibility_host_create(&other->accessibility)==NX_OK);
            advance(other);
            if(i==2) { /* A logical target alone must not claim native focus. */
                CHECK(nx_focus_set(aetheri_desktop_view_context(other->view),20)==NX_OK);advance(other);
            }
            accessibility_publish(other);other->dirty=false;
        }
        a.output_names[2]=42;
        global_remove(&a,NULL,99);
        CHECK(!a.views[0].remove_pending && !a.views[1].remove_pending && !a.views[2].remove_pending);
    }
    uint32_t other_selected[2]={a.views[1].model.selected,a.views[2].model.selected};
    enum ae_desktop_app other_app[2]={a.views[1].model.selected_app,a.views[2].model.selected_app};
    struct sigaction action={0};action.sa_handler=stop_signal;sigemptyset(&action.sa_mask);
    CHECK(!sigaction(SIGTERM,&action,NULL) && !sigaction(SIGINT,&action,NULL));
    action.sa_handler=remove_output;CHECK(!sigaction(SIGUSR1,&action,NULL));
    puts("READY");
    while(!stop_requested) {
        if(remove_requested) {
            remove_requested=0;struct view *other=&a.views[2];CHECK(other->view && other->accessibility);
            a.pointer_view=other;a.axis_y=10;a.activation_phase=1;a.activation_output=2;
            a.activation_enabled=true;a.toggle_output=2;
            global_remove(&a,NULL,42);CHECK(other->remove_pending && a.views[0].view && a.views[1].view);
            retire_output(&a,2);
            CHECK(!other->view && !other->accessibility && !other->configured && !a.output_names[2] &&
                  !a.pointer_view && a.axis_y==0 && !a.activation_phase && !a.activation_enabled && a.toggle_output==-1);
            a.output_count=3;global(&a,NULL,43,"wl_output",4);
            CHECK(a.unmapped_outputs==1 && !a.output_names[2] && !other->view && a.views[0].view && a.views[1].view);
            puts("REMOVED 2");
        }
        for(unsigned i=0;i<3;++i)if(a.views[i].view && a.views[i].dirty) {
            advance(&a.views[i]);accessibility_publish(&a.views[i]);a.views[i].dirty=false;
        }
        CHECK(!a.views[1].model.query[0] && !a.views[2].model.query[0]);
        CHECK(a.views[1].model.selected==other_selected[0] && a.views[2].model.selected==other_selected[1] &&
              a.views[1].model.selected_app==other_app[0] && a.views[2].model.selected_app==other_app[1]);
        struct pollfd fds[3];uint32_t timeout=accessibility_prepare(&a,fds,1000);
        int result=poll(fds,3,(int)timeout);
        if(result<0 && errno==EINTR)continue;
        CHECK(result>=0);
    }
    for(unsigned i=0;i<3;++i) {
        if(a.views[i].accessibility)CHECK(nx_linux_accessibility_host_destroy(&a.views[i].accessibility)==NX_OK);
        aetheri_desktop_view_destroy(a.views[i].view);
    }
    free(a.font);return 0;
}
