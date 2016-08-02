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
#include <unistd.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

struct frame_callback_arg
{
    struct sr_core_frame **frames_tail;
    char *error_msg;
    unsigned nframes;
};

struct thread_callback_arg
{
    struct sr_core_thread **threads_tail;
    char *error_msg;
};

static const int CB_STOP_UNWIND = DWARF_CB_ABORT+1;

static int
frame_callback(Dwfl_Frame *frame, void *data)
{
    struct frame_callback_arg *frame_arg = data;
    char **error_msg = &frame_arg->error_msg;

    Dwarf_Addr pc;
    bool minus_one;
    if (!dwfl_frame_pc(frame, &pc, &minus_one))
    {
        set_error_dwfl("dwfl_frame_pc");
        return DWARF_CB_ABORT;
    }

    Dwfl *dwfl = dwfl_thread_dwfl(dwfl_frame_thread(frame));
    struct sr_core_frame *result = resolve_frame(dwfl, pc, minus_one);

    /* Do not unwind below __libc_start_main. */
    if (0 == sr_strcmp0(result->function_name, "__libc_start_main"))
    {
        sr_core_frame_free(result);
        return CB_STOP_UNWIND;
    }

    *frame_arg->frames_tail = result;
    frame_arg->frames_tail = &result->next;
    frame_arg->nframes++;

    return DWARF_CB_OK;
}

static void
truncate_long_thread(struct sr_core_thread *thread, struct frame_callback_arg *frame_arg)
{
    /* Truncate the stacktrace to CORE_STACKTRACE_FRAME_LIMIT least recent frames. */
    while (thread->frames && frame_arg->nframes > CORE_STACKTRACE_FRAME_LIMIT)
    {
        struct sr_core_frame *old_frame = thread->frames;
        thread->frames = old_frame->next;
        sr_core_frame_free(old_frame);
        frame_arg->nframes--;
    }
}

static int
unwind_thread(Dwfl_Thread *thread, void *data)
{
    struct thread_callback_arg *thread_arg = data;
    char **error_msg = &thread_arg->error_msg;

    struct sr_core_thread *result = sr_core_thread_new();
    if (!result)
    {
        set_error("Failed to initialize thread memory");
        return DWARF_CB_ABORT;
    }
    result->id = (int64_t)dwfl_thread_tid(thread);

    struct frame_callback_arg frame_arg =
    {
        .frames_tail = &(result->frames),
        .error_msg = NULL,
        .nframes = 0
    };

    int ret = dwfl_thread_getframes(thread, frame_callback, &frame_arg);
    if (ret == -1)
    {
        warn("dwfl_thread_getframes failed for thread id %d: %s",
             (int)result->id, dwfl_errmsg(-1));
    }
    else if (ret == DWARF_CB_ABORT)
    {
        *error_msg = frame_arg.error_msg;
        goto abort;
    }
    else if (ret != 0 && ret != CB_STOP_UNWIND)
    {
        *error_msg = sr_strdup("Unknown error in dwfl_thread_getframes");
        goto abort;
    }

    if (!error_msg && !result->frames)
    {
        set_error("No frames found for thread id %d", (int)result->id);
        goto abort;
    }

    truncate_long_thread(result, &frame_arg);

    *thread_arg->threads_tail = result;
    thread_arg->threads_tail = &result->next;

    return DWARF_CB_OK;

abort:
    sr_core_thread_free(result);
    return DWARF_CB_ABORT;
}

struct sr_core_stacktrace *
sr_parse_coredump(const char *core_file,
                  const char *exe_file,
                  char **error_msg)
{
    struct sr_core_stacktrace *stacktrace = NULL;

    /* Initialize error_msg to 'no error'. */
    if (error_msg)
        *error_msg = NULL;

    struct core_handle *ch = open_coredump(core_file, exe_file, error_msg);
    if (!ch)
        goto fail;

    if (dwfl_core_file_attach(ch->dwfl, ch->eh) < 0)
    {
        set_error_dwfl("dwfl_core_file_attach");
        goto fail;
    }

    stacktrace = sr_core_stacktrace_new();
    if (!stacktrace)
    {
        set_error("Failed to initialize stacktrace memory");
        goto fail;
    }

    struct thread_callback_arg thread_arg =
    {
        .threads_tail = &(stacktrace->threads),
        .error_msg = NULL
    };

    int ret = dwfl_getthreads(ch->dwfl, unwind_thread, &thread_arg);
    if (ret != 0)
    {
        if (ret == -1)
            set_error_dwfl("dwfl_getthreads");
        else if (ret == DWARF_CB_ABORT)
        {
            set_error("%s", thread_arg.error_msg);
            free(thread_arg.error_msg);
        }
        else
            set_error("Unknown error in dwfl_getthreads");

        sr_core_stacktrace_free(stacktrace);
        stacktrace = NULL;
        goto fail;
    }

    stacktrace->executable = sr_strdup(exe_file);
    stacktrace->signal = get_signal_number(ch->eh, core_file);
    /* FIXME: is this the best we can do? */
    stacktrace->crash_thread = stacktrace->threads;

fail:
    core_handle_free(ch);
    return stacktrace;
}

/* If PTRACE_SEIZE is not defined (kernel < 3.4), stub function from
 * core_unwind.c is used. */
#ifdef PTRACE_SEIZE

struct sr_core_stacktrace *
sr_core_stacktrace_from_core_hook(pid_t tid,
                                  const char *executable,
                                  int signum,
                                  char **error_msg)
{
    struct sr_core_stacktrace *stacktrace = NULL;

    /* Initialize error_msg to 'no error'. */
    if (error_msg)
        *error_msg = NULL;

    const Dwfl_Callbacks proc_cb =
    {
        .find_elf = dwfl_linux_proc_find_elf,
        .find_debuginfo = find_debuginfo_none,
    };

    Dwfl *dwfl = dwfl_begin(&proc_cb);

    if (dwfl_linux_proc_report(dwfl, tid) != 0)
    {
        set_error_dwfl("dwfl_linux_proc_report");
        goto fail;
    }

    if (dwfl_report_end(dwfl, NULL, NULL) != 0)
    {
        set_error_dwfl("dwfl_report_end");
        goto fail;
    }

    if (ptrace(PTRACE_SEIZE, tid, NULL, (void *)(uintptr_t)PTRACE_O_TRACEEXIT) != 0)
    {
        set_error("PTRACE_SEIZE (tid %u) failed: %s", (unsigned)tid, strerror(errno));
        goto fail;
    }

    if (close(STDIN_FILENO) != 0)
    {
        set_error("Failed to close stdin: %s", strerror(errno));
        goto fail;
    }

    int status;
    pid_t got = waitpid(tid, &status, 0);
    if (got == -1)
    {
        set_error("waitpid failed: %s", strerror(errno));
        goto fail;
    }

    if (got != tid)
    {
        set_error("waitpid returned %u but %u was expected", (unsigned)got, (unsigned)tid);
        goto fail;
    }

    if (!WIFSTOPPED(status))
    {
        set_error("waitpid returned 0x%x but WIFSTOPPED was expected", (unsigned)status);
        goto fail;
    }

    if ((status >> 8) != (SIGTRAP | (PTRACE_EVENT_EXIT << 8)))
    {
        set_error("waitpid returned 0x%x but (status >> 8) == "
                  "(SIGTRAP | (PTRACE_EVENT_EXIT << 8)) was expected", (unsigned)status);
        goto fail;
    }

    if (dwfl_linux_proc_attach(dwfl, tid, true) != 0)
    {
        set_error_dwfl("dwfl_linux_proc_attach");
        goto fail;
    }

    stacktrace = sr_core_stacktrace_new();
    if (!stacktrace)
    {
        set_error("Failed to initialize stacktrace memory");
        goto fail;
    }

    stacktrace->threads = sr_core_thread_new();
    if (!stacktrace->threads)
    {
        set_error("Failed to initialize thread memory");
        sr_core_stacktrace_free(stacktrace);
        stacktrace = 0;
        goto fail;
    }
    stacktrace->threads->id = tid;

    struct frame_callback_arg frame_arg =
    {
        .frames_tail = &(stacktrace->threads->frames),
        .error_msg = NULL,
        .nframes = 0
    };

    int ret = dwfl_getthread_frames(dwfl, tid, frame_callback, &frame_arg);
    if (ret != 0 && ret != CB_STOP_UNWIND)
    {
        if (ret == -1)
            set_error_dwfl("dwfl_getthread_frames");
        else if (ret == DWARF_CB_ABORT)
        {
            set_error("%s", frame_arg.error_msg);
            free(frame_arg.error_msg);
        }
        else
            set_error("Unknown error in dwfl_getthreads");

        sr_core_stacktrace_free(stacktrace);
        stacktrace = NULL;
        goto fail;
    }

    truncate_long_thread(stacktrace->threads, &frame_arg);

    if (executable)
        stacktrace->executable = sr_strdup(executable);
    if (signum > 0)
        stacktrace->signal = (uint16_t)signum;
    stacktrace->crash_thread = stacktrace->threads;
    stacktrace->only_crash_thread = true;

fail:
    dwfl_end(dwfl);
    return stacktrace;
}

#endif /* PTRACE_SEIZE */

#endif /* WITH_LIBDWFL */
