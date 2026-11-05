/* Exercise the private host's actual focus callbacks without a compositor. */
#define main aetheri_desktop_layer_program_main
#include "main_layer_test.c"
#undef main

int main(int argc, char **argv) {
    CHECK(argc==2);
    struct app a={.launcher=-1,.toggle_output=-1};
    a.font=load_font(argv[1],&a.font_size);CHECK(a.font);
    struct view *v=&a.views[0];
    v->app=&a;v->index=0;v->width=640;v->height=480;
    v->surface=(struct wl_surface *)(uintptr_t)1;v->configured=true;v->expanded=true;
    ae_desktop_model_init(&v->model);ae_desktop_model_search(&v->model,0,true);v->revision=1;
    CHECK(aetheri_desktop_view_create(&v->model,a.font,a.font_size,&v->view));
    CHECK(aetheri_desktop_view_set_host_ime(v->view,1)==NX_OK);
    advance(v);
    struct wl_array keys={0};
    enter(&a,NULL,0,v->surface,&keys);
    uint64_t old=0,current=0;
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&old)==NX_OK && old);
    leave(&a,NULL,0,(struct wl_surface *)(uintptr_t)2);
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK && current==old);
    leave(&a,NULL,0,v->surface);
    current=9;CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_ERROR_STATE && !current);
    enter(&a,NULL,0,v->surface,&keys);
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK && current!=old);
    nx_event event={0};event.type=NX_EVENT_TEXT;event.ime_target=20;event.ime_revision=old;event.scalar='X';
    CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);CHECK(!v->model.query[0]);
    event.ime_revision=current;event.scalar='A';
    CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);CHECK(!strcmp(v->model.query,"A"));
    aetheri_desktop_view_destroy(v->view);free(a.font);return 0;
}
