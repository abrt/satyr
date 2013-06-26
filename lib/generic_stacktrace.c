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
#include "strbuf.h"
#include "location.h"

#include "frame.h"
#include "thread.h"
#include "generic_stacktrace.h"

/* Initialize dispatch table. */
static struct stacktrace_methods* dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] = &core_stacktrace_methods,
    [SR_REPORT_PYTHON] = &python_stacktrace_methods,
    [SR_REPORT_KERNELOOPS] = &koops_stacktrace_methods,
    [SR_REPORT_JAVA] = &java_stacktrace_methods,
    [SR_REPORT_GDB] = &gdb_stacktrace_methods,
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

/* Uses the dispatch table but should not be exposed to library users directly. */
struct sr_stacktrace *
stacktrace_parse_wrapper(enum sr_report_type type, const char **input, char **error_message)
{
    struct sr_location location;
    sr_location_init(&location);

    struct sr_stacktrace *result = DISPATCH(dtable, type, parse_location)(input, &location);

    if (!result)
    {
        *error_message = sr_location_to_string(&location);
        return NULL;
    }

    return result;
}

/* Wrappers of polymorphic functions. */
struct sr_stacktrace *
sr_stacktrace_parse(enum sr_report_type type, const char **input, char **error_message)
{
    return DISPATCH(dtable, type, parse)(input, error_message);
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
    DISPATCH(dtable, stacktrace->type, free)(stacktrace);
}