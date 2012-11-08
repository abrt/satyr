/*
    java_stacktrace.h

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
#ifndef BTPARSER_JAVA_STACKTRACE_H
#define BTPARSER_JAVA_STACKTRACE_H

/**
 * @file
 * @brief java stack trace structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct btp_java_thread;

#include <stdint.h>

struct btp_java_stacktrace
{
    struct btp_java_thread *threads;
};

/**
 * Creates and initializes a new stacktrace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_java_stacktrace_free().
 */
struct btp_java_stacktrace *
btp_java_stacktrace_new();

/**
 * Initializes all members of the stacktrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a stacktrace structure placed on the
 * stack.
 */
void
btp_java_stacktrace_init(struct btp_java_stacktrace *stacktrace);

/**
 * Releases the memory held by the stacktrace and its frames.
 * @param stacktrace
 * If the stacktrace is NULL, no operation is performed.
 */
void
btp_java_stacktrace_free(struct btp_java_stacktrace *stacktrace);

/**
 * Creates a duplicate of the stacktrace.
 * @param stacktrace
 * The stacktrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL.  The returned duplicate must be
 * released by calling the function btp_java_stacktrace_free().
 */
struct btp_java_stacktrace *
btp_java_stacktrace_dup(struct btp_java_stacktrace *stacktrace);

#ifdef __cplusplus
}
#endif

#endif
