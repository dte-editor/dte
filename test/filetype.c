#include <string.h>
#include "test.h"
#include "../src/filetype.h"

static void test_find_ft_filename(void)
{
    static const struct ft_filename_test {
        const char *filename, *expected_filetype;
    } tests[] = {
        {"/usr/local/include/lib.h", "c"},
        {"test.cc~", "c"},
        {"test.lua", "lua"},
        {"test.py", "python"},
        {"makefile", "make"},
        {"GNUmakefile", "make"},
        {".file.yml", "yaml"},
        {"/etc/nginx.conf", "nginx"},
        {"file..rb", "ruby"},
        {"file.rb", "ruby"},
        {"/etc/hosts", "config"},
        {"/etc/fstab", "config"},
        {"/boot/grub/menu.lst", "config"},
        {"/etc/krb5.conf", "ini"},
        {"/etc/ld.so.conf", "config"},
        {"/etc/default/grub", "sh"},
        {"/etc/systemd/user.conf", "ini"},
        {"/etc/nginx/mime.types", "nginx"},
        {"", NULL},
        {"/", NULL},
        {"/etc../etc.c.old/c.old", NULL},
        // Ignored extensions
        {"test.c.bak", "c"},
        {"test.c.new", "c"},
        {"test.c.old~", "c"},
        {"test.c.orig", "c"},
        {"test.c.pacnew", "c"},
        {"test.c.pacorig", "c"},
        {"test.c.pacsave", "c"},
        {"test.c.dpkg-dist", "c"},
        {"test.c.dpkg-old", "c"},
        {"test.c.rpmnew", "c"},
        {"test.c.rpmsave", "c"},
    };
    FOR_EACH_I(i, tests) {
        const struct ft_filename_test *t = &tests[i];
        const char *result = find_ft(t->filename, NULL, 0);
        EXPECT_STREQ(result, t->expected_filetype);
    }
}

static void test_find_ft_firstline(void)
{
    static const struct ft_firstline_test {
        const char *line, *expected_filetype;
    } tests[] = {
        {"<!DOCTYPE html>", "html"},
        {"<!doctype HTML", "html"},
        {"<!doctype htm", NULL},
        {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>", "xml"},
        {"[wrap-file]", "ini"},
        {"[wrap-file", NULL},
        {".TH DTE 1", NULL},
        {"", NULL},
        {" ", NULL},
        {" <?xml", NULL},
        {"\0<?xml", NULL},
        // Hashbangs
        {"#!/usr/bin/ash", "sh"},
        {"#!/usr/bin/awk", "awk"},
        {"#!/usr/bin/bash", "sh"},
        {"#!/usr/bin/bigloo", "scheme"},
        {"#!/usr/bin/ccl", "lisp"},
        {"#!/usr/bin/chicken", "scheme"},
        {"#!/usr/bin/clisp", "lisp"},
        {"#!/usr/bin/coffee", "coffeescript"},
        {"#!/usr/bin/crystal", "ruby"},
        {"#!/usr/bin/dash", "sh"},
        {"#!/usr/bin/ecl", "lisp"},
        {"#!/usr/bin/gawk", "awk"},
        {"#!/usr/bin/gmake", "make"},
        {"#!/usr/bin/gnuplot", "gnuplot"},
        {"#!/usr/bin/groovy", "groovy"},
        {"#!/usr/bin/gsed", "sed"},
        {"#!/usr/bin/guile", "scheme"},
        {"#!/usr/bin/jruby", "ruby"},
        {"#!/usr/bin/ksh", "sh"},
        {"#!/usr/bin/lisp", "lisp"},
        {"#!/usr/bin/luajit", "lua"},
        {"#!/usr/bin/lua", "lua"},
        {"#!/usr/bin/macruby", "ruby"},
        {"#!/usr/bin/make", "make"},
        {"#!/usr/bin/mawk", "awk"},
        {"#!/usr/bin/mksh", "sh"},
        {"#!/usr/bin/moon", "moonscript"},
        {"#!/usr/bin/nawk", "awk"},
        {"#!/usr/bin/node", "javascript"},
        {"#!/usr/bin/openrc-run", "sh"},
        {"#!/usr/bin/pdksh", "sh"},
        {"#!/usr/bin/perl", "perl"},
        {"#!/usr/bin/php", "php"},
        {"#!/usr/bin/python", "python"},
        {"#!/usr/bin/r6rs", "scheme"},
        {"#!/usr/bin/racket", "scheme"},
        {"#!/usr/bin/rake", "ruby"},
        {"#!/usr/bin/ruby", "ruby"},
        {"#!/usr/bin/runhaskell", "haskell"},
        {"#!/usr/bin/sbcl", "lisp"},
        {"#!/usr/bin/sed", "sed"},
        {"#!/bin/sh", "sh"},
        {"#!/usr/bin/tcc", "c"},
        {"#!/usr/bin/tclsh", "tcl"},
        {"#!/usr/bin/wish", "tcl"},
        {"#!/usr/bin/zsh", "sh"},
    };
    FOR_EACH_I(i, tests) {
        const struct ft_firstline_test *t = &tests[i];
        const char *result = find_ft(NULL, t->line, strlen(t->line));
        EXPECT_STREQ(result, t->expected_filetype);
    }
}

void test_filetype(void)
{
    test_find_ft_filename();
    test_find_ft_firstline();
}
