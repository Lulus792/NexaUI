/* App-owned font faces borrow host byte buffers until view destruction. */
bool ae_files_view_add_fallback(struct ae_files_view *v,const void *bytes,size_t length) {
    if(!v || !bytes || !length || length>4u*1024u*1024u || v->width!=0.0f || v->height!=0.0f ||
       v->fallback_count==sizeof(v->fallback_fonts)/sizeof(v->fallback_fonts[0])) return false;
    nx_font *font=NULL;
    if(nx_font_open(bytes,length,NULL,&font)!=NX_OK) return false;
    if(nx_font_collection_add(v->fonts,font)!=NX_OK) {nx_font_close(font);return false;}
    v->fallback_fonts[v->fallback_count++]=font;return true;
}
static bool naming_button_width(struct ae_files_view *v,const char *name,float *width) {
    nx_text_layout *layout=NULL;
    if(!label(v,name,11*v->s->scale,16384,false,&layout) || nx_text_layout_line_count(layout)!=1) return false;
    double measured=nx_text_layout_width(layout);
    if(!isfinite(measured) || measured>16384) return false;
    *width=fmaxf(32*v->s->scale,(float)ceil(measured)+12);return true;
}
