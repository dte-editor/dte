#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#include "error.h"
#include "syntax/merge.h"
#include "util/str-util.h"
#include "util/xmalloc.h"
#include "util/xsnprintf.h"

static HashMap syntaxes = HASHMAP_INIT;

StringList *find_string_list(const Syntax *syn, const char *name)
{
    return hashmap_get(&syn->string_lists, name);
}

State *find_state(const Syntax *syn, const char *name)
{
    return hashmap_get(&syn->states, name);
}

Syntax *find_any_syntax(const char *name)
{
    return hashmap_get(&syntaxes, name);
}

static void visit(State *s)
{
    if (s->visited) {
        return;
    }
    s->visited = true;
    for (size_t i = 0, n = s->conds.count; i < n; i++) {
        const Condition *cond = s->conds.ptrs[i];
        if (cond->a.destination) {
            visit(cond->a.destination);
        }
    }
    if (s->default_action.destination) {
        visit(s->default_action.destination);
    }
}

static void free_condition(Condition *cond)
{
    free(cond->a.emit_name);
    free(cond);
}

static void free_state(State *s)
{
    free(s->emit_name);
    ptr_array_free_cb(&s->conds, FREE_FUNC(free_condition));
    free(s->default_action.emit_name);
    free(s);
}

static void free_string_list(StringList *list)
{
    hashset_free(&list->strings);
    free(list);
}

static void free_syntax_contents(Syntax *syn)
{
    hashmap_free(&syn->states, FREE_FUNC(free_state));
    hashmap_free(&syn->string_lists, FREE_FUNC(free_string_list));
    hashmap_free(&syn->default_colors, NULL);
}

static void free_syntax(Syntax *syn)
{
    free_syntax_contents(syn);
    free(syn->name);
    free(syn);
}

static void free_syntax_cb(Syntax *syn)
{
    free_syntax_contents(syn);
    free(syn);
}

// This function is only called by the test binary, just to ensure
// the various free_*() functions get exercised by ASan/UBSan
void free_syntaxes(void)
{
    hashmap_free(&syntaxes, FREE_FUNC(free_syntax_cb));
}

void finalize_syntax(Syntax *syn, unsigned int saved_nr_errors)
{
    if (syn->states.count == 0) {
        error_msg("Empty syntax");
    }

    for (HashMapIter it = hashmap_iter(&syn->states); hashmap_next(&it); ) {
        const State *s = it.entry->value;
        if (!s->defined) {
            // This state has been referenced but not defined
            error_msg("No such state %s", it.entry->key);
        }
    }
    for (HashMapIter it = hashmap_iter(&syn->string_lists); hashmap_next(&it); ) {
        const StringList *list = it.entry->value;
        if (!list->defined) {
            error_msg("No such list %s", it.entry->key);
        }
    }

    if (syn->heredoc && !is_subsyntax(syn)) {
        error_msg("heredocend can be used only in subsyntaxes");
    }

    if (find_any_syntax(syn->name)) {
        error_msg("Syntax %s already exists", syn->name);
    }

    if (get_nr_errors() != saved_nr_errors) {
        free_syntax(syn);
        return;
    }

    // Unused states and lists cause warning only
    visit(syn->start_state);
    for (HashMapIter it = hashmap_iter(&syn->states); hashmap_next(&it); ) {
        const State *s = it.entry->value;
        if (!s->visited && !s->copied) {
            error_msg("State %s is unreachable", it.entry->key);
        }
    }
    for (HashMapIter it = hashmap_iter(&syn->string_lists); hashmap_next(&it); ) {
        const StringList *list = it.entry->value;
        if (!list->used) {
            error_msg("List %s never used", it.entry->key);
        }
    }

    hashmap_insert(&syntaxes, syn->name, syn);
}

Syntax *find_syntax(const char *name)
{
    Syntax *syn = find_any_syntax(name);
    if (syn && is_subsyntax(syn)) {
        return NULL;
    }
    return syn;
}

static const char *find_default_color(const Syntax *syn, const char *name)
{
    return hashmap_get(&syn->default_colors, name);
}

static void update_action_color(const Syntax *syn, Action *a)
{
    const char *name = a->emit_name;
    if (!name) {
        name = a->destination->emit_name;
    }

    char full[256];
    xsnprintf(full, sizeof full, "%s.%s", syn->name, name);
    a->emit_color = find_color(full);
    if (a->emit_color) {
        return;
    }

    const char *def = find_default_color(syn, name);
    if (!def) {
        return;
    }

    xsnprintf(full, sizeof full, "%s.%s", syn->name, def);
    a->emit_color = find_color(full);
}

void update_state_colors(const Syntax *syn, State *s)
{
    for (size_t i = 0, n = s->conds.count; i < n; i++) {
        Condition *c = s->conds.ptrs[i];
        update_action_color(syn, &c->a);
    }
    update_action_color(syn, &s->default_action);
}

void update_syntax_colors(Syntax *syn)
{
    if (is_subsyntax(syn)) {
        // No point to update colors of a sub-syntax
        return;
    }
    for (HashMapIter it = hashmap_iter(&syn->states); hashmap_next(&it); ) {
        update_state_colors(syn, it.entry->value);
    }
}

void update_all_syntax_colors(void)
{
    for (HashMapIter it = hashmap_iter(&syntaxes); hashmap_next(&it); ) {
        update_syntax_colors(it.entry->value);
    }
}

void find_unused_subsyntaxes(void)
{
    for (HashMapIter it = hashmap_iter(&syntaxes); hashmap_next(&it); ) {
        Syntax *s = it.entry->value;
        if (!s->used && !s->warned_unused_subsyntax && is_subsyntax(s)) {
            error_msg("Subsyntax %s is unused", s->name);
            // Don't complain multiple times about the same unused subsyntaxes
            s->warned_unused_subsyntax = true;
        }
    }
}
