#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "editorconfig.h"
#include "ini.h"
#include "match.h"
#include "../debug.h"
#include "../util/ascii.h"
#include "../util/path.h"
#include "../util/string.h"
#include "../util/string-view.h"
#include "../util/strtonum.h"

typedef struct {
    const char *pathname;
    StringView config_file_dir;
    EditorConfigOptions options;
    String pattern;
} UserData;

static void editorconfig_option_set (
    EditorConfigOptions *options,
    const StringView *name,
    const StringView *val
) {
    unsigned int n = 0;
    if (string_view_equal_literal_icase(name, "indent_style")) {
        if (string_view_equal_literal_icase(val, "space")) {
            options->indent_style = INDENT_STYLE_SPACE;
        } else if (string_view_equal_literal_icase(val, "tab")) {
            options->indent_style = INDENT_STYLE_TAB;
        } else {
            options->indent_style = INDENT_STYLE_UNSPECIFIED;
        }
    } else if (string_view_equal_literal_icase(name, "indent_size")) {
        if (string_view_equal_literal_icase(val, "tab")) {
            options->indent_size_is_tab = true;
            options->indent_size = 0;
        } else {
            buf_parse_uint(val->data, val->length, &n);
            // If buf_parse_uint() failed, n is zero, which is deliberately
            // used to "reset" the option due to an invalid value
            options->indent_size = n;
            options->indent_size_is_tab = false;
        }
    } else if (string_view_equal_literal_icase(name, "tab_width")) {
        buf_parse_uint(val->data, val->length, &n);
        options->tab_width = n;
    } else if (string_view_equal_literal_icase(name, "max_line_length")) {
        buf_parse_uint(val->data, val->length, &n);
        options->max_line_length = n;
    }
}

static int ini_handler(const IniData *data, void *ud) {
    UserData *userdata = ud;

    if (data->section.length == 0) {
        if (
            string_view_equal_literal_icase(&data->name, "root")
            && string_view_equal_literal_icase(&data->value, "true")
        ) {
            // root=true, clear all previous values
            userdata->options = editorconfig_options_init();
        }
        return 1;
    }

    if (data->name_idx == 0) {
        // If name_idx is zero, it indicates that the name/value pair is
        // the first in the section and the pattern must be rebuilt
        string_clear(&userdata->pattern);

        // Escape editorconfig special chars in path
        const StringView ecfile_dir = userdata->config_file_dir;
        for (size_t i = 0, n = ecfile_dir.length; i < n; i++) {
            const char ch = ecfile_dir.data[i];
            switch (ch) {
            case '*': case ',': case '-':
            case '?': case '[': case '\\':
            case ']': case '{': case '}':
                string_add_byte(&userdata->pattern, '\\');
                // Fallthrough
            default:
                string_add_byte(&userdata->pattern, ch);
            }
        }

        if (!string_view_memchr(&data->section, '/')) {
            // No slash in pattern, append "**/"
            string_add_literal(&userdata->pattern, "**/");
        } else if (data->section.data[0] != '/') {
            // Pattern contains at least one slash but not at the start, add one
            string_add_byte(&userdata->pattern, '/');
        }

        string_add_string_view(&userdata->pattern, &data->section);
    } else {
        // Otherwise, the section is the same as was passed in the last
        // callback invocation and the previously constructed pattern
        // can be reused
        BUG_ON(userdata->pattern.len == 0);
    }

    const String *pattern = &userdata->pattern;
    if (ec_pattern_match(pattern->buffer, pattern->len, userdata->pathname)) {
        editorconfig_option_set(&userdata->options, &data->name, &data->value);
    }

    return 1;
}

int get_editorconfig_options(const char *pathname, EditorConfigOptions *opts)
{
    BUG_ON(!path_is_absolute(pathname));
    UserData data = {
        .pathname = pathname,
        .config_file_dir = STRING_VIEW_INIT,
        .pattern = STRING_INIT
    };

    static const char ecfilename[16] = "/.editorconfig";
    char buf[8192];
    memcpy(buf, ecfilename, sizeof ecfilename);

    const char *ptr = pathname + 1;
    size_t dir_len = 1;

    // Iterate up directory tree, looking for ".editorconfig" at each level
    while (1) {
        data.config_file_dir = string_view(buf, dir_len);
        int err_num = ini_parse(buf, ini_handler, &data);
        if (err_num > 0) {
            string_free(&data.pattern);
            return err_num;
        }

        const char *const slash = strchr(ptr, '/');
        if (slash == NULL) {
            break;
        }

        dir_len = slash - pathname;
        memcpy(buf, pathname, dir_len);
        memcpy(buf + dir_len, ecfilename, sizeof ecfilename);
        ptr = slash + 1;
    }

    string_free(&data.pattern);

    // Set indent_size to "tab" if indent_size is not specified and
    // indent_style is set to "tab".
    if (
        data.options.indent_size == 0
        && data.options.indent_style == INDENT_STYLE_TAB
    ) {
        data.options.indent_size_is_tab = true;
    }

    // Set indent_size to tab_width if indent_size is "tab" and
    // tab_width is specified.
    if (data.options.indent_size_is_tab && data.options.tab_width > 0) {
        data.options.indent_size = data.options.tab_width;
    }

    // Set tab_width to indent_size if indent_size is specified as
    // something other than "tab" and tab_width is unspecified
    if (
        data.options.indent_size != 0
        && data.options.tab_width == 0
        && !data.options.indent_size_is_tab
    ) {
        data.options.tab_width = data.options.indent_size;
    }

    *opts = data.options;
    return 0;
}
