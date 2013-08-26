/*
    core_thread.h

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
#ifndef SATYR_CORE_THREAD_H
#define SATYR_CORE_THREAD_H

/**
 * @file
 * @brief Single thread of execution of a core stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <stdbool.h>
#include <inttypes.h>

struct sr_core_frame;
struct sr_strbuf;
struct sr_location;
struct sr_json_value;

/**
 * @brief A thread of execution on call stack of a core dump.
 */
struct sr_core_thread
{
    enum sr_report_type type;

    /** Thread id. */
    int64_t id;

    /**
     * Thread's frames, starting from the top of the stack.
     */
    struct sr_core_frame *frames;

    /**
     * A sibling thread, or NULL if this is the last thread in a
     * stacktrace.
     */
    struct sr_core_thread *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_core_thread_free().
 */
struct sr_core_thread *
sr_core_thread_new();

/**
 * Initializes all members of the thread to default values.
 * No memory is released, members are simply overwritten.
 * This is useful for initializing a thread structure placed on the
 * stack.
 */
void
sr_core_thread_init(struct sr_core_thread *thread);

/**
 * Releases the memory held by the thread. The thread siblings are not
 * released.  Thread frames are released.
 * @param thread
 * If thread is NULL, no operation is performed.
 */
void
sr_core_thread_free(struct sr_core_thread *thread);

/**
 * Creates a duplicate of the thread.
 * @param thread
 * It must be non-NULL pointer. The thread is not modified by calling
 * this function.
 * @param siblings
 * Whether to duplicate also siblings referenced by thread->next. If
 * false, thread->next is not duplicated for the new frame, but it is
 * set to NULL.
 */
struct sr_core_thread *
sr_core_thread_dup(struct sr_core_thread *thread,
                   bool siblings);

/**
 * Compares two threads. When comparing the threads, it compares also
 * their frames, including the frame numbers.
 * @returns
 * Returns 0 if the threads are same.  Returns negative number if t1
 * is found to be 'less' than t2.  Returns positive number if t1 is
 * found to be 'greater' than t2.
 */
int
sr_core_thread_cmp(struct sr_core_thread *thread1,
                   struct sr_core_thread *thread2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' thread.  If 'dest' is NULL, it
 * returns the 'item' frame.
 */
struct sr_core_thread *
sr_core_thread_append(struct sr_core_thread *dest,
                      struct sr_core_thread *item);

struct sr_core_frame *
sr_core_thread_find_exit_frame(struct sr_core_thread *thread);

/**
 * Deserializes thread from JSON representation.
 * @param root
 * JSON value to be deserialized.
 * @param error_message
 * On error, *error_message will contain the description of the error.
 * @returns
 * Resulting thread, or NULL on error.
 */
struct sr_core_thread *
sr_core_thread_from_json(struct sr_json_value *root,
                         char **error_message);

char *
sr_core_thread_to_json(struct sr_core_thread *thread,
                       bool is_crash_thread);

#ifdef __cplusplus
}
#endif

#endif
