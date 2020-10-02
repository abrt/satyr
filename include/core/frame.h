/*
    core_frame.h

    Copyright (C) 2011, 2012  Red Hat, Inc.

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
#ifndef SATYR_CORE_FRAME_H
#define SATYR_CORE_FRAME_H

/**
 * @file
 * @brief Single frame of core stack trace thread.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <inttypes.h>
#include <json.h>
#include <stdbool.h>
#include <glib.h>

struct sr_location;

/**
 * @brief A function call on call stack of a core dump.
 */
struct sr_core_frame
{
    enum sr_report_type type;

    /** Address of the machine code in memory.  This is useful only
     * when build_id is not present for some reason.  For example,
     * this might be a null dereference (address is 0) or calling a
     * method from null class pointer (address is a low number --
     * offset to the class).
     *
     * The address might also be unknown, in which case this field is
     * equal to UINT64_MAX = 2^64 - 1.
     *
     * Some programs generate machine code during runtime (JavaScript
     * engines, JVM, the Gallium llvmpipe driver).
     */
    uint64_t address;

    /** Build id of the ELF binary.  It might be NULL if the frame
     * does not point to memory with code.
     */
    char *build_id;
    // in ELF section
    uint64_t build_id_offset;
    // function name
    char *function_name;
    // so or executable
    char *file_name;
    /** Fingerprint of the function contents, optionally hashed */
    char *fingerprint;

    /** Is the fingerprint hashed or raw? */
    bool fingerprint_hashed;

    /**
     * A sibling frame residing below this one, or NULL if this is the
     * last frame in the parent thread.
     */
    struct sr_core_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_core_frame_free().
 */
struct sr_core_frame *
sr_core_frame_new(void);

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
sr_core_frame_init(struct sr_core_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
sr_core_frame_free(struct sr_core_frame *frame);

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
 * This function never returns NULL. If the returned duplicate is not
 * shallow, it must be released by calling the function
 * sr_gdb_frame_free().
 */
struct sr_core_frame *
sr_core_frame_dup(struct sr_core_frame *frame,
                  bool siblings);

/**
 * Checks whether the frame represents a call of function with certain
 * function name.
 * @param frame
 *   A stack trace frame.
 * @param ...
 *   Names of executables or shared libaries that should contain the
 *   function name.  The list needs to be terminated by NULL.  Just
 *   NULL will cause ANY file name to match and succeed.  The name
 *   of file is searched as a substring.
 * @returns
 *   True if the frame corresponds to a function with function_name,
 *   residing in a specified executable or shared binary.
 */
bool
sr_core_frame_calls_func(struct sr_core_frame *frame,
                         const char *function_name,
                         ...);

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
sr_core_frame_cmp(struct sr_core_frame *frame1,
                  struct sr_core_frame *frame2);

/**
 * Compares two frames for thread distance calculations.
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
sr_core_frame_cmp_distance(struct sr_core_frame *frame1,
                           struct sr_core_frame *frame2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' frame.  If 'dest' is NULL, it
 * returns the 'item' frame.
 */
struct sr_core_frame *
sr_core_frame_append(struct sr_core_frame *dest,
                     struct sr_core_frame *item);

/**
 * Returns a textual representation of the frame.
 * @param frame
 * It must be a non-NULL pointer.  It's not modified by calling this
 * function.
 */
char *
sr_core_frame_to_json(struct sr_core_frame *frame);

/**
 * Deserializes frame structure from JSON representation.
 * @param root
 * JSON value to be deserialized.
 * @param error_message
 * On error, *error_message will contain the description of the error.
 * @returns
 * Resulting frame, or NULL on error.
 */
struct sr_core_frame *
sr_core_frame_from_json(json_object *root,
                        char **error_message);

/**
 * Appends textual representation of the frame to the string buffer dest.
 */
void
sr_core_frame_append_to_str(struct sr_core_frame *frame,
                            GString *dest);

#ifdef __cplusplus
}
#endif

#endif
