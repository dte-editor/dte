#include <stddef.h>
#include "unicode.h"
#include "ascii.h"

#define BISEARCH(u, arr) bisearch((u), (arr), ARRAY_COUNT(arr) - 1)

typedef struct {
    CodePoint first, last;
} CodepointRange;

#include "wcwidth.c"

static inline PURE bool bisearch (
    CodePoint u,
    const CodepointRange *const range,
    size_t max
) {
    size_t min = 0;
    size_t mid;

    if (u < range[0].first || u > range[max].last) {
        return false;
    }

    while (max >= min) {
        mid = (min + max) / 2;
        if (u > range[mid].last) {
            min = mid + 1;
        } else if (u < range[mid].first) {
            max = mid - 1;
        } else {
            return true;
        }
    }

    return false;
}

// Returns true for any whitespace character that isn't "non-breaking",
// i.e. one that is used purely to separate words and may, for example,
// be "broken" (changed to a newline) by hard wrapping.
bool u_is_breakable_whitespace(CodePoint u)
{
    switch (u) {
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case ' ':
    case 0x2000: // En quad
    case 0x2001: // Em quad
    case 0x2002: // En space
    case 0x2003: // Em space
    case 0x2004: // 3-per-em space
    case 0x2005: // 4-per-em space
    case 0x2006: // 6-per-em space
    case 0x2009: // Thin space
    case 0x200a: // Hair space
    case 0x3000: // Ideographic space
        return true;
    }
    return false;
}

bool u_is_word_char(CodePoint u)
{
    return u >= 0x80 || is_alnum_or_underscore(u);
}

bool u_is_unprintable(CodePoint u)
{
    if (BISEARCH(u, unprintable)) {
        return true;
    }
    return !u_is_unicode(u);
}

bool u_is_special_whitespace(CodePoint u)
{
    return BISEARCH(u, special_whitespace);
}

bool u_is_nonspacing_mark(CodePoint u)
{
    return BISEARCH(u, nonspacing_mark);
}

static bool u_is_double_width(CodePoint u)
{
    return BISEARCH(u, double_width);
}

unsigned int u_char_width(CodePoint u)
{
    if (u < 0x80) {
        if (ascii_iscntrl(u)) {
            return 2; // Rendered in caret notation (e.g. ^@)
        }
        return 1;
    } else if (u_is_unprintable(u)) {
        return 4; // Rendered as <xx>
    } else if (u_is_nonspacing_mark(u)) {
        return 0;
    } else if (u < 0x1100U) {
        return 1;
    } else if (u_is_double_width(u)) {
        return 2;
    }

    return 1;
}
