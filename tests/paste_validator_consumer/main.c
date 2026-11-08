#include <nexaui/nexaui.h>
#include <stdio.h>
/* The application validates the complete splice without allocating a combined
   buffer. Its real model policy can inspect all three spans at this boundary. */
static int bounded_name(nx_utf8_span old,size_t start,size_t end,nx_utf8_span insertion,void *user) {
    (void)user;
    return start<=end && end<=old.length && old.length-(end-start)<=255 &&
        insertion.length<=255-(old.length-(end-start));
}
int main(void) {
    nx_text_field *field=NULL;
    if(nx_text_field_create("name",4,4096,&field)!=NX_OK)return 1;
    nx_result result=nx_text_field_set_paste_validator(field,bounded_name,NULL);
    if(result==NX_OK)result=nx_text_field_set_paste_validator(field,NULL,NULL);
    nx_text_field_destroy(field);
    if(result!=NX_OK)return 2;
    puts("Installed paste-validator entry point resolved");return 0;
}
