#ifndef SYNTAX_HIGHLIGHT_H
#define SYNTAX_HIGHLIGHT_H

#include <stdbool.h>
#include <stddef.h>
#include "buffer.h"
#include "syntax/color.h"
#include "util/string-view.h"

TermColor **hl_line(Buffer *b, const StringView *line, size_t line_nr, bool *next_changed);
void hl_fill_start_states(Buffer *b, size_t line_nr);
void hl_insert(Buffer *b, size_t first, size_t lines);
void hl_delete(Buffer *b, size_t first, size_t lines);

#endif
