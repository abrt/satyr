/*
    generic_frame.c

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

#include <stdlib.h>

#include "report_type.h"
#include "internal_utils.h"
#include "generic_frame.h"
#include "generic_thread.h"
#include "stacktrace.h"

/* Initialize dispatch table. */
static struct frame_methods* dtable[SR_REPORT_NUM] =
{
    [SR_REPORT_CORE] = &core_frame_methods,
    [SR_REPORT_PYTHON] = &python_frame_methods,
    [SR_REPORT_KERNELOOPS] = &koops_frame_methods,
    [SR_REPORT_JAVA] = &java_frame_methods,
    [SR_REPORT_GDB] = &gdb_frame_methods,
    [SR_REPORT_RUBY] = &ruby_frame_methods,
    [SR_REPORT_JAVASCRIPT] = &js_frame_methods,
};

void
sr_frame_append_to_str(struct sr_frame *frame, struct sr_strbuf *strbuf)
{
    DISPATCH(dtable, frame->type, append_to_str)(frame, strbuf);
}

struct sr_frame *
sr_frame_next(struct sr_frame *frame)
{
    return DISPATCH(dtable, frame->type, next)(frame);
}

void
sr_frame_set_next(struct sr_frame *cur, struct sr_frame *next)
{
    assert(next == NULL || cur->type == next->type);
    DISPATCH(dtable, cur->type, set_next)(cur, next);
}

int
sr_frame_cmp(struct sr_frame *frame1, struct sr_frame *frame2)
{
    if (frame1->type != frame2->type)
        return frame1->type - frame2->type;

    return DISPATCH(dtable, frame1->type, cmp)(frame1, frame2);
}

int
sr_frame_cmp_distance(struct sr_frame *frame1, struct sr_frame *frame2)
{
    if (frame1->type != frame2->type)
        return frame1->type - frame2->type;

    return DISPATCH(dtable, frame1->type, cmp_distance)(frame1, frame2);
}

void
frame_append_bthash_text(struct sr_frame *frame, enum sr_bthash_flags flags,
                         struct sr_strbuf *strbuf)
{
    DISPATCH(dtable, frame->type, frame_append_bthash_text)
            (frame, flags, strbuf);
}

void
frame_append_duphash_text(struct sr_frame *frame, enum sr_duphash_flags flags,
                          struct sr_strbuf *strbuf)
{
    DISPATCH(dtable, frame->type, frame_append_duphash_text)
            (frame, flags, strbuf);
}

void sr_frame_free(struct sr_frame *frame)
{
    if (!frame)
        return;

    DISPATCH(dtable, frame->type, frame_free)(frame);
}
