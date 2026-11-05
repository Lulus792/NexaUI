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
    advance(v);
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK);
    a.ime_enabled=true;a.ime_surface=v->surface;a.ime_serial=1;a.ime_revision=current;
    CHECK(aetheri_desktop_view_host_ime_surrounding(v->view,current,a.ime_surrounding,sizeof(a.ime_surrounding),
          &a.ime_length,&a.ime_cursor,&a.ime_anchor)==NX_OK);
    CHECK(aetheri_ime_publish(&a.ime_transaction,1,current)==NX_OK);
    nx_rect before={0},after={0};CHECK(aetheri_desktop_view_host_ime_caret(v->view,&before)==NX_OK);
    ime_commit(&a,NULL,"\xff");ime_done(&a,NULL,1);CHECK(!strcmp(v->model.query,"A"));
    ime_delete(&a,NULL,99,0);ime_commit(&a,NULL,"X");ime_done(&a,NULL,1);CHECK(!strcmp(v->model.query,"A"));
    char large[513];for(size_t i=0;i<128;++i)memcpy(large+i*4,"\xf0\x9f\x98\x80",4);large[512]=0;
    uint64_t before_large=current;
    ime_commit(&a,NULL,large);ime_done(&a,NULL,1);
    CHECK(!strcmp(v->model.query,"A") && aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK && current==before_large);
    ime_preedit(&a,NULL,"x",-1,-1);ime_done(&a,NULL,1);
    CHECK(aetheri_desktop_view_host_ime_caret(v->view,&after)==NX_ERROR_STATE && a.ime_enabled);
    ime_preedit(&a,NULL,"x",1,1);ime_done(&a,NULL,1);
    CHECK(!strcmp(v->model.query,"A"));
    CHECK(aetheri_desktop_view_host_ime_caret(v->view,&after)==NX_OK && after.x>before.x);
    ime_commit(&a,NULL,"\xc3\xa9");ime_preedit(&a,NULL,"",0,0);ime_done(&a,NULL,1);
    CHECK(!strcmp(v->model.query,"A\xc3\xa9"));
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK);
    a.ime_serial=2;a.ime_revision=current;
    CHECK(aetheri_desktop_view_host_ime_surrounding(v->view,current,a.ime_surrounding,sizeof(a.ime_surrounding),
          &a.ime_length,&a.ime_cursor,&a.ime_anchor)==NX_OK);
    CHECK(aetheri_ime_publish(&a.ime_transaction,2,current)==NX_OK);
    ime_delete(&a,NULL,2,0);ime_commit(&a,NULL,"B");ime_done(&a,NULL,2);
    CHECK(!strcmp(v->model.query,"AB"));
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK);
    a.ime_serial=3;CHECK(aetheri_ime_publish(&a.ime_transaction,3,current)==NX_OK);
    ime_commit(&a,NULL,"X");leave(&a,NULL,0,v->surface);ime_done(&a,NULL,3);
    CHECK(!strcmp(v->model.query,"AB"));
    enter(&a,NULL,0,v->surface,&keys);
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK);
    a.ime_enabled=true;a.ime_surface=v->surface;a.ime_serial=4;
    CHECK(aetheri_ime_publish(&a.ime_transaction,4,current)==NX_OK);
    CHECK(aetheri_desktop_view_host_ime_caret(v->view,&before)==NX_OK);
    ime_preedit(&a,NULL,"x",1,1);ime_done(&a,NULL,4);
    CHECK(aetheri_desktop_view_host_ime_caret(v->view,&after)==NX_OK && after.x>before.x);
    ime_leave(&a,NULL,v->surface);advance(v);
    CHECK(!a.ime_enabled && aetheri_desktop_view_host_ime_caret(v->view,&after)==NX_OK && after.x==before.x);
    /* The registry callback only schedules teardown; the owner loop performs it. */
    v->surface=NULL;a.ime_surface=NULL;a.output_names[0]=77;
    global_remove(&a,NULL,77);CHECK(v->remove_pending && a.focused==v && v->view);
    retire_output(&a,0);CHECK(!v->view && !a.focused && !v->configured && !a.output_names[0]);
    free(a.font);return 0;
}
