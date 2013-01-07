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
#include "core_frame.h"
#include "core_thread.h"
#include "core_stacktrace.h"
#include "config.h"

#if (defined HAVE_LIBELF_H && defined HAVE_GELF_H && defined HAVE_LIBELF && defined HAVE_LIBDW && defined HAVE_ELFUTILS_LIBDWFL_H && defined HAVE_DWFL_FRAME_STATE_CORE)
#  define WITH_LIBDWFL
#endif

#ifdef WITH_LIBDWFL
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>
#include <elfutils/libdwfl.h>

/* Error/warning reporting macros. Allows the error reporting code to be less
 * verbose with the restrictions that:
 *  - pointer to error message pointer must be always named "error_msg"
 *  - the variable "elf_file" must always contain the filename that is operated
 *    on by the libelf
 */
#define set_error(fmt, ...) _set_error(error_msg, fmt, ##__VA_ARGS__)
#define set_error_elf(func) _set_error(error_msg, "%s failed for '%s': %s", \
        func, elf_file, elf_errmsg(-1))
#define warn_elf(func) warn("%s failed for '%s': %s", \
        func, elf_file, elf_errmsg(-1))
#define set_error_dwfl(func) _set_error(error_msg, "%s failed: %s", \
        func, dwfl_errmsg(-1))

#define list_append(head,tail,item)          \
    do{                                      \
        if (head == NULL)                    \
        {                                    \
            head = tail = item;              \
        }                                    \
        else                                 \
        {                                    \
            tail->next = item;               \
            tail = tail->next;               \
        }                                    \
    } while(0)

static void
_set_error(char **error_msg, const char *fmt, ...)
{
    va_list ap;

    if (error_msg == NULL)
        return;

    va_start(ap, fmt);
    *error_msg = btp_vasprintf(fmt, ap);
    va_end(ap);
}

struct core_handle
{
    int fd;
    Elf *eh;
    Dwfl *dwfl;
    Dwfl_Callbacks cb;
};

/* FIXME: is there another way to pass the executable name to the find_elf
 * callback? */
const char *executable_file = NULL;

static void
warn(const char *fmt, ...)
{
    va_list ap;

    if (!btp_debug_parser)
        return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

}

static void
core_handle_free(struct core_handle *ch)
{
    if (ch)
    {
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
    GElf_Addr bias;

    if (dwfl_module_getelf (mod, &bias) == NULL)
    {
        warn("cannot find ELF for '%s': %s", name, dwfl_errmsg(-1));
        return DWARF_CB_OK;
    }

    return DWARF_CB_OK;
}

/* Gets dwfl handle and executable map data to be used for unwinding */
static struct core_handle *
open_coredump(const char *elf_file, const char *exe_file, char **error_msg)
{
    struct core_handle *ch = btp_mallocz(sizeof(*ch));

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
    ptrdiff_t ret = dwfl_getmodules(ch->dwfl, touch_module, NULL, 0);
    if (ret == -1)
    {
        set_error_dwfl("dwfl_getmodules");
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

static struct btp_core_thread *
unwind_thread(Dwfl *dwfl, Dwfl_Frame_State *state, char **error_msg)
{
    int ret;
    struct btp_core_frame *head = NULL, *tail = NULL;
    pid_t tid = 0;

    if (state)
    {
        tid = dwfl_frame_tid_get(state);
    }

    while (state)
    {
        Dwarf_Addr pc, pc_adjusted;
        bool minus_one;
        if (!dwfl_frame_state_pc(state, &pc, &minus_one))
        {
            warn("Failed to obtain PC: %s", dwfl_errmsg(-1));
            break;
        }
        pc_adjusted = pc - (minus_one ? 1 : 0);

        struct btp_core_frame *frame = btp_core_frame_new();
        frame->address = frame->build_id_offset = (uint64_t)pc;
        list_append(head, tail, frame);

        Dwfl_Module *mod = dwfl_addrmodule(dwfl, (Dwarf_Addr)pc_adjusted);
        if (mod)
        {
            const unsigned char *build_id_bits;
            const char *filename, *funcname;
            GElf_Addr bid_addr;
            Dwarf_Addr start;

            ret = dwfl_module_build_id(mod, &build_id_bits, &bid_addr);
            if (ret > 0)
            {
                frame->build_id = btp_mallocz(2*ret + 1);
                btp_bin2hex(frame->build_id, (const char *)build_id_bits, ret);
            }

            if (dwfl_module_info(mod, NULL, &start, NULL, NULL, NULL,
                                 &filename, NULL) != NULL)
            {
                frame->build_id_offset = (Dwarf_Addr)pc - start;
                if (filename)
                    frame->file_name = btp_strdup(filename);
            }

            funcname = dwfl_module_addrname(mod, (GElf_Addr)pc_adjusted);
            if (funcname)
                frame->function_name = btp_strdup(funcname);
        }

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

    struct btp_core_thread *thread = btp_core_thread_new();
    thread->tid = (int64_t)tid;
    thread->frames = head;
    return thread;
}

struct btp_core_stacktrace *
btp_parse_coredump(const char *core_file,
                   const char *exe_file,
                   char **error_msg)
{
    struct btp_core_stacktrace *stacktrace = NULL;

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

    stacktrace = btp_core_stacktrace_new();
    if (!stacktrace)
    {
        set_error("Failed to initialize stacktrace memory");
        goto fail_destroy_handle;
    }
    struct btp_core_thread *threads_tail = NULL;

    do
    {
        struct btp_core_thread *t = unwind_thread(ch->dwfl, state, error_msg);
        if (*error_msg)
        {
            goto fail_destroy_trace;
        }
        list_append(stacktrace->threads, threads_tail, t);
        state = dwfl_frame_thread_next(state);
    } while (state);

fail_destroy_trace:
    if (*error_msg)
    {
        btp_core_stacktrace_free(stacktrace);
        stacktrace = NULL;
    }
fail_destroy_handle:
    core_handle_free(ch);

    stacktrace->executable = btp_strdup(executable_file);
    /* FIXME: determine signal */
    stacktrace->signal = 0;
    /* FIXME: is this the best we can do? */
    stacktrace->crash_thread = stacktrace->threads;
    return stacktrace;
}

#endif /* WITH_LIBDWFL */
