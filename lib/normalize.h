/*
    normalize.h

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
#ifndef BTPARSER_NORMALIZE_H
#define BTPARSER_NORMALIZE_H

/**
 * @file
 * @brief Normalization of stack traces.
 *
 * Normalization changes stack traces with respect to similarity by
 * removing unnecessary differences.  Normalized stack traces can be
 * used to compute clusters and similarity of stack traces.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct btp_gdb_frame;
struct btp_gdb_thread;
struct btp_gdb_stacktrace;
struct btp_core_frame;
struct btp_core_thread;
struct btp_core_stacktrace;
struct btp_koops_stacktrace;

void
btp_normalize_gdb_thread(struct btp_gdb_thread *thread);

void
btp_normalize_gdb_stacktrace(struct btp_gdb_stacktrace *stacktrace);

void
btp_normalize_koops_stacktrace(struct btp_koops_stacktrace *stacktrace);

// TODO: move to gdb_stacktrace.h
/**
 * Checks whether the thread it contains some function used to exit
 * application.  If a frame with the function is found, it is
 * returned.  If there are multiple frames with abort function, the
 * lowest one is returned.
 * @returns
 * Returns NULL if such a frame is not found.
 */
struct btp_gdb_frame *
btp_glibc_thread_find_exit_frame(struct btp_gdb_thread *thread);

// TODO: move to metrics.h
/**
 * Renames unknown function names ("??") that are between the same
 * function names to be treated as similar in later comparison.
 * Leaves unpair unknown functions unchanged.
 */
void
btp_normalize_gdb_paired_unknown_function_names(struct btp_gdb_thread *thread1,
                                                struct btp_gdb_thread *thread2);

// TODO: merge into normalization or something else
/**
 * Remove frames which are not interesting in comparison with other threads.
 */
void
btp_gdb_normalize_optimize_thread(struct btp_gdb_thread *thread);

#ifdef __cplusplus
}
#endif

#endif
