/*
    gdb_thread.h

    Copyright (C) 2010  Red Hat, Inc.

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
#ifndef SATYR_GDB_THREAD_H
#define SATYR_GDB_THREAD_H

/**
 * @file
 * @brief Single thread of execution of GDB stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

struct sr_gdb_frame;
struct sr_location;
struct sr_gdb_sharedlib;

/**
 * @brief A thread of execution of a GDB-produced stack trace.
 *
 * Represents a thread containing frames.
 */
struct sr_gdb_thread
{
    enum sr_report_type type;

    uint32_t number;

    /**
     * Thread's frames, starting from the top of the stack.
     */
    struct sr_gdb_frame *frames;

    /**
     * A sibling thread, or NULL if this is the last thread in a
     * stacktrace.
     */
    struct sr_gdb_thread *next;

    /**
     * TID of thread that triggered core dump, as seen in the PID namespace in
     * which the thread resides.
     */
    uint32_t tid;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_gdb_thread_free().
 */
struct sr_gdb_thread *
sr_gdb_thread_new(void);

/**
 * Initializes all members of the thread to default values.
 * No memory is released, members are simply overwritten.
 * This is useful for initializing a thread structure placed on the
 * stack.
 */
void
sr_gdb_thread_init(struct sr_gdb_thread *thread);

/**
 * Releases the memory held by the thread. The thread siblings are not
 * released.
 * @param thread
 * If thread is NULL, no operation is performed.
 */
void
sr_gdb_thread_free(struct sr_gdb_thread *thread);

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
struct sr_gdb_thread *
sr_gdb_thread_dup(struct sr_gdb_thread *thread,
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
sr_gdb_thread_cmp(struct sr_gdb_thread *thread1,
                  struct sr_gdb_thread *thread2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' thread.
 */
struct sr_gdb_thread *
sr_gdb_thread_append(struct sr_gdb_thread *dest,
                     struct sr_gdb_thread *item);

/**
 * Counts the number of 'good' frames and the number of all frames in
 * a thread. Good means that the function name is known (so it's not
 * just '??').
 * @param ok_count
 * @param all_count
 * Not zeroed. This function just adds the numbers to ok_count and
 * all_count.
 */
void
sr_gdb_thread_quality_counts(struct sr_gdb_thread *thread,
                             int *ok_count,
                             int *all_count);

/**
 * Returns the quality of the thread. The quality is the ratio of the
 * number of frames with function name fully known to the number of
 * all frames.  This function does not take into account that some
 * frames are more important than others.
 * @param thread
 * Must be a non-NULL pointer. It's not modified in this function.
 * @returns
 * A number between 0 and 1. 0 means the lowest quality, 1 means full
 * thread stacktrace is known. If the thread contains no frames, this
 * function returns 1.
 */
float
sr_gdb_thread_quality(struct sr_gdb_thread *thread);

/**
 * Removes the frame from the thread and then deletes it.
 * @returns
 * True if the frame was found in the thread and removed and deleted.
 * False if the frame was not found in the thread.
 */
bool
sr_gdb_thread_remove_frame(struct sr_gdb_thread *thread,
                           struct sr_gdb_frame *frame);

/**
 * Removes all the frames from the thread that are above certain
 * frame.
 * @returns
 * True if the frame was found, and all the frames that were above the
 * frame in the thread were removed from the thread and then deleted.
 * False if the frame was not found in the thread.
 */
bool
sr_gdb_thread_remove_frames_above(struct sr_gdb_thread *thread,
                                  struct sr_gdb_frame *frame);

/**
 * Keeps only the top n frames in the thread.
 */
void
sr_gdb_thread_remove_frames_below_n(struct sr_gdb_thread *thread,
                                    int n);

/**
 * Appends a textual representation of 'thread' to the 'str'.
 */
void
sr_gdb_thread_append_to_str(struct sr_gdb_thread *thread,
                            GString *dest,
                            bool verbose);

/**
 * If the input contains proper thread with frames, parse the thread,
 * move the input pointer after the thread, and return a structure
 * representing the thread.  Otherwise to not modify the input pointer
 * and return NULL.
 * @param location
 * The caller must provide a pointer to struct sr_location here.  The
 * line and column members are gradually increased as the parser
 * handles the input, keep this in mind to get reasonable values.
 * When this function returns NULL (an error occurred), the structure
 * will contain the error line, column, and message.
 * @returns
 * NULL or newly allocated structure, which should be released by
 * calling sr_gdb_thread_free().
 */
struct sr_gdb_thread *
sr_gdb_thread_parse(const char **input,
                    struct sr_location *location);

/**
 * If the input contains a LWP section in form of "(LWP [0-9]+), move
 * the input pointer after this section. Otherwise do not modify
 * input.
 * @param tid
 * Can be NULL. If not NULL, the parsed [0-9]+ value is stored there.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a LWP section.
 */
int
sr_gdb_thread_parse_lwp(const char **input, uint32_t *tid);

/**
 * This functions is an alias to sr_gdb_thread_parse_lwp(input, NULL);
 */
int
sr_gdb_thread_skip_lwp(const char **input);

/**
 * Create a thread from function and library names.
 * @param input
 * String containing function names and library names separated
 * by space, one frame per line.
 * @returns
 * Newly allocated structure, which should be released by
 * calling sr_gdb_thread_free().
 */
struct sr_gdb_thread *
sr_gdb_thread_parse_funs(const char *input);

/**
 * Prepare a string representing thread which contains just the function
 * and library names. This can be used to store only data necessary for
 * comparison.
 * @returns
 * Newly allocated string, which should be released by
 * calling free(). The string can be parsed by sr_gdb_thread_parse_funs().
 */
char *
sr_gdb_thread_format_funs(struct sr_gdb_thread *thread);

/**
 * Set library names in all frames in the thread according to the
 * the sharedlib data.
 */
void
sr_gdb_thread_set_libnames(struct sr_gdb_thread *thread,
                           struct sr_gdb_sharedlib *libs);

/**
 * Return copy of the thread optimized for comparison.
 */
struct sr_gdb_thread *
sr_gdb_thread_get_optimized(struct sr_gdb_thread *thread,
                            struct sr_gdb_sharedlib *libs, int max_frames);
#ifdef __cplusplus
}
#endif

#endif
