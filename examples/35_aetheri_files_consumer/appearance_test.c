#define _POSIX_C_SOURCE 200809L
#include <nexaui/nexaui.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Inspect the actual generated app's cached draw layouts, not just its full
   semantic names. No private SDK declarations or native-window substitution. */
#include AE_FILES_GENERATED_VIEW
#define VERIFY(x) do {if(!(x)){fprintf(stderr,"Files appearance %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static bool reject_worker(void *user,struct ae_files_session *s) {(void)user;(void)s;VERIFY(false);return false;}
static void reject_join(void *user) {(void)user;VERIFY(false);}
static void *read_font(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");VERIFY(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    VERIFY(n>0 && n<=4*1024*1024 && !fseek(f,0,SEEK_SET));
    void *bytes=malloc((size_t)n);VERIFY(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n && !fclose(f));*length=(size_t)n;return bytes;
}
static struct ae_files_view *make_view(struct ae_files_session *s,int root,const void *font,size_t length) {
    ae_files_session_init(s,reject_worker,reject_join,NULL);enum ae_files_result result;
    s->files=ae_files_create(root,&result);VERIFY(s->files);memcpy(s->root_label,"Documents",10);
    struct ae_files_view *v=NULL;VERIFY(ae_files_view_create(s,font,length,&v));return v;
}
static void settle(struct ae_files_view *v,float width,float height) {
    nx_window_status status={0};bool changed=false;
    for(unsigned i=0;i<8;++i) {VERIFY(ae_files_view_frame(v,width,height,&status,&changed));if(!changed) return;}
    VERIFY(false);
}
static void layout_cases(int root,const void *font,size_t length) {
    struct ae_files_session s;struct ae_files_view *v=make_view(&s,root,font,length);
    ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);
    const float scales[]={1,1.25f,1.5f,2},sizes[][2]={{640,420},{1000,680},{1280,900}};
    unsigned cases=0;
    for(unsigned density=1;density<=2;++density) for(unsigned language=0;language<2;++language)
    for(size_t scale=0;scale<4;++scale) for(unsigned grid=0;grid<2;++grid) for(size_t size=0;size<3;++size) {
        s.german=language!=0;s.grid=grid!=0;s.scale=scales[scale];s.dirty=true;
        VERIFY(nx_context_set_text_raster_scale(v->ui,(float)density)==NX_OK);
        settle(v,sizes[size][0],sizes[size][1]);
        const char *expected=language?"Abbrechen":"Cancel";size_t found=0;
        for(size_t i=0;i<v->label_count;++i) if(!strcmp(v->labels[i].source,expected)) {
            nx_utf8_span actual;VERIFY(nx_text_layout_text(v->labels[i].layout,&actual)==NX_OK);
            if(actual.length!=strlen(expected) || memcmp(actual.data,expected,actual.length))
                fprintf(stderr,"caption density=%u language=%u scale=%g grid=%u size=%g,%g rendered=%.*s\n",
                    density,language,(double)s.scale,grid,(double)sizes[size][0],(double)sizes[size][1],(int)actual.length,actual.data);
            VERIFY(actual.length==strlen(expected) && !memcmp(actual.data,expected,actual.length));++found;
        }
        VERIFY(found && v->cancel.width>=32*s.scale && v->confirm.width>=32*s.scale && v->name.width>0);
        VERIFY(v->cancel.x>=0 && v->cancel.y>=0 && v->cancel.x+v->cancel.width<=sizes[size][0] && v->cancel.y+v->cancel.height<=sizes[size][1]);
        VERIFY(v->confirm.x>=v->cancel.x+v->cancel.width);
        if(!s.grid) VERIFY(v->name.x+v->name.width<=v->cancel.x);
        else VERIFY(v->name.y+v->name.height<=v->cancel.y);
        ++cases;
    }
    VERIFY(cases==96);ae_files_view_destroy(v);ae_files_destroy(s.files);
    puts("Files appearance:96 actual EN/DE caption/layout cases passed");
}
#ifndef FILES_APPEARANCE_LAYOUT_ONLY
static void fallback_cases(int root,const void *font,size_t length,const void *fallback,size_t fallback_length) {
    struct ae_files_session s;struct ae_files_view *v=make_view(&s,root,font,length);
    uint16_t glyph=1;VERIFY(nx_font_glyph_index(v->font,0x1f30d,&glyph)==NX_OK && !glyph);
    VERIFY(!ae_files_view_add_fallback(NULL,fallback,fallback_length));
    VERIFY(!ae_files_view_add_fallback(v,NULL,fallback_length));
    VERIFY(!ae_files_view_add_fallback(v,fallback,4u*1024u*1024u+1u));
    VERIFY(!ae_files_view_add_fallback(v,"x",1));
    size_t count=0;VERIFY(nx_font_collection_count(v->fonts,&count)==NX_OK && count==1);
    VERIFY(ae_files_view_add_fallback(v,fallback,fallback_length));
    nx_font_match match;VERIFY(nx_font_collection_resolve(v->fonts,0x1f30d,&match)==NX_OK && match.font==v->fallback_fonts[0] && match.glyph_index);
    VERIFY(nx_font_collection_resolve(v->fonts,'A',&match)==NX_OK && match.font==v->font && match.glyph_index);
    for(unsigned i=1;i<4;++i) VERIFY(ae_files_view_add_fallback(v,fallback,fallback_length));
    VERIFY(!ae_files_view_add_fallback(v,fallback,fallback_length));
    VERIFY(nx_font_collection_count(v->fonts,&count)==NX_OK && count==5);
    ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);settle(v,1000,680);
    VERIFY(!ae_files_view_add_fallback(v,fallback,fallback_length));
    ae_files_view_destroy(v);ae_files_destroy(s.files);
    puts("Files appearance:bounded fallback admission, actual glyph choice and borrowed lifetime passed");
}
#endif
int main(int argc,char **argv) {
    VERIFY((argc==2 || argc==3) && getuid()!=0);size_t length=0;void *font=read_font(argv[1],&length);
    char path[]="files-appearance-XXXXXX";VERIFY(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);VERIFY(root>=0);
    if(argc==2) layout_cases(root,font,length);
#ifndef FILES_APPEARANCE_LAYOUT_ONLY
    else {size_t fallback_length=0;void *fallback=read_font(argv[2],&fallback_length);fallback_cases(root,font,length,fallback,fallback_length);free(fallback);}
#endif
    VERIFY(!close(root) && !rmdir(path));free(font);return 0;
}
