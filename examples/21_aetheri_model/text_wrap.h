#ifndef NEXAUI_AETHERI_MODEL_TEXT_WRAP_H
#define NEXAUI_AETHERI_MODEL_TEXT_WRAP_H
#include <nexaui/nexaui.h>
/* Internal example helper; returns a layout with bounded logical width. */
int aetheri_small_text_layout(nx_font_collection *fonts,const char *text,size_t length,
                              double size,double width,nx_text_layout **out);
#endif
