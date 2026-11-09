#include "attributes.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static nx_utf8_span span(const char *s) {return (nx_utf8_span){s,strlen(s)};}
static bool opaque(nx_color c) {
    return isfinite(c.r) && isfinite(c.g) && isfinite(c.b) && c.r>=0 && c.r<=1 &&
        c.g>=0 && c.g<=1 && c.b>=0 && c.b<=1 && c.a==1;
}
static void color(char out[32],nx_color c) {
    /* ATK/AT-SPI RGB text colors use unsigned16-bit channels. */
    (void)snprintf(out,32,"%u,%u,%u",(unsigned)floor((double)c.r*65535.+.5),
        (unsigned)floor((double)c.g*65535.+.5),(unsigned)floor((double)c.b*65535.+.5));
}
static void points(char out[32],double size) {
    /* ASCII decimal independent of LC_NUMERIC, rounded to1e-6 logical point. */
    unsigned long long scaled=(unsigned long long)floor(size*1000000.+.5);
    unsigned long long fraction=scaled%1000000u;
    if(!fraction)(void)snprintf(out,32,"%llu",scaled/1000000u);
    else {
        (void)snprintf(out,32,"%llu.%06llu",scaled/1000000u,fraction);
        size_t n=strlen(out);while(n && out[n-1]=='0')out[--n]=0;
    }
}
static void add(nx_text_document_presentation *p,struct editor_attributes *s,
    const char *name,const char *value) {
    s->defaults[p->default_count++]=(nx_text_document_attribute){span(name),span(value)};
}
static bool boundary(nx_utf8_span text,size_t byte) {
    return byte<=text.length && (byte==text.length || ((unsigned char)text.data[byte]&0xc0u)!=0x80u);
}
nx_result editor_attributes_project(struct editor_attributes *s,
    const struct editor_attribute_desc *d,nx_text_document_presentation *p) {
    if(!p)return NX_ERROR_ARGUMENT;
    p->defaults=NULL;p->default_count=0;p->runs=NULL;p->run_count=0;
    if(s) {s->default_count=0;s->run_count=0;}
    if(!s || !d || (!d->text.data && d->text.length) || !isfinite(d->pixels_per_em) ||
       !isfinite(d->logical_dpi) || d->pixels_per_em<=0 || d->pixels_per_em>65535 ||
       d->logical_dpi<1 || d->logical_dpi>65535 || !opaque(d->foreground) ||
       !opaque(d->background) || !opaque(d->selected) || d->selection_start>d->selection_end ||
       !boundary(d->text,d->selection_start) || !boundary(d->text,d->selection_end))return NX_ERROR_ARGUMENT;
    double size=d->pixels_per_em*72./d->logical_dpi;
    if(size>1000000.)return NX_ERROR_LIMIT;
    points(s->size,size);color(s->foreground,d->foreground);color(s->background,d->background);color(s->selected,d->selected);
    p->defaults=s->defaults;
    add(p,s,"size",s->size);add(p,s,"fg-color",s->foreground);add(p,s,"bg-color",s->background);
    add(p,s,"editable",d->editable?"true":"false");
    size_t count=0;
    s->family_result=d->font?nx_font_name_utf8(d->font,16,s->family,sizeof(s->family),&count):NX_ERROR_UNSUPPORTED;
    if(s->family_result==NX_ERROR_UNSUPPORTED && d->font)
        s->family_result=nx_font_name_utf8(d->font,1,s->family,sizeof(s->family),&count);
    if(s->family_result==NX_OK && count)add(p,s,"family-name",s->family);
    nx_font_style style;
    s->style_result=d->font?nx_font_get_style(d->font,&style):NX_ERROR_UNSUPPORTED;
    if(s->style_result==NX_OK) {
        static const char *const stretch[]={"ultra_condensed","extra_condensed","condensed","semi_condensed","normal","semi_expanded","expanded","extra_expanded","ultra_expanded"};
        (void)snprintf(s->weight,sizeof(s->weight),"%u",(unsigned)style.weight_class);
        add(p,s,"weight",s->weight);add(p,s,"style",style.slant==NX_FONT_SLANT_OBLIQUE?"oblique":style.slant==NX_FONT_SLANT_ITALIC?"italic":"normal");
        add(p,s,"stretch",stretch[style.width_class-1u]);
    }
    if(d->selected_text && d->selection_start<d->selection_end) {
        s->selection[0]=(nx_text_document_attribute){span("bg-color"),span(s->selected)};
        s->selection[1]=(nx_text_document_attribute){span("fg-color"),span(d->contrast?"0,0,0":s->foreground)};
        s->run=(nx_text_document_attribute_run){d->selection_start,d->selection_end,s->selection,2};
        p->runs=&s->run;p->run_count=1;
    }
    s->default_count=p->default_count;s->run_count=p->run_count;
    return NX_OK;
}
