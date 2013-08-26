/*
    java_frame.h

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
#ifndef SATYR_JAVA_FRAME_H
#define SATYR_JAVA_FRAME_H

/**
 * @file
 * @brief java frame structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <stdbool.h>
#include <stdint.h>

struct sr_strbuf;
struct sr_location;
struct sr_json_value;

struct sr_java_frame
{
    enum sr_report_type type;

    /**
     * FQDN - Fully qualified domain name. Can be NULL.
     *
     * Method:
     *   <Namespace>.<Type>.<Function name>
     *
     * Exception:
     *   <Namespace>.<Type>
     */
    char *name;

    /**
     * A Java file. Can be NULL.
     * (Always NULL if frame is exception)
     */
    char *file_name;

    /**
     * Line no. in the Java file. 0 is used when file_line is missing.
     * (Always 0 if frame is exception)
     */
    uint32_t file_line;

    /**
     * A path to jar file or class file. Can be NULL.
     * (Always NULL if frame is exception)
     */
    char *class_path;

    /**
     * True if method is native.
     * (Always false if frame is exception)
     */
    bool is_native;

    /**
     * True if frame is exception.
     */
    bool is_exception;

    /**
     * Exception message. Can be NULL.
     * (Always NULL if frame is NOT exception).
     */
    char *message;

    struct sr_java_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_java_frame_free().
 */
struct sr_java_frame *
sr_java_frame_new();

/**
 * Creates and initializes a new exception in frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_java_frame_free().
 */
struct sr_java_frame *
sr_java_frame_new_exception();

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
sr_java_frame_init(struct sr_java_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
sr_java_frame_free(struct sr_java_frame *frame);

/**
 * Releases the memory held by the frame all its siblings.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
sr_java_frame_free_full(struct sr_java_frame *frame);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' frame.
 */
struct sr_java_frame *
sr_java_frame_append(struct sr_java_frame *dest,
                     struct sr_java_frame *item);

/**
 * Gets a number of frame in list.
 * @param frame
 * If the frame is NULL, no operation is performed.
 * @returns
 * A number of frames.
 */
struct sr_java_frame *
sr_java_frame_get_last(struct sr_java_frame *frame);

/**
 * Creates a duplicate of the frame.
 * @param frame
 * It must be non-NULL pointer. The frame is not modified by calling
 * this function.
 * @param siblings
 * Whether to duplicate also siblings referenced by frame->next.  If
 * false, frame->next is not duplicated for the new frame, but it is
 * set to NULL.
 * @returns
 * This function never returns NULL. The returned duplicate frame must
 * be released by calling the function sr_java_frame_free().
 */
struct sr_java_frame *
sr_java_frame_dup(struct sr_java_frame *frame,
                  bool siblings);

/**
 * Compares two frames.
 * @param frame1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param frame2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * Returns 0 if the frames are same.  Returns negative number if
 * frame1 is found to be 'less' than frame2.  Returns positive number
 * if frame1 is found to be 'greater' than frame2.
 */
int
sr_java_frame_cmp(struct sr_java_frame *frame1,
                  struct sr_java_frame *frame2);

/**
 * Compares two frames.
 * @param frame1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param frame2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * Returns 0 if the frames are same.  Returns negative number if
 * frame1 is found to be 'less' than frame2.  Returns positive number
 * if frame1 is found to be 'greater' than frame2.
 */
int
sr_java_frame_cmp_distance(struct sr_java_frame *frame1,
                           struct sr_java_frame *frame2);


/**
 * Appends the textual representation of the frame to the string
 * buffer.
 * @param frame
 * It must be a non-NULL pointer.  It's not modified by calling this
 * function.
 */
void
sr_java_frame_append_to_str(struct sr_java_frame *frame,
                            struct sr_strbuf *dest);

/**
 * If the input contains proper exception with frames, parse the exception,
 * move the input pointer after the exception, and return a structure
 * representing the exception. Otherwise to not modify the input pointer
 * and return NULL.
 * @param location
 * The caller must provide a pointer to struct sr_location here.  The
 * line and column members are gradually increased as the parser
 * handles the input, keep this in mind to get reasonable values.
 * When this function returns NULL (an error occurred), the structure
 * will contain the error line, column, and message.
 * @returns
 * NULL or newly allocated structure, which should be released by
 * calling sr_java_frame_free().
 */

struct sr_java_frame *
sr_java_frame_parse_exception(const char **input,
                              struct sr_location *location);

/**
 * If the input contains a complete frame, this function parses the
 * frame text, returns it in a structure, and moves the input pointer
 * after the frame.  If the input does not contain proper, complete
 * frame, the function does not modify input and returns NULL.
 * @returns
 * Allocated pointer with a frame structure. The pointer should be
 * released by sr_java_frame_free().
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  When this function returns NULL, the structure will contain
 * the error line, column, and message.  The line and column members
 * of the location are gradually increased as the parser handles the
 * input, so the location should be initialized before calling this
 * function to get reasonable values.
 */
struct sr_java_frame *
sr_java_frame_parse(const char **input,
                    struct sr_location *location);

/**
 * Returns a textual representation of the frame.
 * @param frame
 * It must be a non-NULL pointer.  It's not modified by calling this
 * function.
 */
char *
sr_java_frame_to_json(struct sr_java_frame *frame);

/**
 * Deserializes frame structure from JSON representation.
 * @param root
 * JSON value to be deserialized.
 * @param error_message
 * On error, *error_message will contain the description of the error.
 * @returns
 * Resulting frame, or NULL on error.
 */
struct sr_java_frame *
sr_java_frame_from_json(struct sr_json_value *root, char **error_message);

#ifdef __cplusplus
}
#endif

#endif
