/*
    elves.c

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
#include "elves.h"
#include "utils.h"
#include "config.h"

#if (defined HAVE_LIBDW && defined HAVE_LIBELF)
#  define WITH_ELFUTILS
#endif

#ifdef WITH_ELFUTILS
#include <dwarf.h>
#include <elfutils/libdw.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <glib.h>

/**
 * Finds a section by its name in an ELF file.
 * @param elf
 *   A libelf handle representing the file.
 * @param section_name
 *   Name of section to be found.
 * @param data_dest
 *   Save the resulting elf data pointer here.  Cannot be NULL.
 * @param shdr_dest
 *   Save the section header here. Cannot be NULL.
 * @param error_message
 *   Will be filled by an error message if the function fails (returns
 *   zero).  Caller is responsible for calling free() on the string
 *   pointer.  If function succeeds, the pointer is not touched by the
 *   function.
 * @returns
 *   Zero on error, index of the section on success.
 */
static unsigned
find_elf_section_by_name(Elf *elf,
                         const char *section_name,
                         Elf_Data **data_dest,
                         GElf_Shdr *shdr_dest,
                         char **error_message)
{
    /* Find the string table index */
    size_t shdr_string_index;
    if (0 != elf_getshdrstrndx(elf, &shdr_string_index))
    {
        *error_message = sr_asprintf("elf_getshdrstrndx failed");
        return 0;
    }

    unsigned section_index = 0;
    Elf_Scn *section = NULL;
    while ((section = elf_nextscn(elf, section)) != NULL)
    {
        /* Starting index is 1. */
        ++section_index;

        GElf_Shdr shdr;
        if (gelf_getshdr(section, &shdr) != &shdr)
        {
            *error_message = sr_asprintf("gelf_getshdr failed");
            return 0;
        }

        const char *current_section_name = elf_strptr(elf,
                                                      shdr_string_index,
                                                      shdr.sh_name);

        if (!current_section_name)
        {
            *error_message = sr_asprintf("elf_strptr failed");
            return 0;
        }

        if (0 == strcmp(current_section_name, section_name))
        {
            /* We found the right section!  Save the data. */
            *data_dest = elf_getdata(section, NULL);
            if (!*data_dest)
            {
                *error_message = sr_asprintf("elf_getdata failed");
                return 0;
            }

            /* Save the section header. */
            *shdr_dest = shdr;
            return section_index;
        }
    }

    *error_message = sr_asprintf("Section %s not found", section_name);
    return 0;
}
#endif /* WITH_ELFUTILS */

struct sr_elf_plt_entry *
sr_elf_get_procedure_linkage_table(const char *filename,
                                   char **error_message)
{
#ifdef WITH_ELFUTILS
    /* Open the input file. */
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        *error_message = sr_asprintf("Failed to open file %s: %s",
                                     filename,
                                     strerror(errno));

        return NULL;
    }

    /* Initialize libelf on the opened file. */
    Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
    if (!elf)
    {
        *error_message = sr_asprintf("Failed to run elf_begin on file %s: %s",
                                     filename,
                                     elf_errmsg(-1));

        close(fd);
        return NULL;
    }

    /* Find the .plt section. */
    GElf_Shdr shdr;
    Elf_Data *plt_data;
    char *find_section_error_message;
    size_t plt_section_index = find_elf_section_by_name(elf,
                                                        ".plt",
                                                        &plt_data,
                                                        &shdr,
                                                        &find_section_error_message);
    if (0 == plt_section_index)
    {
        *error_message = sr_asprintf("Failed to find .plt section for %s: %s",
                                     filename,
                                     find_section_error_message);

        free(find_section_error_message);
        elf_end(elf);
        close(fd);
        return NULL;
    }

    /* Find the relocation section for .plt (typically .rela.plt), together
     * with its symbol and string table
     */
    uint64_t plt_base = shdr.sh_addr;
    Elf_Data *rela_plt_data = NULL;
    Elf_Data *plt_symbols = NULL;
    size_t stringtable = 0;
    Elf_Scn *section = NULL;
    while ((section = elf_nextscn(elf, section)) != NULL)
    {
        if (gelf_getshdr(section, &shdr) != &shdr)
        {
            *error_message = sr_asprintf("gelf_getshdr failed for %s: %s",
                                         filename,
                                         elf_errmsg(-1));

            elf_end(elf);
            close(fd);
            return NULL;
        }

        if (shdr.sh_type == SHT_RELA &&
            shdr.sh_info == plt_section_index)
        {
            rela_plt_data = elf_getdata(section, NULL);
            if (!rela_plt_data)
            {
                *error_message = sr_asprintf("elf_getdata failed for %s: %s",
                                             filename,
                                             elf_errmsg(-1));

                elf_end(elf);
                close(fd);
                return NULL;
            }

            /* Get symbol section for .rela.plt */
            Elf_Scn *symbol_section = elf_getscn(elf, shdr.sh_link);
            if (!symbol_section)
            {
                *error_message = sr_asprintf("elf_getscn failed for %s: %s",
                                             filename,
                                             elf_errmsg(-1));

                elf_end(elf);
                close(fd);
                return NULL;
            }

            plt_symbols = elf_getdata(symbol_section, NULL);
            if (!plt_symbols)
            {
                *error_message = sr_asprintf("elf_getdata failed for %s: %s",
                                             filename,
                                             elf_errmsg(-1));

                elf_end(elf);
                close(fd);
                return NULL;
            }

            /* Get string table for the symbol table. */
            if (gelf_getshdr(symbol_section, &shdr) != &shdr)
            {
                *error_message = sr_asprintf("gelf_getshdr failed for %s: %s",
                                             filename,
                                             elf_errmsg(-1));

                elf_end(elf);
                close(fd);
                return NULL;
            }

            stringtable = shdr.sh_link;
            break;
        }
    }

    if (0 == stringtable)
    {
        *error_message = sr_asprintf("Unable to read symbol table for .plt for file %s",
                                     filename);

        elf_end(elf);
        close(fd);
        return NULL;
    }

    /* PLT looks like this (see also AMD64 ABI, page 78):
     *
     * Disassembly of section .plt:
     *
     * 0000003463e01010 <attr_removef@plt-0x10>:
     *   3463e01010:   ff 35 2a 2c 20 00       pushq  0x202c2a(%rip)         <-- here is plt_base
     *   3463e01016:   ff 25 2c 2c 20 00       jmpq   *0x202c2c(%rip)            each "slot" is 16B wide
     *   3463e0101c:   0f 1f 40 00             nopl   0x0(%rax)                  0-th slot is skipped
     *
     * 0000003463e01020 <attr_removef@plt>:
     *   3463e01020:   ff 25 2a 2c 20 00       jmpq   *0x202c2a(%rip)
     *   3463e01026:   68 00 00 00 00          pushq  $0x0                   <-- this is the number we want
     *   3463e0102b:   e9 e0 ff ff ff          jmpq   3463e01010 <_init+0x18>
     *
     * 0000003463e01030 <fgetxattr@plt>:
     *   3463e01030:   ff 25 22 2c 20 00       jmpq   *0x202c22(%rip)
     *   3463e01036:   68 01 00 00 00          pushq  $0x1
     *   3463e0103b:   e9 d0 ff ff ff          jmpq   3463e01010 <_init+0x18>
     */
    struct sr_elf_plt_entry *result = NULL, *last = NULL;
    for (unsigned plt_offset = 16; plt_offset < plt_data->d_size; plt_offset += 16)
    {
        uint32_t *plt_index = (uint32_t*)(plt_data->d_buf + plt_offset + 7);

        GElf_Rela rela;
        if (gelf_getrela(rela_plt_data, *plt_index, &rela) != &rela)
        {
            *error_message = sr_asprintf("gelf_getrela failed for %s: %s",
                                         filename,
                                         elf_errmsg(-1));

            sr_elf_procedure_linkage_table_free(result);
            elf_end(elf);
            close(fd);
            return NULL;
        }

        GElf_Sym symb;
        if (gelf_getsym(plt_symbols, GELF_R_SYM(rela.r_info), &symb) != &symb)
        {
            *error_message = sr_asprintf("gelf_getsym failed for %s: %s",
                                         filename,
                                         elf_errmsg(-1));

            sr_elf_procedure_linkage_table_free(result);
            elf_end(elf);
            close(fd);
            return NULL;
        }

        struct sr_elf_plt_entry *entry = sr_malloc(sizeof(struct sr_elf_plt_entry));
        entry->symbol_name = sr_strdup(elf_strptr(elf, stringtable,
                                                  symb.st_name));

        entry->address = (uint64_t)(plt_base + plt_offset);
        entry->next = NULL;

        if (result)
        {
            last->next = entry;
            last = entry;
        }
        else
            result = last = entry;
    }

    elf_end(elf);
    close(fd);
    return result;
#else /* WITH_ELFUTILS */
    *error_message = sr_asprintf("satyr compiled without elfutils");
    return NULL;
#endif /* WITH_ELFUTILS */
}

void
sr_elf_procedure_linkage_table_free(struct sr_elf_plt_entry *entries)
{
    while (entries)
    {
        struct sr_elf_plt_entry *entry = entries;
        entries = entry->next;
        free(entry->symbol_name);
        free(entry);
    }
}

struct sr_elf_plt_entry *
sr_elf_plt_find_for_address(struct sr_elf_plt_entry *plt,
                            uint64_t address)
{
    struct sr_elf_plt_entry *entry = plt;
    while (entry)
    {
        if (entry->address == address)
            return entry;

        entry = entry->next;
    }

    return NULL;

}


#ifdef WITH_ELFUTILS
/* Given DWARF pointer encoding, return the length of the pointer in
 * bytes.
 */
static unsigned
encoded_size(const uint8_t encoding, const unsigned char *e_ident)
{
    switch (encoding & 0x07)
    {
        case DW_EH_PE_udata2:
            return 2;
        case DW_EH_PE_udata4:
            return 4;
        case DW_EH_PE_udata8:
            return 8;
        case DW_EH_PE_absptr:
            return (e_ident[EI_CLASS] == ELFCLASS32 ? 4 : 8);
        default:
            return 0; /* Don't know/care. */
    }
}

struct cie
{
    Dwarf_Off cie_offset;
    int ptr_len;
    bool pcrel;

    struct cie *next;
};

static void
cie_free(struct cie *entries)
{
    while (entries)
    {
        struct cie *entry = entries;
        entries = entry->next;
        free(entry);
    }
}


static struct cie *
read_cie(Dwarf_CFI_Entry *cfi,
         Dwarf_Off cfi_offset,
         unsigned char *e_ident,
         char **error_message)
{
    /* Default FDE encoding (i.e. no R in augmentation string) is
     * DW_EH_PE_absptr.
     */
    struct cie *cie = g_malloc0(sizeof(*cie));
    cie->cie_offset = cfi_offset;
    cie->ptr_len = encoded_size(DW_EH_PE_absptr, e_ident);

    /* Search the augmentation data for FDE pointer encoding.
     * Unfortunately, 'P' can come before 'R' (which we are looking
     * for), so we may have to parse the whole thing. See the
     * abovementioned blog post for details.
     */
    const char *augmentation = cfi->cie.augmentation;
    const uint8_t *augmentation_data = cfi->cie.augmentation_data;
    if (*augmentation == 'z')
        ++augmentation;

    while (*augmentation != '\0')
    {
        switch (*augmentation)
        {
        case 'R':
            cie->ptr_len = encoded_size(*augmentation_data, e_ident);
            if (cie->ptr_len != 4 && cie->ptr_len != 8)
            {
                *error_message = sr_asprintf("Unknown FDE encoding (CIE %jx)",
                                             (uintmax_t)cfi_offset);
                free(cie);
                return NULL;
            }

            if ((*augmentation_data & 0x70) == DW_EH_PE_pcrel)
                cie->pcrel = true;

            return cie;
        case 'L':
            ++augmentation_data;
            break;
        case 'P':
        {
            unsigned size = encoded_size(*augmentation_data, e_ident);
            if (0 == size)
            {
                *error_message = sr_asprintf("Unknown size for personality encoding (CIE %jx)",
                                             (uintmax_t)cfi_offset);

                free(cie);
                return NULL;
            }

            augmentation_data += size + 1;
            break;
        }
        default:
            *error_message = sr_asprintf("Unknown augmentation char (CIE %jx)",
                                         (uintmax_t)cfi_offset);
            free(cie);
            return NULL;
        }

        ++augmentation;
    }

    return cie;
}

/* Read len bytes and interpret them as a number. Pointer p does not
 * have to be aligned.
 *
 * Assumption: we'll always run on architecture the ELF is run on,
 * therefore we don't consider byte order.
 */
static uint64_t
fde_read_address(const uint8_t *p, unsigned len)
{
    union {
        uint8_t b[8];
        /* uint16_t n2; */
        uint32_t n4;
        uint64_t n8;
    } u;
    u.n8 = 0;

    for (unsigned i = 0; i < len; i++)
        u.b[i] = *p++;

    return (len == 4 ? (uint64_t)u.n4 : u.n8);
}
#endif /* WITH_ELFUTILS */

struct sr_elf_fde *
sr_elf_get_eh_frame(const char *filename,
                    char **error_message)
{
#ifdef WITH_ELFUTILS
    /* Open the input file. */
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        *error_message = sr_asprintf("Failed to open file %s: %s",
                                     filename,
                                     strerror(errno));
        return NULL;
    }

    /* Initialize libelf on the opened file. */
    Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
    if (!elf)
    {
        *error_message = sr_asprintf("Failed to run elf_begin on file %s: %s",
                                     filename,
                                     elf_errmsg(-1));
        close(fd);
        return NULL;
    }

    unsigned char *e_ident = (unsigned char *)elf_getident(elf, NULL);
    if (!e_ident)
    {
        *error_message = sr_asprintf("elf_getident failed for %s: %s",
                                     filename,
                                     elf_errmsg(-1));
        elf_end(elf);
        close(fd);
        return NULL;
    }

    /* Look up the .eh_frame section */
    GElf_Shdr shdr;
    Elf_Data *section_data;
    char *find_section_error_message;
    if (!find_elf_section_by_name(elf,
                                  ".eh_frame",
                                  &section_data,
                                  &shdr,
                                  &find_section_error_message))
    {
        *error_message = sr_asprintf("Failed to find .eh_frame section for %s: %s",
                                     filename,
                                     find_section_error_message);

        free(find_section_error_message);
        elf_end(elf);
        close(fd);
        return NULL;
    }

    /* Get the address at which the executable segment is loaded.  If
     * the .eh_frame addresses are absolute, this is used to convert
     * them to relative to the beginning of executable segment.  We
     * are looking for the first LOAD segment that is executable, I
     * hope this is sufficient.
     */
    size_t phnum;
    if (elf_getphdrnum(elf, &phnum) != 0)
    {
        *error_message = sr_asprintf("elf_getphdrnum failed for %s: %s",
                                     filename,
                                     elf_errmsg(-1));
        elf_end(elf);
        close(fd);
        return NULL;
    }

    uint64_t exec_base = UINT64_MAX;
    for (unsigned i = 0; i < phnum; i++)
    {
        GElf_Phdr phdr;
        if (gelf_getphdr(elf, i, &phdr) != &phdr)
        {
            *error_message = sr_asprintf("gelf_getphdr failed for %s: %s",
                                         filename,
                                         elf_errmsg(-1));
            elf_end(elf);
            close(fd);
            return NULL;
        }

        if (phdr.p_type == PT_LOAD && phdr.p_flags & PF_X)
        {
            exec_base = phdr.p_vaddr;
            break;
        }
    }

    if (exec_base == UINT64_MAX)
    {
        *error_message = sr_asprintf("Can't determine executable base for %s",
                                     filename);

        elf_end(elf);
        close(fd);
        return NULL;
    }

    /* We now have a handle to .eh_frame data. We'll use
     * dwarf_next_cfi to iterate through all FDEs looking for those
     * matching the addresses we have.
     *
     * Some info on .eh_frame can be found at
     * http://www.airs.com/blog/archives/460 and in DWARF
     * documentation for .debug_frame. The initial_location and
     * address_range decoding is 'inspired' by elfutils source.
     *
     * @todo If this linear scan is too slow, we can do binary search
     * on .eh_frame_hdr -- see http://www.airs.com/blog/archives/462
     */

    struct sr_elf_fde *result = NULL, *last = NULL;
    struct cie *cie_list = NULL, *cie_list_last = NULL;

    Dwarf_Off cfi_offset_next = 0;
    while (true)
    {
        Dwarf_CFI_Entry cfi;
        Dwarf_Off cfi_offset = cfi_offset_next;
        int ret = dwarf_next_cfi(e_ident,
                                 section_data,
                                 1,
                                 cfi_offset,
                                 &cfi_offset_next,
                                 &cfi);

        if (ret > 0)
        {
            /* We're at the end. */
            break;
        }

        if (ret < 0)
        {
            /* Error. If cfi_offset_next was updated, we may skip the
             * erroneous cfi. */
            if (cfi_offset_next > cfi_offset)
                continue;

            *error_message = sr_asprintf("dwarf_next_cfi failed for %s: %s",
                                         filename,
                                         dwarf_errmsg(-1));

            cie_free(cie_list);
            sr_elf_eh_frame_free(result);
            elf_end(elf);
            close(fd);
            return NULL;
        }

        if (dwarf_cfi_cie_p(&cfi))
        {
            /* Current CFI is a CIE. We store its offset and FDE encoding
             * attributes to be used when reading FDEs.
             */

            /* Default FDE encoding (i.e. no R in augmentation string) is
             * DW_EH_PE_absptr.
             */
            char *cie_error_message;
            struct cie *cie = read_cie(&cfi,
                                       cfi_offset,
                                       e_ident,
                                       &cie_error_message);
            if (!cie)
            {
                *error_message = sr_asprintf("CIE reading failed for %s: %s",
                                             filename,
                                             cie_error_message);

                free(cie_error_message);
                cie_free(cie_list);
                sr_elf_eh_frame_free(result);
                elf_end(elf);
                close(fd);
                return NULL;
            }

            /* Append the newly parsed CIE to our list of CIEs. */
            if (cie_list)
            {
                cie_list_last->next = cie;
                cie_list_last = cie;
            }
            else
                cie_list = cie_list_last = cie;
        }
        else
        {
            /* Current CFI is an FDE.
             */
            struct cie *cie = cie_list;

            /* Find the CIE data that we should have saved earlier. */
            while (cie)
            {
                /* In .eh_frame, CIE_pointer is relative, but libdw converts it
                 * to absolute offset. */
                if (cfi.fde.CIE_pointer == cie->cie_offset)
                    break; /* Found. */

                cie = cie->next;
            }

            if (!cie)
            {
                *error_message = sr_asprintf("CIE not found for FDE %jx in %s",
                                             (uintmax_t)cfi_offset,
                                             filename);

                cie_free(cie_list);
                sr_elf_eh_frame_free(result);
                elf_end(elf);
                close(fd);
                return NULL;
            }

            /* Read the two numbers we need and if they are PC-relative,
             * compute the offset from VMA base
             */

            uint64_t initial_location = fde_read_address(cfi.fde.start,
                                                         cie->ptr_len);

            uint64_t address_range = fde_read_address(cfi.fde.start + cie->ptr_len,
                                                      cie->ptr_len);

            if (cie->pcrel)
            {
                /* We need to determine how long is the 'length' (and
                 * consequently CIE id) field of this FDE -- it can be
                 * either 4 or 12 bytes long. */
                uint64_t length = fde_read_address(section_data->d_buf + cfi_offset, 4);
                uint64_t skip = (length == 0xffffffffUL ? 12 : 4);
                uint64_t mask = (cie->ptr_len == 4 ? 0xffffffffUL : 0xffffffffffffffffUL);
                initial_location += shdr.sh_offset + cfi_offset + 2 * skip;
                initial_location &= mask;
            }
            else
            {
                /* Assuming that not pcrel means absolute address
                 * (what if the file is a library?).  Convert to
                 * text-section-start-relative.
                 */
                initial_location -= exec_base;
            }

            struct sr_elf_fde *fde = sr_malloc(sizeof(struct sr_elf_fde));
            fde->exec_base = exec_base;
            fde->start_address = initial_location;
            fde->length = address_range;
            fde->next = NULL;

            /* Append the newly parsed Frame Description Entry to our
             * list of FDEs.
             */
            if (result)
            {
                last->next = fde;
                last = fde;
            }
            else
                result = last = fde;
        }
    }

    cie_free(cie_list);
    elf_end(elf);
    close(fd);
    return result;
#else /* WITH_ELFUTILS */
    *error_message = sr_asprintf("satyr compiled without elfutils");
    return NULL;
#endif /* WITH_ELFUTILS */
}

void
sr_elf_eh_frame_free(struct sr_elf_fde *entries)
{
    while (entries)
    {
        struct sr_elf_fde *entry = entries;
        entries = entry->next;
        free(entry);
    }
}

struct sr_elf_fde *
sr_elf_find_fde_for_offset(struct sr_elf_fde *eh_frame,
                           uint64_t build_id_offset)
{
    struct sr_elf_fde *fde = eh_frame;
    while (fde)
    {
        if (build_id_offset >= fde->start_address &&
            build_id_offset < fde->start_address + fde->length)
        {
            return fde;
        }

        fde = fde->next;
    }

    return NULL;
}

struct sr_elf_fde *
sr_elf_find_fde_for_address(struct sr_elf_fde *eh_frame,
                            uint64_t address)
{
    struct sr_elf_fde *fde = eh_frame;
    while (fde)
    {
        if (address >= fde->start_address + fde->exec_base &&
            address < fde->start_address + fde->exec_base + fde->length)
        {
            return fde;
        }

        fde = fde->next;
    }

    return NULL;
}

struct sr_elf_fde *
sr_elf_find_fde_for_start_address(struct sr_elf_fde *eh_frame,
                                  uint64_t start_address)
{
    struct sr_elf_fde *fde = eh_frame;
    while (fde)
    {
        if (start_address == fde->start_address + fde->exec_base)
            return fde;

        fde = fde->next;
    }

    return NULL;
}

char *
sr_elf_fde_to_json(struct sr_elf_fde *fde,
                   bool recursive)
{
    GString *strbuf = g_string_new(NULL);

    if (recursive)
    {
        struct sr_elf_fde *loop = fde;
        while (loop)
        {
            if (loop == fde)
                g_string_append(strbuf, "[ ");
            else
                g_string_append(strbuf, ", ");

            char *fde_json = sr_elf_fde_to_json(loop, false);
            char *indented_fde_json = sr_indent_except_first_line(fde_json, 2);
            g_string_append(strbuf, indented_fde_json);
            free(indented_fde_json);
            free(fde_json);
            loop = loop->next;
            if (loop)
                g_string_append(strbuf, "\n");
        }

        g_string_append(strbuf, " ]");
    }
    else
    {
        /* Start address. */
        g_string_append_printf(strbuf,
                              "{   \"start_address\": %"PRIu64"\n",
                              fde->start_address);

        /* Length. */
        g_string_append_printf(strbuf,
                              ",   \"length\": %"PRIu64"\n",
                              fde->length);

        g_string_append(strbuf, "}");
    }

    return g_string_free(strbuf, FALSE);
}
