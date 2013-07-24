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
#include <sys/procfs.h> /* struct elf_prstatus */

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

static short
get_signal_number(Elf *e, const char *elf_file)
{
    const char NOTE_CORE[] = "CORE";

    size_t nphdr;
    if (elf_getphdrnum(e, &nphdr) != 0)
    {
        warn_elf("elf_getphdrnum");
        return 0;
    }

    /* Go through phdrs, look for prstatus note */
    int i;
    for (i = 0; i < nphdr; i++)
    {
        GElf_Phdr phdr;
        if (gelf_getphdr(e, i, &phdr) != &phdr)
        {
            warn_elf("gelf_getphdr");
            continue;
        }

        if (phdr.p_type != PT_NOTE)
        {
            continue;
        }

        Elf_Data *data, *name_data, *desc_data;
        GElf_Nhdr nhdr;
        size_t note_offset = 0;
        size_t name_offset, desc_offset;
        /* Elf_Data buffers are freed when elf_end is called. */
        data = elf_getdata_rawchunk(e, phdr.p_offset, phdr.p_filesz,
                                    ELF_T_NHDR);
        if (!data)
        {
            warn_elf("elf_getdata_rawchunk");
            continue;
        }

        while ((note_offset = gelf_getnote(data, note_offset, &nhdr,
                                           &name_offset, &desc_offset)) != 0)
        {
            /*
            printf("Note: type:%x name:%x+%d desc:%x+%d\n", nhdr.n_type,
                   name_offset, nhdr.n_namesz, desc_offset, nhdr.n_descsz);
            */

            if (nhdr.n_type != NT_PRSTATUS
                || nhdr.n_namesz < sizeof(NOTE_CORE))
                continue;

            name_data = elf_getdata_rawchunk(e, phdr.p_offset + name_offset,
                                             nhdr.n_namesz, ELF_T_BYTE);
            desc_data = elf_getdata_rawchunk(e, phdr.p_offset + desc_offset,
                                             nhdr.n_descsz, ELF_T_BYTE);
            if (!(name_data && desc_data))
                continue;

            if (name_data->d_size < sizeof(NOTE_CORE))
                continue;

            if (strcmp(NOTE_CORE, name_data->d_buf))
                continue;

            if (desc_data->d_size != sizeof(struct elf_prstatus))
            {
                warn("PRSTATUS core note of size %zu found, expected size: %zu",
                    desc_data->d_size, sizeof(struct elf_prstatus));
                continue;
            }

            struct elf_prstatus *prstatus = (struct elf_prstatus*)desc_data->d_buf;
            short signal = prstatus->pr_cursig;
            if (signal)
                return signal;
        }
    }

    return 0;
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
