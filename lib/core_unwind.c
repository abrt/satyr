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
/* Based on code written by Denys Vlasenko:
   https://fedorahosted.org/pipermail/crash-catcher/2012-March/002529.html */
/*
 * TODO segv handler?
 * TODO think about tests
 * TODO think about integration
 */

#include "utils.h"
#include "core_stacktrace.h"

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libelf.h>
#include <gelf.h>
#include <libunwind-coredump.h>

struct mapping_data {
    uintptr_t start;
    char *filename;
    struct mapping_data *next;
};

struct core_segment_data
{
    uintptr_t offset;
    uintptr_t vaddr;
    uintptr_t filesz;
    uintptr_t memsz;
    char *filename;
    char *build_id;
    struct core_segment_data *next;
};

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

static void
mapping_data_free(struct mapping_data *map)
{
    struct mapping_data *next;

    while (map)
    {
        free(map->filename);

        next = map->next;
        free(map);
        map = next;
    }
}

static void
core_segment_data_free(struct core_segment_data *seg)
{
    struct core_segment_data *next;

    while (seg)
    {
        free(seg->filename);
        free(seg->build_id);

        next = seg->next;
        free(seg);
        seg = next;
    }
}

/* Returns the list of mapping_data's, one per executable file
 * mentioned in map file.
 */
static struct mapping_data *
load_map_file(const char *maps_file)
{
    struct mapping_data *head = NULL, *tail = NULL;

    char *file_contents = btp_file_to_string(maps_file);
    if (!file_contents)
    {
        warn("btp_file_to_string failed");
        return NULL;
    }

    char *line = file_contents;
    while (*line)
    {
        /* Parse lines of this form:
         * 08048000-08050000 r-xp 00000000 fd:01 665656 /usr/bin/md5sum
         * 08050000-08051000 r--p 00007000 fd:01 665656 /usr/bin/md5sum
         * 08051000-08052000 rw-p 00008000 fd:01 665656 /usr/bin/md5sum
         * 088ad000-088ce000 rw-p 00000000 00:00 0      [heap]
         * b76fe000-b7700000 rw-p 00000000 00:00 0
         * b7700000-b7701000 r-xp 00000000 00:00 0      [vdso]
         * bfdc3000-bfde4000 rw-p 00000000 00:00 0      [stack]
         */
        unsigned long long start;//, end, fileofs;
        char mode[20];
        char filename[1024];

        /* %c conversion doesn't store terminating NUL,
         * need to prepare for that! */
        memset(filename, 0, sizeof(filename));
        int r = sscanf(line, "%llx-%*s %19s %*s %*s %*s %1023[^\n]",
                       &start, /*&end,*/
                       mode, /*&fileofs, &maj, &min, &inode,*/
                       filename);
        line += strcspn(line, "\n") + 1;

        if (r < 2)
        {
            warn("Malformed line in maps file, ignored");
            continue;
        }
        /* not a read/execute private mapping? */
        if (strcmp(mode, "r-xp") != 0)
            continue;
        /* filename is missing? */
        if (filename[0] != '/')
            continue;

        struct mapping_data *mapping = btp_mallocz(sizeof(*mapping));
        mapping->start = start;
        mapping->filename = btp_strdup(filename);
        //printf("mapping: vaddr:%llx name:'%s'\n",
        //       (unsigned long long)start, filename);

        list_append(head, tail, mapping);
    }

    free(file_contents);
    return head;
}

/* Returns hex-encoded build id of elf_file, or NULL if none is found. If the
 * mtime of core_mtime is newer than that of elf_file, error is returned. The
 * string pointed to by error_msg contains the error message, or is not written
 * to if there is no error.
 */
static char*
build_id_from_file(const char *elf_file, time_t core_mtime, char **error_msg)
{
    int i;
    int fd;
    char *result = NULL;
    Elf *e = NULL;

    /* Initialize libelf, open the file and get its Elf handle. */
    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        set_error_elf("elf_version");
        return NULL;
    }

    fd = open(elf_file, O_RDONLY);
    if (fd < 0)
    {
        set_error("Unable to open '%s': %s", elf_file, strerror(errno));
        return NULL;
    }

    struct stat stat;
    if (fstat(fd, &stat) == -1)
    {
        set_error("Unable to stat '%s': %s", elf_file, strerror(errno));
        goto fail_close;
    }

    if (stat.st_mtime > core_mtime)
    {
        set_error("File '%s' is newer than the coredump, aborting unwind", elf_file);
        goto fail_close;
    }

    e = elf_begin(fd, ELF_C_READ, NULL);
    if (e == NULL)
    {
        warn_elf("elf_begin");
        goto fail_close;
    }

    size_t nphdr;
    if (elf_getphdrnum(e, &nphdr) != 0)
    {
        warn_elf("elf_getphdrnum");
        goto end;
    }

    /* Go through phdrs, look for note containing build id. */
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
            //printf("Note: type:%x name:%x+%d desc:%x+%d\n", nhdr.n_type,
            //       name_offset, nhdr.n_namesz, desc_offset, nhdr.n_descsz);
            if (nhdr.n_type != NT_GNU_BUILD_ID
                || nhdr.n_namesz < sizeof(ELF_NOTE_GNU))
                continue;

            name_data = elf_getdata_rawchunk(e, phdr.p_offset + name_offset,
                                             nhdr.n_namesz, ELF_T_BYTE);
            desc_data = elf_getdata_rawchunk(e, phdr.p_offset + desc_offset,
                                             nhdr.n_descsz, ELF_T_BYTE);
            if (!(name_data && desc_data))
                continue;

            if (name_data->d_size < sizeof(ELF_NOTE_GNU))
                continue;

            if (strcmp(ELF_NOTE_GNU, name_data->d_buf))
                continue;

            result = btp_malloc(nhdr.n_descsz * 2 + 1);
            btp_bin2hex(result, (char*)desc_data->d_buf,
                        nhdr.n_descsz)[0] = '\0';
            goto end;
        }
    }

end:
    elf_end(e);
fail_close:
    close(fd);

    return result;
}

/* Extracts information about executable segments in the coredump. If we can
 * find an executable for the segment, we check whether the executable is older
 * than the coredump and extract a build id from it.
 */
static struct core_segment_data *
analyze_coredump(const char *elf_file, struct mapping_data *maps,
                 char **error_msg)
{
    struct core_segment_data *head = NULL, *tail = NULL;
    int fd;
    Elf *e = NULL;
    GElf_Ehdr ehdr;

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

    struct stat stat;
    time_t core_mtime;
    if (fstat(fd, &stat) == -1)
    {
        set_error("Unable to stat '%s': %s", elf_file, strerror(errno));
        goto fail;
    }

    core_mtime = stat.st_mtime;
    //printf("Core mtime: %s\n", ctime(&core_mtime));

    e = elf_begin(fd, ELF_C_READ, NULL);
    if (e == NULL)
    {
        set_error_elf("elf_begin");
        goto fail_close;
    }

    /* Check that we are working with a coredump. */
    if (gelf_getehdr(e, &ehdr) == NULL || ehdr.e_type != ET_CORE)
    {
        set_error("File '%s' is not a coredump", elf_file);
        goto fail;
    }

    /* Iterate over segments, read phdrs. */
    int i;
    size_t nphdr;
    if (elf_getphdrnum(e, &nphdr) != 0)
    {
        set_error_elf("elf_getphdrnum");
        goto fail;
    }

    for (i = 0; i < nphdr; i++)
    {
        GElf_Phdr phdr;
        if (gelf_getphdr(e, i, &phdr) != &phdr)
        {
            warn_elf("gelf_getphdr");
            continue;
        }

        if (phdr.p_type != PT_LOAD || !(phdr.p_flags & PF_X))
        {
            continue;
        }

        struct core_segment_data *segment = btp_mallocz(sizeof(*segment));
        segment->offset = phdr.p_offset;
        segment->filesz = phdr.p_filesz;
        segment->vaddr  = phdr.p_vaddr;
        segment->memsz  = phdr.p_memsz;

        /* Assign a filename to the segment if we know it. */
        struct mapping_data *m;
        for (m = maps; m != NULL; m = m->next)
        {
            if (m->start == phdr.p_vaddr)
            {
                segment->filename = btp_strdup(m->filename);

                /* Even though the segment contains (in most cases) the build
                 * id, we read it from the binary pointed to by 'm->filename',
                 * as we can use libelf for this (which would likely be unsafe
                 * for the incomplete segment in the coredump) and we will need
                 * to have consistent binary file for libunwind to use anyway.
                 *
                 * The function also checks whether the binary is not newer
                 * than the core. If it is, we do not proceed with the
                 * backtrace generation as the binary likely mismatches the one
                 * that was present during the crash. That would probably
                 * confuse libunwind.
                 * NOTE: It would be better to compare the build id inside the
                 * core with the one in the file instead of mtimes. Or maybe
                 * the whole first page (what about relocations)?
                 */
                segment->build_id = build_id_from_file(m->filename, core_mtime,
                                                       error_msg);
                if (*error_msg)
                {
                    goto fail_close;
                }

                //printf("%s: %s\n", segment->filename, segment->build_id);
                break;
            }
        }

        list_append(head, tail, segment);
    }

fail_close:
    elf_end(e);
fail:
    close(fd);

    if (*error_msg == NULL && head == NULL)
    {
        set_error("No segments found in coredump '%s'", elf_file);
    }

    return head;
}

static GList *
unwind_thread(struct UCD_info *ui, unw_addr_space_t as,
              struct core_segment_data *segments, int thread_no,
              char **error_msg)
{
    int ret;
    unw_cursor_t c;
    GList *trace = NULL;

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

        struct core_segment_data *seg, *ip_seg = NULL;
        for (seg = segments; seg != NULL; seg = seg->next)
        {
            if (seg->vaddr <= ip && seg->vaddr + seg->memsz > ip)
            {
                ip_seg = seg;
                break;
            }
        }

        struct backtrace_entry *entry = btp_mallocz(sizeof(*entry));
        entry->address = ip;
        entry->build_id = (ip_seg && ip_seg->build_id) ?
            btp_strdup(ip_seg->build_id) : NULL;
        entry->build_id_offset = ip_seg ? (ip - ip_seg->vaddr) : ip;
        entry->filename = (ip_seg && ip_seg->filename) ?
            btp_strdup(ip_seg->filename) : NULL;

        unw_proc_info_t pi;
        unw_word_t off;
        char funcname[10*1024]; /* mangled C++ names are HUGE */
        ret = unw_get_proc_info(&c, &pi);
        if (ret >= 0 && pi.start_ip + 1 != pi.end_ip)
        {
            entry->function_initial_loc = pi.start_ip;
            entry->function_length = pi.end_ip - pi.start_ip - 1;

            /* NOTE: unw_get_proc_name returns the closest label when the
             * symbols are stripped from the file, which is usually incorrect
             * and can be confusing. We use the start and end IPs of the
             * procedure to determine whether the label lies inside it and
             * ignore it if it doesn't.
             * It would be more correct to check whether ip-off==start_ip, but
             * label inside the procedure can still be helpful (and gdb shows
             * that one as well).
             */
            ret = unw_get_proc_name(&c, funcname, sizeof(funcname)-1, &off);
            if (ret == 0 && ip-off >= pi.start_ip)
            {
                entry->symbol = btp_strdup(funcname);
            }
        }

        trace = g_list_append(trace, entry);
        printf("%s 0x%llx %s %s -\n",
                (ip_seg && ip_seg->build_id) ? ip_seg->build_id : "-",
                (unsigned long long)(ip_seg ? ip - ip_seg->vaddr : ip),
                (entry->symbol ? entry->symbol : "-"),
                (ip_seg && ip_seg->filename) ? ip_seg->filename : "-"
        );

        ret = unw_step(&c);
        if (ret == 0)
            break;
        if (ret < 0)
        {
            warn("unw_step failed: %s", unw_strerror(ret));
            break;
        }
    }

    if (error_msg == NULL && trace == NULL)
    {
        set_error("No frames found for thread %d", thread_no);
    }

    return trace;
}

GList *
btp_parse_coredump(const char *core_file, const char *maps_file,
                   char **error_msg)
{
    GList *trace = NULL;

    /* Initialize error_msg to 'no error'. */
    if (error_msg)
        *error_msg = NULL;

    struct mapping_data *mappings = load_map_file(maps_file);
    if (mappings == NULL)
    {
        set_error("Failed to read maps file");
        return NULL;
    }

    struct core_segment_data *segments = analyze_coredump(core_file, mappings,
                                                          error_msg);
    if (*error_msg)
        goto fail_free_maps;

    unw_addr_space_t as;
    struct UCD_info *ui;
    as = unw_create_addr_space(&_UCD_accessors, 0);
    if (!as)
    {
        set_error("Failed to create address space");
        goto fail_free_both;
    }

    ui = _UCD_create(core_file);
    if (!ui)
    {
        set_error("Failed to set up core dump accessors for '%s'", core_file);
        goto fail_destroy_as;
    }

    struct mapping_data *map;
    for (map = mappings; map != NULL; map = map->next)
    {
        if (_UCD_add_backing_file_at_vaddr(ui, map->start, map->filename) < 0)
        {
            set_error("Can't add backing file '%s' at addr 0x%jx",
                      map->filename, (uintmax_t)map->start);
            goto fail_destroy_ui;
        }
    }

    /* NOTE: the stack unwinding should be run for each thread in the
     * new version, like this:
     *
     * int tnum, nthreads = _UCD_get_num_threads(ui);
     * for (tnum = 0; tnum < nthreads; tnum++)
     * {
     *     trace = unwind_thread(ui, as, segments, tnum, error_msg);
     *     if (*error_msg) {
     *         // ...
     *     }
     *     // ...
     * }
     */

    trace = unwind_thread(ui, as, segments, 0, error_msg);

fail_destroy_ui:
    _UCD_destroy(ui);
fail_destroy_as:
    unw_destroy_addr_space(as);
fail_free_both:
    core_segment_data_free(segments);
fail_free_maps:
    mapping_data_free(mappings);

    return trace;
}

void
print_mapping_data(const char *core_file, const char *maps_file)
{
    //printf("%p\n", _UCD_add_backing_file_at_vaddr);

    char *error;
    btp_parse_coredump(core_file, maps_file, &error);
    if (error)
        printf("ERROR: %s\n", error);
}
