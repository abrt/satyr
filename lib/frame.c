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
#include "strbuf.h"
#include "frame.h"

#include "gdb/frame.h"
#include "core/frame.h"
#include "python/frame.h"
#include "koops/frame.h"
#include "java/frame.h"

#include <stdio.h>

SR_NEXT_FUNC(core_next, struct sr_frame, struct sr_core_frame)
SR_NEXT_FUNC(python_next, struct sr_frame, struct sr_python_frame)
SR_NEXT_FUNC(koops_next, struct sr_frame, struct sr_koops_frame)
SR_NEXT_FUNC(java_next, struct sr_frame, struct sr_java_frame)
SR_NEXT_FUNC(gdb_next, struct sr_frame, struct sr_gdb_frame)

/* Initialize dispatch table. */

typedef void (*append_to_str_fn_t)(struct sr_frame *, struct sr_strbuf *);
typedef struct sr_frame* (*next_fn_t)(struct sr_frame *);

struct methods
{
    append_to_str_fn_t append_to_str;
    next_fn_t next;
};

static struct methods dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] =
    {
        .append_to_str = (append_to_str_fn_t) sr_core_frame_append_to_str,
        .next = (next_fn_t) core_next,
    },
    [SR_REPORT_PYTHON] =
    {
        .append_to_str = (append_to_str_fn_t) sr_python_frame_append_to_str,
        .next = (next_fn_t) python_next,
    },
    [SR_REPORT_KERNELOOPS] =
    {
        .append_to_str = (append_to_str_fn_t) sr_koops_frame_append_to_str,
        .next = (next_fn_t) koops_next,
    },
    [SR_REPORT_JAVA] =
    {
        .append_to_str = (append_to_str_fn_t) sr_java_frame_append_to_str,
        .next = (next_fn_t) java_next,
    },
    [SR_REPORT_GDB] =
    {
        .append_to_str = (append_to_str_fn_t) sr_gdb_frame_append_to_str,
        .next = (next_fn_t) gdb_next,
    },
};

void
sr_frame_append_to_str(struct sr_frame *frame, struct sr_strbuf *strbuf)
{
    SR_DISPATCH(frame->type, append_to_str)(frame, strbuf);
}

struct sr_frame *
sr_frame_next(struct sr_frame *frame)
{
    return SR_DISPATCH(frame->type, next)(frame);
}
