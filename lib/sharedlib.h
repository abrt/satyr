/*
    sharedlib.h

    Copyright (C) 2011  Red Hat, Inc.

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
#ifndef BTPARSER_SHAREDLIB_H
#define BTPARSER_SHAREDLIB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    SYMS_OK,
    SYMS_WRONG,
    SYMS_NOT_FOUND,
};

struct btp_sharedlib
{
    unsigned long long from;
    unsigned long long to;
    int symbols;
    char *soname;
    struct btp_sharedlib *next;
};

/**
 * Creates and initializes a new sharedlib structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_sharedlib_free().
 */
struct btp_sharedlib *
btp_sharedlib_new();

/**
 * Initializes all members of the sharedlib to default values.
 * No memory is released, members are simply overwritten.
 * This is useful for initializing a sharedlib structure placed on the
 * stack.
 */
void
btp_sharedlib_init(struct btp_sharedlib *sharedlib);

/**
 * Releases the memory held by the sharedlib.
 * Sharedlibs referenced by .next are not released.
 * @param sharedlib
 * If sharedlib is NULL, no operation is performed.
 */
void
btp_sharedlib_free(struct btp_sharedlib *sharedlib);

/**
 * Appends 'b' to the list 'a'.
 */
void
btp_sharedlib_append(struct btp_sharedlib *a,
                     struct btp_sharedlib *b);

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
struct btp_sharedlib *
btp_sharedlib_dup(struct btp_sharedlib *sharedlib,
                  bool siblings);

/**
 * Returns the number of sharedlibs in the list.
 */
int
btp_sharedlib_count(struct btp_sharedlib *sharedlib);

/**
 * Finds whether the address belongs to some sharedlib
 * from the list starting by 'first'.
 * @returns
 * Pointer to an existing structure or NULL if not found.
 */
struct btp_sharedlib *
btp_sharedlib_find_address(struct btp_sharedlib *first,
                           unsigned long long address);

/**
 * Parses the output of GDB's 'info sharedlib' command.
 * @param input
 * String representing the backtrace.
 * @returns
 * First element of the list of loaded libraries.
 */
struct btp_sharedlib *
btp_sharedlib_parse(const char *input);

#ifdef __cplusplus
}
#endif

#endif
