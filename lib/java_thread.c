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
#include "json.h"
#include "generic_thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Method table */

static void
java_append_bthash_text(struct sr_java_thread *thread, enum sr_bthash_flags flags,
                        GString *strbuf);

DEFINE_FRAMES_FUNC(java_frames, struct sr_java_thread)
DEFINE_SET_FRAMES_FUNC(java_set_frames, struct sr_java_thread)
DEFINE_NEXT_FUNC(java_next, struct sr_thread, struct sr_java_thread)
DEFINE_SET_NEXT_FUNC(java_set_next, struct sr_thread, struct sr_java_thread)
DEFINE_DUP_WRAPPER_FUNC(java_dup, struct sr_java_thread, sr_java_thread_dup)

struct thread_methods java_thread_methods =
{
    .frames = (frames_fn_t) java_frames,
    .set_frames = (set_frames_fn_t) java_set_frames,
    .cmp = (thread_cmp_fn_t) sr_java_thread_cmp,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) java_next,
    .set_next = (set_next_thread_fn_t) java_set_next,
    .thread_append_bthash_text =
        (thread_append_bthash_text_fn_t) java_append_bthash_text,
    .thread_free = (thread_free_fn_t) sr_java_thread_free,
    .remove_frame = (remove_frame_fn_t) thread_remove_frame,
    .remove_frames_above =
        (remove_frames_above_fn_t) thread_remove_frames_above,
    .thread_dup = (thread_dup_fn_t) java_dup,
    .normalize = (normalize_fn_t) thread_no_normalization,
};

/* Public functions */

struct sr_java_thread *
sr_java_thread_new()
{
    struct sr_java_thread *thread =
        g_malloc(sizeof(*thread));
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
        result->name = g_strdup(result->name);

    return result;
}

int
sr_java_thread_cmp(struct sr_java_thread *thread1,
                   struct sr_java_thread *thread2)
{
    int res = g_strcmp0(thread1->name, thread2->name);
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
                             GString *dest)
{
    GString *exception = g_string_new(NULL);
    struct sr_java_frame *frame = thread->frames;
    while (frame)
    {
        if (frame->is_exception && exception->len > 0)
        {
            gchar *caused_by = g_strdup_printf("Caused by: %s\t...\n", exception->str);
            g_string_prepend(dest, caused_by);
            g_string_erase(exception, 0, -1);
            g_free(caused_by);
        }

        sr_java_frame_append_to_str(frame, exception);
        g_string_append_c(exception, '\n');

        frame = frame->next;
    }

    if (exception->len > 0)
        g_string_prepend(dest, exception->str);

    gchar *ex_in_thread = g_strdup_printf("Exception in thread \"%s\" ",
                                          thread->name ? thread->name : "");
    g_string_prepend(dest, ex_in_thread);
    g_free(ex_in_thread);

    g_string_free(exception, TRUE);
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

        thread->name = g_strndup(mark, cursor - mark);

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
    GString *buf = g_string_new(NULL);

    while (frame)
    {
        if (frame->name)
        {
            g_string_append(buf, frame->name);
            g_string_append_c(buf, '\n');
        }

        frame = frame->next;
    }

    return g_string_free(buf, FALSE);
}

char *
sr_java_thread_to_json(struct sr_java_thread *thread)
{
    GString *strbuf = g_string_new(NULL);

    if (thread->name)
    {
        g_string_append(strbuf, ",   \"name\": ");
        sr_json_append_escaped(strbuf, thread->name);
        g_string_append(strbuf, "\n");
    }

    if (thread->frames)
    {
        g_string_append(strbuf, ",   \"frames\":\n");
        struct sr_java_frame *frame = thread->frames;
        while (frame)
        {
            if (frame == thread->frames)
                g_string_append(strbuf, "      [ ");
            else
                g_string_append(strbuf, "      , ");

            char *frame_json = sr_java_frame_to_json(frame);
            char *indented_frame_json = sr_indent_except_first_line(frame_json, 8);
            g_string_append(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                g_string_append(strbuf, "\n");
        }

        g_string_append(strbuf, " ]\n");
    }

    if (strbuf->len > 0)
        strbuf->str[0] = '{';
    else
        g_string_append_c(strbuf, '{');

    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
}

struct sr_java_thread *
sr_java_thread_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "thread", error_message))
        return NULL;

    struct sr_java_thread *result = sr_java_thread_new();

    if (!JSON_READ_STRING(root, "name", &result->name))
        goto fail;

    /* Read frames. */
    json_object *frames;

    if (json_object_object_get_ex(root, "frames", &frames))
    {
        size_t array_length;

        if (!json_check_type(frames, json_type_array, "frames", error_message))
            goto fail;

        array_length = json_object_array_length(frames);

        for (size_t i = 0; i < array_length; i++)
        {
            json_object *frame_json;
            struct sr_java_frame *frame;

            frame_json = json_object_array_get_idx(frames, i);
            frame = sr_java_frame_from_json(frame_json, error_message);
            if (!frame)
                goto fail;

            result->frames = sr_java_frame_append(result->frames, frame);
        }
    }

    return result;

fail:
    sr_java_thread_free(result);
    return NULL;
}

static void
java_append_bthash_text(struct sr_java_thread *thread, enum sr_bthash_flags flags,
                        GString *strbuf)
{
    g_string_append_printf(strbuf, "Thread %s\n", OR_UNKNOWN(thread->name));
}
