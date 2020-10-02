/*
    core_thread.c

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
#include "core/thread.h"
#include "core/frame.h"
#include "utils.h"
#include "json.h"
#include "generic_thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include "normalize.h"
#include <string.h>

/* Method table */

DEFINE_FRAMES_FUNC(core_frames, struct sr_core_thread)
DEFINE_SET_FRAMES_FUNC(core_set_frames, struct sr_core_thread)
DEFINE_NEXT_FUNC(core_next, struct sr_thread, struct sr_core_thread)
DEFINE_SET_NEXT_FUNC(core_set_next, struct sr_thread, struct sr_core_thread)
DEFINE_DUP_WRAPPER_FUNC(core_dup, struct sr_core_thread, sr_core_thread_dup)

struct thread_methods core_thread_methods =
{
    .frames = (frames_fn_t) core_frames,
    .set_frames = (set_frames_fn_t) core_set_frames,
    .cmp = (thread_cmp_fn_t) sr_core_thread_cmp,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) core_next,
    .set_next = (set_next_thread_fn_t) core_set_next,
    .thread_append_bthash_text =
        (thread_append_bthash_text_fn_t) thread_no_bthash_text,
    .thread_free = (thread_free_fn_t) sr_core_thread_free,
    .remove_frame = (remove_frame_fn_t) thread_remove_frame,
    .remove_frames_above =
        (remove_frames_above_fn_t) thread_remove_frames_above,
    .thread_dup = (thread_dup_fn_t) core_dup,
    .normalize = (normalize_fn_t) sr_normalize_core_thread,
};

/* Public functions */

struct sr_core_thread *
sr_core_thread_new()
{
    struct sr_core_thread *thread = g_malloc(sizeof(*thread));
    sr_core_thread_init(thread);
    return thread;
}

void
sr_core_thread_init(struct sr_core_thread *thread)
{
    thread->frames = NULL;
    thread->next = NULL;
    thread->type = SR_REPORT_CORE;
}

void
sr_core_thread_free(struct sr_core_thread *thread)
{
    if (!thread)
        return;

    while (thread->frames)
    {
        struct sr_core_frame *frame = thread->frames;
        thread->frames = frame->next;
        sr_core_frame_free(frame);
    }

    free(thread);
}

struct sr_core_thread *
sr_core_thread_dup(struct sr_core_thread *thread,
                    bool siblings)
{
    struct sr_core_thread *result = sr_core_thread_new();
    memcpy(result, thread, sizeof(struct sr_core_thread));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_core_thread_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    if (result->frames)
        result->frames = sr_core_frame_dup(result->frames, true);

    return result;
}

int
sr_core_thread_cmp(struct sr_core_thread *thread1,
                   struct sr_core_thread *thread2)
{
    struct sr_core_frame *frame1 = thread1->frames,
        *frame2 = thread2->frames;
    do {
        if (frame1 && !frame2)
            return 1;
        else if (frame2 && !frame1)
            return -1;
        else if (frame1 && frame2)
        {
            int frames = sr_core_frame_cmp(frame1, frame2);
            if (frames != 0)
                return frames;
            frame1 = frame1->next;
            frame2 = frame2->next;
        }
    } while (frame1 || frame2);

    return 0;
}

struct sr_core_thread *
sr_core_thread_append(struct sr_core_thread *dest,
                      struct sr_core_thread *item)
{
    if (!dest)
        return item;

    struct sr_core_thread *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

bool
sr_core_thread_is_exit_frame(struct sr_core_frame *frame)
{
    return
        sr_core_frame_calls_func(frame, "__run_exit_handlers", NULL) ||
        sr_core_frame_calls_func(frame, "raise", "libc.so", "libc-", "libpthread.so", NULL) ||
        sr_core_frame_calls_func(frame, "__GI_raise", NULL) ||
        sr_core_frame_calls_func(frame, "exit", NULL) ||
        sr_core_frame_calls_func(frame, "abort", "libc.so", "libc-", NULL) ||
        sr_core_frame_calls_func(frame, "__GI_abort", NULL) ||
        /* Terminates a function in case of buffer overflow. */
        sr_core_frame_calls_func(frame, "__chk_fail", "libc.so", NULL) ||
        sr_core_frame_calls_func(frame, "__stack_chk_fail", "libc.so", NULL) ||
        sr_core_frame_calls_func(frame, "kill", NULL);
}

struct sr_core_frame *
sr_core_thread_find_exit_frame(struct sr_core_thread *thread)
{
    struct sr_core_frame *frame = thread->frames;
    struct sr_core_frame *result = NULL;
    while (frame)
    {
        if (sr_core_thread_is_exit_frame(frame))
            result = frame;

        frame = frame->next;
    }

    return result;
}

struct sr_core_thread *
sr_core_thread_from_json(json_object *root,
                         char **error_message)
{
    if (!json_check_type(root, json_type_object, "thread", error_message))
        return NULL;

    struct sr_core_thread *result = sr_core_thread_new();

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
            struct sr_core_frame *frame;

            frame_json = json_object_array_get_idx(frames, i);
            frame = sr_core_frame_from_json(frame_json, error_message);
            if (!frame)
                goto fail;

            result->frames = sr_core_frame_append(result->frames, frame);
        }
    }

    return result;

fail:
    sr_core_thread_free(result);
    return NULL;
}

char *
sr_core_thread_to_json(struct sr_core_thread *thread, bool is_crash_thread)
{
    GString *strbuf = g_string_new(NULL);
    if (thread->frames)
    {
        if (is_crash_thread)
        {
            g_string_append(strbuf, "{   \"crash_thread\": true\n");
            g_string_append(strbuf, ",");
        }
        else
            g_string_append(strbuf, "{");
        g_string_append(strbuf, "   \"frames\":\n");

        struct sr_core_frame *frame = thread->frames;
        while (frame)
        {
            if (frame == thread->frames)
                g_string_append(strbuf, "      [ ");
            else
                g_string_append(strbuf, "      , ");

            char *frame_json = sr_core_frame_to_json(frame);
            char *indented_frame_json = sr_indent_except_first_line(frame_json, 8);
            g_string_append(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                g_string_append(strbuf, "\n");
        }

        g_string_append(strbuf, " ]\n");
        g_string_append(strbuf, "}");
    }
    else
        g_string_append(strbuf, "{}");

    return g_string_free(strbuf, FALSE);
}
