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
#include "frame.h"
#include "generic_thread.h"

#include <stdio.h>

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

struct sr_thread *
thread_no_next_thread(struct sr_thread *thread)
{
    return NULL;
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
};

struct sr_frame *
sr_thread_frames(struct sr_thread *thread)
{
    return DISPATCH(dtable, thread->type, frames)(thread);
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
