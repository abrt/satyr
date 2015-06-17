/*
    gdb_thread.c

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
#include "gdb/thread.h"
#include "gdb/frame.h"
#include "gdb/sharedlib.h"
#include "normalize.h"
#include "location.h"
#include "utils.h"
#include "strbuf.h"
#include "generic_thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Method table */

static void
gdb_append_bthash_text(struct sr_gdb_thread *thread, enum sr_bthash_flags flags,
                       struct sr_strbuf *strbuf);

DEFINE_FRAMES_FUNC(gdb_frames, struct sr_gdb_thread)
DEFINE_SET_FRAMES_FUNC(gdb_set_frames, struct sr_gdb_thread)
DEFINE_NEXT_FUNC(gdb_next, struct sr_thread, struct sr_gdb_thread)
DEFINE_SET_NEXT_FUNC(gdb_set_next, struct sr_thread, struct sr_gdb_thread)
DEFINE_DUP_WRAPPER_FUNC(gdb_dup, struct sr_gdb_thread, sr_gdb_thread_dup)

struct thread_methods gdb_thread_methods =
{
    .frames = (frames_fn_t) gdb_frames,
    .set_frames = (set_frames_fn_t) gdb_set_frames,
    .cmp = (thread_cmp_fn_t) sr_gdb_thread_cmp,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) gdb_next,
    .set_next = (set_next_thread_fn_t) gdb_set_next,
    .thread_append_bthash_text =
        (thread_append_bthash_text_fn_t) gdb_append_bthash_text,
    .thread_free = (thread_free_fn_t) sr_gdb_thread_free,
    .remove_frame = (remove_frame_fn_t) thread_remove_frame,
    .remove_frames_above =
        (remove_frames_above_fn_t) thread_remove_frames_above,
    .thread_dup = (thread_dup_fn_t) gdb_dup,
    .normalize = (normalize_fn_t) sr_normalize_gdb_thread,
};

/* Public functions */

struct sr_gdb_thread *
sr_gdb_thread_new()
{
    struct sr_gdb_thread *thread = sr_malloc(sizeof(struct sr_gdb_thread));
    sr_gdb_thread_init(thread);
    return thread;
}

void
sr_gdb_thread_init(struct sr_gdb_thread *thread)
{
    thread->number = -1;
    thread->tid = -1;
    thread->frames = NULL;
    thread->next = NULL;
    thread->type = SR_REPORT_GDB;
}

void
sr_gdb_thread_free(struct sr_gdb_thread *thread)
{
    if (!thread)
        return;

    while (thread->frames)
    {
        struct sr_gdb_frame *frame = thread->frames;
        thread->frames = frame->next;
        sr_gdb_frame_free(frame);
    }

    free(thread);
}

struct sr_gdb_thread *
sr_gdb_thread_dup(struct sr_gdb_thread *thread, bool siblings)
{
    struct sr_gdb_thread *result = sr_gdb_thread_new();
    memcpy(result, thread, sizeof(struct sr_gdb_thread));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_gdb_thread_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    if (result->frames)
        result->frames = sr_gdb_frame_dup(result->frames, true);

    return result;
}

int
sr_gdb_thread_cmp(struct sr_gdb_thread *thread1,
                  struct sr_gdb_thread *thread2)
{
    int number = thread1->number - thread2->number;
    if (number != 0)
        return number;

    struct sr_gdb_frame *frame1 = thread1->frames,
        *frame2 = thread2->frames;
    do {
        if (frame1 && !frame2)
            return 1;
        else if (frame2 && !frame1)
            return -1;
        else if (frame1 && frame2)
        {
            int frames = sr_gdb_frame_cmp(frame1, frame2, true);
            if (frames != 0)
                return frames;
            frame1 = frame1->next;
            frame2 = frame2->next;
        }
    } while (frame1 || frame2);

    return 0;
}

struct sr_gdb_thread *
sr_gdb_thread_append(struct sr_gdb_thread *dest,
                     struct sr_gdb_thread *item)
{
    if (!dest)
        return item;

    struct sr_gdb_thread *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

void
sr_gdb_thread_quality_counts(struct sr_gdb_thread *thread,
                             int *ok_count,
                             int *all_count)
{
    struct sr_gdb_frame *frame = thread->frames;
    while (frame)
    {
        *all_count += 1;
        bool has_name = (frame->function_name
            && 0 != strcmp(frame->function_name, "??"));
        bool has_source = (frame->source_file && *frame->source_file);

        if (frame->signal_handler_called || (has_name && has_source))
        {
            *ok_count += 1;
        }

        frame = frame->next;
    }
}

float
sr_gdb_thread_quality(struct sr_gdb_thread *thread)
{
    int ok_count = 0, all_count = 0;
    sr_gdb_thread_quality_counts(thread, &ok_count, &all_count);
    if (0 == all_count)
        return 1;

    return ok_count / (float)all_count;
}

bool
sr_gdb_thread_remove_frame(struct sr_gdb_thread *thread,
                           struct sr_gdb_frame *frame)
{
    struct sr_gdb_frame *loop_frame = thread->frames,
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                thread->frames = loop_frame->next;

            sr_gdb_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }

    return false;
}

bool
sr_gdb_thread_remove_frames_above(struct sr_gdb_thread *thread,
                                  struct sr_gdb_frame *frame)
{
    /* Check that the frame is present in the thread. */
    struct sr_gdb_frame *loop_frame = thread->frames;
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
        sr_gdb_frame_free(thread->frames);
        thread->frames = loop_frame;
    }

    return true;
}

void
sr_gdb_thread_remove_frames_below_n(struct sr_gdb_thread *thread,
                                    int n)
{
    assert(n >= 0);

    /* Skip some frames to get the required stack depth. */
    int i = n;
    struct sr_gdb_frame *frame = thread->frames,
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
        struct sr_gdb_frame *delete_frame = frame;
        frame = frame->next;
        sr_gdb_frame_free(delete_frame);
    }
}

void
sr_gdb_thread_append_to_str(struct sr_gdb_thread *thread,
                            struct sr_strbuf *dest,
                            bool verbose)
{
    int frame_count = sr_thread_frame_count(
            (struct sr_thread*) thread);
    if (verbose)
    {
        sr_strbuf_append_strf(dest, "Thread no. %"PRIu32" (%d frames)\n",
                              thread->number,
                              frame_count);
    }
    else
        sr_strbuf_append_str(dest, "Thread\n");

    struct sr_gdb_frame *frame = thread->frames;
    while (frame)
    {
        sr_gdb_frame_append_to_str(frame, dest, verbose);
        sr_strbuf_append_char(dest, '\n');
        frame = frame->next;
    }
}

struct sr_gdb_thread *
sr_gdb_thread_parse(const char **input,
                    struct sr_location *location)
{
    const char *local_input = *input;
    struct sr_gdb_thread *imthread = sr_gdb_thread_new();

    /* Read the Thread keyword, which is mandatory. */
    int chars = sr_skip_string(&local_input, "Thread");
    location->column += chars;
    if (0 != chars)
    {
        /* Parse the rest of the Thread header */

        /* Skip spaces, at least one space is mandatory. */
        int spaces = sr_skip_char_sequence(&local_input, ' ');
        location->column += spaces;
        if (0 == spaces)
        {
            location->message = "Space expected after the \"Thread\" keyword.";
            sr_gdb_thread_free(imthread);
            return NULL;
        }

        /* Read thread number. */
        int digits = sr_parse_uint32(&local_input, &imthread->number);
        location->column += digits;
        if (0 == digits)
        {
            location->message = "Thread number expected.";
            sr_gdb_thread_free(imthread);
            return NULL;
        }

        /* Skip spaces after the thread number and before parentheses. */
        spaces = sr_skip_char_sequence(&local_input, ' ');
        location->column += spaces;
        if (0 == spaces)
        {
            location->message = "Space expected after the thread number.";
            sr_gdb_thread_free(imthread);
            return NULL;
        }

        /* Read the LWP section in parentheses, optional. */
        location->column += sr_gdb_thread_parse_lwp(&local_input, &imthread->tid);

        /* Read the Thread keyword in parentheses, optional. */
        chars = sr_skip_string(&local_input, "(Thread ");
        location->column += chars;
        if (0 != chars)
        {
            /* Read the thread identification number. It can be either in
             * decimal or hexadecimal form.
             * Examples:
             * "Thread 10 (Thread 2476):"
             * "Thread 8 (Thread 0xb07fdb70 (LWP 6357)):"
             */
            digits = sr_skip_hexadecimal_0xuint(&local_input);
            if (0 == digits)
                digits = sr_skip_uint(&local_input);
            location->column += digits;
            if (0 == digits)
            {
                location->message = "The thread identification number expected.";
                sr_gdb_thread_free(imthread);
                return NULL;
            }

            /* Handle the optional " (LWP [0-9]+)" section. */
            location->column += sr_skip_char_sequence(&local_input, ' ');
            location->column += sr_gdb_thread_parse_lwp(&local_input, &imthread->tid);

            /* Read the end of the parenthesis. */
            if (!sr_skip_char(&local_input, ')'))
            {
                location->message = "Closing parenthesis for Thread expected.";
                sr_gdb_thread_free(imthread);
                return NULL;
            }
        }

        /* Read the end of the header line. */
        chars = sr_skip_string(&local_input, ":\n");
        if (0 == chars)
        {
            location->message = "Expected a colon followed by a newline ':\\n'.";
            sr_gdb_thread_free(imthread);
            return NULL;
        }
        /* Add the newline from the last sr_skip_string. */
        sr_location_add(location, 2, 0);
    }
    else
    {
        /* No Thread header available, parse as one thread */
        imthread->number = 0;
    }

    /* Read the frames. */
    struct sr_gdb_frame *frame, *prevframe = NULL;
    struct sr_location frame_location;
    sr_location_init(&frame_location);
    while ((frame = sr_gdb_frame_parse(&local_input, &frame_location)))
    {
        if (prevframe)
        {
            sr_gdb_frame_append(prevframe, frame);
            prevframe = frame;
        }
        else
            imthread->frames = prevframe = frame;

        sr_location_add(location,
                         frame_location.line,
                         frame_location.column);
    }

    if (!imthread->frames)
    {
        location->message = frame_location.message;
        sr_gdb_thread_free(imthread);
        return NULL;
    }

    *input = local_input;
    return imthread;
}

int
sr_gdb_thread_parse_lwp(const char **input, uint32_t *tid)
{
    const char *local_input = *input;
    int count = sr_skip_string(&local_input, "(LWP ");
    if (0 == count)
        return 0;

    uint32_t value;
    int digits = sr_parse_uint32(&local_input, &value);
    if (0 == digits)
        return 0;
    count += digits;
    if (!sr_skip_char(&local_input, ')'))
        return 0;
    *input = local_input;
    if (tid != NULL)
        *tid = value;
    return count + 1;
}

int
sr_gdb_thread_skip_lwp(const char **input)
{
    return sr_gdb_thread_parse_lwp(input, /*Ignore value*/NULL);
}

struct sr_gdb_thread *
sr_gdb_thread_parse_funs(const char *input)
{
    const char *next, *libname, *c;
    struct sr_gdb_thread *thread = sr_gdb_thread_new();
    struct sr_gdb_frame *frame, **pframe = &thread->frames;
    int number = 0;

    while (input && *input)
    {
        next = strchr(input + 1, '\n');
        if (!next)
            break;

        /* Search for last space before newline. */
        for (c = input, libname = next; ; libname = c) {
            c = strchr(c + 1, ' ');
            if (!c || c > next)
                break;
        }

        frame = sr_gdb_frame_new();
        frame->function_name = sr_strndup(input, libname - input);
        if (libname + 1 < next)
            frame->library_name = sr_strndup(libname + 1, next - libname - 1);

        input = next + 1;
        frame->number = number++;
        *pframe = frame;
        pframe = &frame->next;
    }

    return thread;
}

char *
sr_gdb_thread_format_funs(struct sr_gdb_thread *thread)
{
    struct sr_gdb_frame *frame = thread->frames;
    struct sr_strbuf *buf = sr_strbuf_new();

    while (frame)
    {
        if (frame->function_name)
        {
            sr_strbuf_append_str(buf, frame->function_name);
            if (frame->library_name)
            {
                sr_strbuf_append_char(buf, ' ');
                sr_strbuf_append_str(buf, frame->library_name);
            }
            sr_strbuf_append_char(buf, '\n');
        }

        frame = frame->next;
    }

    return sr_strbuf_free_nobuf(buf);
}

void
sr_gdb_thread_set_libnames(struct sr_gdb_thread *thread, struct sr_gdb_sharedlib *libs)
{
    struct sr_gdb_frame *frame = thread->frames;
    while (frame)
    {
        struct sr_gdb_sharedlib *lib = sr_gdb_sharedlib_find_address(libs,
                                                                     frame->address);
        if (lib)
        {
            char *s1, *s2;

            /* Strip directory and version after the .so suffix. */
            s1 = strrchr(lib->soname, '/');
            if (!s1)
                s1 = lib->soname;
            else
                s1++;
            s2 = strstr(s1, ".so");
            if (!s2)
                s2 = s1 + strlen(s1);
            else
                s2 += strlen(".so");

            if (frame->library_name)
                free(frame->library_name);
            frame->library_name = sr_strndup(s1, s2 - s1);
        }
        frame = frame->next;
    }
}

struct sr_gdb_thread *
sr_gdb_thread_get_optimized(struct sr_gdb_thread *thread,
                            struct sr_gdb_sharedlib *libs, int max_frames)
{
    thread = sr_gdb_thread_dup(thread, false);

    if (libs)
        sr_gdb_thread_set_libnames(thread, libs);
    sr_normalize_gdb_thread(thread);
    sr_gdb_normalize_optimize_thread(thread);

    /* Remove frames with no function name (i.e. signal handlers). */
    struct sr_gdb_frame *frame = thread->frames, *frame_next;
    while (frame)
    {
        frame_next = frame->next;
        if (!frame->function_name)
            sr_gdb_thread_remove_frame(thread, frame);
        frame = frame_next;
    }

    if (max_frames > 0)
        sr_gdb_thread_remove_frames_below_n(thread, max_frames);

    return thread;
}

static void
gdb_append_bthash_text(struct sr_gdb_thread *thread, enum sr_bthash_flags flags,
                       struct sr_strbuf *strbuf)
{
    sr_strbuf_append_strf(strbuf, "Thread %"PRIu32"\n", thread->number);
}
