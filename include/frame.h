/*
    frame.h

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
#ifndef SATYR_FRAME_H
#define SATYR_FRAME_H

/**
 * Functions declared here work with all frame types, i.e.
 * * sr_core_frame
 * * sr_python_frame
 * * sr_koops_frame
 * * sr_java_frame
 * * sr_gdb_frame
 * * sr_ruby_frame
 * * sr_js_frame
 * You may need to explicitly cast the pointer to struct sr_frame in order to
 * avoid compiler warning.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "report_type.h"

struct sr_strbuf;

struct sr_frame
{
   enum sr_report_type type;
};

/**
 * Return the next frame linked from this frame (corresponds to reading the
 * "next" struct member).
 */
struct sr_frame *
sr_frame_next(struct sr_frame *frame);

/**
 * Set the next pointer.
 */
void
sr_frame_set_next(struct sr_frame *cur, struct sr_frame *next);

/**
 * Appends textual representation of the frame to buffer strbuf.
 */
void
sr_frame_append_to_str(struct sr_frame *frame, struct sr_strbuf *strbuf);

/**
 * Compares two frames. Returns 0 on equality. Frames of distinct type are
 * always unequal.
 */
int
sr_frame_cmp(struct sr_frame *t1, struct sr_frame *t2);

/**
 * Compares two frames for the purpose of thread distance computation. Returns
 * 0 on equality. Frames of distinct type are always unequal.
 */
int
sr_frame_cmp_distance(struct sr_frame *t1, struct sr_frame *t2);

void
sr_frame_free(struct sr_frame *frame);

#ifdef __cplusplus
}
#endif

#endif
