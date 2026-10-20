#ifndef NEXAUI_TEXT_COST_HOOKS_H
#define NEXAUI_TEXT_COST_HOOKS_H
#include <stddef.h>
/* Only the generated diagnostic core calls these. Installed builds do not. */
void nx_cost_begin(unsigned stage);
void nx_cost_end(unsigned stage);
void nx_cost_bytes(size_t bytes);
#endif
