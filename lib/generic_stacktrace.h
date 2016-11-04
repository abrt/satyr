/*
    generic_stacktrace.h

    Copyright (C) 2013  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#ifndef SATYR_GENERIC_STACKTRACE_H
#define SATYR_GENERIC_STACKTRACE_H

#include "stacktrace.h"
#include "thread.h"

struct sr_json_value;
struct sr_strbuf;

typedef struct sr_stacktrace* (*parse_fn_t)(const char *, char **);
typedef struct sr_stacktrace* (*parse_location_fn_t)(const char **, struct sr_location *);
typedef char* (*to_short_text_fn_t)(struct sr_stacktrace*, int);
typedef char* (*to_json_fn_t)(struct sr_stacktrace *);
typedef struct sr_stacktrace* (*from_json_fn_t)(struct sr_json_value *, char **);
typedef char* (*get_reason_fn_t)(struct sr_stacktrace *);
typedef struct sr_thread* (*find_crash_thread_fn_t)(struct sr_stacktrace *);
typedef struct sr_thread* (*threads_fn_t)(struct sr_stacktrace *);
typedef void (*set_threads_fn_t)(struct sr_stacktrace *, struct sr_thread *);
typedef void (*stacktrace_free_fn_t)(struct sr_stacktrace *);
typedef void (*stacktrace_append_bthash_text_fn_t)(struct sr_stacktrace *, enum sr_bthash_flags,
                                                   struct sr_strbuf *);

struct stacktrace_methods
{
    parse_fn_t parse;
    parse_location_fn_t parse_location;
    to_short_text_fn_t to_short_text;
    to_json_fn_t to_json;
    from_json_fn_t from_json;
    get_reason_fn_t get_reason;
    find_crash_thread_fn_t find_crash_thread;
    threads_fn_t threads;
    set_threads_fn_t set_threads;
    stacktrace_free_fn_t stacktrace_free;
    stacktrace_append_bthash_text_fn_t stacktrace_append_bthash_text;
};

extern struct stacktrace_methods core_stacktrace_methods, python_stacktrace_methods,
       koops_stacktrace_methods, gdb_stacktrace_methods, java_stacktrace_methods,
       ruby_stacktrace_methods, js_stacktrace_methods;

/* Macros to generate accessors for the "threads" member. */
#define DEFINE_THREADS_FUNC(name, concrete_t) \
    DEFINE_GETTER(name, threads, struct sr_stacktrace, concrete_t, struct sr_thread)

#define DEFINE_SET_THREADS_FUNC(name, concrete_t) \
    DEFINE_SETTER(name, threads, struct sr_stacktrace, concrete_t, struct sr_thread)

#define DEFINE_PARSE_WRAPPER_FUNC(name, type)                        \
    static struct sr_stacktrace *                                    \
    name(const char *input, char **error_message)                    \
    {                                                                \
        return stacktrace_parse_wrapper(type, input, error_message); \
    }

/* generic functions */
struct sr_stacktrace *
stacktrace_parse_wrapper(enum sr_report_type type, const char *input, char **error_message);

char *
stacktrace_to_short_text(struct sr_stacktrace *stacktrace, int max_frames);

struct sr_thread *
stacktrace_one_thread_only(struct sr_stacktrace *stacktrace);

#endif
