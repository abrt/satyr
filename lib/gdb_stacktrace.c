/*
    gdb_stacktrace.c

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
#include "gdb_stacktrace.h"
#include "gdb_thread.h"
#include "gdb_frame.h"
#include "gdb_sharedlib.h"
#include "utils.h"
#include "strbuf.h"
#include "location.h"
#include "normalize.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct sr_gdb_stacktrace *
sr_gdb_stacktrace_new()
{
    struct sr_gdb_stacktrace *stacktrace = sr_malloc(sizeof(struct sr_gdb_stacktrace));
    sr_gdb_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_gdb_stacktrace_init(struct sr_gdb_stacktrace *stacktrace)
{
    stacktrace->threads = NULL;
    stacktrace->crash = NULL;
    stacktrace->libs = NULL;
}

void
sr_gdb_stacktrace_free(struct sr_gdb_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->threads)
    {
        struct sr_gdb_thread *thread = stacktrace->threads;
        stacktrace->threads = thread->next;
        sr_gdb_thread_free(thread);
    }

    while (stacktrace->libs)
    {
        struct sr_gdb_sharedlib *sharedlib = stacktrace->libs;
        stacktrace->libs = sharedlib->next;
        sr_gdb_sharedlib_free(sharedlib);
    }

    if (stacktrace->crash)
        sr_gdb_frame_free(stacktrace->crash);

    free(stacktrace);
}

struct sr_gdb_stacktrace *
sr_gdb_stacktrace_dup(struct sr_gdb_stacktrace *stacktrace)
{
    struct sr_gdb_stacktrace *result = sr_gdb_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_gdb_stacktrace));

    if (stacktrace->crash)
        result->crash = sr_gdb_frame_dup(stacktrace->crash, false);
    if (stacktrace->threads)
        result->threads = sr_gdb_thread_dup(stacktrace->threads, true);
    if (stacktrace->libs)
        result->libs = sr_gdb_sharedlib_dup(stacktrace->libs, true);

    return result;
}

int
sr_gdb_stacktrace_get_thread_count(struct sr_gdb_stacktrace *stacktrace)
{
    struct sr_gdb_thread *thread = stacktrace->threads;
    int count = 0;
    while (thread)
    {
        thread = thread->next;
        ++count;
    }
    return count;
}

void
sr_gdb_stacktrace_remove_threads_except_one(struct sr_gdb_stacktrace *stacktrace,
                                            struct sr_gdb_thread *thread)
{
    while (stacktrace->threads)
    {
        struct sr_gdb_thread *delete_thread = stacktrace->threads;
        stacktrace->threads = delete_thread->next;
        if (delete_thread != thread)
            sr_gdb_thread_free(delete_thread);
    }

    thread->next = NULL;
    stacktrace->threads = thread;
}

/**
 * Loop through all threads and if a single one contains the crash
 * frame on the top, return it. Otherwise, return NULL.
 *
 * If require_abort is true, it is also required that the thread
 * containing the crash frame contains some known "abort" function. In
 * this case there can be multiple threads with the crash frame on the
 * top, but only one of them might contain the abort function to
 * succeed.
 */
static struct sr_gdb_thread *
find_crash_thread_from_crash_frame(struct sr_gdb_stacktrace *stacktrace,
                                   bool require_abort)
{
    if (sr_debug_parser)
        printf("%s(stacktrace, %s)\n", __FUNCTION__, require_abort ? "true" : "false");

    assert(stacktrace->threads); /* checked by the caller */
    if (!stacktrace->crash || !stacktrace->crash->function_name)
        return NULL;

    struct sr_gdb_thread *result = NULL;
    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
        struct sr_gdb_frame *top_frame = thread->frames;
        bool same_name = top_frame &&
            top_frame->function_name &&
            0 == strcmp(top_frame->function_name, stacktrace->crash->function_name);
        bool abort_requirement_satisfied = !require_abort ||
            sr_glibc_thread_find_exit_frame(thread);
        if (sr_debug_parser)
        {
            printf(" - thread #%d: same_name %s, abort_satisfied %s\n",
                   thread->number,
                   same_name ? "true" : "false",
                   abort_requirement_satisfied ? "true" : "false");
        }

        if (same_name && abort_requirement_satisfied)
        {
            if (NULL == result)
                result = thread;
            else
            {
                /* Second frame with the same function. Failure. */
                return NULL;
            }
        }

        thread = thread->next;
    }

    return result;
}

struct sr_gdb_thread *
sr_gdb_stacktrace_find_crash_thread(struct sr_gdb_stacktrace *stacktrace)
{
    /* If there is no thread, be silent and report NULL. */
    if (!stacktrace->threads)
        return NULL;

    /* If there is just one thread, it is simple. */
    if (!stacktrace->threads->next)
        return stacktrace->threads;

    /* If we have a crash frame *and* there is just one thread which has
     * this frame on the top, it is also simple.
     */
    struct sr_gdb_thread *thread;
    thread = find_crash_thread_from_crash_frame(stacktrace, false);
    if (thread)
        return thread;

    /* There are multiple threads with a frame indistinguishable from
     * the crash frame on the top of stack.
     * Try to search for known abort functions.
     */
    thread = find_crash_thread_from_crash_frame(stacktrace, true);

    /* We might want to search a thread with known abort function, and
     * without the crash frame here. However, it hasn't been needed so
     * far.
     */
    return thread; /* result or null */
}


void
sr_gdb_stacktrace_limit_frame_depth(struct sr_gdb_stacktrace *stacktrace,
                                    int depth)
{
    assert(depth > 0);
    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
        sr_gdb_thread_remove_frames_below_n(thread, depth);
        thread = thread->next;
    }
}

float
sr_gdb_stacktrace_quality_simple(struct sr_gdb_stacktrace *stacktrace)
{
    int ok_count = 0, all_count = 0;
    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
        sr_gdb_thread_quality_counts(thread, &ok_count, &all_count);
        thread = thread->next;
    }

    if (all_count == 0)
        return 0;

    return ok_count / (float)all_count;
}

float
sr_gdb_stacktrace_quality_complex(struct sr_gdb_stacktrace *stacktrace)
{
    stacktrace = sr_gdb_stacktrace_dup(stacktrace);

    /* Find the crash thread, and then normalize the stacktrace. It is
     * not possible to find the crash thread after the stacktrace has
     * been normalized.
     */
    struct sr_gdb_thread *crash_thread =
        sr_gdb_stacktrace_find_crash_thread(stacktrace);

    sr_normalize_gdb_stacktrace(stacktrace);

    /* Get the quality q1 of the full stacktrace. */
    float q1 = sr_gdb_stacktrace_quality_simple(stacktrace);

    if (!crash_thread)
    {
        sr_gdb_stacktrace_free(stacktrace);
        return q1;
    }

    /* Get the quality q2 of the crash thread. */
    float q2 = sr_gdb_thread_quality(crash_thread);

    /* Get the quality q3 of the frames around the crash.  First,
     * duplicate the crash thread so we can cut it. Then find an exit
     * frame, and remove it and everything above it
     * (__run_exit_handlers and such). Then remove all the redundant
     * frames (assert calls etc.) Then limit the frame count to 5.
     */
    sr_gdb_thread_remove_frames_below_n(crash_thread, 5);
    float q3 = sr_gdb_thread_quality(crash_thread);

    sr_gdb_stacktrace_free(stacktrace);

    /* Compute and return the final stacktrace quality q. */
    return 0.25f * q1 + 0.35f * q2 + 0.4f * q3;
}

char *
sr_gdb_stacktrace_to_text(struct sr_gdb_stacktrace *stacktrace, bool verbose)
{
    struct sr_strbuf *str = sr_strbuf_new();
    if (verbose)
    {
        sr_strbuf_append_strf(str, "Thread count: %d\n",
                              sr_gdb_stacktrace_get_thread_count(stacktrace));
    }

    if (stacktrace->crash && verbose)
    {
        sr_strbuf_append_str(str, "Crash frame: ");
        sr_gdb_frame_append_to_str(stacktrace->crash, str, verbose);
    }

    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
        sr_gdb_thread_append_to_str(thread, str, verbose);
        thread = thread->next;
    }

    return sr_strbuf_free_nobuf(str);
}

struct sr_gdb_frame *
sr_gdb_stacktrace_get_crash_frame(struct sr_gdb_stacktrace *stacktrace)
{
    stacktrace = sr_gdb_stacktrace_dup(stacktrace);

    struct sr_gdb_thread *crash_thread =
        sr_gdb_stacktrace_find_crash_thread(stacktrace);

    if (!crash_thread)
    {
        sr_gdb_stacktrace_free(stacktrace);
        return NULL;
    }

    sr_normalize_gdb_stacktrace(stacktrace);
    struct sr_gdb_frame *crash_frame = crash_thread->frames;
    crash_frame = sr_gdb_frame_dup(crash_frame, false);
    sr_gdb_stacktrace_free(stacktrace);
    return crash_frame;
}

char *
sr_gdb_stacktrace_get_duplication_hash(struct sr_gdb_stacktrace *stacktrace)
{
    stacktrace = sr_gdb_stacktrace_dup(stacktrace);
    struct sr_gdb_thread *crash_thread = sr_gdb_stacktrace_find_crash_thread(stacktrace);
    if (crash_thread)
        sr_gdb_stacktrace_remove_threads_except_one(stacktrace, crash_thread);

    sr_normalize_gdb_stacktrace(stacktrace);
    sr_gdb_stacktrace_limit_frame_depth(stacktrace, 3);
    char *hash = sr_gdb_stacktrace_to_text(stacktrace, false);
    sr_gdb_stacktrace_free(stacktrace);
    return hash;
}

struct sr_gdb_stacktrace *
sr_gdb_stacktrace_parse(const char **input,
                        struct sr_location *location)
{
    const char *local_input = *input;
    /* im - intermediate */
    struct sr_gdb_stacktrace *imstacktrace = sr_gdb_stacktrace_new();
    imstacktrace->libs = sr_gdb_sharedlib_parse(*input);

    /* The header is mandatory, but it might contain no frame header,
     * in some broken stacktraces. In that case, stacktrace.crash value
     * is kept as NULL.
     */
    if (!sr_gdb_stacktrace_parse_header(&local_input,
                                        &imstacktrace->crash,
                                        location))
    {
        sr_gdb_stacktrace_free(imstacktrace);
        return NULL;
    }

    struct sr_gdb_thread *thread, *prevthread = NULL;
    while ((thread = sr_gdb_thread_parse(&local_input, location)))
    {
        if (prevthread)
        {
            sr_gdb_thread_append(prevthread, thread);
            prevthread = thread;
        }
        else
            imstacktrace->threads = prevthread = thread;
    }
    if (!imstacktrace->threads)
    {
        sr_gdb_stacktrace_free(imstacktrace);
        return NULL;
    }

    *input = local_input;
    return imstacktrace;
}

bool
sr_gdb_stacktrace_parse_header(const char **input,
                               struct sr_gdb_frame **frame,
                               struct sr_location *location)
{
    int first_thread_line, first_thread_column;
    const char *first_thread = sr_strstr_location(*input,
                                                  "\nThread ",
                                                  &first_thread_line,
                                                  &first_thread_column);

    /* Skip the newline. */
    if (first_thread)
    {
        ++first_thread;
        first_thread_line += 1;
        first_thread_column = 0;
    }

    int first_frame_line, first_frame_column;
    const char *first_frame = sr_strstr_location(*input,
                                                 "\n#",
                                                 &first_frame_line,
                                                 &first_frame_column);

    /* Skip the newline. */
    if (first_frame)
    {
        ++first_frame;
        first_frame_line += 1;
        first_frame_column = 0;
    }

    if (first_thread)
    {
        if (first_frame && first_frame < first_thread)
        {
            /* Common case. The crash frame is present in the input
             * before the list of threads begins.
             */
            *input = first_frame;
            sr_location_add(location, first_frame_line, first_frame_column);
        }
        else
        {
	    /* Uncommon case (caused by some kernel bug) where the
             * frame is missing from the header.  The stacktrace
             * contains just threads.  We silently skip the header and
             * return true.
             */
  	    *input = first_thread;
            sr_location_add(location,
                            first_thread_line,
                            first_thread_column);
            *frame = NULL;
	    return true;
        }
    }
    else if (first_frame)
    {
        /* Degenerate case when the stacktrace contains no thread, but
         * the frame is there.
         */
        *input = first_frame;
        sr_location_add(location, first_frame_line, first_frame_column);
    }
    else
    {
        /* Degenerate case where the input is empty or completely
         * meaningless. Report a failure.
         */
        location->message = "No frame and no thread found.";
        return false;
    }

    /* Parse the frame header. */
    *frame = sr_gdb_frame_parse(input, location);
    return *frame;
}

void
sr_gdb_stacktrace_set_libnames(struct sr_gdb_stacktrace *stacktrace)
{
    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
        struct sr_gdb_frame *frame = thread->frames;
        while (frame)
        {
            struct sr_gdb_sharedlib *lib = sr_gdb_sharedlib_find_address(stacktrace->libs,
                                                                         frame->address);
            if (lib)
            {
                char *s1, *s2;

                /* Strip directory and version after the .so suffix. */
                s1 = strrchr(lib->soname, '/');
                if (!s1)
                    s1 = lib->soname;
                else
                    s1++;
                s2 = strstr(s1, ".so");
                if (!s2)
                    s2 = s1 + strlen(s1);
                else
                    s2 += strlen(".so");

                if (frame->library_name)
                    free(frame->library_name);
                frame->library_name = sr_strndup(s1, s2 - s1);
            }
            frame = frame->next;
        }
        thread = thread->next;
    }
}

struct sr_gdb_thread *
sr_gdb_stacktrace_get_optimized_thread(struct sr_gdb_stacktrace *stacktrace,
                                       int max_frames)
{
    struct sr_gdb_thread *crash_thread;

    stacktrace = sr_gdb_stacktrace_dup(stacktrace);
    crash_thread = sr_gdb_stacktrace_find_crash_thread(stacktrace);

    if (!crash_thread)
    {
        sr_gdb_stacktrace_free(stacktrace);
        return NULL;
    }

    sr_gdb_stacktrace_remove_threads_except_one(stacktrace, crash_thread);
    sr_gdb_stacktrace_set_libnames(stacktrace);
    sr_normalize_gdb_thread(crash_thread);
    sr_gdb_normalize_optimize_thread(crash_thread);

    /* Remove frames with no function name (i.e. signal handlers). */
    struct sr_gdb_frame *frame = crash_thread->frames, *frame_next;
    while (frame)
    {
        frame_next = frame->next;
        if (!frame->function_name)
            sr_gdb_thread_remove_frame(crash_thread, frame);
        frame = frame_next;
    }

    if (max_frames > 0)
        sr_gdb_thread_remove_frames_below_n(crash_thread, max_frames);

    crash_thread = sr_gdb_thread_dup(crash_thread, false);
    sr_gdb_stacktrace_free(stacktrace);

    return crash_thread;
}
