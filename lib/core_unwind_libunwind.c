/*
    core_unwind.c

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
#include "core/frame.h"
#include "core/thread.h"
#include "core/stacktrace.h"
#include "internal_unwind.h"
#include "internal_utils.h"

#ifdef WITH_LIBUNWIND

#include <stdio.h>
#include <libunwind-coredump.h>

static struct sr_core_thread *
unwind_thread(struct UCD_info *ui,
              unw_addr_space_t as,
              Dwfl *dwfl,
              int thread_no,
              char **error_msg)
{
    int ret;
    unw_cursor_t c;
    struct sr_core_frame *trace = NULL;

    _UCD_select_thread(ui, thread_no);

    ret = unw_init_remote(&c, as, ui);
    if (ret < 0)
    {
        set_error("unw_init_remote failed: %s", unw_strerror(ret));
        return NULL;
    }

    int count = 1000;
    while (--count > 0)
    {
        unw_word_t ip;
        ret = unw_get_reg(&c, UNW_REG_IP, &ip);
        if (ret < 0)
            warn("unw_get_reg(UNW_REG_IP) failed: %s", unw_strerror(ret));

        /* Seen this happen when unwinding thread that did not start
         * in main(). */
        if (ip == 0)
            break;

        struct sr_core_frame *entry = resolve_frame(dwfl, ip, false);

        if (!entry->function_name)
        {
            size_t funcname_len = 512;
            char *funcname = sr_malloc(funcname_len);

            if (unw_get_proc_name(&c, funcname, funcname_len, NULL) == 0)
                entry->function_name = funcname;
            else
                free(funcname);
        }

        trace = sr_core_frame_append(trace, entry);
        /*
        printf("%s 0x%llx %s %s -\n",
                (ip_seg && ip_seg->build_id) ? ip_seg->build_id : "-",
                (unsigned long long)(ip_seg ? ip - ip_seg->vaddr : ip),
                (entry->symbol ? entry->symbol : "-"),
                (ip_seg && ip_seg->filename) ? ip_seg->filename : "-");
        */

        /* Do not unwind below __libc_start_main. */
        if (0 == sr_strcmp0(entry->function_name, "__libc_start_main"))
            break;

        ret = unw_step(&c);
        if (ret == 0)
            break;

        if (ret < 0)
        {
            warn("unw_step failed: %s", unw_strerror(ret));
            break;
        }
    }

    if (error_msg && !*error_msg && !trace)
    {
        set_error("No frames found for thread %d", thread_no);
    }

    struct sr_core_thread *thread = sr_core_thread_new();
    thread->frames = trace;
    return thread;
}

static int
get_signal_number_libunwind(struct UCD_info *ui)
{
    int tnum, nthreads = _UCD_get_num_threads(ui);
    for (tnum = 0; tnum < nthreads; ++tnum)
    {
        _UCD_select_thread(ui, tnum);
        int signo = _UCD_get_cursig(ui);

        /* Return first nonzero signal, gdb/bfd seem to work this way. */
        if (signo)
            return signo;
    }

    return 0;
}

struct sr_core_stacktrace *
sr_parse_coredump_maps(const char *core_file,
                       const char *exe_file,
                       const char *maps_file,
                       char **error_msg)
{
    struct sr_core_stacktrace *stacktrace = NULL;

    /* Initialize error_msg to 'no error'. */
    if (error_msg)
        *error_msg = NULL;

    struct core_handle *ch = open_coredump(core_file, exe_file, maps_file, error_msg);
    if (*error_msg)
        return NULL;

    unw_addr_space_t as;
    struct UCD_info *ui;
    as = unw_create_addr_space(&_UCD_accessors, 0);
    if (!as)
    {
        set_error("Failed to create address space");
        goto fail_destroy_handle;
    }

    ui = _UCD_create(core_file);
    if (!ui)
    {
        set_error("Failed to set up core dump accessors for '%s'", core_file);
        goto fail_destroy_as;
    }

    struct exe_mapping_data *s;
    for (s = ch->segments; s != NULL; s = s->next)
    {
        if (_UCD_add_backing_file_at_vaddr(ui, s->start, s->filename) < 0)
        {
            /* Sometimes produces:
             * >_UCD_add_backing_file_at_segment:
             * Error reading from '/usr/lib/modules/3.6.9-2.fc17.x86_64/vdso/vdso.so'
             * Ignore errors for now & fail later.
             */
            warn("Can't add backing file '%s' at addr 0x%jx", s->filename,
                 (uintmax_t)s->start);
            /* goto fail_destroy_ui; */
        }
    }

    stacktrace = sr_core_stacktrace_new();

    int tnum, nthreads = _UCD_get_num_threads(ui);
    for (tnum = 0; tnum < nthreads; ++tnum)
    {
        struct sr_core_thread *trace = unwind_thread(ui, as, ch->dwfl, tnum, error_msg);
        if (trace)
        {
            stacktrace->threads = sr_core_thread_append(stacktrace->threads, trace);
        }
        else
        {
            sr_core_stacktrace_free(stacktrace);
            stacktrace = NULL;
            break;
        }
    }

    stacktrace->executable = realpath(exe_file, NULL);
    stacktrace->signal = get_signal_number_libunwind(ui);
    /* FIXME: is this the best we can do? */
    stacktrace->crash_thread = stacktrace->threads;

    _UCD_destroy(ui);
fail_destroy_as:
    unw_destroy_addr_space(as);
fail_destroy_handle:
    core_handle_free(ch);

    return stacktrace;
}

#endif /* WITH_LIBUNWIND */
