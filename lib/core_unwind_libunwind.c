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
#include "core_frame.h"
#include "core_thread.h"
#include "core_stacktrace.h"
#include "config.h"

#if (defined HAVE_LIBELF_H && defined HAVE_GELF_H && defined HAVE_LIBELF && defined HAVE_LIBDW && defined HAVE_ELFUTILS_LIBDWFL_H && defined HAVE_DWFL_FRAME_STATE_CORE)
#  define WITH_LIBDWFL
#endif

#if !defined WITH_LIBDWFL && (defined HAVE_LIBUNWIND && defined HAVE_LIBUNWIND_COREDUMP && defined HAVE_LIBUNWIND_GENERIC && defined HAVE_LIBUNWIND_COREDUMP_H && defined HAVE_LIBELF_H && defined HAVE_GELF_H && defined HAVE_LIBELF && defined HAVE_LIBDW && defined HAVE_ELFUTILS_LIBDWFL_H)
#  define WITH_LIBUNWIND
#endif

#ifdef WITH_LIBUNWIND

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
#include <libunwind-coredump.h>
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

struct exe_mapping_data
{
    uint64_t start;
    char *filename;
    struct exe_mapping_data *next;
};

struct core_handle
{
    int fd;
    Elf *eh;
    Dwfl *dwfl;
    struct exe_mapping_data *segments;
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
exe_mapping_data_free(struct exe_mapping_data *seg)
{
    struct exe_mapping_data *next;

    while (seg)
    {
        free(seg->filename);

        next = seg->next;
        free(seg);
        seg = next;
    }
}

static void
core_handle_free(struct core_handle *ch)
{
    if (ch)
    {
        exe_mapping_data_free(ch->segments);
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
find_elf_core(Dwfl_Module *mod,
              void **userdata,
              const char *modname,
              Dwarf_Addr base,
              char **file_name,
              Elf **elfp)
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
find_debuginfo_none(Dwfl_Module *mod,
                    void **userdata,
                    const char *modname,
                    GElf_Addr base,
                    const char *file_name,
                    const char *debuglink_file,
                    GElf_Word debuglink_crc,
                    char **debuginfo_file_name)
{
    return -1;
}

static int
cb_exe_maps(Dwfl_Module *mod,
            void **userdata,
            const char *name,
            Dwarf_Addr start_addr,
            void *arg)
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
        **tailp = btp_mallocz(sizeof(struct exe_mapping_data));
        (**tailp)->start = (uint64_t)base;
        (**tailp)->filename = btp_strdup(filename);
        *tailp = &((**tailp)->next);
    }

    return DWARF_CB_OK;
}

/* Gets dwfl handle and executable map data to be used for unwinding */
static struct core_handle *
analyze_coredump(const char *elf_file,
                 const char *exe_file,
                 char **error_msg)
{
    struct exe_mapping_data *head = NULL, **tail = &head;
    int fd;
    Elf *e = NULL;

    /* Initialize libelf, open the file and get its Elf handle. */
    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        set_error_elf("elf_version");
        return NULL;
    }

    /* Open input file, and parse it. */
    fd = open(elf_file, O_RDONLY);
    if (fd < 0)
    {
        set_error("Unable to open '%s': %s", elf_file, strerror(errno));
        return NULL;
    }

    e = elf_begin(fd, ELF_C_READ, NULL);
    if (e == NULL)
    {
        set_error_elf("elf_begin");
        goto fail_close;
    }

    /* Check that we are working with a coredump. */
    GElf_Ehdr ehdr;
    if (gelf_getehdr(e, &ehdr) == NULL || ehdr.e_type != ET_CORE)
    {
        set_error("File '%s' is not a coredump", elf_file);
        goto fail_elf;
    }

    Dwfl_Callbacks dwcb =
    {
        .find_elf = find_elf_core,
        .find_debuginfo = find_debuginfo_none,
    /*  Use the line below to also use debuginfo files for symbols
        .find_debuginfo = dwfl_build_id_find_debuginfo,
    */
        .section_address = dwfl_offline_section_address
    };
    executable_file = exe_file;

    Dwfl *dwfl = dwfl_begin(&dwcb);

    if (dwfl_core_file_report(dwfl, e) == -1)
    {
        set_error_dwfl("dwfl_core_file_report");
        goto fail_dwfl;
    }

    if (dwfl_report_end(dwfl, NULL, NULL) != 0)
    {
        set_error_dwfl("dwfl_report_end");
        goto fail_dwfl;
    }

    ptrdiff_t ret = dwfl_getmodules(dwfl, cb_exe_maps, &tail, 0);
    if (ret == -1)
    {
        set_error_dwfl("dwfl_getmodules");
        goto fail_dwfl;
    }

    if (!*error_msg && !head)
    {
        set_error("No segments found in coredump '%s'", elf_file);
        goto fail_dwfl;
    }

    struct core_handle *ch = btp_mallocz(sizeof(*ch));
    ch->fd = fd;
    ch->eh = e;
    ch->dwfl = dwfl;
    ch->segments = head;
    return ch;

fail_dwfl:
    dwfl_end(dwfl);
    exe_mapping_data_free(head);
fail_elf:
    elf_end(e);
fail_close:
    close(fd);

    return NULL;
}

static struct btp_core_thread *
unwind_thread(struct UCD_info *ui,
              unw_addr_space_t as,
              Dwfl *dwfl,
              int thread_no,
              char **error_msg)
{
    int ret;
    unw_cursor_t c;
    struct btp_core_frame *trace = NULL;

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

        struct btp_core_frame *entry = btp_core_frame_new();
        entry->address = entry->build_id_offset = ip;
        entry->build_id = entry->file_name = NULL;
        entry->function_name = NULL;

        Dwfl_Module *mod = dwfl_addrmodule(dwfl, (Dwarf_Addr)ip);
        if (mod)
        {
            const unsigned char *build_id_bits;
            const char *filename, *funcname;
            GElf_Addr bid_addr;
            Dwarf_Addr start;

            ret = dwfl_module_build_id(mod, &build_id_bits, &bid_addr);
            if (ret > 0)
            {
                entry->build_id = btp_mallocz(2 * ret + 1);
                btp_bin2hex(entry->build_id, (const char *)build_id_bits, ret);
            }

            if (dwfl_module_info(mod, NULL, &start, NULL, NULL, NULL,
                                 &filename, NULL) != NULL)
            {
                entry->build_id_offset = (Dwarf_Addr)ip - start;
                if (filename)
                    entry->file_name = btp_strdup(filename);
            }

            funcname = dwfl_module_addrname(mod, (GElf_Addr)ip);
            if (funcname)
                entry->function_name = btp_strdup(funcname);
        }

        trace = btp_core_frame_append(trace, entry);
        /*
        printf("%s 0x%llx %s %s -\n",
                (ip_seg && ip_seg->build_id) ? ip_seg->build_id : "-",
                (unsigned long long)(ip_seg ? ip - ip_seg->vaddr : ip),
                (entry->symbol ? entry->symbol : "-"),
                (ip_seg && ip_seg->filename) ? ip_seg->filename : "-");
        */
        ret = unw_step(&c);
        if (ret == 0)
            break;

        if (ret < 0)
        {
            warn("unw_step failed: %s", unw_strerror(ret));
            break;
        }
    }

    if (!error_msg && !trace)
    {
        set_error("No frames found for thread %d", thread_no);
    }

    struct btp_core_thread *thread = btp_core_thread_new();
    thread->frames = trace;
    return thread;
}

struct btp_core_stacktrace *
btp_parse_coredump(const char *core_file,
                   const char *exe_file,
                   char **error_msg)
{
    struct btp_core_thread *trace = NULL;

    /* Initialize error_msg to 'no error'. */
    if (error_msg)
        *error_msg = NULL;

    struct core_handle *ch = analyze_coredump(core_file, exe_file,
                                              error_msg);
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
            set_error("Can't add backing file '%s' at addr 0x%jx",
                      s->filename, (uintmax_t)s->start);
            goto fail_destroy_ui;
        }
    }

    /* NOTE: the stack unwinding should be run for each thread in the
     * new version, like this:
     *
     * int tnum, nthreads = _UCD_get_num_threads(ui);
     * for (tnum = 0; tnum < nthreads; tnum++)
     * {
     *     trace = unwind_thread(ui, as, ch->dwfl, tnum, error_msg);
     *     if (*error_msg) {
     *         // ...
     *     }
     *     // ...
     * }
     */

    trace = unwind_thread(ui, as, ch->dwfl, 0, error_msg);

fail_destroy_ui:
    _UCD_destroy(ui);
fail_destroy_as:
    unw_destroy_addr_space(as);
fail_destroy_handle:
    core_handle_free(ch);

    if (trace)
    {
        struct btp_core_stacktrace *stacktrace = btp_core_stacktrace_new();
        stacktrace->threads = trace;
        return stacktrace;
    }

    return NULL;
}

#endif /* WITH_LIBUNWIND */
