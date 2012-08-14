/*
    utils_unstrip.h

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
#ifndef BTPARSER_UTILS_UNSTRIP_H
#define BTPARSER_UTILS_UNSTRIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/**
 * Output of the unstrip utility.
 */
struct btp_unstrip_entry
{
    uint64_t start;
    uint64_t length;
    char *build_id;
    char *file_name;
    char *mod_name;
    struct btp_unstrip_entry *next;
};

struct btp_unstrip_entry *
btp_unstrip_parse(const char *unstrip_output);

struct btp_unstrip_entry *
btp_unstrip_find_address(struct btp_unstrip_entry *entries,
                         uint64_t address);

void
btp_unstrip_free(struct btp_unstrip_entry *entries);

#ifdef __cplusplus
}
#endif

#endif
