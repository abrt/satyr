/*
    generic_stacktrace.c

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

#include <stdlib.h>

#include "internal_utils.h"
#include "location.h"
#include "sha1.h"
#include "json.h"

#include "frame.h"
#include "thread.h"
#include "generic_stacktrace.h"
#include "generic_thread.h"
#include "generic_frame.h"

/* Initialize dispatch table. */
static struct stacktrace_methods* dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] = &core_stacktrace_methods,
    [SR_REPORT_PYTHON] = &python_stacktrace_methods,
    [SR_REPORT_KERNELOOPS] = &koops_stacktrace_methods,
    [SR_REPORT_JAVA] = &java_stacktrace_methods,
    [SR_REPORT_GDB] = &gdb_stacktrace_methods,
    [SR_REPORT_RUBY] = &ruby_stacktrace_methods,
    [SR_REPORT_JAVASCRIPT] = &js_stacktrace_methods,
};

/* In case stacktrace type supports only one thread, return the stacktrace itself. */
struct sr_thread *
stacktrace_one_thread_only(struct sr_stacktrace *stacktrace)
{
    return (struct sr_thread *)stacktrace;
}

char *
stacktrace_to_short_text(struct sr_stacktrace *stacktrace, int max_frames)
{
    struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
    if (!thread)
        return NULL;

    GString *strbuf = g_string_new(NULL);
    struct sr_frame *frame;
    int i = 0;

    for (frame = sr_thread_frames(thread); frame; frame = sr_frame_next(frame))
    {
        i++;
        g_string_append_printf(strbuf, "#%d ", i);
        sr_frame_append_to_str(frame, strbuf);
        g_string_append_c(strbuf, '\n');

        if (max_frames && i >= max_frames)
            break;
    }

    return g_string_free(strbuf, FALSE);
}

/* Uses the dispatch table but should not be exposed to library users directly. */
struct sr_stacktrace *
stacktrace_parse_wrapper(enum sr_report_type type, const char *input, char **error_message)
{
    struct sr_location location;
    sr_location_init(&location);

    struct sr_stacktrace *result = DISPATCH(dtable, type, parse_location)(&input, &location);

    if (!result)
    {
        *error_message = sr_location_to_string(&location);
        return NULL;
    }

    return result;
}

/* Wrappers of polymorphic functions. */
struct sr_stacktrace *
sr_stacktrace_parse(enum sr_report_type type, const char *input, char **error_message)
{
    return DISPATCH(dtable, type, parse)(input, error_message);
}

struct sr_stacktrace *
sr_stacktrace_from_json(enum sr_report_type type, json_object *root, char **error_message)
{
    return DISPATCH(dtable, type, from_json)(root, error_message);
}

struct sr_stacktrace *
sr_stacktrace_from_json_text(enum sr_report_type type, const char *input, char **error_message)
{
    enum json_tokener_error error;
    json_object *json_root = json_tokener_parse_verbose(input, &error);

    if (!json_root)
    {
        if (NULL != error_message)
        {
            const char *description;

            description = json_tokener_error_desc(error);

            *error_message = sr_strdup(description);
        }

        return NULL;
    }

    struct sr_stacktrace *stacktrace =
        sr_stacktrace_from_json(type, json_root, error_message);

    json_object_put(json_root);
    return stacktrace;
}

char *
sr_stacktrace_to_short_text(struct sr_stacktrace *stacktrace, int max_frames)
{
    return DISPATCH(dtable, stacktrace->type, to_short_text)(stacktrace, max_frames);
}

char *
sr_stacktrace_to_json(struct sr_stacktrace *stacktrace)
{
    return DISPATCH(dtable, stacktrace->type, to_json)(stacktrace);
}

char *
sr_stacktrace_get_reason(struct sr_stacktrace *stacktrace)
{
    return DISPATCH(dtable, stacktrace->type, get_reason)(stacktrace);
}

struct sr_thread *
sr_stacktrace_find_crash_thread(struct sr_stacktrace *stacktrace)
{
    return DISPATCH(dtable, stacktrace->type, find_crash_thread)(stacktrace);
}

struct sr_thread *
sr_stacktrace_threads(struct sr_stacktrace *stacktrace)
{
    return DISPATCH(dtable, stacktrace->type, threads)(stacktrace);
}

void
sr_stacktrace_set_threads(struct sr_stacktrace *stacktrace, struct sr_thread *threads)
{
    assert(threads == NULL || stacktrace->type == threads->type);
    DISPATCH(dtable, stacktrace->type, set_threads)(stacktrace, threads);
}

void
sr_stacktrace_free(struct sr_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    DISPATCH(dtable, stacktrace->type, stacktrace_free)(stacktrace);
}

char *
sr_stacktrace_get_bthash(struct sr_stacktrace *stacktrace, enum sr_bthash_flags flags)
{
    char *ret;
    GString *strbuf = g_string_new(NULL);

    /* Append data contained in the stacktrace structure. */
    DISPATCH(dtable, stacktrace->type, stacktrace_append_bthash_text)
            (stacktrace, flags, strbuf);

    for (struct sr_thread *thread = sr_stacktrace_threads(stacktrace);
         thread;
         thread = sr_thread_next(thread))
    {
        /* Data containted in the thread structure (if any). */
        thread_append_bthash_text(thread, flags, strbuf);

        for (struct sr_frame *frame = sr_thread_frames(thread);
             frame;
             frame = sr_frame_next(frame))
        {
            frame_append_bthash_text(frame, flags, strbuf);
        }

        /* Blank line in between threads. */
        if (sr_thread_next(thread))
            g_string_append_c(strbuf, '\n');
    }

    if (flags & SR_BTHASH_NOHASH)
        ret = g_string_free(strbuf, FALSE);
    else
    {
        ret = sr_sha1_hash_string(strbuf->str);
        g_string_free(strbuf, TRUE);
    }

    return ret;
}
