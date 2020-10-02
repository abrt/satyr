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
#include "java/stacktrace.h"
#include "java/thread.h"
#include "java/frame.h"
#include "utils.h"
#include "json.h"
#include "generic_stacktrace.h"
#include "internal_utils.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* Method table */

static void
java_append_bthash_text(struct sr_java_stacktrace *stacktrace, enum sr_bthash_flags flags,
                        GString *strbuf)
{
    /* nop */
}

DEFINE_THREADS_FUNC(java_threads, struct sr_java_stacktrace)
DEFINE_SET_THREADS_FUNC(java_set_threads, struct sr_java_stacktrace)
DEFINE_PARSE_WRAPPER_FUNC(java_parse, SR_REPORT_JAVA)

struct stacktrace_methods java_stacktrace_methods =
{
    .parse = (parse_fn_t) java_parse,
    .parse_location = (parse_location_fn_t) sr_java_stacktrace_parse,
    .to_short_text = (to_short_text_fn_t) stacktrace_to_short_text,
    .to_json = (to_json_fn_t) sr_java_stacktrace_to_json,
    .from_json = (from_json_fn_t) sr_java_stacktrace_from_json,
    .get_reason = (get_reason_fn_t) sr_java_stacktrace_get_reason,
    .find_crash_thread =
        (find_crash_thread_fn_t) sr_java_find_crash_thread,
    .threads = (threads_fn_t) java_threads,
    .set_threads = (set_threads_fn_t) java_set_threads,
    .stacktrace_free = (stacktrace_free_fn_t) sr_java_stacktrace_free,
    .stacktrace_append_bthash_text =
        (stacktrace_append_bthash_text_fn_t) java_append_bthash_text,
};

/* Public functions */

struct sr_java_stacktrace *
sr_java_stacktrace_new()
{
    struct sr_java_stacktrace *stacktrace =
        g_malloc(sizeof(*stacktrace));

    sr_java_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_java_stacktrace_init(struct sr_java_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(*stacktrace));
    stacktrace->type = SR_REPORT_JAVA;
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
    GString *strbuf = g_string_new(NULL);

    g_string_append(strbuf, "{   \"threads\":");
    if (stacktrace->threads)
        g_string_append(strbuf, "\n");
    else
        g_string_append(strbuf, " [");

    struct sr_java_thread *thread = stacktrace->threads;
    while (thread)
    {
        if (thread == stacktrace->threads)
            g_string_append(strbuf, "      [ ");
        else
            g_string_append(strbuf, "      , ");

        char *thread_json = sr_java_thread_to_json(thread);
        char *indented_thread_json = sr_indent_except_first_line(thread_json, 8);
        g_string_append(strbuf, indented_thread_json);
        free(indented_thread_json);
        free(thread_json);
        thread = thread->next;
        if (thread)
            g_string_append(strbuf, "\n");
    }

    g_string_append(strbuf, " ]\n");
    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
}

struct sr_java_stacktrace *
sr_java_stacktrace_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "stacktrace", error_message))
        return NULL;

    struct sr_java_stacktrace *result = sr_java_stacktrace_new();

    json_object *threads;

    if (json_object_object_get_ex(root, "threads", &threads))
    {
        size_t array_length;

        if (!json_check_type(threads, json_type_array, "threads", error_message))
            goto fail;

        array_length = json_object_array_length(threads);

        for (size_t i = 0; i < array_length; i++)
        {
            json_object *thread_json;
            struct sr_java_thread *thread;

            thread_json = json_object_array_get_idx(threads, i);
            thread = sr_java_thread_from_json(thread_json, error_message);
            if (!thread)
                goto fail;

            result->threads = sr_java_thread_append(result->threads, thread);
        }
    }

    return result;

fail:
    sr_java_stacktrace_free(result);
    return NULL;
}

char *
sr_java_stacktrace_get_reason(struct sr_java_stacktrace *stacktrace)
{
    char *exc = "<unknown>";
    char *file = "<unknown>";
    uint32_t line = 0;

    if (stacktrace->threads)
    {
        struct sr_java_thread *t = stacktrace->threads;
        if (t->frames)
        {
            struct sr_java_frame *f = t->frames;
            bool exc_found = false;

            while (f)
            {
                /* exception name - first frame w/ is_exception */
                if (f->is_exception && f->name && !exc_found)
                {
                    exc = f->name;
                    exc_found = true;
                }

                /* file - bottom of the stack */
                if (!f->next && f->file_name)
                {
                    file = f->file_name;
                    line = f->file_line;
                }

                f = f->next;
            }
        }
    }

    return g_strdup_printf("Exception %s occurred in %s:%"PRIu32, exc, file, line);
}

struct sr_java_thread *
sr_java_find_crash_thread(struct sr_java_stacktrace *stacktrace)
{
    /* FIXME: is the first thread the crash (or otherwise interesting) thread?
     */
    return stacktrace->threads;
}
