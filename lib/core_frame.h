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
#ifndef BTPARSER_CORE_FRAME_H
#define BTPARSER_CORE_FRAME_H

/**
 * @file
 * @brief Single frame of core stack trace thread.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

struct btp_strbuf;
struct btp_location;

/**
 * A frame representing a function call on a call stack of a thread.
 */
struct btp_core_frame
{
    /** Address of the machine code in memory.  This is useful only
     * when build_id is not present for some reason.  For example,
     * this might be a null dereference (address is 0) or calling a
     * method from null class pointer (address is a low number --
     * offset to the class).
     *
     * Some programs generate machine code during runtime (JavaScript
     * engines, JVM, the Gallium llvmpipe driver).
     */
    uint64_t   address;

    /** Build id of the ELF binary.  It might be NULL if the frame
     * does not point to memory with code. */
    char      *build_id;
    // in ELF section
    uint64_t   build_id_offset;
    // function name
    char      *function_name;
    // so or executable
    char      *file_name;
    /** Hash of the function contents. */
    char      *fingerprint;

    /**
     * A sibling frame residing below this one, or NULL if this is the
     * last frame in the parent thread.
     */
    struct btp_core_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_core_frame_free().
 */
struct btp_core_frame *
btp_core_frame_new();

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
btp_core_frame_init(struct btp_core_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
btp_core_frame_free(struct btp_core_frame *frame);

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
 * btp_gdb_frame_free().
 */
struct btp_core_frame *
btp_core_frame_dup(struct btp_core_frame *frame,
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
btp_core_frame_cmp(struct btp_core_frame *frame1,
                   struct btp_core_frame *frame2);


/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' frame.  If 'dest' is NULL, it
 * returns the 'item' frame.
 */
struct btp_core_frame *
btp_core_frame_append(struct btp_core_frame *dest,
                      struct btp_core_frame *item);

/**
 * Appends the textual representation of the frame to the string
 * buffer.
 * @param frame
 * It must be a non-NULL pointer.  It's not modified by calling this
 * function.
 */
void
btp_core_frame_append_to_str(struct btp_core_frame *frame,
                             struct btp_strbuf *dest);

#ifdef __cplusplus
}
#endif

#endif
