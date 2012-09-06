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
#ifndef BTPARSER_CORE_THREAD_H
#define BTPARSER_CORE_THREAD_H

/**
 * @file
 * @brief Single thread of execution of a core stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct btp_core_frame;
struct btp_strbuf;
struct btp_location;

/**
 * @brief A thread of execution on call stack of a core dump.
 */
struct btp_core_thread
{
    /**
     * Thread's frames, starting from the top of the stack.
     */
    struct btp_core_frame *frames;

    /**
     * A sibling thread, or NULL if this is the last thread in a
     * stacktrace.
     */
    struct btp_core_thread *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_core_thread_free().
 */
struct btp_core_thread *
btp_core_thread_new();

/**
 * Initializes all members of the thread to default values.
 * No memory is released, members are simply overwritten.
 * This is useful for initializing a thread structure placed on the
 * stack.
 */
void
btp_core_thread_init(struct btp_core_thread *thread);

/**
 * Releases the memory held by the thread. The thread siblings are not
 * released.
 * @param thread
 * If thread is NULL, no operation is performed.
 */
void
btp_core_thread_free(struct btp_core_thread *thread);

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
struct btp_core_thread *
btp_core_thread_dup(struct btp_core_thread *thread,
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
btp_core_thread_cmp(struct btp_core_thread *thread1,
                    struct btp_core_thread *thread2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' thread.  If 'dest' is NULL, it
 * returns the 'item' frame.
 */
struct btp_core_thread *
btp_core_thread_append(struct btp_core_thread *dest,
                       struct btp_core_thread *item);

/**
 * Returns the number of frames in the thread.
 */
int
btp_core_thread_get_frame_count(struct btp_core_thread *thread);

/**
 * Appends a textual representation of a thread to a string buffer.
 */
void
btp_core_thread_append_to_str(struct btp_core_thread *thread,
                              struct btp_strbuf *dest);

#ifdef __cplusplus
}
#endif

#endif
