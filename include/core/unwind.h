/*
    core_unwind.h

    Copyright (C) 2010, 2011, 2012  Red Hat, Inc.

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
#ifndef SATYR_CORE_UNWIND_H
#define SATYR_CORE_UNWIND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

struct sr_core_stacktrace;
struct sr_gdb_stacktrace;
struct sr_core_stracetrace_unwind_state;

struct sr_core_stacktrace *
sr_parse_coredump(const char *coredump_filename,
                  const char *executable_filename,
                  char **error_message);

struct sr_core_stacktrace *
sr_core_stacktrace_from_gdb(const char *gdb_output,
                            const char *coredump_filename,
                            const char *executable_filename,
                            char **error_message);

/* This function can be used to unwind stack of live ("dying") process, invoked
 * from the core dump hook (/proc/sys/kernel/core_pattern).
 *
 * Beware:
 *
 * - It can only unwind one thread of the process, the thread that caused the
 *   terminating signal to be sent. You must supply that thread's tid.
 * - The function calls close() on stdin, meaning that in the core handler you
 *   cannot access the core image after calling this function.
 */
struct sr_core_stacktrace *
sr_core_stacktrace_from_core_hook(pid_t thread_id,
                                  const char *executable_filename,
                                  int signum,
                                  char **error_message);

struct sr_core_stracetrace_unwind_state *
sr_core_stacktrace_from_core_hook_prepare(pid_t tid, char **error_msg);

struct sr_core_stacktrace *
sr_core_stacktrace_from_core_hook_generate(pid_t tid,
                                           const char *executable,
                                           int signum,
                                           struct sr_core_stracetrace_unwind_state *state,
                                           char **error_msg);

void
sr_core_stacktrace_unwind_state_free(struct sr_core_stracetrace_unwind_state *state);

#ifdef __cplusplus
}
#endif

#endif
