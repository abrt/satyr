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
#include "core_thread.h"
#include "core_frame.h"
#include "utils.h"
#include "strbuf.h"
#include "json.h"
#include <string.h>

struct sr_core_thread *
sr_core_thread_new()
{
    struct sr_core_thread *thread = sr_malloc(sizeof(struct sr_core_thread));
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

struct sr_core_frame *
sr_core_thread_find_exit_frame(struct sr_core_thread *thread)
{
    struct sr_core_frame *frame = thread->frames;
    struct sr_core_frame *result = NULL;
    while (frame)
    {
        bool is_exit_frame =
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

        if (is_exit_frame)
            result = frame;

        frame = frame->next;
    }

    return result;
}

struct sr_core_thread *
sr_core_thread_from_json(struct sr_json_value *root,
                         char **error_message)
{
    if (root->type != SR_JSON_OBJECT)
    {
        *error_message = sr_strdup("Invalid type of root value; object expected.");
        return NULL;
    }

    struct sr_core_thread *result = sr_core_thread_new();

    /* Read frames. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("frames", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_ARRAY)
        {
            *error_message = sr_strdup("Invalid type of \"frames\"; array expected.");
            sr_core_thread_free(result);
            return NULL;
        }

        for (unsigned j = 0; j < root->u.object.values[i].value->u.array.length; ++j)
        {
            struct sr_core_frame *frame = sr_core_frame_from_json(
                root->u.object.values[i].value->u.array.values[j],
                error_message);

            if (!frame)
            {
                sr_core_thread_free(result);
                return NULL;
            }

            result->frames = sr_core_frame_append(result->frames, frame);
        }

        break;
    }

    return result;
}

char *
sr_core_thread_to_json(struct sr_core_thread *thread, bool is_crash_thread)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();
    if (thread->frames)
    {
        if (is_crash_thread)
        {
            sr_strbuf_append_str(strbuf, "{   \"crash_thread\": true\n");
            sr_strbuf_append_str(strbuf, ",");
        }
        else
            sr_strbuf_append_str(strbuf, "{");
        sr_strbuf_append_str(strbuf, "   \"frames\":\n");

        struct sr_core_frame *frame = thread->frames;
        while (frame)
        {
            if (frame == thread->frames)
                sr_strbuf_append_str(strbuf, "      [ ");
            else
                sr_strbuf_append_str(strbuf, "      , ");

            char *frame_json = sr_core_frame_to_json(frame);
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

    return sr_strbuf_free_nobuf(strbuf);
}
