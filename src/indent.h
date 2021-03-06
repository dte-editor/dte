#ifndef INDENT_H
#define INDENT_H

#include <stdbool.h>
#include <stddef.h>
#include "util/string-view.h"

typedef struct {
    // Size in bytes
    size_t bytes;

    // Width in chars
    size_t width;

    // Number of whole indentation levels (depends on the indent-width option)
    size_t level;

    // Only spaces or tabs depending on expand-tab, indent-width and tab-width.
    // Note that "sane" line can contain spaces after tabs for alignment.
    bool sane;

    // The line is empty or contains only white space
    bool wsonly;
} IndentInfo;

char *make_indent(size_t width);
char *get_indent_for_next_line(const StringView *line);
void get_indent_info(const StringView *buf, IndentInfo *info);
size_t get_indent_level_bytes_left(void);
size_t get_indent_level_bytes_right(void);
char *alloc_indent(size_t count, size_t *sizep);

#endif
