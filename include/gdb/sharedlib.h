/*
    gdb_sharedlib.h

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
#ifndef SATYR_GDB_SHAREDLIB_H
#define SATYR_GDB_SHAREDLIB_H

/**
 * @file
 * @brief Shared library information as produced by GDB.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <inttypes.h>

enum
{
    SYMS_OK,
    SYMS_WRONG,
    SYMS_NOT_FOUND,
};

/**
 * @brief A shared library memory location as reported by GDB.
 */
struct sr_gdb_sharedlib
{
    uint64_t from;
    uint64_t to;
    int symbols;
    char *soname;
    struct sr_gdb_sharedlib *next;
};

/**
 * Creates and initializes a new sharedlib structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_gdb_sharedlib_free().
 */
struct sr_gdb_sharedlib *
sr_gdb_sharedlib_new();

/**
 * Initializes all members of the sharedlib to default values.  No
 * memory is released, members are simply overwritten.  This is useful
 * for initializing a sharedlib structure placed on the stack.
 */
void
sr_gdb_sharedlib_init(struct sr_gdb_sharedlib *sharedlib);

/**
 * Releases the memory held by the sharedlib.  Sharedlibs referenced
 * by .next are not released.
 * @param sharedlib
 * If sharedlib is NULL, no operation is performed.
 */
void
sr_gdb_sharedlib_free(struct sr_gdb_sharedlib *sharedlib);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' sharedlib.  If 'dest' is NULL, it
 * returns the 'item' sharedlib.
 */
struct sr_gdb_sharedlib *
sr_gdb_sharedlib_append(struct sr_gdb_sharedlib *dest,
                        struct sr_gdb_sharedlib *item);

/**
 * Creates a duplicate of the sharedlib structure.
 * @param sharedlib
 * Structure to be duplicated.
 * @param siblings
 * Whether to duplicate a single structure or whole list.
 * @returns
 * Never returns NULL. Returns the duplicated structure
 * or the first structure in the duplicated list.
 */
struct sr_gdb_sharedlib *
sr_gdb_sharedlib_dup(struct sr_gdb_sharedlib *sharedlib,
                     bool siblings);

/**
 * Returns the number of sharedlibs in the list.
 */
int
sr_gdb_sharedlib_count(struct sr_gdb_sharedlib *sharedlib);

/**
 * Finds whether the address belongs to some sharedlib
 * from the list starting by 'first'.
 * @returns
 * Pointer to an existing structure or NULL if not found.
 */
struct sr_gdb_sharedlib *
sr_gdb_sharedlib_find_address(struct sr_gdb_sharedlib *first,
                              uint64_t address);

/**
 * Parses the output of GDB's 'info sharedlib' command.
 * @param input
 * String representing the stacktrace.
 * @returns
 * First element of the list of loaded libraries.
 */
struct sr_gdb_sharedlib *
sr_gdb_sharedlib_parse(const char *input);

#ifdef __cplusplus
}
#endif

#endif
