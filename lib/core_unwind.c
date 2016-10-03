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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/procfs.h> /* struct elf_prstatus */
#include <sys/ptrace.h> /* PTRACE_SEIZE */

#include <elfutils/version.h>

#include "utils.h"
#include "core/unwind.h"
#include "internal_unwind.h"
#include "internal_utils.h"

#include "location.h"
#include "gdb/frame.h"
#include "gdb/thread.h"
#include "gdb/stacktrace.h"
#include "core/frame.h"
#include "core/thread.h"
#include "core/stacktrace.h"

#if !defined WITH_LIBDWFL && !defined WITH_LIBUNWIND

struct sr_core_stacktrace *
sr_parse_coredump(const char *coredump_filename,
                  const char *executable_filename,
                  char **error_message)
{
    *error_message = sr_asprintf("satyr is built without unwind support");
    return NULL;
}

#endif /* !defined WITH_LIBDWFL && !defined WITH_LIBUNWIND */

#if (!defined WITH_LIBDWFL || !defined PTRACE_SEIZE)

struct sr_core_stacktrace *
sr_core_stacktrace_from_core_hook(pid_t thread_id,
                                  const char *executable_filename,
                                  int signum,
                                  char **error_message)
{
    *error_message = sr_asprintf("satyr is built without live process unwind support");
    return NULL;
}

#endif /* !defined WITH_LIBDWFL || !defined PTRACE_SEIZE */

/* FIXME: is there another way to pass the executable name to the find_elf
 * callback? */
const char *executable_file = NULL;

void
_set_error(char **error_msg, const char *fmt, ...)
{
    va_list ap;

    if (error_msg == NULL)
        return;

    va_start(ap, fmt);
    *error_msg = sr_vasprintf(fmt, ap);
    va_end(ap);
}

void
core_handle_free(struct core_handle *ch)
{
    if (ch)
    {
        struct exe_mapping_data *seg = ch->segments, *next;
        while (seg)
        {
            free(seg->filename);

            next = seg->next;
            free(seg);
            seg = next;
        }

        if (ch->dwfl)
            dwfl_end(ch->dwfl);
        if (ch->eh)
            elf_end(ch->eh);
        if (ch->fd > 0)
            close(ch->fd);
        free(ch);
    }
}

static int
find_elf_core (Dwfl_Module *mod, void **userdata, const char *modname,
               Dwarf_Addr base, char **file_name, Elf **elfp)
{
    int ret = -1;

    if (strcmp("[exe]", modname) == 0 || strcmp("[pie]", modname) == 0)
    {
        int fd = open(executable_file, O_RDONLY);
        if (fd < 0)
            return -1;

        *file_name = realpath(executable_file, NULL);
        *elfp = elf_begin(fd, ELF_C_READ_MMAP, NULL);
        if (*elfp == NULL)
        {
            warn("Unable to open executable '%s': %s", executable_file,
                 elf_errmsg(-1));
            close(fd);
            return -1;
        }

        ret = fd;
    }
    else
    {
        ret = dwfl_build_id_find_elf(mod, userdata, modname, base,
                                     file_name, elfp);
    }

    return ret;
}

/* Do not use debuginfo files at all. */
int
find_debuginfo_none (Dwfl_Module *mod, void **userdata, const char *modname,
                     GElf_Addr base, const char *file_name,
                     const char *debuglink_file, GElf_Word debuglink_crc,
                     char **debuginfo_file_name)
{
    return -1;
}

static int
touch_module(Dwfl_Module *mod, void **userdata, const char *name,
             Dwarf_Addr start_addr, void *arg)
{
    struct exe_mapping_data ***tailp = arg;
    const char *filename = NULL;
    GElf_Addr bias;
    Dwarf_Addr base;

    if (dwfl_module_getelf (mod, &bias) == NULL)
    {
        warn("cannot find ELF for '%s': %s", name, dwfl_errmsg(-1));
        return DWARF_CB_OK;
    }

    dwfl_module_info(mod, NULL, &base, NULL, NULL, NULL, &filename, NULL);

    if (filename)
    {
        **tailp = sr_mallocz(sizeof(struct exe_mapping_data));
        (**tailp)->start = (uint64_t)base;
        (**tailp)->filename = sr_strdup(filename);
        *tailp = &((**tailp)->next);
    }

    return DWARF_CB_OK;
}

struct core_handle *
open_coredump(const char *elf_file, const char *exe_file, char **error_msg)
{
    struct core_handle *ch = sr_mallocz(sizeof(*ch));
    struct exe_mapping_data *head = NULL, **tail = &head;

    /* Initialize libelf, open the file and get its Elf handle. */
    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        set_error_elf("elf_version");
        goto fail_free;
    }

    /* Open input file, and parse it. */
    ch->fd = open(elf_file, O_RDONLY);
    if (ch->fd < 0)
    {
        set_error("Unable to open '%s': %s", elf_file, strerror(errno));
        goto fail_free;
    }

    ch->eh = elf_begin(ch->fd, ELF_C_READ_MMAP, NULL);
    if (ch->eh == NULL)
    {
        set_error_elf("elf_begin");
        goto fail_close;
    }

    /* Check that we are working with a coredump. */
    GElf_Ehdr ehdr;
    if (gelf_getehdr(ch->eh, &ehdr) == NULL || ehdr.e_type != ET_CORE)
    {
        set_error("File '%s' is not a coredump", elf_file);
        goto fail_elf;
    }

    executable_file = exe_file;
    ch->cb.find_elf = find_elf_core;
    ch->cb.find_debuginfo = find_debuginfo_none;
    ch->cb.section_address = dwfl_offline_section_address;
    ch->dwfl = dwfl_begin(&ch->cb);

#if _ELFUTILS_PREREQ(0, 158)
    if (dwfl_core_file_report(ch->dwfl, ch->eh, exe_file) == -1)
#else
    if (dwfl_core_file_report(ch->dwfl, ch->eh) == -1)
#endif
    {
        set_error_dwfl("dwfl_core_file_report");
        goto fail_dwfl;
    }

    if (dwfl_report_end(ch->dwfl, NULL, NULL) != 0)
    {
        set_error_dwfl("dwfl_report_end");
        goto fail_dwfl;
    }

    /* needed so that module filenames are available during unwinding */
    ptrdiff_t ret = dwfl_getmodules(ch->dwfl, touch_module, &tail, 0);
    if (ret == -1)
    {
        set_error_dwfl("dwfl_getmodules");
        goto fail_dwfl;
    }
    ch->segments = head;

    if (!head)
    {
        if (error_msg && !*error_msg)
            set_error("No segments found in coredump '%s'", elf_file);
        goto fail_dwfl;
    }

    return ch;

fail_dwfl:
    dwfl_end(ch->dwfl);
fail_elf:
    elf_end(ch->eh);
fail_close:
    close(ch->fd);
fail_free:
    free(ch);

    return NULL;
}

struct sr_core_frame *
resolve_frame(Dwfl *dwfl, Dwarf_Addr ip, bool minus_one)
{
    struct sr_core_frame *frame = sr_core_frame_new();
    frame->address = frame->build_id_offset = (uint64_t)ip;

    /* see dwfl_frame_state_pc for meaning of this parameter */
    Dwarf_Addr ip_adjusted = ip - (minus_one ? 1 : 0);

    Dwfl_Module *mod = dwfl_addrmodule(dwfl, ip_adjusted);
    if (mod)
    {
        int ret;
        const unsigned char *build_id_bits;
        const char *filename, *funcname;
        GElf_Addr bias, bid_addr;
        Dwarf_Addr start;

        /* Initialize the module's main Elf for dwfl_module_build_id and dwfl_module_info */
        /* No need to deallocate the variable 'bias' and the return value.*/
        if (NULL == dwfl_module_getelf(mod, &bias))
            warn("The module's main Elf was not found");

        ret = dwfl_module_build_id(mod, &build_id_bits, &bid_addr);
        if (ret > 0)
        {
            frame->build_id = sr_mallocz(2*ret + 1);
            sr_bin2hex(frame->build_id, (const char *)build_id_bits, ret);
        }

        const char *modname = dwfl_module_info(mod, NULL, &start, NULL, NULL, NULL,
                                               &filename, NULL);

        if (modname)
        {
            frame->build_id_offset = ip - start;
            frame->file_name = filename ? sr_strdup(filename) : sr_strdup(modname);
        }

        funcname = dwfl_module_addrname(mod, (GElf_Addr)ip_adjusted);
        if (funcname)
        {
            char *demangled = sr_demangle_symbol(funcname);
            frame->function_name = (demangled ? demangled : sr_strdup(funcname));
        }
    }

    return frame;
}

short
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
sr_core_stacktrace_from_gdb(const char *gdb_output, const char *core_file,
                            const char *exe_file, char **error_msg)
{
    /* I'm not going to rewrite it now since the function is not being used. */
    assert(error_msg);
    /* Initialize error_msg to 'no error'. */
    *error_msg = NULL;

    struct core_handle *ch = open_coredump(core_file, exe_file, error_msg);
    if (*error_msg)
        return NULL;

    struct sr_gdb_stacktrace *gdb_stacktrace;
    struct sr_location location;
    sr_location_init(&location);

    gdb_stacktrace = sr_gdb_stacktrace_parse(&gdb_output, &location);
    if (!gdb_stacktrace)
    {
        *error_msg = sr_location_to_string(&location);
        core_handle_free(ch);
        return NULL;
    }

    struct sr_core_stacktrace *core_stacktrace = sr_core_stacktrace_new();

    for (struct sr_gdb_thread *gdb_thread = gdb_stacktrace->threads;
         gdb_thread;
         gdb_thread = gdb_thread->next)
    {
        struct sr_core_thread *core_thread = sr_core_thread_new();

        unsigned long nframes = CORE_STACKTRACE_FRAME_LIMIT;
        struct sr_gdb_frame *top_frame = gdb_thread->frames;
        for (struct sr_gdb_frame *gdb_frame = gdb_thread->frames;
             gdb_frame;
             gdb_frame = gdb_frame->next)
        {
            if (gdb_frame->signal_handler_called)
                continue;

            if (nframes)
                --nframes;
            else
                top_frame = top_frame->next;
        }

        for (struct sr_gdb_frame *gdb_frame = top_frame;
             gdb_frame;
             gdb_frame = gdb_frame->next)
        {
            if (gdb_frame->signal_handler_called)
                continue;

            struct sr_core_frame *core_frame = resolve_frame(ch->dwfl,
                    gdb_frame->address, false);

            core_thread->frames = sr_core_frame_append(core_thread->frames,
                    core_frame);
        }

        if (sr_gdb_stacktrace_find_crash_thread(gdb_stacktrace) == gdb_thread)
        {
            core_stacktrace->crash_thread = core_thread;
        }

        core_stacktrace->threads = sr_core_thread_append(
                core_stacktrace->threads, core_thread);
    }

    core_stacktrace->signal = get_signal_number(ch->eh, core_file);
    core_stacktrace->executable = realpath(exe_file, NULL);

    core_handle_free(ch);
    sr_gdb_stacktrace_free(gdb_stacktrace);
    return core_stacktrace;
}
