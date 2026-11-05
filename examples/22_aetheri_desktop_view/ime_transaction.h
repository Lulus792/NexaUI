#ifndef AETHERI_DESKTOP_IME_TRANSACTION_H
#define AETHERI_DESKTOP_IME_TRANSACTION_H
#include <nexaui/nexaui.h>
#include <stdint.h>

/* Private text-input-v3 transaction state. The host owns protocol objects,
   focus, serial increments, model admission, and application of each result. */
enum { AETHERI_IME_TEXT_MAX=4096, AETHERI_IME_SCALAR_MAX=128 };
typedef struct aetheri_ime_result {
    char preedit[AETHERI_IME_TEXT_MAX+1];size_t preedit_length;
    int32_t begin,end;
    uint32_t scalars[AETHERI_IME_SCALAR_MAX];size_t scalar_count;
    uint32_t before,after;
    int can_commit_state;
} aetheri_ime_result;
typedef struct aetheri_ime_transaction {
    char preedit[AETHERI_IME_TEXT_MAX+1],commit[AETHERI_IME_TEXT_MAX+1];
    size_t preedit_length,commit_length;
    int32_t begin,end;
    uint32_t before,after,first_serial,last_serial;
    uint64_t revision,pending_revision;
    int active,invalid,pending_active;
} aetheri_ime_transaction;

void aetheri_ime_retire(aetheri_ime_transaction *state);
/* Serial is the actual non-wrapping count of protocol commit requests. */
nx_result aetheri_ime_publish(aetheri_ime_transaction *state,uint32_t serial,uint64_t revision);
nx_result aetheri_ime_preedit(aetheri_ime_transaction *state,const char *text,size_t length,int32_t begin,int32_t end);
nx_result aetheri_ime_commit(aetheri_ime_transaction *state,const char *text,size_t length);
void aetheri_ime_delete(aetheri_ime_transaction *state,uint32_t before,uint32_t after);
/* Always clears pending events. On success the caller must still recheck focus,
   field revision, surrounding text boundaries and batch application. */
nx_result aetheri_ime_done(aetheri_ime_transaction *state,uint32_t serial,uint64_t current_revision,aetheri_ime_result *out);
#endif
