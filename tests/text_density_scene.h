#ifndef NEXAUI_TEST_TEXT_DENSITY_SCENE_H
#define NEXAUI_TEST_TEXT_DENSITY_SCENE_H
/* Include search_font_fixture.h before this shared native fixture. The font
   and layout die before rendering, while the returned stamp stays borrowed. */
static int text_density_scene(nx_context *ui,nx_text_stamp **out,float density) {
    uint8_t bytes[512];font_fixture(bytes);
    nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_text_layout *layout=NULL;
    nx_text_stamp *stamp=NULL;int failed=1;
    if(nx_font_open(bytes,sizeof(bytes),NULL,&font)!=NX_OK||
       nx_font_collection_create(NULL,&fonts)!=NX_OK||
       nx_font_collection_add(fonts,font)!=NX_OK||
       nx_text_layout_create(fonts,"010\n101",7,NX_BIDI_LTR,23.25,75,&layout)!=NX_OK||
       nx_text_stamp_create_scaled(layout,density,65536,NULL,&stamp)!=NX_OK)goto done;
    if(nx_begin_frame(ui,180,140,0)!=NX_OK||
       nx_push_clip(ui,(nx_rect){1.25f,2.5f,165,125})!=NX_OK||
       nx_draw_text_layout_scaled(ui,layout,(nx_point){7.25f,4.5f},
           (nx_color){.5f,.8f,1,.7f},density,65536)!=NX_OK||
       nx_push_clip(ui,(nx_rect){12.5f,30.25f,57.5f,36.5f})!=NX_OK||
       nx_label_stamp(ui,1,0,(nx_rect){9.5f,31.25f,70,60},stamp,
           (nx_point){9.5f,31.25f},(nx_color){1,.7f,.3f,.65f})!=NX_OK||
       nx_pop_clip(ui)!=NX_OK||nx_pop_clip(ui)!=NX_OK||nx_end_frame(ui)!=NX_OK)goto done;
    *out=stamp;stamp=NULL;failed=0;
done:
    nx_text_stamp_destroy(stamp);nx_text_layout_destroy(layout);
    nx_font_collection_destroy(fonts);nx_font_close(font);
    memset(bytes,0,sizeof(bytes));return failed;
}
#endif
