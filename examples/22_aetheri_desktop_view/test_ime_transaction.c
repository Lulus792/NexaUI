#include "ime_transaction.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
int main(void) {
    aetheri_ime_transaction s={0};aetheri_ime_result out;
    CHECK(aetheri_ime_publish(&s,1,10)==NX_OK);
    CHECK(aetheri_ime_publish(&s,2,10)==NX_OK);
    CHECK(aetheri_ime_preedit(&s,"\xc3\xa9",2,2,2)==NX_OK);
    CHECK(aetheri_ime_done(&s,1,10,&out)==NX_OK && !out.can_commit_state &&
          out.preedit_length==2 && !memcmp(out.preedit,"\xc3\xa9",2));
    CHECK(aetheri_ime_commit(&s,"a\xc3\xa9",3)==NX_OK);
    aetheri_ime_delete(&s,2,3);
    CHECK(aetheri_ime_done(&s,2,10,&out)==NX_OK && out.can_commit_state &&
          out.scalar_count==2 && out.scalars[0]=='a' && out.scalars[1]==0xe9 &&
          out.before==2 && out.after==3 && !out.preedit_length);
    CHECK(aetheri_ime_publish(&s,3,11)==NX_OK);
    CHECK(aetheri_ime_commit(&s,"x",1)==NX_OK);
    CHECK(aetheri_ime_done(&s,2,11,&out)==NX_ERROR_STATE && !out.scalar_count);
    CHECK(aetheri_ime_done(&s,3,11,&out)==NX_OK && !out.scalar_count);
    CHECK(aetheri_ime_commit(&s,"z",1)==NX_OK);
    CHECK(aetheri_ime_publish(&s,4,12)==NX_OK);
    CHECK(aetheri_ime_done(&s,4,12,&out)==NX_ERROR_STATE && !out.scalar_count);
    CHECK(aetheri_ime_publish(&s,5,11)==NX_OK);
    CHECK(aetheri_ime_commit(&s,"\xff",1)==NX_ERROR_ARGUMENT);
    CHECK(aetheri_ime_done(&s,5,11,&out)==NX_ERROR_ARGUMENT && !out.scalar_count);
    CHECK(aetheri_ime_preedit(&s,"\xc3\xa9",2,1,2)==NX_ERROR_ARGUMENT);
    CHECK(aetheri_ime_done(&s,5,11,&out)==NX_ERROR_ARGUMENT);
    CHECK(aetheri_ime_commit(&s,"\n",1)==NX_OK);
    CHECK(aetheri_ime_done(&s,5,11,&out)==NX_ERROR_ARGUMENT);
    char large[129];memset(large,'a',sizeof(large));
    CHECK(aetheri_ime_commit(&s,large,sizeof(large))==NX_OK);
    CHECK(aetheri_ime_done(&s,5,11,&out)==NX_ERROR_LIMIT && !out.scalar_count);
    CHECK(aetheri_ime_preedit(&s,NULL,4097,0,0)==NX_ERROR_LIMIT);
    CHECK(aetheri_ime_done(&s,5,11,&out)==NX_ERROR_ARGUMENT);
    CHECK(aetheri_ime_commit(&s,"x",1)==NX_OK);aetheri_ime_retire(&s);
    CHECK(aetheri_ime_done(&s,5,11,&out)==NX_ERROR_STATE);
    CHECK(aetheri_ime_publish(&s,5,11)==NX_ERROR_STATE);
    CHECK(aetheri_ime_publish(&s,6,11)==NX_OK);
    CHECK(aetheri_ime_done(&s,6,11,&out)==NX_OK && !out.scalar_count);
    CHECK(aetheri_ime_publish(&s,UINT32_MAX,11)==NX_OK);
    CHECK(aetheri_ime_publish(&s,1,11)==NX_ERROR_STATE);
    return 0;
}
