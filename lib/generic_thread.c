/*
    generic_thread.c

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

#include "report_type.h"
#include "internal_utils.h"
#include "generic_frame.h"
#include "generic_thread.h"
#include "stacktrace.h"
#include "sha1.h"
#include "strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

//XXX
/* Note that python and koops do not have multiple threads, thus the functions
 * here operate directly on the stacktrace structures. */

int
thread_frame_count(struct sr_thread *thread)
{
    struct sr_frame *frame = sr_thread_frames(thread);
    int count = 0;
    while (frame)
    {
        frame = sr_frame_next(frame);
        ++count;
    }

    return count;
}

bool
thread_remove_frame(struct sr_thread *thread, struct sr_frame *frame)
{
    struct sr_frame *loop_frame = sr_thread_frames(thread),
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                sr_frame_set_next(prev_frame, sr_frame_next(loop_frame));
            else
                sr_thread_set_frames(thread, sr_frame_next(loop_frame));

            sr_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = sr_frame_next(loop_frame);
    }

    return false;
}

bool
thread_remove_frames_above(struct sr_thread *thread, struct sr_frame *frame)
{
    /* Check that the frame is present in the thread. */
    struct sr_frame *loop_frame = sr_thread_frames(thread);
    while (loop_frame)
    {
        if (loop_frame == frame)
            break;

        loop_frame = sr_frame_next(loop_frame);
    }

    if (!loop_frame)
        return false;

    /* Delete all the frames up to the frame. */
    while (sr_thread_frames(thread) != frame)
    {
        loop_frame = sr_frame_next(sr_thread_frames(thread));
        sr_frame_free(sr_thread_frames(thread));
        sr_thread_set_frames(thread, loop_frame);
    }

    return true;
}

struct sr_thread *
thread_no_next_thread(struct sr_thread *thread)
{
    return NULL;
}

void
thread_no_bthash_text(struct sr_thread *thread, enum sr_bthash_flags flags,
                      struct sr_strbuf *strbuf)
{
    /* nop */
}

void
thread_no_normalization(struct sr_thread *thread)
{
    /* nop */
}

/* Initialize dispatch table. */

/* Table that maps type-specific functions to the corresponding report types.
 */
static struct thread_methods* dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] = &core_thread_methods,
    [SR_REPORT_PYTHON] = &python_thread_methods,
    [SR_REPORT_KERNELOOPS] = &koops_thread_methods,
    [SR_REPORT_JAVA] = &java_thread_methods,
    [SR_REPORT_GDB] = &gdb_thread_methods,
    [SR_REPORT_RUBY] = &ruby_thread_methods,
    [SR_REPORT_JAVASCRIPT] = &js_thread_methods,
};

struct sr_frame *
sr_thread_frames(struct sr_thread *thread)
{
    return DISPATCH(dtable, thread->type, frames)(thread);
}

void
sr_thread_set_frames(struct sr_thread *thread, struct sr_frame *frame)
{
    assert(frame == NULL || thread->type == frame->type);
    DISPATCH(dtable, thread->type, set_frames)(thread, frame);
}

int
sr_thread_frame_count(struct sr_thread *thread)
{
    return DISPATCH(dtable, thread->type, frame_count)(thread);
}

int
sr_thread_cmp(struct sr_thread *t1, struct sr_thread *t2)
{
    if (t1->type != t2->type)
        return t1->type - t2->type;

    return DISPATCH(dtable, t1->type, cmp)(t1, t2);
}

struct sr_thread *
sr_thread_next(struct sr_thread *thread)
{
    return DISPATCH(dtable, thread->type, next)(thread);
}

void
sr_thread_set_next(struct sr_thread *cur, struct sr_thread *next)
{
    assert(next == NULL || cur->type == next->type);
    DISPATCH(dtable, cur->type, set_next)(cur, next);
}

void
thread_append_bthash_text(struct sr_thread *thread, enum sr_bthash_flags flags,
                          struct sr_strbuf *strbuf)
{
    DISPATCH(dtable, thread->type, thread_append_bthash_text)
            (thread, flags, strbuf);
}

void
sr_thread_free(struct sr_thread *thread)
{
    if (!thread)
        return;

    DISPATCH(dtable, thread->type, thread_free)(thread);
}

bool
sr_thread_remove_frame(struct sr_thread *thread, struct sr_frame *frame)
{
    assert(thread->type == frame->type);
    return DISPATCH(dtable, thread->type, remove_frame)(thread, frame);
}

bool
sr_thread_remove_frames_above(struct sr_thread *thread, struct sr_frame *frame)
{
    assert(thread->type == frame->type);
    return DISPATCH(dtable, thread->type, remove_frames_above)(thread, frame);
}

struct sr_thread*
sr_thread_dup(struct sr_thread *thread)
{
    return DISPATCH(dtable, thread->type, thread_dup)(thread);
}

void
sr_thread_normalize(struct sr_thread *thread)
{
    DISPATCH(dtable, thread->type, normalize)(thread);
}

char *
sr_thread_get_duphash(struct sr_thread *thread, int nframes, char *prefix,
                      enum sr_duphash_flags flags)
{
    char *ret;
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Normalization is destructive, we need to make a copy. */
    thread = sr_thread_dup(thread);

    if (!(flags & SR_DUPHASH_NONORMALIZE))
        sr_thread_normalize(thread);

    /* User supplied hash text prefix. */
    if (prefix)
        sr_strbuf_append_str(strbuf, prefix);

    /* Here would be the place to append thread-specific information. However,
     * no current problem type has any for duphash. So we just append
     * "Thread\n" here because that is what btparser does, in order to produce
     * compatible hashes at least for gdb problems. */
    /*
    DISPATCH(dtable, thread->type, thread_append_duphash_text)
            (thread, flags, strbuf);
    */
    if (!(flags & SR_DUPHASH_KOOPS_COMPAT))
        sr_strbuf_append_str(strbuf, "Thread\n");

    /* Number of nframes, (almost) not limited if nframes = 0. */
    if (nframes == 0)
        nframes = INT_MAX;

    for (struct sr_frame *frame = sr_thread_frames(thread);
         frame && nframes > 0;
         frame = sr_frame_next(frame))
    {
        size_t prev_len = strbuf->len;

        frame_append_duphash_text(frame, flags, strbuf);

        /* Don't count the frame if nothing was appended. */
        if (strbuf->len > prev_len)
            nframes--;
    }

    if ((flags & SR_DUPHASH_KOOPS_COMPAT) && strbuf->len == 0)
    {
        sr_strbuf_free(strbuf);
        ret = NULL;
    }
    else if (flags & SR_DUPHASH_NOHASH)
        ret = sr_strbuf_free_nobuf(strbuf);
    else
    {
        ret = sr_sha1_hash_string(strbuf->buf);
        sr_strbuf_free(strbuf);
    }

    sr_thread_free(thread);

    return ret;
}
