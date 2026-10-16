#ifndef NEXAUI_TEXT_COMMAND_H
#define NEXAUI_TEXT_COMMAND_H
#include "nexaui/nexaui.h"

static inline uint32_t nx_text_command_base(uint32_t command) {
    return command & ~NX_TEXT_COMMAND_EXTEND_SELECTION;
}
static inline int nx_text_command_valid(uint32_t command) {
    uint32_t base=nx_text_command_base(command);
    return base>NX_TEXT_COMMAND_NONE && base<NX_TEXT_COMMAND_COUNT &&
        (!(command&NX_TEXT_COMMAND_EXTEND_SELECTION) || base<=NX_TEXT_COMMAND_DOCUMENT_END ||
         base==NX_TEXT_COMMAND_MOVE_PAGE_UP || base==NX_TEXT_COMMAND_MOVE_PAGE_DOWN);
}
/* Read existing boundary indexes / erase a visual range without changing the
   selection before the atomic edit (including its undo record). */
int nx_text_buffer_word_boundary_internal(const nx_text_buffer *buffer,size_t byte);
nx_result nx_text_buffer_erase_internal(nx_text_buffer *buffer,size_t start,size_t end);
#endif
