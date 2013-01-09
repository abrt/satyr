/*
    elves.h

    Copyright (C) 2011, 2012  Red Hat, Inc.

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
#ifndef BTPARSER_ELVES_H
#define BTPARSER_ELVES_H

/**
 * @file
 * @brief Loading PLT and FDEs from ELF binaries.
 *
 * File name elf.h cannot be used due to collision with <elf.h> system
 * include.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

/**
 * @brief A single item of the Procedure Linkage Table present in ELF
 * binaries.
 */
struct btp_elf_plt_entry
{
    /** Address of the entry. */
    uint64_t address;
    /** Symbol name corresponding to the address. */
    char *symbol_name;

    struct btp_elf_plt_entry *next;
};

/**
 * @brief A single Frame Description Entry of the .eh_frame section
 * present in ELF binaries.
 */
struct btp_elf_fde
{
    /** Offset where all functions starts.  start_address is relative
     * to this exec base.
     */
    uint64_t exec_base;

    /**
     * Offset where a function starts.  If the function is present in
     * the Procedure Linkage Table, this address matches some address
     * in btp_elf_plt_entry.
     */
    uint64_t start_address;

    /** Length of the function in bytes. */
    uint64_t length;

    struct btp_elf_fde *next;
};

/**
 * Reads the Procedure Linkage Table from an ELF file.
 * @param error_message
 *   Will be filled by an error message if the function fails (returns
 *   NULL).  Caller is responsible for calling free() on the string
 *   pointer.  If function succeeds, the pointer is not touched by the
 *   function.
 * @returns
 *   Linked list of PLT entries on success. NULL otherwise.
 */
struct btp_elf_plt_entry *
btp_elf_get_procedure_linkage_table(const char *filename,
                                    char **error_message);

void
btp_elf_procedure_linkage_table_free(struct btp_elf_plt_entry *entries);

struct btp_elf_plt_entry *
btp_elf_plt_find_for_address(struct btp_elf_plt_entry *plt,
                             uint64_t address);

/**
 * Reads the .eh_frame section from an ELF file.
 * @param error_message
 *   Will be filled by an error message if the function fails (returns
 *   NULL).  Caller is responsible for calling free() on the string
 *   pointer.  If function succeeds, the pointer is not touched by the
 *   function.
 * @returns
 *   Returns a linked list of function ranges (function offset and
 *   size) on success. Otherwise NULL.
 */
struct btp_elf_fde *
btp_elf_get_eh_frame(const char *filename,
                     char **error_message);

void
btp_elf_eh_frame_free(struct btp_elf_fde *entries);

struct btp_elf_fde *
btp_elf_find_fde_for_address(struct btp_elf_fde *eh_frame,
                             uint64_t build_id_offset);

char *
btp_elf_fde_to_json(struct btp_elf_fde *fde,
                    bool recursive);

#ifdef __cplusplus
}
#endif

#endif
