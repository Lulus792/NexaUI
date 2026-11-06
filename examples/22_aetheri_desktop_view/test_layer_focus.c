/* Exercise the private host's actual focus callbacks without a compositor. */
#define main aetheri_desktop_layer_program_main
#include "main_layer_test.c"
#undef main

int main(int argc, char **argv) {
    CHECK(argc==2);
    struct app mapping={.output_count=3};
    for(unsigned i=0;i<3;++i){mapping.outputs[i]=(struct wl_output *)(uintptr_t)(i+1u);mapping.output_names[i]=100u+i;
        mapping.observed_outputs[i].app=&mapping;mapping.observed_outputs[i].scale=1;}
    output_scale(&mapping.observed_outputs[0],NULL,2);
    CHECK(mapping.observed_outputs[0].scale==1);output_done(&mapping.observed_outputs[0],NULL);
    CHECK(mapping.observed_outputs[0].scale==2);
    output_name(&mapping.observed_outputs[0],NULL,"HDMI");
    output_name(&mapping.observed_outputs[1],NULL,"DP-2");
    output_name(&mapping.observed_outputs[2],NULL,"eDP");
    desktop_output(&mapping,NULL,0,"eDP");desktop_output(&mapping,NULL,1,"HDMI");desktop_output(&mapping,NULL,2,"DP-2");
    desktop_done(&mapping,NULL);
    struct app bad=mapping;
    strcpy(bad.observed_outputs[1].name,"HDMI");
    CHECK(!map_initial_outputs(&bad) && bad.outputs[0]==mapping.outputs[0] && bad.output_names[0]==100);
    bad=mapping;bad.observed_outputs[1].name[0]=0;CHECK(!map_initial_outputs(&bad));
    bad=mapping;output_name(&bad.observed_outputs[1],NULL,"DP-2");CHECK(!map_initial_outputs(&bad));
    bad=mapping;strcpy(bad.desktop_output_names[1],"eDP");CHECK(!map_initial_outputs(&bad));
    bad=mapping;global_remove(&bad,NULL,101);CHECK(bad.initial_output_removed && !map_initial_outputs(&bad));
    CHECK(map_initial_outputs(&mapping) && mapping.output_mapping_done);
    CHECK(mapping.output_names[0]==102 && mapping.output_names[1]==100 && mapping.output_names[2]==101);
    CHECK(mapping.outputs[0]==(struct wl_output *)(uintptr_t)3 && mapping.outputs[1]==(struct wl_output *)(uintptr_t)1 &&
          mapping.outputs[2]==(struct wl_output *)(uintptr_t)2);
    CHECK(mapping.mapped_observations[1]==&mapping.observed_outputs[0] && mapping.mapped_observations[1]->scale==2);
    struct view geometry={.width=640,.height=480};nx_pixel_size pixels={0};
    CHECK(view_pixels(&geometry,2,&pixels) && pixels.width==1280 && pixels.height==960);
    geometry.width=UINT32_MAX;CHECK(!view_pixels(&geometry,8,&pixels));
    output_scale(&mapping.observed_outputs[0],NULL,3);
    CHECK(mapping.mapped_observations[1]->scale==2 && !mapping.views[1].dirty);
    output_done(&mapping.observed_outputs[0],NULL);
    CHECK(mapping.mapped_observations[1]->scale==3 && mapping.views[1].dirty && !mapping.views[0].dirty);
    CHECK(!map_initial_outputs(&mapping) && mapping.output_names[0]==102);
    global_remove(&mapping,NULL,100);CHECK(mapping.views[1].remove_pending && !mapping.views[0].remove_pending);
    struct app a={.launcher=-1,.toggle_output=-1,.output_mapping_done=true};
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
    a.xkb=xkb_context_new(XKB_CONTEXT_NO_FLAGS);CHECK(a.xkb);
    a.keymap=xkb_keymap_new_from_names(a.xkb,NULL,XKB_KEYMAP_COMPILE_NO_FLAGS);CHECK(a.keymap);
    a.key_state=xkb_state_new(a.keymap);CHECK(a.key_state);
    xkb_mod_index_t control=xkb_keymap_mod_get_index(a.keymap,XKB_MOD_NAME_CTRL);
    xkb_mod_index_t shift=xkb_keymap_mod_get_index(a.keymap,XKB_MOD_NAME_SHIFT);
    CHECK(control<32 && shift<32);
    modifiers(&a,NULL,0,(1u<<control)|(1u<<shift),0,0,0);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_PRESSED);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_RELEASED);
    CHECK(nx_focus_id(aetheri_desktop_view_context(v->view))==13);
    modifiers(&a,NULL,0,0,0,0,0);
    key(&a,NULL,0,0,KEY_ENTER,WL_KEYBOARD_KEY_STATE_PRESSED);
    key(&a,NULL,0,0,KEY_ENTER,WL_KEYBOARD_KEY_STATE_RELEASED);
    modifiers(&a,NULL,0,1u<<control,0,0,0);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_PRESSED);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_RELEASED);
    CHECK(nx_focus_id(aetheri_desktop_view_context(v->view))==20);
    modifiers(&a,NULL,0,0,0,0,0);
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
    /* Tab away and back may arrive in one Wayland dispatch. Losing the field
       must end the old native enable before focus returns to the same view. */
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK);
    a.ime_enabled=true;a.ime_surface=v->surface;a.ime_serial=5;a.ime_enable_serial=5;
    CHECK(aetheri_ime_publish(&a.ime_transaction,5,current)==NX_OK);
    modifiers(&a,NULL,0,(1u<<control)|(1u<<shift),0,0,0);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_PRESSED);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_RELEASED);
    CHECK(!a.ime_enabled && !a.ime_enable_serial);
    modifiers(&a,NULL,0,1u<<control,0,0,0);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_PRESSED);
    key(&a,NULL,0,0,KEY_TAB,WL_KEYBOARD_KEY_STATE_RELEASED);
    modifiers(&a,NULL,0,0,0,0,0);
    CHECK(nx_focus_id(aetheri_desktop_view_context(v->view))==20);
    /* A reply from an earlier enable must not reset the current context.
       A stale preedit from this enable must stop its invisible composition. */
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK && current>1);
    a.ime_enabled=true;a.ime_surface=v->surface;a.ime_serial=7;a.ime_enable_serial=6;
    CHECK(aetheri_ime_publish(&a.ime_transaction,6,current-1)==NX_OK);
    CHECK(aetheri_ime_publish(&a.ime_transaction,7,current)==NX_OK);
    ime_preedit(&a,NULL,"x",1,1);ime_done(&a,NULL,5);
    CHECK(a.ime_enabled && !strcmp(v->model.query,"AB"));
    ime_preedit(&a,NULL,"x",1,1);ime_done(&a,NULL,6);
    CHECK(!a.ime_enabled && !strcmp(v->model.query,"AB"));
    /* The registry callback only schedules teardown; the owner loop performs it. */
    a.ime_enabled=true;a.ime_surface=v->surface;a.ime_serial=8;
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK);
    CHECK(aetheri_desktop_view_host_ime_surrounding(v->view,current,a.ime_surrounding,sizeof(a.ime_surrounding),
          &a.ime_length,&a.ime_cursor,&a.ime_anchor)==NX_OK);
    CHECK(aetheri_ime_publish(&a.ime_transaction,8,current)==NX_OK);
    a.output_names[0]=77;
    global_remove(&a,NULL,77);CHECK(v->remove_pending && a.focused==v && v->view);
    key(&a,NULL,0,0,KEY_X,WL_KEYBOARD_KEY_STATE_PRESSED);
    CHECK(!strcmp(v->model.query,"AB"));
    ime_commit(&a,NULL,"Y");ime_done(&a,NULL,8);
    CHECK(!strcmp(v->model.query,"AB"));
    v->surface=NULL;a.ime_surface=NULL;
    retire_output(&a,0);CHECK(!v->view && !a.focused && !v->configured && !a.output_names[0]);
    /* Keyboard enter may precede a deferred expanded-frame presentation.
       The newly built search field must receive the first subsequent key. */
    ae_desktop_model_init(&v->model);v->revision=1;v->width=640;v->height=480;
    v->surface=(struct wl_surface *)(uintptr_t)1;v->configured=true;v->expanded=false;
    CHECK(aetheri_desktop_view_create(&v->model,a.font,a.font_size,&v->view));advance(v);
    ae_desktop_model_search(&v->model,0,true);++v->revision;v->expanded=true;
    v->refresh_barrier=(struct wl_callback *)(uintptr_t)1;
    enter(&a,NULL,0,v->surface,&keys);
    CHECK(!v->focus_search_pending && nx_focus_id(aetheri_desktop_view_context(v->view))==20);
    key(&a,NULL,0,0,KEY_X,WL_KEYBOARD_KEY_STATE_PRESSED);
    CHECK(!strcmp(v->model.query,"x"));
    v->refresh_barrier=NULL;v->surface=NULL;aetheri_desktop_view_destroy(v->view);v->view=NULL;
    xkb_state_unref(a.key_state);xkb_keymap_unref(a.keymap);xkb_context_unref(a.xkb);
    free(a.font);return 0;
}
