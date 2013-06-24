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
#include "java/thread.h"
#include "java/frame.h"
#include "location.h"
#include "utils.h"
#include "strbuf.h"
#include "json.h"
#include "generic_thread.h"
#include "internal_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Method table */

DEFINE_FRAMES_FUNC(java_frames, struct sr_java_thread)
DEFINE_SET_FRAMES_FUNC(java_set_frames, struct sr_java_thread)
DEFINE_NEXT_FUNC(java_next, struct sr_thread, struct sr_java_thread)
DEFINE_SET_NEXT_FUNC(java_set_next, struct sr_thread, struct sr_java_thread)

struct thread_methods java_thread_methods =
{
    .frames = (frames_fn_t) java_frames,
    .set_frames = (set_frames_fn_t) java_set_frames,
    .cmp = (thread_cmp_fn_t) sr_java_thread_cmp,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) java_next,
    .set_next = (set_next_thread_fn_t) java_set_next,
};

/* Public functions */

struct sr_java_thread *
sr_java_thread_new()
{
    struct sr_java_thread *thread =
        sr_malloc(sizeof(*thread));
    sr_java_thread_init(thread);
    return thread;
}

void
sr_java_thread_init(struct sr_java_thread *thread)
{
    thread->name = NULL;
    thread->frames = NULL;
    thread->next = NULL;
    thread->type = SR_REPORT_JAVA;
}

void
sr_java_thread_free(struct sr_java_thread *thread)
{
    if (!thread)
        return;

    sr_java_frame_free_full(thread->frames);

    free(thread->name);
    free(thread);
}

struct sr_java_thread *
sr_java_thread_dup(struct sr_java_thread *thread, bool siblings)
{
    struct sr_java_thread *result = sr_java_thread_new();
    memcpy(result, thread, sizeof(*result));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_java_thread_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    if (result->frames)
        result->frames = sr_java_frame_dup(result->frames, true);

    if (result->name)
        result->name = sr_strdup(result->name);

    return result;
}

int
sr_java_thread_cmp(struct sr_java_thread *thread1,
                   struct sr_java_thread *thread2)
{
    int res = sr_strcmp0(thread1->name, thread2->name);
    if (res)
        return res;

    struct sr_java_frame *frame1 = thread1->frames;
    struct sr_java_frame *frame2 = thread2->frames;

    if (frame1 && !frame2)
        return 1;
    else if (frame2 && !frame1)
        return -1;
    else if (frame1 && frame2)
        return sr_java_frame_cmp(frame1, frame2);

    return 0;
}

struct sr_java_thread *
sr_java_thread_append(struct sr_java_thread *dest,
                      struct sr_java_thread *item)
{
    if (!dest)
        return item;

    struct sr_java_thread *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

void
sr_java_thread_quality_counts(struct sr_java_thread *thread,
                              int *ok_count,
                              int *all_count)
{
    struct sr_java_frame *frame = thread->frames;
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
sr_java_thread_quality(struct sr_java_thread *thread)
{
    int ok_count = 0, all_count = 0;
    sr_java_thread_quality_counts(thread, &ok_count, &all_count);
    if (0 == all_count)
        return 1;

    return ok_count / (float)all_count;
}

bool
sr_java_thread_remove_frame(struct sr_java_thread *thread,
                            struct sr_java_frame *frame)
{
    struct sr_java_frame *loop_frame = thread->frames,
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                thread->frames = loop_frame->next;

            sr_java_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }

    return false;
}

bool
sr_java_thread_remove_frames_above(struct sr_java_thread *thread,
                                   struct sr_java_frame *frame)
{
    /* Check that the frame is present in the thread. */
    struct sr_java_frame *loop_frame = thread->frames;
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
        sr_java_frame_free(thread->frames);
        thread->frames = loop_frame;
    }

    return true;
}

void
sr_java_thread_remove_frames_below_n(struct sr_java_thread *thread,
                                     int n)
{
    assert(n >= 0);

    /* Skip some frames to get the required stack depth. */
    int i = n;
    struct sr_java_frame *frame = thread->frames,
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
        struct sr_java_frame *delete_frame = frame;
        frame = frame->next;
        sr_java_frame_free(delete_frame);
    }
}

void
sr_java_thread_append_to_str(struct sr_java_thread *thread,
                             struct sr_strbuf *dest)
{
    struct sr_strbuf *exception = sr_strbuf_new();
    struct sr_java_frame *frame = thread->frames;
    while (frame)
    {
        if (frame->is_exception && exception->len > 0)
        {
            sr_strbuf_prepend_strf(dest, "Caused by: %s\t...\n",
                                   exception->buf);
            sr_strbuf_clear(exception);
        }

        sr_java_frame_append_to_str(frame, exception);
        sr_strbuf_append_char(exception, '\n');

        frame = frame->next;
    }

    if (exception->len > 0)
        sr_strbuf_prepend_str(dest, exception->buf);

    sr_strbuf_prepend_strf(dest, "Exception in thread \"%s\" ",
                            thread->name ? thread->name : "");

    sr_strbuf_free(exception);
}

struct sr_java_thread *
sr_java_thread_parse(const char **input,
                     struct sr_location *location)
{
    const char *cursor = *input;
    /* Exception in thread "main" java.lang.NullPointerException: foo */
    int chars = sr_skip_string(&cursor, "Exception in thread \"");
    sr_location_add(location, 0, chars);

    struct sr_java_thread *thread = sr_java_thread_new();
    if (chars)
    {
        const char *mark = cursor;
        /* main" java.lang.NullPointerException: foo */
        sr_location_add(location, 0, sr_skip_char_cspan(&cursor, "\"\n"));

        /* " java.lang.NullPointerException: foo */
        if (*cursor != '"')
        {
            location->message = "\"Thread\" name end expected";
            sr_java_thread_free(thread);
            return NULL;
        }

        thread->name = sr_strndup(mark, cursor - mark);

        sr_location_eat_char(location, *(++cursor));
    }

    /* java.lang.NullPointerException: foo */
    if (!(thread->frames = sr_java_frame_parse_exception(&cursor, location)))
    {
        sr_java_thread_free(thread);
        return NULL;
    }

    *input = cursor;

    return thread;
}

char *
sr_java_thread_format_funs(struct sr_java_thread *thread)
{
    struct sr_java_frame *frame = thread->frames;
    struct sr_strbuf *buf = sr_strbuf_new();

    while (frame)
    {
        if (frame->name)
        {
            sr_strbuf_append_str(buf, frame->name);
            sr_strbuf_append_char(buf, '\n');
        }

        frame = frame->next;
    }

    return sr_strbuf_free_nobuf(buf);
}

char *
sr_java_thread_to_json(struct sr_java_thread *thread)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    if (thread->name)
    {
        sr_strbuf_append_str(strbuf, ",   \"name\": ");
        sr_json_append_escaped(strbuf, thread->name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (thread->frames)
    {
        sr_strbuf_append_str(strbuf, ",   \"frames\":\n");
        struct sr_java_frame *frame = thread->frames;
        while (frame)
        {
            if (frame == thread->frames)
                sr_strbuf_append_str(strbuf, "      [ ");
            else
                sr_strbuf_append_str(strbuf, "      , ");

            char *frame_json = sr_java_frame_to_json(frame);
            char *indented_frame_json = sr_indent_except_first_line(frame_json, 8);
            sr_strbuf_append_str(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                sr_strbuf_append_str(strbuf, "\n");
        }

        sr_strbuf_append_str(strbuf, " ]\n");
        sr_strbuf_append_str(strbuf, "}");
    }
    else
        sr_strbuf_append_str(strbuf, "{}");

    if (strbuf->len > 0)
        strbuf->buf[0] = '{';
    else
        sr_strbuf_append_char(strbuf, '{');

    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}

