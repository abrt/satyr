/*
    java_exception.h

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
#ifndef BTPARSER_JAVA_EXCEPTION_H
#define BTPARSER_JAVA_EXCEPTION_H

/**
 * @file
 * @brief Single exception of execution of JAVA stack trace.
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
 * @brief A exception of execution of a JAVA-produced stack trace.
 *
 * Represents a exception containing frames.
 */
struct btp_java_exception
{
    /**
     * Exception caught in this exception. Can be NULL
     */
    char *name;

    /**
     * Message delivered by the exception. Can be NULL
     */
    char *message;

    /**
     * exception's frames, starting from the top of the stack.
     */
    struct btp_java_frame *frames;

    /**
     * An inner exception, or NULL if this exception doesn't have an inner
     * exception
     */
    struct btp_java_exception *inner;
};

/**
 * Creates and initializes a new exception structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_java_exception_free().
 */
struct btp_java_exception *
btp_java_exception_new();

/**
 * Initializes all members of the exception to default values.
 * No memory is released, members are simply overwritten.
 * This is useful for initializing a exception structure placed on the
 * stack.
 */
void
btp_java_exception_init(struct btp_java_exception *exception);

/**
 * Releases the memory held by the exception including inner exception.
 * @param exception
 * If exception is NULL, no operation is performed.
 */
void
btp_java_exception_free(struct btp_java_exception *exception);

/**
 * Replaces exception by its inner exception if the exception has one.
 * @param exception
 * It must be non-NULL pointer. The exception is not modified by calling
 * this function.
 * @return TRUE if exceptions was poped, otherwise FASLE
 */
bool
btp_java_exception_pop(struct btp_java_exception *exception);

/**
 * Creates a duplicate of the exception.
 * @param exception
 * It must be non-NULL pointer. The exception is not modified by calling
 * this function.
 * @param deep
 * Whether to duplicate also the inner exception. If
 * false, exception->inner is not duplicated for the new frame, but it is
 * set to NULL.
 */
struct btp_java_exception *
btp_java_exception_dup(struct btp_java_exception *exception,
                       bool deep);

/**
 * Compares two exceptions. When comparing the exceptions, it compares also
 * their frames, including the frame numbers.
 * @returns
 * Returns 0 if the exceptions are same.  Returns negative number if t1
 * is found to be 'less' than t2.  Returns positive number if t1 is
 * found to be 'greater' than t2.
 */
int
btp_java_exception_cmp(struct btp_java_exception *exception1,
                       struct btp_java_exception *exception2,
                       bool deep);

/**
 * Returns the number of frames in the exception.
 */
int
btp_java_exception_get_frame_count(struct btp_java_exception *exception,
                                   bool deep);

/**
 * Counts the number of 'good' frames and the number of all frames in
 * a exception. Good means that the function name is known (so it's not
 * just 'Unknown Source' | 'Native method').
 * @param ok_count
 * @param all_count
 * @param deep
 * If logical true, work out the sum from inner exceptions too.
 * Not zeroed. This function just adds the numbers to ok_count and
 * all_count.
 */
void
btp_java_exception_quality_counts(struct btp_java_exception *exception,
                                  int *ok_count,
                                  int *all_count,
                                  bool deep);

/**
 * Returns the quality of the exception. The quality is the ratio of the
 * number of frames with function name fully known to the number of
 * all frames.  This function does not take into account that some
 * frames are more important than others.
 * @param exception
 * Must be a non-NULL pointer. It's not modified in this function.
 * @param deep
 * If logical true, work out the quality from inner exceptions too.
 * @returns
 * A number between 0 and 1. 0 means the lowest quality, 1 means full
 * exception stacktrace is known. If the exception contains no frames, this
 * function returns 1.
 */
float
btp_java_exception_quality(struct btp_java_exception *exception, bool deep);

/**
 * Removes the frame from the exception and then deletes it.
 * @param exception
 * Modified exception. Non-NULL pointer.
 * @param frame
 * Removed frame. Non-NULL pointer.
 * @param deep
 * If logical true, remove the frame from inner exception if frame is not found
 * in the current one.
 * @returns
 * True if the frame was found in the exception and removed and deleted.
 * False if the frame was not found in the exception.
 */
bool
btp_java_exception_remove_frame(struct btp_java_exception *exception,
                                struct btp_java_frame *frame,
                                bool deep);

/**
 * Removes all the frames from the exception that are above certain
 * frame.
 * @param exception
 * Modified exception. Non-NULL pointer.
 * @param frame
 * A new topmost frame. Non-NULL pointer.
 * @param deep
 * If logical true, remove inner exception because an inner exception
 * is always higher in stack trace.
 * @returns
 * True if the frame was found, and all the frames that were above the
 * frame in the exception were removed from the exception and then deleted.
 * False if the frame was not found in the exception.
 */
bool
btp_java_exception_remove_frames_above(struct btp_java_exception *exception,
                                       struct btp_java_frame *frame,
                                       bool deep);

/**
 * Keeps only the top n frames in the exception.
 * @param exception
 * Modified exception. Non-NULL pointer.
 * @param n
 * A number of left frames
 * @param deep
 * If logical true, take inner exceptions into account. It can leads to
 * replacement of the passed exception byt its inner excpetion.
 * @returns
 * if count of frames is shorter then n returns the diffrences; * otherwise 0
 */
unsigned
btp_java_exception_remove_frames_below_n(struct btp_java_exception *exception,
                                         unsigned n,
                                         bool deep);

/**
 * Appends a textual representation of 'exception' to the 'str'.
 * @param exception
 * Formated exception. Non-NULL pointer.
 * @param dest
 * An output buffer. Non-NULL pointer.
 */
void
btp_java_exception_append_to_str(struct btp_java_exception *exception,
                                 struct btp_strbuf *dest);

/**
 * If the input contains proper exception with frames, parse the exception,
 * move the input pointer after the exception, and return a structure
 * representing the exception.  Otherwise to not modify the input pointer
 * and return NULL.
 * @param location
 * The caller must provide a pointer to struct btp_location here.  The
 * line and column members are gradually increased as the parser
 * handles the input, keep this in mind to get reasonable values.
 * When this function returns NULL (an error occurred), the structure
 * will contain the error line, column, and message.
 * @returns
 * NULL or newly allocated structure, which should be released by
 * calling btp_java_exception_free().
 */
struct btp_java_exception *
btp_java_exception_parse(const char **input,
                         struct btp_location *location);

#ifdef __cplusplus
}
#endif

#endif
