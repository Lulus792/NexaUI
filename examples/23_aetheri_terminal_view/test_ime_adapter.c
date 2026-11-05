#include "ime_adapter.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)

static int width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
int main(void) {
    terminal_ime_adapter ime={0};terminal_ime_guard g={41,7,1};
    CHECK(terminal_ime_bind(&ime,g)==NX_OK);
    /* a, ä, wide CJK, combining acute: 4 scalars, 8 UTF-8 bytes. */
    const char draft[]="a\xc3\xa4\xe7\x95\x8c\xcc\x81";
    CHECK(terminal_ime_preedit(&ime,g,draft,8,1,6)==NX_OK);
    CHECK(ime.composition.size==8 && ime.composition.count==4 &&
          ime.composition.caret && ime.composition.cursor==1 && ime.composition.anchor==3);
    struct ae_terminal_composition_view view={0};
    CHECK(ae_terminal_composition_view(&ime.composition,4,width,&view));
    CHECK(view.caret && view.cursor==1 && view.selection_begin==1 && view.selection_end==4);
    struct ae_terminal_composition old=ime.composition;
    CHECK(terminal_ime_preedit(&ime,g,draft,8,2,6)==NX_ERROR_ARGUMENT);
    CHECK(!memcmp(&old,&ime.composition,sizeof(old)));
    CHECK(terminal_ime_preedit(&ime,g,draft,8,1,-1)==NX_ERROR_ARGUMENT);
    CHECK(!memcmp(&old,&ime.composition,sizeof(old)));
    CHECK(terminal_ime_preedit(&ime,g,"bad\x1b",4,0,0)==NX_ERROR_ARGUMENT);
    CHECK(!memcmp(&old,&ime.composition,sizeof(old)));
    CHECK(terminal_ime_preedit(&ime,g,"\xc0\xaf",2,0,0)==NX_ERROR_ARGUMENT);
    CHECK(!memcmp(&old,&ime.composition,sizeof(old)));
    /* The original SDL model reserves -1 length; do not silently turn a
       one-scalar reverse selection into an unselected caret. */
    CHECK(terminal_ime_preedit(&ime,g,draft,8,3,1)==NX_ERROR_UNSUPPORTED);
    CHECK(!memcmp(&old,&ime.composition,sizeof(old)));
    CHECK(terminal_ime_preedit(&ime,g,draft,8,6,1)==NX_OK);
    CHECK(ime.composition.cursor==3 && ime.composition.anchor==1);
    CHECK(terminal_ime_preedit(&ime,g,draft,8,-1,-1)==NX_OK && !ime.composition.caret);
    CHECK(terminal_ime_preedit(&ime,g,NULL,0,0,0)==NX_OK && !ime.composition.size);
    CHECK(terminal_ime_preedit(&ime,g,draft,8,1,1)==NX_OK);
    terminal_ime_guard lost=g;lost.focused=0;
    CHECK(terminal_ime_preedit(&ime,lost,draft,8,1,1)==NX_ERROR_STATE && !ime.composition.size);
    CHECK(terminal_ime_preedit(&ime,g,draft,8,1,1)==NX_ERROR_STATE);
    terminal_ime_guard again={41,8,1};CHECK(terminal_ime_bind(&ime,again)==NX_OK);
    CHECK(terminal_ime_preedit(&ime,g,draft,8,1,1)==NX_ERROR_STATE && !ime.composition.size);
    CHECK(terminal_ime_bind(&ime,again)==NX_OK);
    terminal_ime_guard other={42,8,1};
    CHECK(terminal_ime_preedit(&ime,other,draft,8,1,1)==NX_ERROR_STATE);
    CHECK(terminal_ime_bind(&ime,again)==NX_OK);
    nx_event e={.type=NX_EVENT_TEXT,.ime_target=41,.ime_revision=8,.scalar='a',.ime_before=NX_IME_COMMIT_START};
    nx_utf8_span committed={0};int ready=1;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && !ready && !committed.data);
    e.scalar=0xe4;e.ime_before=NX_IME_COMMIT_END;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && ready &&
          committed.length==3 && !memcmp(committed.data,"a\xc3\xa4",3));
    e.scalar='x';e.ime_before=NX_IME_COMMIT_START;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && !ready);
    terminal_ime_interrupt(&ime);
    e.scalar='y';e.ime_before=NX_IME_COMMIT_END;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_STATE && !ready && !committed.data);
    e.scalar='x';e.ime_before=NX_IME_COMMIT_START|NX_IME_COMMIT_FILTERED;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_STATE && !ready);
    e.ime_before=NX_IME_COMMIT_START|NX_IME_COMMIT_END;e.scalar=0x202e;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_ARGUMENT && !ready);
    e.scalar='\t';CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_ARGUMENT && !ready);
    e.scalar='x';e.ime_before=NX_IME_COMMIT_START;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && !ready);
    e.ime_before=NX_IME_COMMIT_END;e.ime_revision=7;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_STATE && !ready);
    e.ime_revision=8;e.ime_before=NX_IME_COMMIT_START;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && !ready);
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_STATE && !ready);
    e.scalar='z';e.ime_before=NX_IME_COMMIT_START;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && !ready);
    e.ime_before=0;
    for(size_t i=1;i<AE_TERM_PASTE_BYTES;++i)
        CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_OK && !ready);
    e.ime_before=NX_IME_COMMIT_END;
    CHECK(terminal_ime_commit_event(&ime,again,&e,&ready,&committed)==NX_ERROR_LIMIT && !ready && !committed.data);
    terminal_ime_reset(&ime);terminal_ime_reset(&ime);
    puts("Aetheri composition: byte-to-scalar, selection, rejection and stale guards passed");
    return 0;
}
