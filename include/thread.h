/*
    thread.h

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
#ifndef SATYR_THREAD_H
#define SATYR_THREAD_H

/**
 * Functions declared here work with all thread types. Furthermore, for problem
 * types that do not have a thread structure because they are always
 * single-threaded, you can pass the stacktrace structure directly:
 * * sr_core_thread
 * * sr_python_stacktrace
 * * sr_koops_stacktrace
 * * sr_java_thread
 * * sr_gdb_thread
 * You may need to explicitly cast the pointer to struct sr_thread in order to
 * avoid compiler warning.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "report_type.h"

struct sr_thread
{
   enum sr_report_type type;
};

/**
 * Returns pointer to the first frame in thread.
 */
struct sr_frame *
sr_thread_frames(struct sr_thread *thread);

/**
 * Set the frame linked list pointer.
 */
void
sr_thread_set_frames(struct sr_thread *thread, struct sr_frame *frames);

/**
 * Returns the number of frames in the thread.
 */
int
sr_thread_frame_count(struct sr_thread *thread);

/**
 * Compares two threads. Returns 0 on equality. Threads of distinct type are
 * always unequal.
 */
int
sr_thread_cmp(struct sr_thread *t1, struct sr_thread *t2);

/**
 * Next thread in linked list (same as reading the "next" structure member).
 */
struct sr_thread *
sr_thread_next(struct sr_thread *thread);

/**
 * Set the next pointer.
 */
void
sr_thread_set_next(struct sr_thread *cur, struct sr_thread *next);

/**
 * Releases the memory held by the thread. The thread siblings are not
 * released.
 * @param thread
 * If thread is NULL, no operation is performed.
 */
void
sr_thread_free(struct sr_thread *thread);

/**
 * Removes the frame from the thread and then deletes it.
 * @returns
 * True if the frame was found in the thread and removed and deleted.
 * False if the frame was not found in the thread.
 */
bool
sr_thread_remove_frame(struct sr_thread *thread, struct sr_frame *frame);

/**
 * Removes all the frames from the thread that are above certain
 * frame.
 * @returns
 * True if the frame was found, and all the frames that were above the
 * frame in the thread were removed from the thread and then deleted.
 * False if the frame was not found in the thread.
 */
bool
sr_thread_remove_frames_above(struct sr_thread *thread, struct sr_frame *frame);

/**
 * Return a copy of the thread (without its siblings).
 */
struct sr_thread *
sr_thread_dup(struct sr_thread *thread);

#ifdef __cplusplus
}
#endif

#endif
