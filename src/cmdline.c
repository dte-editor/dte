#include <stdlib.h>
#include <string.h>
#include "cmdline.h"
#include "completion.h"
#include "editor.h"
#include "history.h"
#include "terminal/input.h"
#include "util/ascii.h"
#include "util/bsearch.h"
#include "util/utf8.h"

static void cmdline_delete(CommandLine *c)
{
    size_t pos = c->pos;
    size_t len = 1;

    if (pos == c->buf.len) {
        return;
    }

    u_get_char(c->buf.buffer, c->buf.len, &pos);
    len = pos - c->pos;
    string_remove(&c->buf, c->pos, len);
}

void cmdline_clear(CommandLine *c)
{
    string_clear(&c->buf);
    c->pos = 0;
    c->search_pos = NULL;
}

static void set_text(CommandLine *c, const char *text)
{
    string_clear(&c->buf);
    const size_t text_len = strlen(text);
    string_append_buf(&c->buf, text, text_len);
    c->pos = text_len;
}

void cmdline_set_text(CommandLine *c, const char *text)
{
    set_text(c, text);
    c->search_pos = NULL;
}

static void cmd_bol(const CommandArgs* UNUSED_ARG(a))
{
    editor.cmdline.pos = 0;
    reset_completion();
}

static void cmd_cancel(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    string_clear(&c->buf);
    c->pos = 0;
    c->search_pos = NULL;
    set_input_mode(INPUT_NORMAL);
    reset_completion();
}

static void cmd_delete(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    cmdline_delete(c);
    c->search_pos = NULL;
    reset_completion();
}

static void cmd_delete_eol(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    c->buf.len = c->pos;
    c->search_pos = NULL;
    reset_completion();
}

static void cmd_delete_word(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    const unsigned char *buf = c->buf.buffer;
    const size_t len = c->buf.len;
    size_t i = c->pos;

    if (i == len) {
        return;
    }

    while (i < len && is_word_byte(buf[i])) {
        i++;
    }

    while (i < len && !is_word_byte(buf[i])) {
        i++;
    }

    string_remove(&c->buf, c->pos, i - c->pos);

    c->search_pos = NULL;
    reset_completion();
}

static void cmd_eol(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    c->pos = c->buf.len;
    reset_completion();
}

static void cmd_erase(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    if (c->pos > 0) {
        u_prev_char(c->buf.buffer, &c->pos);
        cmdline_delete(c);
    }
    c->search_pos = NULL;
    reset_completion();
}

static void cmd_erase_bol(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    string_remove(&c->buf, 0, c->pos);
    c->pos = 0;
    c->search_pos = NULL;
    reset_completion();
}

static void cmd_erase_word(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    size_t i = c->pos;
    if (i == 0) {
        return;
    }

    // open /path/to/file^W => open /path/to/

    // erase whitespace
    while (i && ascii_isspace(c->buf.buffer[i - 1])) {
        i--;
    }

    // erase non-word bytes
    while (i && !is_word_byte(c->buf.buffer[i - 1])) {
        i--;
    }

    // erase word bytes
    while (i && is_word_byte(c->buf.buffer[i - 1])) {
        i--;
    }

    string_remove(&c->buf, i, c->pos - i);
    c->pos = i;
    c->search_pos = NULL;
    reset_completion();
}

static const History *get_history(void)
{
    switch (editor.input_mode) {
    case INPUT_COMMAND:
        return &editor.command_history;
    case INPUT_SEARCH:
        return &editor.search_history;
    case INPUT_NORMAL:
        return NULL;
    }
    BUG("unhandled input mode");
    return NULL;
}

static void cmd_history_prev(const CommandArgs* UNUSED_ARG(a))
{
    const History *hist = get_history();
    if (unlikely(!hist)) {
        return;
    }

    CommandLine *c = &editor.cmdline;
    if (!c->search_pos) {
        free(c->search_text);
        c->search_text = string_clone_cstring(&c->buf);
    }

    if (history_search_forward(hist, &c->search_pos, c->search_text)) {
        set_text(c, c->search_pos->text);
    }

    reset_completion();
}

static void cmd_history_next(const CommandArgs* UNUSED_ARG(a))
{
    const History *hist = get_history();
    if (unlikely(!hist)) {
        return;
    }

    CommandLine *c = &editor.cmdline;
    if (!c->search_pos) {
        goto out;
    }

    if (history_search_backward(hist, &c->search_pos, c->search_text)) {
        set_text(c, c->search_pos->text);
    } else {
        set_text(c, c->search_text);
        c->search_pos = NULL;
    }

out:
    reset_completion();
}

static void cmd_left(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    if (c->pos) {
        u_prev_char(c->buf.buffer, &c->pos);
    }
    reset_completion();
}

static void cmd_right(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    if (c->pos < c->buf.len) {
        u_get_char(c->buf.buffer, c->buf.len, &c->pos);
    }
    reset_completion();
}

static void cmd_word_bwd(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    if (c->pos <= 1) {
        c->pos = 0;
        return;
    }

    const unsigned char *const buf = c->buf.buffer;
    size_t i = c->pos - 1;

    while (i > 0 && !is_word_byte(buf[i])) {
        i--;
    }

    while (i > 0 && is_word_byte(buf[i])) {
        i--;
    }

    if (i > 0) {
        i++;
    }

    c->pos = i;
    reset_completion();
}

static void cmd_word_fwd(const CommandArgs* UNUSED_ARG(a))
{
    CommandLine *c = &editor.cmdline;
    const unsigned char *buf = c->buf.buffer;
    const size_t len = c->buf.len;
    size_t i = c->pos;

    while (i < len && is_word_byte(buf[i])) {
        i++;
    }

    while (i < len && !is_word_byte(buf[i])) {
        i++;
    }

    c->pos = i;
    reset_completion();
}

static const Command cmds[] = {
    {"bol", "", false, 0, 0, cmd_bol},
    {"cancel", "", false, 0, 0, cmd_cancel},
    {"delete", "", false, 0, 0, cmd_delete},
    {"delete-eol", "", false, 0, 0, cmd_delete_eol},
    {"delete-word", "", false, 0, 0, cmd_delete_word},
    {"eol", "", false, 0, 0, cmd_eol},
    {"erase", "", false, 0, 0, cmd_erase},
    {"erase-bol", "", false, 0, 0, cmd_erase_bol},
    {"erase-word", "", false, 0, 0, cmd_erase_word},
    {"history-next", "", false, 0, 0, cmd_history_next},
    {"history-prev", "", false, 0, 0, cmd_history_prev},
    {"left", "", false, 0, 0, cmd_left},
    {"right", "", false, 0, 0, cmd_right},
    {"word-bwd", "", false, 0, 0, cmd_word_bwd},
    {"word-fwd", "", false, 0, 0, cmd_word_fwd},
};

static const Command *find_cmd_mode_command(const char *name)
{
    return BSEARCH(name, cmds, command_cmp);
}

const CommandSet cmd_mode_commands = {
    .lookup = find_cmd_mode_command,
    .allow_recording = NULL,
    .aliases = HASHMAP_INIT,
};
