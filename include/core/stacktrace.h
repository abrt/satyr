/*
    core_stacktrace.h

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
#ifndef SATYR_CORE_STACKTRACE_H
#define SATYR_CORE_STACKTRACE_H

/**
 * @file
 * @brief A stack trace of a core dump.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <inttypes.h>
#include <json.h>
#include <stdbool.h>

struct sr_core_thread;
struct sr_location;

/**
 * @brief A stack trace of a core dump.
 */
struct sr_core_stacktrace
{
    enum sr_report_type type;

    /** Signal number. */
    uint16_t signal;

    char *executable;

    /**
     * @brief Thread responsible for the crash.
     *
     * It might be NULL if the crash thread is not detected.
     */
    struct sr_core_thread *crash_thread;

    struct sr_core_thread *threads;

    /** Does the trace contain only crash thread, or all threads? */
    bool only_crash_thread;
};

/**
 * Creates and initializes a new stacktrace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_core_stacktrace_free().
 */
struct sr_core_stacktrace *
sr_core_stacktrace_new(void);

/**
 * Initializes all members of the stacktrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a stacktrace structure placed on the
 * stack.
 */
void
sr_core_stacktrace_init(struct sr_core_stacktrace *stacktrace);

/**
 * Releases the memory held by the stacktrace, its threads and frames.
 * @param stacktrace
 * If the stacktrace is NULL, no operation is performed.
 */
void
sr_core_stacktrace_free(struct sr_core_stacktrace *stacktrace);

/**
 * Creates a duplicate of the stacktrace.
 * @param stacktrace
 * The stacktrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL.  The returned duplicate must be
 * released by calling the function sr_core_stacktrace_free().
 */
struct sr_core_stacktrace *
sr_core_stacktrace_dup(struct sr_core_stacktrace *stacktrace);

/**
 * Returns a number of threads in the stacktrace.
 * @param stacktrace
 * It's not modified by calling this function.
 */
int
sr_core_stacktrace_get_thread_count( struct sr_core_stacktrace *stacktrace);

struct sr_core_thread *
sr_core_stacktrace_find_crash_thread(struct sr_core_stacktrace *stacktrace);

/**
 * @note
 * Stacktrace can be serialized to JSON string via
 * sr_core_stacktrace_to_json().
 */
struct sr_core_stacktrace *
sr_core_stacktrace_from_json(json_object *root,
                             char **error_message);

struct sr_core_stacktrace *
sr_core_stacktrace_from_json_text(const char *text,
                                  char **error_message);

/**
 * Returns brief, human-readable explanation of the stacktrace.
 */
char *
sr_core_stacktrace_get_reason(struct sr_core_stacktrace *stacktrace);

/**
 * Serializes stacktrace to string.
 * @returns
 * Newly allocated memory containing the textual representation of the
 * provided stacktrace.  Caller should free the memory when it's no
 * longer needed.
 */
char *
sr_core_stacktrace_to_json(struct sr_core_stacktrace *stacktrace);

struct sr_core_stacktrace *
sr_core_stacktrace_create(const char *gdb_stacktrace_text,
                          const char *unstrip_text,
                          const char *executable_path);

#ifdef __cplusplus
}
#endif

#endif
