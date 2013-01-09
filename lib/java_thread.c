/*
    java_thread.c

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
#include "java_thread.h"
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
    thread->frames = NULL;
    thread->next = NULL;
}

void
btp_java_thread_free(struct btp_java_thread *thread)
{
    if (!thread)
        return;

    btp_java_frame_free_full(thread->frames);

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

    if (result->frames)
        result->frames = btp_java_frame_dup(result->frames, true);

    if (result->name)
        result->name = btp_strdup(result->name);

    return result;
}

int
btp_java_thread_cmp(struct btp_java_thread *thread1,
                   struct btp_java_thread *thread2)
{
    int res = btp_strcmp0(thread1->name, thread2->name);
    if (res)
        return res;

    struct btp_java_frame *frame1 = thread1->frames;
    struct btp_java_frame *frame2 = thread2->frames;

    if (frame1 && !frame2)
        return 1;
    else if (frame2 && !frame1)
        return -1;
    else if (frame1 && frame2)
        return btp_java_frame_cmp(frame1, frame2);

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
    struct btp_java_frame *frame = thread->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }

    return count;
}

void
btp_java_thread_quality_counts(struct btp_java_thread *thread,
                               int *ok_count,
                               int *all_count)
{
    struct btp_java_frame *frame = thread->frames;
    while (frame)
    {
        ++(*all_count);

        /* Functions with unknown file names are NOT OK */
        if (frame->is_native || frame->file_name)
            ++(*ok_count);

        frame = frame->next;
    }
}

float
btp_java_thread_quality(struct btp_java_thread *thread)
{
    int ok_count = 0, all_count = 0;
    btp_java_thread_quality_counts(thread, &ok_count, &all_count);
    if (0 == all_count)
        return 1;

    return ok_count / (float)all_count;
}

bool
btp_java_thread_remove_frame(struct btp_java_thread *thread,
                             struct btp_java_frame *frame)
{
    struct btp_java_frame *loop_frame = thread->frames,
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                thread->frames = loop_frame->next;

            btp_java_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }

    return false;
}

bool
btp_java_thread_remove_frames_above(struct btp_java_thread *thread,
                                    struct btp_java_frame *frame)
{
    /* Check that the frame is present in the thread. */
    struct btp_java_frame *loop_frame = thread->frames;
    while (loop_frame)
    {
        if (loop_frame == frame)
            break;

        loop_frame = loop_frame->next;
    }

    if (!loop_frame)
        return false;

    /* Delete all the frames up to the frame. */
    while (thread->frames != frame)
    {
        loop_frame = thread->frames->next;
        btp_java_frame_free(thread->frames);
        thread->frames = loop_frame;
    }

    return true;
}

void
btp_java_thread_remove_frames_below_n(struct btp_java_thread *thread,
                                      int n)
{
    assert(n >= 0);

    /* Skip some frames to get the required stack depth. */
    int i = n;
    struct btp_java_frame *frame = thread->frames,
        *last_frame = NULL;

    while (frame && i)
    {
        last_frame = frame;
        frame = frame->next;
        --i;
    }

    /* Delete the remaining frames. */
    if (last_frame)
        last_frame->next = NULL;
    else
        thread->frames = NULL;

    while (frame)
    {
        struct btp_java_frame *delete_frame = frame;
        frame = frame->next;
        btp_java_frame_free(delete_frame);
    }
}

void
btp_java_thread_append_to_str(struct btp_java_thread *thread,
                              struct btp_strbuf *dest)
{
    struct btp_strbuf *exception = btp_strbuf_new();
    struct btp_java_frame *frame = thread->frames;
    while (frame)
    {
        if (frame->is_exception && exception->len > 0)
        {
            btp_strbuf_prepend_strf(dest, "Caused by: %s\t...\n",
                                    exception->buf);
            btp_strbuf_clear(exception);
        }

        btp_java_frame_append_to_str(frame, exception);
        btp_strbuf_append_char(exception, '\n');

        frame = frame->next;
    }

    if (exception->len > 0)
        btp_strbuf_prepend_str(dest, exception->buf);

    btp_strbuf_prepend_strf(dest, "Exception in thread \"%s\" ",
                            thread->name ? thread->name : "");

    btp_strbuf_free(exception);
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
    if (!(thread->frames = btp_java_frame_parse_exception(&cursor, location)))
    {
        btp_java_thread_free(thread);
        return NULL;
    }

    *input = cursor;

    return thread;
}
