/*
    stacktrace.h

    Copyright (C) 2013  Red Hat, Inc.

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
#ifndef SATYR_STACKTRACE_H
#define SATYR_STACKTRACE_H

/**
 * Functions declared here work with all stacktrace types:
 * * sr_core_stacktrace
 * * sr_python_stacktrace
 * * sr_koops_stacktrace
 * * sr_java_stacktrace
 * * sr_gdb_stacktrace
 * * sr_ruby_stacktrace
 * * sr_js_stacktrace
 * You may need to explicitly cast the pointer to struct sr_stacktrace in order
 * to avoid compiler warning.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "report_type.h"

struct sr_json_value;

struct sr_stacktrace
{
    enum sr_report_type type;
};

/* Flags that influence how the bthash is computed.
 */
enum sr_bthash_flags
{
    /* Default hashing process.
     */
    SR_BTHASH_NORMAL = 1 << 0,

    /* Return the plaintext that would be hashed. Useful mainly for debugging.
     */
    SR_BTHASH_NOHASH = 1 << 1,
};

/**
 * Parses the stacktrace pointed to by input. You need to provide the correct
 * stacktrace type in the first parameter.
 */
struct sr_stacktrace *
sr_stacktrace_parse(enum sr_report_type type, const char *input, char **error_message);

/**
 * Returns short textual representation of given stacktrace. At most max_frames
 * are printed. Caller needs to free the result using free() afterwards.
 */
char *
sr_stacktrace_to_short_text(struct sr_stacktrace *stacktrace, int max_frames);

/**
 * Returns the thread structure that (probably) caused the crash.
 */
struct sr_thread *
sr_stacktrace_find_crash_thread(struct sr_stacktrace *stacktrace);

/**
 * Returns pointer to the first thread.
 */
struct sr_thread *
sr_stacktrace_threads(struct sr_stacktrace *stacktrace);

/**
 * Set the threads linked list pointer.
 */
void
sr_stacktrace_set_threads(struct sr_stacktrace *stacktrace, struct sr_thread *threads);

/**
 * Convert stacktrace to json and return it as text.
 */
char *
sr_stacktrace_to_json(struct sr_stacktrace *stacktrace);

/**
 * Deserialize stacktrace from its json representation.
 */
struct sr_stacktrace*
sr_stacktrace_from_json(enum sr_report_type, struct sr_json_value *root, char **error_message);

/**
 * Deserialize stacktrace from its json representation.
 */
struct sr_stacktrace*
sr_stacktrace_from_json_text(enum sr_report_type, const char *input, char **error_message);

/**
 * Returns brief, human-readable explanation of the stacktrace.
 */
char *
sr_stacktrace_get_reason(struct sr_stacktrace *stacktrace);

/**
 * Returns hash of a backtrace. This is a hash in the usual sense that the same
 * stacktraces always have the same hash while two distinct stacktraces have
 * negligible probability of having the same hash. The string is allocated by
 * malloc().
 */
char *
sr_stacktrace_get_bthash(struct sr_stacktrace *stacktrace, enum sr_bthash_flags flags);

/**
 * Releases all the memory associated with the stacktrace pointer.
 */
void
sr_stacktrace_free(struct sr_stacktrace *stacktrace);

#ifdef __cplusplus
}
#endif

#endif
