/*
    gdb_sharedlib.c

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
#include "gdb/sharedlib.h"
#include "utils.h"
#include "strbuf.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct sr_gdb_sharedlib *
sr_gdb_sharedlib_new()
{
    struct sr_gdb_sharedlib *result = sr_malloc(sizeof(struct sr_gdb_sharedlib));
    sr_gdb_sharedlib_init(result);
    return result;
}

void
sr_gdb_sharedlib_init(struct sr_gdb_sharedlib *sharedlib)
{
    sharedlib->from = -1;
    sharedlib->to = -1;
    sharedlib->symbols = SYMS_NOT_FOUND;
    sharedlib->soname = NULL;
    sharedlib->next = NULL;
}

void
sr_gdb_sharedlib_free(struct sr_gdb_sharedlib *sharedlib)
{
    if (!sharedlib)
        return;

    free(sharedlib->soname);
    free(sharedlib);
}

struct sr_gdb_sharedlib *
sr_gdb_sharedlib_dup(struct sr_gdb_sharedlib *sharedlib,
                     bool siblings)
{
    struct sr_gdb_sharedlib *result = sr_gdb_sharedlib_new();
    memcpy(result, sharedlib, sizeof(struct sr_gdb_sharedlib));
    result->soname = sr_strdup(sharedlib->soname);

    if (siblings)
    {
        if (result->next)
            result->next = sr_gdb_sharedlib_dup(result->next, true);
    }
    else
        result->next = NULL;

    return result;
}

struct sr_gdb_sharedlib *
sr_gdb_sharedlib_append(struct sr_gdb_sharedlib *dest,
                        struct sr_gdb_sharedlib *item)
{
    if (!dest)
        return item;

    struct sr_gdb_sharedlib *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

int
sr_gdb_sharedlib_count(struct sr_gdb_sharedlib *sharedlib)
{
    struct sr_gdb_sharedlib *loop = sharedlib;
    int count = 0;
    while (loop)
    {
        loop = loop->next;
        ++count;
    }

    return count;
}

struct sr_gdb_sharedlib *
sr_gdb_sharedlib_find_address(struct sr_gdb_sharedlib *first,
                              uint64_t address)
{
    struct sr_gdb_sharedlib *tmp = first;

    if (address == UINT64_MAX)
        return NULL;

    while (tmp)
    {
        if (address >= tmp->from && address <= tmp->to)
            return tmp;

        tmp = tmp->next;
    }

    return NULL;
}

static char *
find_sharedlib_section_start(const char *input)
{
    /* searching for
       From      To      Syms Read      Shared Object Library
    */
    char *result = strstr(input, "From");
    for (; result; result = strstr(result + 1, "From"))
    {
        /* must be at the beginning of the line
           or at the beginning of whole input */
        if (result != input && *(result - 1) != '\n')
            continue;

        char *tmp = result + strlen("From");
        while (isspace(*tmp))
            ++tmp;

        if (strncmp("To", tmp, strlen("To")) != 0)
            continue;

        tmp += strlen("To");
        while (isspace(*tmp))
            ++tmp;

        if (strncmp("Syms Read", tmp, strlen("Syms Read")) != 0)
            continue;

        tmp += strlen("Syms Read");
        while (isspace(*tmp))
            ++tmp;

        if (strncmp("Shared Object Library\n", tmp, strlen("Shared Object Library\n")) != 0)
            continue;

        /* jump to the next line - the first loaded library */
        return tmp + strlen("Shared Object Library\n");
    }

    return NULL;
}

struct sr_gdb_sharedlib *
sr_gdb_sharedlib_parse(const char *input)
{
    char *tmp = find_sharedlib_section_start(input);
    if (!tmp)
        return NULL;

    /* Parsing
       From                 To                  Syms Read        Shared Object Library
       0x0123456789abcdef   0xfedcba987654321   Yes (*)|Yes|No   /usr/lib64/libsatyr.so.2.2.2
    */
    struct sr_gdb_sharedlib *first = NULL, *current = NULL;
    while (1)
    {
        unsigned long long from = -1, to = -1;

        /* ugly - from/to address is sometimes missing; skip it and jump to symbols */
        if (isspace(*tmp))
        {
            while (isspace(*tmp))
                ++tmp;
        }
        else
        {
            /* From To */
            if (sscanf(tmp, "%Lx %Lx", &from, &to) != 2)
                break;

            while (isxdigit(*tmp) || isspace(*tmp) || *tmp == 'x')
                ++tmp;
        }

        /* Syms Read */
        int symbols;
        if (strncmp("Yes (*)", tmp, strlen("Yes (*)")) == 0)
        {
            tmp += strlen("Yes (*)");
            symbols = SYMS_NOT_FOUND;
        }
        else if (strncmp("Yes", tmp, strlen("Yes")) == 0)
        {
            tmp += strlen("Yes");
            symbols = SYMS_OK;
        }
        else if (strncmp("No", tmp, strlen("No")) == 0)
        {
            tmp += strlen("No");
            symbols = SYMS_WRONG;
        }
        else
            break;

        while (isspace(*tmp))
            ++tmp;

        /* Shared Object Library */
        struct sr_strbuf *buf = sr_strbuf_new();
        while (*tmp && *tmp != '\n')
        {
            sr_strbuf_append_char(buf, *tmp);
            ++tmp;
        }

        if (current)
        {
            /* round 2+ */
            current->next = sr_gdb_sharedlib_new();
            current = current->next;
        }
        else
        {
            /* round 1 */
            current = sr_gdb_sharedlib_new();
            first = current;
        }

        current->from = from;
        current->to = to;
        current->symbols = symbols;
        current->soname = sr_strbuf_free_nobuf(buf);

        /* we are on '\n' character, jump to next line */
        ++tmp;
    }

    return first;
}
