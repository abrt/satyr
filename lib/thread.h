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

#ifdef __cplusplus
}
#endif

#endif
