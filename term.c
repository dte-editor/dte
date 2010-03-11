#include "term.h"
#include "common.h"
#include "editor.h"
#include "options.h"

#include <sys/ioctl.h>
#include <termios.h>

char *term_keycodes[NR_SKEYS];
struct term_cap term_cap;
unsigned int term_flags;

static struct termios termios_save;
static char buffer[64];
static int buffer_pos;

static void buffer_num(unsigned int num)
{
	char stack[32];
	int ret, pos = 0;

	do {
		stack[pos++] = (num % 10) + '0';
		num /= 10;
	} while (num);
	ret = pos;
	do {
		buffer[buffer_pos++] = stack[--pos];
	} while (pos);
}

static int load_terminfo_caps(const char *path, const char *term)
{
	char filename[512];

	snprintf(filename, sizeof(filename), "%s/%c/%s", path, term[0], term);
	return terminfo_get_caps(filename);
}

int term_init(const char *term, unsigned int flags)
{
	int rc;

	term_flags = flags;

	if (term == NULL)
		term = getenv("TERM");
	if (term == NULL || term[0] == 0)
		term = "linux";

	rc = -2;
	if (flags & TERM_USE_TERMINFO) {
		const char *path = getenv("TERMINFO");

		if (path == NULL || path[0] == 0) {
			char buf[1024];
			snprintf(buf, sizeof(buf), "%s/.terminfo", home_dir);
			rc = load_terminfo_caps(buf, term);
			if (rc)
				rc = load_terminfo_caps("/usr/share/terminfo", term);
		} else {
			rc = load_terminfo_caps(path, term);
		}
	}

	if (rc && flags & TERM_USE_TERMCAP) {
		rc = termcap_get_caps("/etc/termcap", term);
	}
	return rc;
}

void term_raw(void)
{
	/* see termios(3) */
	struct termios termios;

	tcgetattr(0, &termios);
	termios_save = termios;

	/* disable buffering
	 * disable echo
	 * disable generation of signals (free some control keys)
	 */
	termios.c_lflag &= ~(ICANON | ECHO | ISIG);

	/* disable CR to NL conversion (differentiate ^J from enter)
	 * disable flow control (free ^Q and ^S)
	 */
	termios.c_iflag &= ~(ICRNL | IXON | IXOFF);

	/* read at least 1 char on each read() */
	termios.c_cc[VMIN] = 1;

	/* read blocks until there are MIN(VMIN, requested) bytes available */
	termios.c_cc[VTIME] = 0;

	tcsetattr(0, 0, &termios);
}

void term_cooked(void)
{
	tcsetattr(0, 0, &termios_save);
}

static char input_buf[256];
static int input_buf_fill;
static int input_can_be_truncated;

static void consume_input(int len)
{
	input_buf_fill -= len;
	if (input_buf_fill) {
		memmove(input_buf, input_buf + len, input_buf_fill);

		/* keys are sent faster than we can read */
		input_can_be_truncated = 1;
	}
}

static int fill_buffer(void)
{
	int rc;

	if (input_buf_fill == sizeof(input_buf))
		return 0;

	if (!input_buf_fill)
		input_can_be_truncated = 0;

	rc = read(0, input_buf + input_buf_fill, sizeof(input_buf) - input_buf_fill);
	if (rc <= 0)
		return 0;
	input_buf_fill += rc;
	return 1;
}

static int fill_buffer_timeout(void)
{
	struct timeval tv = {
		.tv_sec = options.esc_timeout / 1000,
		.tv_usec = (options.esc_timeout % 1000) * 1000
	};
	fd_set set;
	int rc;

	FD_ZERO(&set);
	FD_SET(0, &set);
	rc = select(1, &set, NULL, NULL, &tv);
	if (rc > 0 && fill_buffer())
		return 1;
	return 0;
}

static int input_get_byte(unsigned char *ch)
{
	if (!input_buf_fill && !fill_buffer())
		return 0;
	*ch = input_buf[0];
	consume_input(1);
	return 1;
}

static int read_special(unsigned int *key, enum term_key_type *type)
{
	static const struct {
		enum term_key_type type;
		unsigned int key;
		const char *code;
	} builtin[] = {
		{ KEY_SPECIAL, SKEY_LEFT,	"\033[D" },
		{ KEY_SPECIAL, SKEY_RIGHT,	"\033[C" },
		{ KEY_SPECIAL, SKEY_UP,		"\033[A" },
		{ KEY_SPECIAL, SKEY_DOWN,	"\033[B" },
		/* keypad */
		{ KEY_SPECIAL, SKEY_HOME,	"\033[1~" },
		{ KEY_SPECIAL, SKEY_END,	"\033[4~" },
		{ KEY_NORMAL, '/',		"\033Oo" },
		{ KEY_NORMAL, '*',		"\033Oj" },
		{ KEY_NORMAL, '-',		"\033Om" },
		{ KEY_NORMAL, '+',		"\033Ok" },
		{ KEY_NORMAL, '\r',		"\033OM" },
	};
	int possibly_truncated = 0;
	int i;

	for (i = 0; i < NR_SKEYS; i++) {
		int len;

		if (!term_keycodes[i])
			continue;

		len = strlen(term_keycodes[i]);
		if (len > input_buf_fill) {
			/* this might be a truncated escape sequence */
			if (!strncmp(term_keycodes[i], input_buf, len))
				possibly_truncated = 1;
			continue;
		}
		if (strncmp(term_keycodes[i], input_buf, len))
			continue;
		*key = i;
		*type = KEY_SPECIAL;
		consume_input(len);
		return 1;
	}
	for (i = 0; i < ARRAY_COUNT(builtin); i++) {
		int len = strlen(builtin[i].code);

		if (len > input_buf_fill) {
			/* this might be a truncated escape sequence */
			if (!strncmp(builtin[i].code, input_buf, len))
				possibly_truncated = 1;
			continue;
		}
		if (strncmp(builtin[i].code, input_buf, len))
			continue;
		*key = builtin[i].key;
		*type = builtin[i].type;
		consume_input(len);
		return 1;
	}

	if (possibly_truncated && input_can_be_truncated && fill_buffer())
		return read_special(key, type);
	return 0;
}

static int read_simple(unsigned int *key, enum term_key_type *type)
{
	unsigned char ch;

	/* > 0 bytes in buf */
	input_get_byte(&ch);

	if (ch == 0x7f || ch == 0x08) {
		*key = SKEY_BACKSPACE;
		*type = KEY_SPECIAL;
		return 1;
	}

	/* normal key */
	if ((term_flags & TERM_UTF8) && ch > 0x7f) {
		/*
		 * 10xx xxxx invalid
		 * 110x xxxx valid
		 * 1110 xxxx valid
		 * 1111 0xxx valid
		 * 1111 1xxx invalid
		 */
		unsigned int u, bit = 1 << 6;
		int count = 0;

		while (ch & bit) {
			bit >>= 1;
			count++;
		}
		if (count == 0 || count > 3) {
			/* invalid first byte */
			return 0;
		}
		u = ch & (bit - 1);
		do {
			if (!input_get_byte(&ch))
				return 0;
			if (ch >> 6 != 2)
				return 0;
			u = (u << 6) | (ch & 0x3f);
		} while (--count);
		*key = u;
	} else {
		*key = ch;
	}
	*type = KEY_NORMAL;
	return 1;
}

int term_read_key(unsigned int *key, enum term_key_type *type)
{
	if (!input_buf_fill && !fill_buffer())
		return 0;

	if (input_buf_fill > 4 && !memchr(input_buf, '\033', input_buf_fill)) {
		*key = 0;
		*type = KEY_PASTE;
		return 1;
	}

	if (input_buf[0] == '\033') {
		if (input_buf_fill > 1 || input_can_be_truncated) {
			if (read_special(key, type))
				return 1;
		}
		if (input_buf_fill == 1) {
			/* sometimes alt-key gets split into two reads */
			fill_buffer_timeout();

			if (input_buf_fill > 1 && input_buf[1] == '\033') {
				/*
				 * Double-esc (+ maybe some other characters)
				 *
				 * Treat the first esc as a single key to make
				 * things like arrow keys work immediately after
				 * leaving (esc) the command line.
				 *
				 * Special key can't start with double-esc so this
				 * should be safe.
				 *
				 * This breaks the esc-key == alt-key rule for the
				 * esc-esc case but it shouldn't matter.
				 */
				return read_simple(key, type);
			}
		}
		if (input_buf_fill > 1) {
			if (input_buf_fill == 2 || input_buf[2] == '\033') {
				/* 'esc key' or 'alt-key' */
				*key = input_buf[1];
				*type = KEY_META;
				consume_input(2);
				return 1;
			}
			/* unknown escape sequence, avoid inserting it */
			input_buf_fill = 0;
			return 0;
		}
	}
	return read_simple(key, type);
}

char *term_read_paste(unsigned int *size)
{
	unsigned int alloc = ROUND_UP(input_buf_fill, 1024);
	unsigned int count = 0;
	unsigned int i;
	char *buf = xmalloc(alloc);

	if (input_buf_fill) {
		memcpy(buf, input_buf, input_buf_fill);
		count = input_buf_fill;
		input_buf_fill = 0;
	}
	while (1) {
		struct timeval tv = {
			.tv_sec = 0,
			.tv_usec = 100 * 1000
		};
		fd_set set;
		int rc;

		FD_ZERO(&set);
		FD_SET(0, &set);
		rc = select(1, &set, NULL, NULL, &tv);
		if (rc < 0 && errno == EINTR)
			continue;
		if (rc <= 0)
			break;

		if (alloc - count < 256) {
			alloc *= 2;
			xrenew(buf, alloc);
		}
		do {
			rc = read(0, buf + count, alloc - count);
		} while (rc < 0 && errno == EINTR);
		if (rc <= 0)
			break;
		count += rc;
	}
	for (i = 0; i < count; i++) {
		if (buf[i] == '\r')
			buf[i] = '\n';
	}
	*size = count;
	return buf;
}

int term_get_size(int *w, int *h)
{
	struct winsize ws;

	if (ioctl(0, TIOCGWINSZ, &ws) != -1) {
		*w = ws.ws_col;
		*h = ws.ws_row;
		return 0;
	}
	return -1;
}

static void buffer_color(char x, unsigned char color)
{
	buffer[buffer_pos++] = ';';
	buffer[buffer_pos++] = x;
	if (color < 8) {
		buffer[buffer_pos++] = '0' + color;
	} else {
		buffer[buffer_pos++] = '8';
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '5';
		buffer[buffer_pos++] = ';';
		buffer_num(color);
	}
}

const char *term_set_color(const struct term_color *color)
{
	struct term_color c = *color;

	if (term_cap.colors <= 16 && c.fg > 7) {
		c.attr |= ATTR_BOLD;
		c.fg &= 7;
	}

	// max 35 bytes (3 + 6 * 2 + 2 * 9 + 2)
	buffer_pos = 0;
	buffer[buffer_pos++] = '\033';
	buffer[buffer_pos++] = '[';
	buffer[buffer_pos++] = '0';

	if (c.attr & ATTR_BOLD) {
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '1';
	}
	if (c.attr & ATTR_LOW_INTENSITY) {
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '2';
	}
	if (c.attr & ATTR_UNDERLINE) {
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '4';
	}
	if (c.attr & ATTR_BLINKING) {
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '5';
	}
	if (c.attr & ATTR_REVERSE_VIDEO) {
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '7';
	}
	if (c.attr & ATTR_INVISIBLE_TEXT) {
		buffer[buffer_pos++] = ';';
		buffer[buffer_pos++] = '8';
	}
	if (c.fg >= 0)
		buffer_color('3', c.fg);
	if (c.bg >= 0)
		buffer_color('4', c.bg);
	buffer[buffer_pos++] = 'm';
	buffer[buffer_pos++] = 0;
	return buffer;
}

const char *term_move_cursor(int x, int y)
{
	if (x < 0 || x >= 999 || y < 0 || y >= 999)
		return "";

	x++;
	y++;
	// max 11 bytes
	buffer_pos = 0;
	buffer[buffer_pos++] = '\033';
	buffer[buffer_pos++] = '[';
	buffer_num(y);
	buffer[buffer_pos++] = ';';
	buffer_num(x);
	buffer[buffer_pos++] = 'H';
	buffer[buffer_pos++] = 0;
	return buffer;
}
