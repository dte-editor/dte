#ifndef SPAWN_H
#define SPAWN_H

#include "ptr-array.h"

/* Errors are read from stderr by default. */
#define SPAWN_READ_STDOUT	(1 << 0)

/* Internal to spawn_compile(). */
#define SPAWN_PRINT_ERRORS	(1 << 1)

/* Redirect to /dev/null? */
#define SPAWN_REDIRECT_STDOUT	(1 << 2)
#define SPAWN_REDIRECT_STDERR	(1 << 3)

/* Press any key to continue */
#define SPAWN_PROMPT		(1 << 4)

struct compile_error {
	char *file;
	char *msg;
	int line;
	int column;
};

struct error_format {
	int ignore;
	signed char msg_idx;
	signed char file_idx;
	signed char line_idx;
	signed char column_idx;
	const char *pattern;
};

struct compiler {
	char *name;
	struct ptr_array error_formats;
};

struct filter_data {
	char *in;
	char *out;
	unsigned int in_len;
	unsigned int out_len;
};

void add_error_fmt(const char *compiler, int ignore, const char *format, char **desc);
struct compiler *find_compiler(const char *name);
int spawn_filter(char **argv, struct filter_data *data);
void spawn_compiler(char **args, unsigned int flags, struct compiler *c);
void spawn(char **args, int fd[3], int prompt);

#endif
