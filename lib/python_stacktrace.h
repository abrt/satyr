/*
    python_stacktrace.h

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
#ifndef BTPARSER_PYTHON_STACKTRACE_H
#define BTPARSER_PYTHON_STACKTRACE_H

/**
 * @file
 * @brief Python stack trace structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct btp_python_frame;
struct btp_location;

struct btp_python_stacktrace
{
    /* Name of the source file where an exception has been raised. */
    char *file_name;

    /* Line of the source file where the exception has been
     * raised. First line has number 1. 0 means the file line is
     * unknown. */
    uint32_t file_line;

    /* Exception class name. */
    char *exception_name;

    struct btp_python_frame *frames;
};

/**
 * Creates and initializes a new stacktrace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_python_stacktrace_free().
 */
struct btp_python_stacktrace *
btp_python_stacktrace_new();

/**
 * Initializes all members of the stacktrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a stacktrace structure placed on the
 * stack.
 */
void
btp_python_stacktrace_init(struct btp_python_stacktrace *stacktrace);

/**
 * Releases the memory held by the stacktrace and its frames.
 * @param stacktrace
 * If the stacktrace is NULL, no operation is performed.
 */
void
btp_python_stacktrace_free(struct btp_python_stacktrace *stacktrace);

/**
 * Creates a duplicate of the stacktrace.
 * @param stacktrace
 * The stacktrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL.  The returned duplicate must be
 * released by calling the function btp_python_stacktrace_free().
 */
struct btp_python_stacktrace *
btp_python_stacktrace_dup(struct btp_python_stacktrace *stacktrace);

/**
 * Returns the number of frames in the stacktrace.
 */
int
btp_python_stacktrace_get_frame_count(struct btp_python_stacktrace *stacktrace);

/**
 * Parses a textual Python stacktrace and puts it into a structure.
 * If parsing fails, the input parameter is not changed and NULL is
 * returned.
 * @param input
 * Pointer to the string with the Python stack trace. If this function
 * returns a non-NULL value, the input pointer is modified to point
 * after the stacktrace that was just parsed.
 */
struct btp_python_stacktrace *
btp_python_stacktrace_parse(const char **input,
                            struct btp_location *location);

/**
 * Serializes stacktrace to string.
 * @returnes
 * Newly allocated memory containing the textual representation of the
 * provided stacktrace.  Caller should free the memory when it's no
 * longer needed.
 */
char *
btp_python_stacktrace_to_json(struct btp_python_stacktrace *stacktrace);

#ifdef __cplusplus
}
#endif

#endif
