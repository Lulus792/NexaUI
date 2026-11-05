#include "text_wrap.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Fast path only for a single unbroken ASCII run. Caret positions come from
   the already-shaped layout; a newly shaped result is checked before use.
   Any unusual ordering or shaping change falls back to the grapheme path. */
static nx_text_layout *ascii_wrap(nx_font_collection *fonts,const char *text,size_t length,
                                  double size,double width,const nx_text_layout *source) {
    if(!length || length>(SIZE_MAX-1)/2 || nx_text_layout_line_count(source)!=1 ||
       nx_text_layout_caret_count(source)!=length+1) return NULL;
    for(size_t i=0;i<length;++i) if((unsigned char)text[i]<33 || (unsigned char)text[i]>126) return NULL;
    nx_text_caret first;
    if(nx_text_layout_caret_at(source,0,&first)!=NX_OK || first.byte_offset!=0 || first.line_index!=0 ||
       !isfinite(first.x)) return NULL;
    char *wrapped=malloc(length*2+1); if(!wrapped) return NULL;
    size_t written=0,start=0; double start_x=first.x,previous_x=first.x;
    for(size_t i=1;i<=length;++i) {
        nx_text_caret caret;
        if(nx_text_layout_caret_at(source,i,&caret)!=NX_OK || caret.byte_offset!=i || caret.line_index!=0 ||
           !isfinite(caret.x) || caret.x<previous_x) {free(wrapped);return NULL;}
        if(caret.x-start_x>width) {
            if(i==start+1) {free(wrapped);return NULL;}
            wrapped[written++]='\n'; start=i-1; start_x=previous_x;
        }
        wrapped[written++]=text[i-1]; previous_x=caret.x;
    }
    nx_text_layout *candidate=NULL;
    nx_result result=nx_text_layout_create(fonts,wrapped,written,NX_BIDI_LTR,size,width,&candidate);
    free(wrapped);
    if(result!=NX_OK) return NULL;
    if(!isfinite(nx_text_layout_width(candidate)) || nx_text_layout_width(candidate)>width) {
        nx_text_layout_destroy(candidate);return NULL;
    }
    return candidate;
}
/* The shared layout deliberately preserves unbreakable words even when wider
   than its limit. In a narrow Settings viewport, add emergency line breaks at
   grapheme boundaries; semantic names continue to use the original bytes. */
int nx_example_small_text_layout(nx_font_collection *fonts,const char *text,size_t length,
                             double size,double width,nx_text_layout **out) {
    if(nx_text_layout_create(fonts,text,length,NX_BIDI_LTR,size,width,out)!=NX_OK) return 0;
    if(nx_text_layout_width(*out)<=width) return 1;
    nx_text_layout *fast=ascii_wrap(fonts,text,length,size,width,*out);
    if(fast) {nx_text_layout_destroy(*out);*out=fast;return 1;}
    nx_text_layout_destroy(*out); *out=NULL;
    if(length>(SIZE_MAX-1)/2) return 0;
    char *wrapped=malloc(length*2+1); if(!wrapped) return 0;
    size_t line=0,pos=0,written=0;
    while(pos<length) {
        size_t next;
        nx_text_layout *probe=NULL;
        if(nx_grapheme_next(text,length,pos,&next)!=NX_OK ||
           nx_text_layout_create(fonts,text+line,next-line,NX_BIDI_LTR,size,width,&probe)!=NX_OK) {
            free(wrapped); return 0;
        }
        int overflow=nx_text_layout_width(probe)>width;
        nx_text_layout_destroy(probe);
        if(overflow) {
            if(pos==line) {free(wrapped);return 0;}
            wrapped[written++]='\n'; line=pos;
        }
        memcpy(wrapped+written,text+pos,next-pos); written+=next-pos; pos=next;
    }
    nx_result result=nx_text_layout_create(fonts,wrapped,written,NX_BIDI_LTR,size,width,out);
    free(wrapped);
    if(result!=NX_OK) return 0;
    if(nx_text_layout_width(*out)>width) {nx_text_layout_destroy(*out);*out=NULL;return 0;}
    return 1;
}
