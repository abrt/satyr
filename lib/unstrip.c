/*
    unstrip.c

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
#include "unstrip.h"
#include "utils.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

struct sr_unstrip_entry *
sr_unstrip_parse(const char *unstrip_output)
{
    struct sr_unstrip_entry *result = NULL, *last = NULL;

    const char *line = unstrip_output;
    while (*line)
    {
        /* beginning of the line */

        /* START+SIZE */
        uint64_t start;
        uint64_t length;
        int chars_read;
        int ret = sscanf(line, "0x%"PRIx64"+0x%"PRIx64" %n",
                         &start, &length, &chars_read);

        if (ret < 2)
            goto eat_line;

        line += chars_read;

        /* BUILDID */
        const char *build_id = line;
        while (isxdigit(*line))
            ++line;

        unsigned build_id_len = line - build_id;

        /* there may be @ADDR after the ID */
        line = sr_skip_non_whitespace(line);
        line = sr_skip_whitespace(line);

        /* FILE */
        const char *file_name = line;
        line = sr_skip_non_whitespace(line);
        unsigned file_name_len = line - file_name;
        line = sr_skip_whitespace(line);

        /* DEBUGFILE */
        line = sr_skip_non_whitespace(line);
        line = sr_skip_whitespace(line);

        /* MODULENAME */
        const char *mod_name = line;
        line = sr_skip_non_whitespace(line);
        unsigned mod_name_len = line - mod_name;

        struct sr_unstrip_entry *entry = g_malloc(sizeof(*entry));
        entry->start = start;
        entry->length = length;
        entry->build_id = g_strndup(build_id, build_id_len);
        entry->file_name = g_strndup(file_name, file_name_len);
        entry->mod_name = g_strndup(mod_name, mod_name_len);
        entry->next = NULL;

        if (!result)
            result = last = entry;
        else
        {
            last->next = entry;
            last = entry;
        }

eat_line:
        while (*line && *line++ != '\n')
            continue;
    }

    return result;
}

struct sr_unstrip_entry *
sr_unstrip_find_address(struct sr_unstrip_entry *entries,
                        uint64_t address)
{
    struct sr_unstrip_entry *loop = entries;
    while (loop)
    {
        if (loop->start <= address && loop->start + loop->length > address)
            return loop;

        loop = loop->next;
    }

    return NULL;
}

void
sr_unstrip_free(struct sr_unstrip_entry *entries)
{
    while (entries)
    {
        struct sr_unstrip_entry *entry = entries;
        entries = entry->next;
        g_free(entry->build_id);
        g_free(entry->file_name);
        g_free(entry->mod_name);
        g_free(entry);
    }
}

