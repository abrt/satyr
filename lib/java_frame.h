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
#ifndef BTPARSER_java_FRAME_H
#define BTPARSER_java_FRAME_H

/**
 * @file
 * @brief java frame structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct btp_java_frame
{
    /**
     * a Java file. Can be NULL
     */
    char *file_name;

    /**
     * Line no. in the Java file
     */
    uint32_t file_line;

    /**
     * A path to jar file or class file. Can be NULl
     */
    char *class_path;

    /**
     * FQDN - Fully qualified domain name. Can be NULL
     * <Namespace>.<Type>.<Function name>
     */
    char *function_name;

    struct btp_java_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_java_frame_free().
 */
struct btp_java_frame *
btp_java_frame_new();

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
btp_java_frame_init(struct btp_java_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
btp_java_frame_free(struct btp_java_frame *frame);

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
 * be released by calling the function btp_java_frame_free().
 */
struct btp_java_frame *
btp_java_frame_dup(struct btp_java_frame *frame,
                   bool siblings);

#ifdef __cplusplus
}
#endif

#endif
