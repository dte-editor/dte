#ifndef ALIAS_H
#define ALIAS_H

void add_alias(const char *name, const char *value);
void sort_aliases(void);
const char *find_alias(const char *const name);
void collect_aliases(const char *const prefix);

#endif
