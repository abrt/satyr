/*
    java_thread.c

    Copyright (C) 2010  Red Hat, Inc.

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
#include "java_thread.h"
#include "java_exception.h"
#include "java_frame.h"
#include "location.h"
#include "utils.h"
#include "strbuf.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct btp_java_thread *
btp_java_thread_new()
{
    struct btp_java_thread *thread =
        btp_malloc(sizeof(*thread));
    btp_java_thread_init(thread);
    return thread;
}

void
btp_java_thread_init(struct btp_java_thread *thread)
{
    thread->name = NULL;
    thread->exception = NULL;
    thread->next = NULL;
}

void
btp_java_thread_free(struct btp_java_thread *thread)
{
    if (!thread)
        return;

    btp_java_exception_free(thread->exception);

    free(thread->name);
    free(thread);
}

struct btp_java_thread *
btp_java_thread_dup(struct btp_java_thread *thread, bool siblings)
{
    struct btp_java_thread *result = btp_java_thread_new();
    memcpy(result, thread, sizeof(*result));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_java_thread_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    if (result->exception)
        result->exception = btp_java_exception_dup(result->exception, true);

    return result;
}

int
btp_java_thread_cmp(struct btp_java_thread *thread1,
                   struct btp_java_thread *thread2)
{
    int res = btp_strcmp0(thread1->name, thread2->name);
    if (res)
        return res;

    struct btp_java_exception *exception1 = thread1->exception;
    struct btp_java_exception *exception2 = thread2->exception;

    if (exception1 && !exception2)
        return 1;
    else if (exception2 && !exception1)
        return -1;
    else if (exception1 && exception2)
    {
        int exceptions = btp_java_exception_cmp(exception1, exception2, true);
        if (exceptions != 0)
            return exceptions;
    }

    return 0;
}

struct btp_java_thread *
btp_java_thread_append(struct btp_java_thread *dest,
                       struct btp_java_thread *item)
{
    if (!dest)
        return item;

    struct btp_java_thread *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

int
btp_java_thread_get_frame_count(struct btp_java_thread *thread)
{
    if (thread->exception)
        return btp_java_exception_get_frame_count(thread->exception, true);

    return 0;
}

void
btp_java_thread_quality_counts(struct btp_java_thread *thread,
                               int *ok_count,
                               int *all_count)
{
    if (thread->exception)
        return btp_java_exception_quality_counts(thread->exception, true);

    return 0;
}

float
btp_java_thread_quality(struct btp_java_thread *thread)
{
    if (thread->exception)
        return btp_java_exception_quality(thread->exception, true);

    return .0;
}

bool
btp_java_thread_remove_frame(struct btp_java_thread *thread,
                             struct btp_java_frame *frame)
{
    if (thread->exception)
        return btp_java_exception_remove_frame(thread->exception, frame, true);

    return false;
}

bool
btp_java_thread_remove_frames_above(struct btp_java_thread *thread,
                                    struct btp_java_frame *frame)
{
    if (thread->exception)
        return btp_java_exception_remove_frames_above(thread->exception, frame, true);

    return false;
}

void
btp_java_thread_remove_frames_below_n(struct btp_java_thread *thread,
                                      int n)
{
    if (thread->exception)
        return btp_java_exception_remove_frames_below_n(thread->exception, n, true);

    return false;
}

void
btp_java_thread_append_to_str(struct btp_java_thread *thread,
                              struct btp_strbuf *dest)
{
    btp_strbuf_append_strf(dest, "Exception in thread \"%s\"",
                           thread->name ? thread->name : "");

    if (thread->exception)
        btp_java_exception_append_to_str(thread->exception, dest);
}

struct btp_java_thread *
btp_java_thread_parse(const char **input,
                      struct btp_location *location)
{
    const char *cursor = *input;
    /* Exception in thread "main" java.lang.NullPointerException: foo */
    int chars = btp_skip_string(&cursor, "Exception in thread \"");
    btp_location_add(location, 0, chars);

    struct btp_java_thread *thread = btp_java_thread_new();
    if (chars)
    {
        const char *mark = cursor;
        /* main" java.lang.NullPointerException: foo */
        btp_location_add(location, 0, btp_skip_char_cspan(&cursor, "\"\n"));

        /* " java.lang.NullPointerException: foo */
        if (*cursor != '"')
        {
            location->message = "\"Thread\" name end expected";
            btp_java_thread_free(thread);
            return NULL;
        }

        thread->name = btp_strndup(mark, cursor - mark);

        btp_location_eat_char(location, *(++cursor));
    }

    /* java.lang.NullPointerException: foo */
    if (!(thread->exception = btp_java_exception_parse(&cursor, location)))
    {
        btp_java_thread_free(thread);
        return NULL;
    }

    *input = cursor;

    return thread;
}
