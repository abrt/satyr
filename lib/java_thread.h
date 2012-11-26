/*
    java_thread.h

    Copyright (C) 2012  ABRT Team
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
#ifndef BTPARSER_JAVA_THREAD_H
#define BTPARSER_JAVA_THREAD_H

/**
 * @file
 * @brief Single thread of execution of JAVA stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct btp_java_frame;
struct btp_strbuf;
struct btp_location;

/**
 * @brief A thread of execution of a JAVA-produced stack trace.
 *
 * Represents a thread containing frames.
 */
struct btp_java_thread
{
    /**
     * Thread name. Can be NULL
     */
    char *name;

    /**
     * Thread's exceptiopn. Can be NULL
     */
    struct btp_java_frame *frames;

    /**
     * A sibling thread, or NULL if this is the last thread in a
     * stacktrace.
     */
    struct btp_java_thread *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_java_thread_free().
 */
struct btp_java_thread *
btp_java_thread_new();

/**
 * Initializes all members of the thread to default values.
 * No memory is released, members are simply overwritten.
 * This is useful for initializing a thread structure placed on the
 * stack.
 */
void
btp_java_thread_init(struct btp_java_thread *thread);

/**
 * Releases the memory held by the thread. The thread siblings are not
 * released.
 * @param thread
 * If thread is NULL, no operation is performed.
 */
void
btp_java_thread_free(struct btp_java_thread *thread);

/**
 * Creates a duplicate of the thread.
 * @param thread
 * It must be non-NULL pointer. The thread is not modified by calling
 * this function.
 * @param siblings
 * Whether to duplicate also siblings referenced by thread->next. If
 * false, thread->next is not duplicated for the new exception, but it is
 * set to NULL.
 */
struct btp_java_thread *
btp_java_thread_dup(struct btp_java_thread *thread,
                    bool siblings);

/**
 * Compares two threads. When comparing the threads, it compares also
 * their frames.
 * @returns
 * Returns 0 if the threads are same.  Returns negative number if t1
 * is found to be 'less' than t2.  Returns positive number if t1 is
 * found to be 'greater' than t2.
 */
int
btp_java_thread_cmp(struct btp_java_thread *thread1,
                    struct btp_java_thread *thread2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' thread.
 */
struct btp_java_thread *
btp_java_thread_append(struct btp_java_thread *dest,
                       struct btp_java_thread *item);

/**
 * Returns the number of frames in the thread.
 */
int
btp_java_thread_get_frame_count(struct btp_java_thread *thread);

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
btp_java_thread_quality_counts(struct btp_java_thread *thread,
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
btp_java_thread_quality(struct btp_java_thread *thread);

/**
 * Removes the frame from the thread and then deletes it.
 * @returns
 * True if the frame was found in the thread and removed and deleted.
 * False if the frame was not found in the thread.
 */
bool
btp_java_thread_remove_frame(struct btp_java_thread *thread,
                             struct btp_java_frame *frame);

/**
 * Removes all the frames from the thread that are above certain
 * frame.
 * @returns
 * True if the frame was found, and all the frames that were above the
 * frame in the thread were removed from the thread and then deleted.
 * False if the frame was not found in the thread.
 */
bool
btp_java_thread_remove_frames_above(struct btp_java_thread *thread,
                                    struct btp_java_frame *frame);

/**
 * Keeps only the top n frames in the thread.
 */
void
btp_java_thread_remove_frames_below_n(struct btp_java_thread *thread,
                                      int n);

/**
 * Appends a textual representation of 'thread' to the 'str'.
 */
void
btp_java_thread_append_to_str(struct btp_java_thread *thread,
                              struct btp_strbuf *dest);

/**
 * If the input contains proper thread with frames, parse the thread,
 * move the input pointer after the thread, and return a structure
 * representing the thread.  Otherwise to not modify the input pointer
 * and return NULL.
 * @param location
 * The caller must provide a pointer to struct btp_location here.  The
 * line and column members are gradually increased as the parser
 * handles the input, keep this in mind to get reasonable values.
 * When this function returns NULL (an error occurred), the structure
 * will contain the error line, column, and message.
 * @returns
 * NULL or newly allocated structure, which should be released by
 * calling btp_java_thread_free().
 */
struct btp_java_thread *
btp_java_thread_parse(const char **input,
                      struct btp_location *location);

/**
 * Create a thread from function and library names.
 * @param input
 * String containing function names and library names separated
 * by space, one frame per line.
 * @returns
 * Newly allocated structure, which should be released by
 * calling btp_java_thread_free().
 */
struct btp_java_thread *
btp_java_thread_parse_funs(const char *input);

/**
 * Prepare a string representing thread which contains just the function
 * and library names. This can be used to store only data necessary for
 * comparison.
 * @returns
 * Newly allocated string, which should be released by
 * calling free(). The string can be parsed by btp_java_thread_parse_funs().
 */
char *
btp_java_thread_format_funs(struct btp_java_thread *thread);

#ifdef __cplusplus
}
#endif

#endif
