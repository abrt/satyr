/*
    core_backtrace.h

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
#ifndef BTPARSER_CORE_BACKTRACE_H
#define BTPARSER_CORE_BACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

struct btp_core_thread;
struct btp_location;

enum btp_core_backtrace_type
{
    BTP_USERSPACE,
    BTP_PYTHON,
    BTP_KERNELOOPS
};

struct btp_core_backtrace
{
    enum btp_core_backtrace_type type;

    struct btp_core_thread *threads;
};

/**
 * Creates and initializes a new backtrace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_core_backtrace_free().
 */
struct btp_core_backtrace *
btp_core_backtrace_new();

/**
 * Initializes all members of the backtrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a backtrace structure placed on the
 * stack.
 */
void
btp_core_backtrace_init(struct btp_core_backtrace *backtrace);

/**
 * Releases the memory held by the backtrace, its threads and frames.
 * @param backtrace
 * If the backtrace is NULL, no operation is performed.
 */
void
btp_core_backtrace_free(struct btp_core_backtrace *backtrace);

/**
 * Creates a duplicate of the backtrace.
 * @param backtrace
 * The backtrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL.  The returned duplicate must be
 * released by calling the function btp_core_backtrace_free().
 */
struct btp_core_backtrace *
btp_core_backtrace_dup(struct btp_core_backtrace *backtrace);

/**
 * Returns a number of threads in the backtrace.
 * @param backtrace
 * It's not modified by calling this function.
 */
int
btp_core_backtrace_get_thread_count(struct btp_core_backtrace *backtrace);

/**
 * Parses a textual backtrace and puts it into a structure.  If
 * parsing fails, the input parameter is not changed and NULL is
 * returned.
 *
 * @note
 * Backtrace can be serialized to string via
 * btp_core_backtrace_to_text().
 */
struct btp_core_backtrace *
btp_core_backtrace_parse(const char **input,
                         struct btp_location *location);

/**
 * Serializes backtrace to string.
 * @returnes
 * Newly allocated memory containing the textual representation of the
 * provided backtrace.  Caller should free the memory when it's no
 * longer needed.
 */
char *
btp_core_backtrace_to_text(struct btp_core_backtrace *backtrace);


struct btp_core_backtrace *
btp_core_backtrace_create(const char *gdb_backtrace_text,
                          const char *unstrip_text,
                          const char *executable_path);

#ifdef __cplusplus
}
#endif

#endif
