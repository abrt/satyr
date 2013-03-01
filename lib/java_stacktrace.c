/*
    java_stacktrace.c

    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

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
#include "java_stacktrace.h"
#include "java_thread.h"
#include "java_frame.h"
#include "utils.h"
#include "strbuf.h"
#include <stdio.h>
#include <string.h>

struct sr_java_stacktrace *
sr_java_stacktrace_new()
{
    struct sr_java_stacktrace *stacktrace =
        sr_malloc(sizeof(*stacktrace));

    sr_java_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_java_stacktrace_init(struct sr_java_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(*stacktrace));
}

void
sr_java_stacktrace_free(struct sr_java_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->threads)
    {
        struct sr_java_thread *thread = stacktrace->threads;
        stacktrace->threads = thread->next;
        sr_java_thread_free(thread);
    }

    free(stacktrace);
}

struct sr_java_stacktrace *
sr_java_stacktrace_dup(struct sr_java_stacktrace *stacktrace)
{
    struct sr_java_stacktrace *result = sr_java_stacktrace_new();
    memcpy(result, stacktrace, sizeof(*result));

    if (result->threads)
        result->threads = sr_java_thread_dup(result->threads, true);

    return result;
}

int
sr_java_stacktrace_cmp(struct sr_java_stacktrace *stacktrace1,
                       struct sr_java_stacktrace *stacktrace2)
{
    struct sr_java_thread *thread1 = stacktrace1->threads;
    struct sr_java_thread *thread2 = stacktrace2->threads;

    do
    {
        if ( thread1 && !thread2)
            return 1;
        if (!thread1 &&  thread2)
            return -1;
        if ( thread1 &&  thread2)
        {
            int threads = sr_java_thread_cmp(thread1, thread2);
            if (threads != 0)
                return threads;
            thread1 = thread1->next;
            thread2 = thread2->next;
        }
    } while (thread1 || thread2);

    return 0;
}

/*
Example input:

----- BEGIN -----
Exception in thread "main" java.lang.NullPointerException
        at SimpleTest.throwNullPointerException(SimpleTest.java:36)
        at SimpleTest.throwAndDontCatchException(SimpleTest.java:70)
        at SimpleTest.main(SimpleTest.java:82)
----- END -----

relevant lines:
*/

struct sr_java_stacktrace *
sr_java_stacktrace_parse(const char **input, struct sr_location *location)
{
    struct sr_java_thread *thread = sr_java_thread_parse(input, location);
    if (thread == NULL)
        return NULL;

    struct sr_java_stacktrace *stacktrace = sr_java_stacktrace_new();
    stacktrace->threads = thread;

    return stacktrace;
}

char *
sr_java_stacktrace_to_json(struct sr_java_stacktrace *stacktrace)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    sr_strbuf_append_str(strbuf, "{   \"threads\":");
    if (stacktrace->threads)
        sr_strbuf_append_str(strbuf, "\n");
    else
        sr_strbuf_append_str(strbuf, " [");

    struct sr_java_thread *thread = stacktrace->threads;
    while (thread)
    {
        if (thread == stacktrace->threads)
            sr_strbuf_append_str(strbuf, "      [ ");
        else
            sr_strbuf_append_str(strbuf, "      , ");

        char *thread_json = sr_java_thread_to_json(thread);
        char *indented_thread_json = sr_indent_except_first_line(thread_json, 8);
        sr_strbuf_append_str(strbuf, indented_thread_json);
        free(indented_thread_json);
        free(thread_json);
        thread = thread->next;
        if (thread)
            sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_str(strbuf, " ]\n");
    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}
