#include "color.h"
#include "ptr-array.h"
#include "common.h"
#include "completion.h"
#include "error.h"
#include "editor.h"

IGNORE_WARNING("-Wunused-parameter")
#include "lookup/attributes.c"
#include "lookup/colors.c"
UNIGNORE_WARNINGS

static const char builtin_color_names[][16] = {
    "default",
    "nontext",
    "noline",
    "wserror",
    "selection",
    "currentline",
    "linenumber",
    "statusline",
    "commandline",
    "errormsg",
    "infomsg",
    "tabbar",
    "activetab",
    "inactivetab",
};
static_assert(ARRAY_COUNT(builtin_color_names) == NR_BC);

static PointerArray hl_colors = PTR_ARRAY_INIT;

void fill_builtin_colors(void)
{
    for (size_t i = 0; i < NR_BC; i++) {
        editor.builtin_colors[i] = &find_color(builtin_color_names[i])->color;
    }
}

HlColor *set_highlight_color(const char *name, const TermColor *color)
{
    for (size_t i = 0; i < hl_colors.count; i++) {
        HlColor *c = hl_colors.ptrs[i];
        if (streq(name, c->name)) {
            c->color = *color;
            return c;
        }
    }

    HlColor *c = xnew(HlColor, 1);
    c->name = xstrdup(name);
    c->color = *color;
    ptr_array_add(&hl_colors, c);
    return c;
}

static HlColor *find_real_color(const char *name)
{
    for (size_t i = 0; i < hl_colors.count; i++) {
        HlColor *c = hl_colors.ptrs[i];
        if (streq(c->name, name)) {
            return c;
        }
    }
    return NULL;
}

HlColor *find_color(const char *name)
{
    HlColor *color = find_real_color(name);
    if (color) {
        return color;
    }

    const char *dot = strchr(name, '.');
    if (dot) {
        return find_real_color(dot + 1);
    }

    return NULL;
}

// NOTE: you have to call update_all_syntax_colors() after this
void remove_extra_colors(void)
{
    BUG_ON(hl_colors.count < NR_BC);
    for (size_t i = NR_BC; i < hl_colors.count; i++) {
        HlColor *c = hl_colors.ptrs[i];

        // Make possible use after free error easy to see
        c->color.fg = COLOR_RED;
        c->color.bg = COLOR_YELLOW;
        c->color.attr = ATTR_BOLD;
        free(c->name);
        c->name = NULL;
        free(c);

        hl_colors.ptrs[i] = NULL;
    }
    hl_colors.count = NR_BC;
}

static bool parse_color(const char *str, int *val)
{
    const char *ptr = str;
    long r, g, b;

    if (parse_long(&ptr, &r)) {
        if (*ptr == 0) {
            if (r < -2 || r > 255) {
                return false;
            }
            // color index -2..255
            *val = r;
            return true;
        }
        if (
            r < 0 || r > 5 || *ptr++ != '/' || !parse_long(&ptr, &g) ||
            g < 0 || g > 5 || *ptr++ != '/' || !parse_long(&ptr, &b) ||
            b < 0 || b > 5 || *ptr
        ) {
            return false;
        }

        // r/g/b to color index 16..231 (6x6x6 color cube)
        *val = 16 + r * 36 + g * 6 + b;
        return true;
    }

    const ColorHashSlot *slot = lookup_color(str, strlen(str));
    if (slot) {
        *val = slot->color;
        return true;
    }

    return false;
}

static bool parse_attr(const char *str, unsigned short *attr)
{
    const AttrHashSlot *slot = lookup_attr(str, strlen(str));
    if (slot) {
        *attr |= slot->attr;
        return true;
    }

    return false;
}

bool parse_term_color(TermColor *color, char **strs)
{
    color->fg = -1;
    color->bg = -1;
    color->attr = 0;
    for (size_t i = 0, count = 0; strs[i]; i++) {
        const char *str = strs[i];
        int val;
        if (parse_color(str, &val)) {
            if (count > 1) {
                if (val == -2) {
                    // "keep" is also a valid attribute
                    color->attr |= ATTR_KEEP;
                } else {
                    error_msg("too many colors");
                    return false;
                }
            } else {
                if (!count) {
                    color->fg = val;
                } else {
                    color->bg = val;
                }
                count++;
            }
        } else if (!parse_attr(str, &color->attr)) {
            error_msg("invalid color or attribute %s", str);
            return false;
        }
    }
    return true;
}

void collect_hl_colors(const char *prefix)
{
    for (size_t i = 0; i < hl_colors.count; i++) {
        HlColor *c = hl_colors.ptrs[i];
        if (str_has_prefix(c->name, prefix)) {
            add_completion(xstrdup(c->name));
        }
    }
}

void collect_colors_and_attributes(const char *prefix)
{
    for (size_t i = COLORS_MIN_HASH_VALUE; i <= COLORS_MAX_HASH_VALUE; i++) {
        const char *name = color_table[i].name;
        if (name && str_has_prefix(name, prefix)) {
            if (color_table[i].color == -2) {
                continue; // Skip "keep" because it's in attr_table too
            }
            add_completion(xstrdup(name));
        }
    }
    for (size_t i = ATTRS_MIN_HASH_VALUE; i <= ATTRS_MAX_HASH_VALUE; i++) {
        const char *name = attr_table[i].name;
        if (name && str_has_prefix(name, prefix)) {
            add_completion(xstrdup(name));
        }
    }
}
