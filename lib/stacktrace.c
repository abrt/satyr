/*
    stacktrace.c

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

#include "utils.h"
#include "strbuf.h"
#include "location.h"

#include "frame.h"
#include "stacktrace.h"
#include "thread.h"

#include "gdb/stacktrace.h"
#include "core/stacktrace.h"
#include "python/stacktrace.h"
#include "koops/stacktrace.h"
#include "java/stacktrace.h"

#include <stdlib.h>

static char *
gdb_return_null(struct sr_stacktrace *stacktrace)
{
    return NULL;
}

/* In case stacktrace type supports only one thread, return the stacktrace itself. */
static struct sr_thread *
one_thread_only(struct sr_stacktrace *stacktrace)
{
    return (struct sr_thread *)stacktrace;
}

static char *
generic_to_short_text(struct sr_stacktrace *stacktrace, int max_frames)
{
    struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
    if (!thread)
        return NULL;

    struct sr_strbuf *strbuf = sr_strbuf_new();
    struct sr_frame *frame;
    int i = 0;

    for (frame = sr_thread_frames(thread); frame; frame = sr_frame_next(frame))
    {
        i++;
        sr_strbuf_append_strf(strbuf, "#%d ", i);
        sr_frame_append_to_str(frame, strbuf);
        sr_strbuf_append_char(strbuf, '\n');

        if (max_frames && i >= max_frames)
            break;
    }

    return sr_strbuf_free_nobuf(strbuf);
}

/* Uses the dispatch table, but we don't want to expose it in the API. */
static struct sr_stacktrace *
generic_parse_wrapper(enum sr_report_type type, const char **input, char **error_message);

/* Initialize dispatch table. */

typedef struct sr_stacktrace* (*parse_fn_t)(const char **, char **);
typedef struct sr_stacktrace* (*parse_location_fn_t)(const char **, struct sr_location *);
typedef char* (*to_short_text_fn_t)(struct sr_stacktrace*, int);
typedef char* (*to_json_fn_t)(struct sr_stacktrace *);
typedef char* (*get_reason_fn_t)(struct sr_stacktrace *);
typedef struct sr_thread* (*find_crash_thread_fn_t)(struct sr_stacktrace *);
typedef void (*free_fn_t)(struct sr_stacktrace *);

struct methods
{
    parse_fn_t parse;
    parse_location_fn_t parse_location;
    to_short_text_fn_t to_short_text;
    to_json_fn_t to_json;
    get_reason_fn_t get_reason;
    find_crash_thread_fn_t find_crash_thread;
    free_fn_t free;
};

static struct methods dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] =
    {
        /* core parser returns error_message directly */
        .parse = (parse_fn_t) sr_core_stacktrace_from_json_text,
        .parse_location = (parse_location_fn_t) NULL,
        .to_short_text = (to_short_text_fn_t) generic_to_short_text,
        .to_json = (to_json_fn_t) sr_core_stacktrace_to_json,
        .get_reason = (get_reason_fn_t) sr_core_stacktrace_get_reason,
        .find_crash_thread =
            (find_crash_thread_fn_t) sr_core_stacktrace_find_crash_thread,
        .free = (free_fn_t) sr_core_stacktrace_free,
    },
    [SR_REPORT_PYTHON] =
    {
        /* for other types, we have to convert location to message first */
        .parse = (parse_fn_t) generic_parse_wrapper,
        .parse_location = (parse_location_fn_t) sr_python_stacktrace_parse,
        .to_short_text = (to_short_text_fn_t) generic_to_short_text,
        .to_json = (to_json_fn_t) sr_python_stacktrace_to_json,
        .get_reason = (get_reason_fn_t) sr_python_stacktrace_get_reason,
        .find_crash_thread = (find_crash_thread_fn_t) one_thread_only,
        .free = (free_fn_t) sr_python_stacktrace_free,
    },
    [SR_REPORT_KERNELOOPS] =
    {
        .parse = (parse_fn_t) generic_parse_wrapper,
        .parse_location = (parse_location_fn_t) sr_koops_stacktrace_parse,
        .to_short_text = (to_short_text_fn_t) generic_to_short_text,
        .to_json = (to_json_fn_t) sr_koops_stacktrace_to_json,
        .get_reason = (get_reason_fn_t) sr_koops_stacktrace_get_reason,
        .find_crash_thread = (find_crash_thread_fn_t) one_thread_only,
        .free = (free_fn_t) sr_koops_stacktrace_free,
    },
    [SR_REPORT_JAVA] =
    {
        .parse = (parse_fn_t) generic_parse_wrapper,
        .parse_location = (parse_location_fn_t) sr_java_stacktrace_parse,
        .to_short_text = (to_short_text_fn_t) generic_to_short_text,
        .to_json = (to_json_fn_t) sr_java_stacktrace_to_json,
        .get_reason = (get_reason_fn_t) sr_java_stacktrace_get_reason,
        .find_crash_thread =
            (find_crash_thread_fn_t) sr_java_find_crash_thread,
        .free = (free_fn_t) sr_java_stacktrace_free,
    },
    [SR_REPORT_GDB] =
    {
        .parse = (parse_fn_t) generic_parse_wrapper,
        .parse_location = (parse_location_fn_t) sr_gdb_stacktrace_parse,
        .to_short_text = (to_short_text_fn_t) sr_gdb_stacktrace_to_short_text,
        .to_json = (to_json_fn_t) gdb_return_null,
        .get_reason = (get_reason_fn_t) gdb_return_null,
        .find_crash_thread =
            (find_crash_thread_fn_t) sr_gdb_stacktrace_find_crash_thread,
        .free = (free_fn_t) sr_gdb_stacktrace_free,
    },
};

static struct sr_stacktrace *
generic_parse_wrapper(enum sr_report_type type, const char **input, char **error_message)
{
    struct sr_location location;
    sr_location_init(&location);

    struct sr_stacktrace *result = SR_DISPATCH(type, parse_location)(input, &location);

    if (!result)
    {
        *error_message = sr_location_to_string(&location);
        return NULL;
    }

    return result;
}

struct sr_stacktrace *
sr_stacktrace_parse(enum sr_report_type type, const char **input, char **error_message)
{
    return SR_DISPATCH(type, parse)(input, error_message);
}

char *
sr_stacktrace_to_short_text(struct sr_stacktrace *stacktrace, int max_frames)
{
    return SR_DISPATCH(stacktrace->type, to_short_text)(stacktrace, max_frames);
}

char *
sr_stacktrace_to_json(struct sr_stacktrace *stacktrace)
{
    return SR_DISPATCH(stacktrace->type, to_json)(stacktrace);
}

char *
sr_stacktrace_get_reason(struct sr_stacktrace *stacktrace)
{
    return SR_DISPATCH(stacktrace->type, get_reason)(stacktrace);
}

struct sr_thread *
sr_stacktrace_find_crash_thread(struct sr_stacktrace *stacktrace)
{
    return SR_DISPATCH(stacktrace->type, find_crash_thread)(stacktrace);
}

void
sr_stacktrace_free(struct sr_stacktrace *stacktrace)
{
    SR_DISPATCH(stacktrace->type, free)(stacktrace);
}
