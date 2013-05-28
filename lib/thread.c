/*
    stacktrace.h

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
#include "utils.h"
#include "thread.h"
#include "frame.h"

#include "gdb_thread.h"
#include "core_thread.h"
#include "python_stacktrace.h"
#include "koops_stacktrace.h"
#include "java_thread.h"

#include <stdio.h>

/* Macro to generate accessors for the "frames" member. */
#define SR_FRAMES_FUNC(name, concrete_t)                        \
    static struct sr_frame *                                    \
    name(struct sr_frame *node)                                 \
    {                                                           \
        return (struct sr_frame *)((concrete_t *)node)->frames; \
    }                                                           \

/* Note that python and koops do not have multiple threads, thus the functions
 * here operate directly on the stacktrace structures. */
SR_FRAMES_FUNC(core_frames, struct sr_core_thread)
SR_FRAMES_FUNC(python_frames, struct sr_python_stacktrace)
SR_FRAMES_FUNC(koops_frames, struct sr_koops_stacktrace)
SR_FRAMES_FUNC(java_frames, struct sr_java_thread)
SR_FRAMES_FUNC(gdb_frames, struct sr_gdb_thread)

SR_NEXT_FUNC(core_next, struct sr_thread, struct sr_core_thread)
SR_NEXT_FUNC(java_next, struct sr_thread, struct sr_java_thread)
SR_NEXT_FUNC(gdb_next, struct sr_thread, struct sr_gdb_thread)

static struct sr_thread *
no_next_thread(struct sr_thread *thread)
{
    return NULL;
}

/* Initialize dispatch table. */

typedef struct sr_frame* (*frames_fn_t)(struct sr_thread*);
typedef int (*cmp_fn_t)(struct sr_thread*, struct sr_thread*);
typedef struct sr_thread* (*next_fn_t)(struct sr_thread*);

struct methods
{
    frames_fn_t frames;
    cmp_fn_t cmp;
    next_fn_t next;
};

/* Table that maps type-specific functions to the corresponding report types.
 */
static struct methods dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] =
    {
        .frames = (frames_fn_t) core_frames,
        .cmp = (cmp_fn_t) sr_core_thread_cmp,
        .next = (next_fn_t) core_next,
    },
    [SR_REPORT_PYTHON] =
    {
        .frames = (frames_fn_t) python_frames,
        .cmp = (cmp_fn_t) NULL,
        .next = (next_fn_t) no_next_thread,
    },
    [SR_REPORT_KERNELOOPS] =
    {
        .frames = (frames_fn_t) koops_frames,
        .cmp = (cmp_fn_t) NULL,
        .next = (next_fn_t) no_next_thread,
    },
    [SR_REPORT_JAVA] =
    {
        .frames = (frames_fn_t) java_frames,
        .cmp = (cmp_fn_t) sr_java_thread_cmp,
        .next = (next_fn_t) java_next,
    },
    [SR_REPORT_GDB] =
    {
        .frames = (frames_fn_t) gdb_frames,
        .cmp = (cmp_fn_t) sr_gdb_thread_cmp,
        .next = (next_fn_t) gdb_next,
    },
};

struct sr_frame *
sr_thread_frames(struct sr_thread *thread)
{
    return SR_DISPATCH(thread->type, frames)(thread);
}

int
sr_thread_cmp(struct sr_thread *t1, struct sr_thread *t2)
{
    if (t1->type != t2->type)
        return t1->type - t2->type;

    return SR_DISPATCH(t1->type, cmp)(t1, t2);
}

struct sr_thread *
sr_thread_next(struct sr_thread *thread)
{
    return SR_DISPATCH(thread->type, next)(thread);
}
