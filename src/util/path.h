#ifndef UTIL_PATH_H
#define UTIL_PATH_H

#include <stdbool.h>
#include <string.h>
#include "macros.h"
#include "string-view.h"

NONNULL_ARGS
static inline bool path_is_absolute(const char *path)
{
    return path[0] == '/';
}

static inline StringView path_slice_dirname(const char *filename)
{
    const char *const slash = strrchr(filename, '/');
    if (slash == NULL) {
        return string_view(".", 1);
    }
    return string_view(filename, slash - filename);
}

char *path_absolute(const char *filename) MALLOC NONNULL_ARGS;
char *relative_filename(const char *f, const char *cwd) XSTRDUP;
char *path_dirname(const char *filename) XSTRDUP;
const char *path_basename(const char *filename) NONNULL_ARGS_AND_RETURN;

#endif
