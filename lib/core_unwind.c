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

#include "utils.h"
#include "core/unwind.h"
#include "internal_unwind.h"

#if !defined WITH_LIBDWFL && !defined WITH_LIBUNWIND

struct sr_core_stacktrace *
sr_parse_coredump(const char *coredump_filename,
                  const char *executable_filename,
                  char **error_message)
{
    *error_message = sr_asprintf("satyr is built without unwind support");
    return NULL;
}

#else /* !defined WITH_LIBDWFL && !defined WITH_LIBUNWIND */

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
warn(const char *fmt, ...)
{
    va_list ap;

    if (!sr_debug_parser)
        return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
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
        *elfp = elf_begin(fd, ELF_C_READ, NULL);
        if (*elfp == NULL)
        {
            warn("Unable to open executable '%s': %s", executable_file,
                 elf_errmsg(-1));
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
static int
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

    ch->eh = elf_begin(ch->fd, ELF_C_READ, NULL);
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

    if (dwfl_core_file_report(ch->dwfl, ch->eh) == -1)
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

    if (!*error_msg && !head)
    {
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
        GElf_Addr bid_addr;
        Dwarf_Addr start;

        ret = dwfl_module_build_id(mod, &build_id_bits, &bid_addr);
        if (ret > 0)
        {
            frame->build_id = sr_mallocz(2*ret + 1);
            sr_bin2hex(frame->build_id, (const char *)build_id_bits, ret);
        }

        if (dwfl_module_info(mod, NULL, &start, NULL, NULL, NULL,
                             &filename, NULL) != NULL)
        {
            frame->build_id_offset = ip - start;
            if (filename)
                frame->file_name = sr_strdup(filename);
        }

        funcname = dwfl_module_addrname(mod, (GElf_Addr)ip_adjusted);
        if (funcname)
            frame->function_name = sr_strdup(funcname);
    }

    return frame;
}

#endif /* !defined WITH_LIBDWFL && !defined WITH_LIBUNWIND */
