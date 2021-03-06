#ifndef SHOW_H
#define SHOW_H

#include <stdbool.h>
#include "util/macros.h"
#include "util/string.h"

void show(const char *type, const char *key, bool cflag) NONNULL_ARG(1);
void collect_show_subcommands(const char *prefix) NONNULL_ARGS;
void collect_show_subcommand_args(const char *name, const char *arg_prefix) NONNULL_ARGS;
void collect_env(const char *prefix) NONNULL_ARGS;;
void collect_normal_aliases(const char *prefix) NONNULL_ARGS;
String dump_normal_aliases(void);

#endif
