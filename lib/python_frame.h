/*
    python_frame.h

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
#ifndef BTPARSER_PYTHON_FRAME_H
#define BTPARSER_PYTHON_FRAME_H

/**
 * @file
 * @brief Python frame structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct btp_location;

struct btp_python_frame
{
    char *file_name;

    uint32_t file_line;

    bool is_module;

    char *function_name;

    char *line_contents;

    struct btp_python_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_python_frame_free().
 */
struct btp_python_frame *
btp_python_frame_new();

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
btp_python_frame_init(struct btp_python_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
btp_python_frame_free(struct btp_python_frame *frame);

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
 * be released by calling the function btp_python_frame_free().
 */
struct btp_python_frame *
btp_python_frame_dup(struct btp_python_frame *frame,
                     bool siblings);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' frame.  If 'dest' is NULL, it
 * returns the 'item' frame.
 */
struct btp_python_frame *
btp_python_frame_append(struct btp_python_frame *dest,
                        struct btp_python_frame *item);

/**
 * If the input contains a complete frame, this function parses the
 * frame text, returns it in a structure, and moves the input pointer
 * after the frame.  If the input does not contain proper, complete
 * frame, the function does not modify input and returns NULL.
 * @returns
 * Allocated pointer with a frame structure. The pointer should be
 * released by btp_python_frame_free().
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  When this function returns NULL, the structure will contain
 * the error line, column, and message.  The line and column members
 * of the location are gradually increased as the parser handles the
 * input, so the location should be initialized before calling this
 * function to get reasonable values.
 */
struct btp_python_frame *
btp_python_frame_parse(const char **input,
                       struct btp_location *location);

/**
 * Returns a textual representation of the frame.
 * @param frame
 * It must be a non-NULL pointer.  It's not modified by calling this
 * function.
 */
char *
btp_python_frame_to_json(struct btp_python_frame *frame);

#ifdef __cplusplus
}
#endif

#endif
