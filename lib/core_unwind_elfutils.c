/*
    core_unwind_elfutils.c

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
#include "utils.h"
#include "internal_utils.h"
#include "core/frame.h"
#include "core/thread.h"
#include "core/stacktrace.h"
#include "internal_unwind.h"

#ifdef WITH_LIBDWFL

#include <stdio.h>
#include <string.h>

static struct sr_core_thread *
unwind_thread(Dwfl *dwfl, Dwfl_Frame_State *state, char **error_msg)
{
    struct sr_core_frame *head = NULL, *tail = NULL;
    pid_t tid = 0;

    if (state)
    {
        tid = dwfl_frame_tid_get(state);
    }

    while (state)
    {
        Dwarf_Addr pc;
        bool minus_one;
        if (!dwfl_frame_state_pc(state, &pc, &minus_one))
        {
            warn("Failed to obtain PC: %s", dwfl_errmsg(-1));
            break;
        }

        struct sr_core_frame *frame = resolve_frame(dwfl, pc, minus_one);
        list_append(head, tail, frame);

        /* Do not unwind below __libc_start_main. */
        if (0 == sr_strcmp0(frame->function_name, "__libc_start_main"))
            break;

        if (!dwfl_frame_unwind(&state))
        {
            warn("Cannot unwind frame: %s", dwfl_errmsg(-1));
            break;
        }
    }

    if (!error_msg && !head)
    {
        set_error("No frames found for thread id %d", (int)tid);
    }

    struct sr_core_thread *thread = sr_core_thread_new();
    thread->id = (int64_t)tid;
    thread->frames = head;
    return thread;
}

struct sr_core_stacktrace *
sr_parse_coredump(const char *core_file,
                   const char *exe_file,
                   char **error_msg)
{
    struct sr_core_stacktrace *stacktrace = NULL;
    short signal = 0;

    /* Initialize error_msg to 'no error'. */
    if (error_msg)
        *error_msg = NULL;

    struct core_handle *ch = open_coredump(core_file, exe_file, error_msg);
    if (*error_msg)
        return NULL;

    Dwfl_Frame_State *state = dwfl_frame_state_core(ch->dwfl, core_file);
    if (!state)
    {
        set_error("Failed to initialize frame state from core '%s'",
                   core_file);
        goto fail_destroy_handle;
    }

    stacktrace = sr_core_stacktrace_new();
    if (!stacktrace)
    {
        set_error("Failed to initialize stacktrace memory");
        goto fail_destroy_handle;
    }
    struct sr_core_thread *threads_tail = NULL;

    do
    {
        struct sr_core_thread *t = unwind_thread(ch->dwfl, state, error_msg);
        if (*error_msg)
        {
            goto fail_destroy_trace;
        }
        list_append(stacktrace->threads, threads_tail, t);
        state = dwfl_frame_thread_next(state);
    } while (state);

    signal = get_signal_number(ch->eh, core_file);

fail_destroy_trace:
    if (*error_msg)
    {
        sr_core_stacktrace_free(stacktrace);
        stacktrace = NULL;
    }
fail_destroy_handle:
    core_handle_free(ch);

    stacktrace->executable = sr_strdup(exe_file);
    stacktrace->signal = signal;
    /* FIXME: is this the best we can do? */
    stacktrace->crash_thread = stacktrace->threads;
    return stacktrace;
}

#endif /* WITH_LIBDWFL */
