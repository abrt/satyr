/*
    gdb_stacktrace.h

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
#ifndef BTPARSER_GDB_STACKTRACE_H
#define BTPARSER_GDB_STACKTRACE_H

/**
 * @file
 * @brief Stack trace as produced by GDB.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct btp_gdb_thread;
struct btp_gdb_frame;
struct btp_location;

/**
 * @brief A stack trace produced by GDB.
 *
 * A stacktrace obtained at the time of a program crash, consisting of
 * several threads which contains frames.
 *
 * This structure represents a stacktrace as produced by the GNU
 * Debugger.
 */
struct btp_gdb_stacktrace
{
    struct btp_gdb_thread *threads;

    /**
     * The frame where the crash happened according to debugger.  It
     * might be that we can not tell to which thread this frame
     * belongs, because some threads end with mutually
     * indistinguishable frames.
     */
    struct btp_gdb_frame *crash;

    /**
     * Shared libraries loaded at the moment of crash.
     */
    struct btp_gdb_sharedlib *libs;
};

/**
 * Creates and initializes a new stacktrace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_gdb_stacktrace_free().
 */
struct btp_gdb_stacktrace *
btp_gdb_stacktrace_new();

/**
 * Initializes all members of the stacktrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a stacktrace structure placed on the
 * stack.
 */
void
btp_gdb_stacktrace_init(struct btp_gdb_stacktrace *stacktrace);

/**
 * Releases the memory held by the stacktrace, its threads, frames,
 * shared libraries.
 * @param stacktrace
 * If the stacktrace is NULL, no operation is performed.
 */
void
btp_gdb_stacktrace_free(struct btp_gdb_stacktrace *stacktrace);

/**
 * Creates a duplicate of the stacktrace.
 * @param stacktrace
 * The stacktrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL.  The returned duplicate must be
 * released by calling the function btp_gdb_stacktrace_free().
 */
struct btp_gdb_stacktrace *
btp_gdb_stacktrace_dup(struct btp_gdb_stacktrace *stacktrace);

/**
 * Returns a number of threads in the stacktrace.
 * @param stacktrace
 * It's not modified by calling this function.
 */
int
btp_gdb_stacktrace_get_thread_count(struct btp_gdb_stacktrace *stacktrace);

/**
 * Removes all threads from the stacktrace and deletes them, except the
 * one provided as a parameter.
 * @param thread
 * This function does not check whether the thread is a member of the
 * stacktrace.  If it's not, all threads are removed from the stacktrace
 * and then deleted.
 */
void
btp_gdb_stacktrace_remove_threads_except_one(struct btp_gdb_stacktrace *stacktrace,
                                            struct btp_gdb_thread *thread);

/**
 * Searches all threads and tries to find the one that caused the
 * crash.  It might return NULL if the thread cannot be determined.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 */
struct btp_gdb_thread *
btp_gdb_stacktrace_find_crash_thread(struct btp_gdb_stacktrace *stacktrace);

/**
 * Remove frames from the bottom of threads in the stacktrace, until
 * all threads have at most 'depth' frames.
 * @param stacktrace
 * Must be non-NULL pointer.
 */
void
btp_gdb_stacktrace_limit_frame_depth(struct btp_gdb_stacktrace *stacktrace,
                                    int depth);

/**
 * Evaluates the quality of the stacktrace. The quality is the ratio of
 * the number of frames with function name fully known to the number
 * of all frames.  This function does not take into account that some
 * frames are more important than others.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * A number between 0 and 1. 0 means the lowest quality, 1 means full
 * stacktrace is known (all function names are known).
 */
float
btp_gdb_stacktrace_quality_simple(struct btp_gdb_stacktrace *stacktrace);

/**
 * Evaluates the quality of the stacktrace. The quality is determined
 * depending on the ratio of frames with function name fully known to
 * all frames.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * A number between 0 and 1. 0 means the lowest quality, 1 means full
 * stacktrace is known.  The returned value takes into account that the
 * thread which caused the crash is more important than the other
 * threads, and the frames around the crash frame are more important
 * than distant frames.
 */
float
btp_gdb_stacktrace_quality_complex(struct btp_gdb_stacktrace *stacktrace);

/**
 * Returns textual representation of the stacktrace.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * This function never returns NULL. The caller is responsible for
 * releasing the returned memory using function free().
 */
char *
btp_gdb_stacktrace_to_text(struct btp_gdb_stacktrace *stacktrace,
                          bool verbose);

/**
 * Analyzes the stacktrace to get the frame where a crash occurred.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * The returned value must be released by calling btp_gdb_frame_free()
 * when it's no longer needed, because it is a deep copy of the crash
 * frame from the stacktrace.  NULL is returned if the crash frame is
 * not found.
 */
struct btp_gdb_frame *
btp_gdb_stacktrace_get_crash_frame(struct btp_gdb_stacktrace *stacktrace);

/**
 * Calculates the duplication hash string of the stacktrace.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * This function never returns NULL. The caller is responsible for
 * releasing the returned memory using function free().
 */
char *
btp_gdb_stacktrace_get_duplication_hash(struct btp_gdb_stacktrace *stacktrace);

/**
 * Parses a textual stacktrace and puts it into a structure.  If
 * parsing fails, the input parameter is not changed and NULL is
 * returned.
 * @code
 * struct btp_location location;
 * btp_location_init(&location);
 * char *input = "...";
 * struct btp_gdb_stacktrace *stacktrace;
 * stacktrace = btp_gdb_stacktrace_parse(input, location);
 * if (!stacktrace)
 * {
 *   fprintf(stderr,
 *           "Failed to parse the stacktrace.\n"
 *           "Line %d, column %d: %s\n",
 *           location.line,
 *           location.column,
 *           location.message);
 *   exit(-1);
 * }
 * btp_gdb_stacktrace_free(stacktrace);
 * @endcode
 * @param input
 * Pointer to the string with the stacktrace. If this function returns
 * true, this pointer is modified to point after the stacktrace that
 * was just parsed.
 * @param location
 * The caller must provide a pointer to an instance of btp_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized by btp_location_init() before calling this function
 * to get reasonable values.  When this function returns false (an
 * error occurred), the structure will contain the error line, column,
 * and message.
 * @returns
 * A newly allocated stacktrace structure or NULL. A stacktrace struct
 * is returned when at least one thread was parsed from the input and
 * no error occurred. The returned structure should be released by
 * btp_gdb_stacktrace_free().
 */
struct btp_gdb_stacktrace *
btp_gdb_stacktrace_parse(const char **input,
                        struct btp_location *location);

/**
 * Parse stacktrace header if it is available in the stacktrace.  The
 * header usually contains frame where the program crashed.
 * @param input
 * Pointer that will be moved to point behind the header if the header
 * is successfully detected and parsed.
 * @param frame
 * If this function succeeds and returns true, *frame contains the
 * crash frame that is usually a part of the header. If no frame is
 * detected in the header, *frame is set to NULL.
 * @code
 * [New Thread 11919]
 * [New Thread 11917]
 * Core was generated by `evince file:///tmp/Factura04-05-2010.pdf'.
 * Program terminated with signal 8, Arithmetic exception.
 * #0  0x000000322a2362b9 in repeat (image=<value optimized out>,
 *     mask=<value optimized out>, mask_bits=<value optimized out>)
 *     at pixman-bits-image.c:145
 * 145     pixman-bits-image.c: No such file or directory.
 *         in pixman-bits-image.c
 * @endcode
 */
bool
btp_gdb_stacktrace_parse_header(const char **input,
                               struct btp_gdb_frame **frame,
                               struct btp_location *location);

/**
 * Set library names in all frames in the stacktrace according to the
 * the sharedlib data.
 */
void
btp_gdb_stacktrace_set_libnames(struct btp_gdb_stacktrace *stacktrace);

/**
 * Return crash thread optimized for comparison. It's normalized, with
 * library names set and functions without names (signal handlers) are
 * removed.
 * @param stacktrace
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param max_frames
 * The maximum number of frames in the returned crash thread.
 * Superfluous frames are removed from the returned thread.
 * @returns
 * A newly allocated thread structure or NULL. NULL is returned when the
 * crashing thread could not be found. The returned structure should be
 * released by btp_gdb_thread_free() by the caller.
 */
struct btp_gdb_thread *
btp_gdb_stacktrace_get_optimized_thread(struct btp_gdb_stacktrace *stacktrace,
                                       int max_frames);

#ifdef __cplusplus
}
#endif

#endif
