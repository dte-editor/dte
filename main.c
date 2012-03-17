#include "editor.h"
#include "window.h"
#include "frame.h"
#include "term.h"
#include "config.h"
#include "screen.h"
#include "syntax.h"
#include "alias.h"
#include "obuf.h"
#include "history.h"
#include "file-history.h"
#include "search.h"
#include "error.h"

#include <locale.h>
#include <langinfo.h>

static const char *builtin_rc =
// obvious bindings
"bind left left\n"
"bind right right\n"
"bind up up\n"
"bind down down\n"
"bind home bol\n"
"bind end eol\n"
"bind pgup pgup\n"
"bind pgdown pgdown\n"
"bind delete delete\n"
"bind ^\\[ unselect\n"
"bind ^Z suspend\n"
// backspace is either ^? or ^H
"bind ^\\? erase\n"
"bind ^H erase\n"
// there must be a way to get to the command line
"bind ^C command\n"
// these colors are assumed to exist
"hi default\n"
"hi nontext blue keep\n"
"hi wserror default yellow\n"
"hi selection keep gray keep\n"
"hi currentline keep keep keep\n"
"hi linenumber\n"
"hi statusline black gray\n"
"hi commandline\n"
"hi errormsg bold red\n"
"hi infomsg bold blue\n"
"hi tabbar black gray\n"
"hi activetab bold\n"
"hi inactivetab black gray\n"
// must initialize string options
"set statusline-left \" %f%s%m%r%s%M\"\n"
"set statusline-right \" %y,%X   %u   %E %n %t   %p \"\n";

void set_signal_handler(int signum, void (*handler)(int))
{
	struct sigaction act;

	clear(&act);
	sigemptyset(&act.sa_mask);
	act.sa_handler = handler;
	sigaction(signum, &act, NULL);
}

static void handle_sigtstp(int signum)
{
	if (!child_controls_terminal && editor_status != EDITOR_INITIALIZING)
		ui_end();
	kill(0, SIGSTOP);
}

static void handle_sigcont(int signum)
{
	if (!child_controls_terminal && editor_status != EDITOR_INITIALIZING) {
		term_raw();
		resize();
	}
}

static void handle_sigwinch(int signum)
{
	resized = 1;
}

static void close_all_views(void)
{
	int i, j;

	for (i = 0; i < windows.count; i++) {
		struct window *w = WINDOW(i);
		for (j = 0; j < w->views.count; j++)
			view_delete(VIEW(i, j));
		w->views.count = 0;
	}
}

static const char *opt_arg(const char *opt, const char *arg)
{
	if (arg == NULL) {
		fprintf(stderr, "missing argument for option %s\n", opt);
		exit(1);
	}
	return arg;
}

int main(int argc, char *argv[])
{
	const char *home = getenv("HOME");
	const char *tag = NULL;
	const char *rc = NULL;
	const char *command = NULL;
	int use_terminfo = 1;
	int use_termcap = 1;
	int i, read_rc = 1;

	if (!home)
		home = "";
	home_dir = xstrdup(home);

	for (i = 1; i < argc; i++) {
		const char *opt = argv[i];

		if (opt[0] != '-' || !opt[1])
			break;
		if (!opt[2]) {
			switch (opt[1]) {
			case 'C':
				use_termcap = 0;
				continue;
			case 'I':
				use_terminfo = 0;
				continue;
			case 'R':
				read_rc = 0;
				continue;
			case 't':
				tag = opt_arg(opt, argv[++i]);
				continue;
			case 'r':
				rc = opt_arg(opt, argv[++i]);
				continue;
			case 'c':
				command = opt_arg(opt, argv[++i]);
				continue;
			case 'V':
				printf("%s %s\nWritten by Timo Hirvonen\n", program, version);
				return 0;
			}
			if (opt[1] == '-') {
				i++;
				break;
			}
		}
		printf("Usage: %s [-R] [-V] [-c command] [-t tag] [-r rcfile] [file]...\n", argv[0]);
		return 1;
	}

	// create this early. needed if lock-files is true
	mkdir(editor_file(""), 0755);

	setlocale(LC_CTYPE, "");
	charset = nl_langinfo(CODESET);
	if (strcmp(charset, "UTF-8") == 0)
		term_utf8 = 1;

	if (term_init(use_terminfo, use_termcap))
		error_msg("No terminal entry found.");

	exec_config(commands, builtin_rc, strlen(builtin_rc));
	config_line = 0;
	set_basic_colors();

	root_frame = new_frame();
	window = window_new();
	window->frame = root_frame;
	root_frame->window = window;

	if (read_rc) {
		if (rc) {
			read_config(commands, rc, 1);
		} else if (read_config(commands, editor_file("rc"), 0)) {
			read_config(commands, ssprintf("%s/rc", pkgdatadir), 1);
		}
	}

	update_all_syntax_colors();
	sort_aliases();

	/* Terminal does not generate signals for control keys. */
	set_signal_handler(SIGINT, SIG_IGN);
	set_signal_handler(SIGQUIT, SIG_IGN);
	set_signal_handler(SIGPIPE, SIG_IGN);

	/* Terminal does not generate signal for ^Z but someone can send
	 * us SIGSTOP or SIGTSTP nevertheless.
	 */
	set_signal_handler(SIGTSTP, handle_sigtstp);

	set_signal_handler(SIGCONT, handle_sigcont);
	set_signal_handler(SIGWINCH, handle_sigwinch);

	obuf.alloc = 8192;
	obuf.buf = xmalloc(obuf.alloc);

	load_file_history();
	history_load(&command_history, editor_file("command-history"), command_history_size);
	history_load(&search_history, editor_file("search-history"), search_history_size);
	if (search_history.count)
		search_set_regexp(search_history.ptrs[search_history.count - 1]);

	/* Initialize terminal but don't update screen yet.  Also display
	 * "Press any key to continue" prompt if there were any errors
	 * during reading configuration files.
	 */
	term_raw();
	if (nr_errors) {
		any_key();
		clear_error();
	}

	editor_status = EDITOR_RUNNING;

	for (; i < argc; i++)
		open_buffer(argv[i], 0, NULL);
	if (window->views.count == 0)
		open_empty_buffer();
	set_view(window->views.ptrs[0]);

	if (command || tag)
		resize();

	if (command)
		handle_command(commands, command);
	if (tag) {
		const char *ptrs[3] = { "tag", tag, NULL };
		struct ptr_array array = { (void **)ptrs, 3, 3 };
		run_commands(commands, &array);
	}
	resize();
	main_loop();
	ui_end();
	history_save(&command_history, editor_file("command-history"));
	history_save(&search_history, editor_file("search-history"));
	close_all_views();
	save_file_history();
	return 0;
}
