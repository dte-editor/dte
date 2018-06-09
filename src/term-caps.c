#include "term.h"
#include "common.h"
#include "lookup/xterm-keys.c"

#define ANSI_ATTRS (ATTR_UNDERLINE | ATTR_REVERSE | ATTR_BLINK | ATTR_BOLD)

#define KEY(c, k) { \
    .code = (c), \
    .code_length = (sizeof(c) - 1), \
    .key = (k) \
}

#define XKEYS(p, key) \
    KEY(p,     key | MOD_SHIFT), \
    KEY(p "3", key | MOD_META), \
    KEY(p "4", key | MOD_SHIFT | MOD_META), \
    KEY(p "5", key | MOD_CTRL), \
    KEY(p "6", key | MOD_SHIFT | MOD_CTRL), \
    KEY(p "7", key | MOD_META | MOD_CTRL), \
    KEY(p "8", key | MOD_SHIFT | MOD_META | MOD_CTRL)

static ssize_t parse_key_sequence_from_keymap(const char *buf, size_t fill, Key *key)
{
    bool possibly_truncated = false;
    for (size_t i = 0; i < terminal.keymap_length; i++) {
        const TermKeyMap *const km = &terminal.keymap[i];
        const char *const keycode = km->code;
        const size_t len = km->code_length;
        BUG_ON(keycode == NULL);
        BUG_ON(len == 0);
        if (len > fill) {
            // This might be a truncated escape sequence
            if (
                possibly_truncated == false
                && !memcmp(keycode, buf, fill)
            ) {
                possibly_truncated = true;
            }
            continue;
        }
        if (memcmp(keycode, buf, len)) {
            continue;
        }
        *key = km->key;
        return len;
    }
    return possibly_truncated ? -1 : 0;
}

#ifndef TERMINFO_DISABLE

// These are normally declared in the <curses.h> and <term.h> headers.
// They are not included here because of the insane number of unprefixed
// symbols they declare and because of previous bugs caused by using them.
int setupterm(const char *term, int filedes, int *errret);
int tigetflag(const char *capname);
int tigetnum(const char *capname);
char *tigetstr(const char *capname);

static char *curses_str_cap(const char *const name)
{
    char *str = tigetstr(name);
    if (str == (char *)-1) {
        // Not a string cap (bug?)
        return NULL;
    }
    // NULL = canceled or absent
    return str;
}

// See terminfo(5)
static void term_read_caps(void)
{
    terminal.parse_key_sequence = &parse_key_sequence_from_keymap;
    terminal.back_color_erase = tigetflag("bce");
    terminal.max_colors = tigetnum("colors");
    terminal.width = tigetnum("cols");
    terminal.height = tigetnum("lines");

    terminal.attributes =
        (xstreq(curses_str_cap("smul"), "\033[4m") ? ATTR_UNDERLINE : 0)
        + (xstreq(curses_str_cap("rev"), "\033[7m") ? ATTR_REVERSE : 0)
        + (xstreq(curses_str_cap("blink"), "\033[5m") ? ATTR_BLINK : 0)
        + (xstreq(curses_str_cap("dim"), "\033[2m") ? ATTR_DIM : 0)
        + (xstreq(curses_str_cap("bold"), "\033[1m") ? ATTR_BOLD : 0)
        + (xstreq(curses_str_cap("invis"), "\033[8m") ? ATTR_INVIS : 0)
        + (xstreq(curses_str_cap("sitm"), "\033[3m") ? ATTR_ITALIC : 0)
    ;

    const int ncv = tigetnum("ncv");
    if (ncv <= 0) {
        terminal.ncv_attributes = 0;
    } else {
        terminal.ncv_attributes = (unsigned short)ncv;
        // The ATTR_* bitflag values used in this codebase are mostly
        // the same as those used by terminfo, with the exception of
        // ITALIC, which is bit 7 here, but bit 15 in terminfo. It
        // must therefore be manually converted.
        if ((ncv & (1 << 15)) && !(ncv & ATTR_ITALIC)) {
            terminal.ncv_attributes |= ATTR_ITALIC;
        }
    }

    static TermControlCodes tcc;
    tcc = (TermControlCodes) {
        .clear_to_eol = curses_str_cap("el"),
        .keypad_off = curses_str_cap("rmkx"),
        .keypad_on = curses_str_cap("smkx"),
        .cup_mode_off = curses_str_cap("rmcup"),
        .cup_mode_on = curses_str_cap("smcup"),
        .show_cursor = curses_str_cap("cnorm"),
        .hide_cursor = curses_str_cap("civis")
    };
    terminal.control_codes = &tcc;

    static TermKeyMap keymap[] = {
        KEY("kcuu1", KEY_UP),
        KEY("kcud1", KEY_DOWN),
        KEY("kcub1", KEY_LEFT),
        KEY("kcuf1", KEY_RIGHT),
        KEY("kdch1", KEY_DELETE),
        KEY("kpp", KEY_PAGE_UP),
        KEY("knp", KEY_PAGE_DOWN),
        KEY("khome", KEY_HOME),
        KEY("kend", KEY_END),
        KEY("kich1", KEY_INSERT),
        KEY("kf1", KEY_F1),
        KEY("kf2", KEY_F2),
        KEY("kf3", KEY_F3),
        KEY("kf4", KEY_F4),
        KEY("kf5", KEY_F5),
        KEY("kf6", KEY_F6),
        KEY("kf7", KEY_F7),
        KEY("kf8", KEY_F8),
        KEY("kf9", KEY_F9),
        KEY("kf10", KEY_F10),
        KEY("kf11", KEY_F11),
        KEY("kf12", KEY_F12),

        XKEYS("kUP", KEY_UP),
        XKEYS("kDN", KEY_DOWN),
        XKEYS("kLFT", KEY_LEFT),
        XKEYS("kRIT", KEY_RIGHT),
        XKEYS("kDC", KEY_DELETE),
        XKEYS("kPRV", KEY_PAGE_UP),
        XKEYS("kNXT", KEY_PAGE_DOWN),
        XKEYS("kHOM", KEY_HOME),
        XKEYS("kEND", KEY_END),
    };

    static_assert(ARRAY_COUNT(keymap) == 22 + (9 * 7));

    size_t n = 0;
    for (size_t i = 0; i < ARRAY_COUNT(keymap); i++) {
        const char *const code = curses_str_cap(keymap[i].code);
        if (code && code[0] != '\0') {
            keymap[n].code = code;
            keymap[n].code_length = strlen(code);
            keymap[n].key = keymap[i].key;
            n++;
        }
    }

    terminal.keymap = keymap;
    terminal.keymap_length = n;
}

static void term_init_fallback(const char *const term)
{
    // Initialize terminfo database (or call exit(3) on failure)
    setupterm(term, 1, (int*)0);

    term_read_caps();
}

#else

static const TermKeyMap ansi_keymap[] = {
    KEY("\033[A", KEY_UP),
    KEY("\033[B", KEY_DOWN),
    KEY("\033[C", KEY_RIGHT),
    KEY("\033[D", KEY_LEFT),
};

static const TermControlCodes ansi_control_codes = {
    .clear_to_eol = "\033[K"
};

static const TerminalInfo terminal_ansi = {
    .max_colors = 8,
    .width = 80,
    .height = 24,
    .attributes = ANSI_ATTRS,
    .ncv_attributes = ATTR_UNDERLINE,
    .keymap = ansi_keymap,
    .keymap_length = ARRAY_COUNT(ansi_keymap),
    .control_codes = &ansi_control_codes,
    .parse_key_sequence = &parse_key_sequence_from_keymap
};

static void term_init_fallback(const char *const UNUSED(term))
{
    terminal = terminal_ansi;
}

#endif // ifndef TERMINFO_DISABLE

static const TermKeyMap rxvt_keymap[] = {
    KEY("\033[2~", KEY_INSERT),
    KEY("\033[3~", KEY_DELETE),
    KEY("\033[7~", KEY_HOME),
    KEY("\033[8~", KEY_END),
    KEY("\033[5~", KEY_PAGE_UP),
    KEY("\033[6~", KEY_PAGE_DOWN),
    KEY("\033[D", KEY_LEFT),
    KEY("\033[C", KEY_RIGHT),
    KEY("\033[A", KEY_UP),
    KEY("\033[B", KEY_DOWN),
    KEY("\033[11~", KEY_F1),
    KEY("\033[12~", KEY_F2),
    KEY("\033[13~", KEY_F3),
    KEY("\033[14~", KEY_F4),
    KEY("\033[15~", KEY_F5),
    KEY("\033[17~", KEY_F6),
    KEY("\033[18~", KEY_F7),
    KEY("\033[19~", KEY_F8),
    KEY("\033[20~", KEY_F9),
    KEY("\033[21~", KEY_F10),
    KEY("\033[23~", KEY_F11),
    KEY("\033[24~", KEY_F12),
    KEY("\033[7$", MOD_SHIFT | KEY_HOME),
    KEY("\033[8$", MOD_SHIFT | KEY_END),
    KEY("\033[d", MOD_SHIFT | KEY_LEFT),
    KEY("\033[c", MOD_SHIFT | KEY_RIGHT),
    KEY("\033[a", MOD_SHIFT | KEY_UP),
    KEY("\033[b", MOD_SHIFT | KEY_DOWN),
    KEY("\033Od", MOD_CTRL | KEY_LEFT),
    KEY("\033Oc", MOD_CTRL | KEY_RIGHT),
    KEY("\033Oa", MOD_CTRL | KEY_UP),
    KEY("\033Ob", MOD_CTRL | KEY_DOWN),
    KEY("\033\033[D", MOD_META | KEY_LEFT),
    KEY("\033\033[C", MOD_META | KEY_RIGHT),
    KEY("\033\033[A", MOD_META | KEY_UP),
    KEY("\033\033[B", MOD_META | KEY_DOWN),
    KEY("\033\033[d", MOD_META | MOD_SHIFT | KEY_LEFT),
    KEY("\033\033[c", MOD_META | MOD_SHIFT | KEY_RIGHT),
    KEY("\033\033[a", MOD_META | MOD_SHIFT | KEY_UP),
    KEY("\033\033[b", MOD_META | MOD_SHIFT | KEY_DOWN),
};

static const TermControlCodes xterm_control_codes = {
    .clear_to_eol = "\033[K",
    .keypad_off = "\033[?1l\033>",
    .keypad_on = "\033[?1h\033=",
    .cup_mode_off = "\033[?1049l",
    .cup_mode_on = "\033[?1049h",
    .hide_cursor = "\033[?25l",
    .show_cursor = "\033[?25h",
    .save_title = "\033[22;2t",
    .restore_title = "\033[23;2t",
    .set_title_begin = "\033]2;",
    .set_title_end = "\007",
};

static const TermControlCodes rxvt_control_codes = {
    .clear_to_eol = "\033[K",
    .keypad_off = "\033>",
    .keypad_on = "\033=",
    .cup_mode_off = "\033[2J\033[?47l\0338",
    .cup_mode_on = "\0337\033[?47h",
    .hide_cursor = "\033[?25l",
    .show_cursor = "\033[?25h",
    .save_title = "\033[22;2t",
    .restore_title = "\033[23;2t",
    .set_title_begin = "\033]2;",
    .set_title_end = "\007",
};

static const TerminalInfo terminal_xterm = {
    .back_color_erase = false,
    .max_colors = 8,
    .width = 80,
    .height = 24,
    .attributes = ANSI_ATTRS | ATTR_INVIS | ATTR_DIM | ATTR_ITALIC,
    .keymap = NULL,
    .keymap_length = 0,
    .control_codes = &xterm_control_codes,
    .parse_key_sequence = &parse_xterm_key_sequence
};

static const TerminalInfo terminal_st = {
    .back_color_erase = true,
    .max_colors = 8,
    .width = 80,
    .height = 24,
    .attributes = ANSI_ATTRS | ATTR_INVIS | ATTR_DIM | ATTR_ITALIC,
    .keymap = NULL,
    .keymap_length = 0,
    .control_codes = &xterm_control_codes,
    .parse_key_sequence = &parse_xterm_key_sequence
};

static const TerminalInfo terminal_rxvt = {
    .back_color_erase = true,
    .max_colors = 8,
    .width = 80,
    .height = 24,
    .attributes = ANSI_ATTRS,
    .keymap = rxvt_keymap,
    .keymap_length = ARRAY_COUNT(rxvt_keymap),
    .control_codes = &rxvt_control_codes,
    .parse_key_sequence = &parse_key_sequence_from_keymap
};

static bool term_match(const char *term, const char *prefix)
{
    if (str_has_prefix(term, prefix) == false) {
        return false;
    }
    switch (term[strlen(prefix)]) {
    // Exact match
    case '\0':
    // Prefix match
    case '-':
    case '+':
        return true;
    }
    return false;
}

void term_init(void)
{
    const char *const term = getenv("TERM");
    if (term == NULL || term[0] == '\0') {
        fputs("'TERM' not set\n", stderr);
        exit(1);
    }

    if (getenv("DTE_FORCE_TERMINFO")) {
#ifndef TERMINFO_DISABLE
        term_init_fallback(term);
        return;
#else
        fputs("'DTE_FORCE_TERMINFO' set but terminfo not linked\n", stderr);
        exit(1);
#endif
    }

    if (
        term_match(term, "tmux")
        || term_match(term, "xterm")
        || term_match(term, "screen")
    ) {
        terminal = terminal_xterm;
    } else if (term_match(term, "st") || term_match(term, "stterm")) {
        terminal = terminal_st;
    } else if (term_match(term, "rxvt")) {
        terminal = terminal_rxvt;
    } else {
        term_init_fallback(term);
    }

    if (str_has_suffix(term, "256color")) {
        terminal.max_colors = 256;
    }
}
