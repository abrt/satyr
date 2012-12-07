/*
    operating_system.c

    Copyright (C) 2012  Red Hat, Inc.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301 USA.
*/
#include "operating_system.h"
#include "utils.h"
#include "strbuf.h"
#include <string.h>
#include <ctype.h>
#include <stddef.h>


struct btp_operating_system *
btp_operating_system_new()
{
    struct btp_operating_system *operating_system = btp_malloc(sizeof(struct btp_operating_system));
    btp_operating_system_init(operating_system);
    return operating_system;
}

void
btp_operating_system_init(struct btp_operating_system *operating_system)
{
    operating_system->name = NULL;
    operating_system->version = NULL;
    operating_system->architecture = NULL;
    operating_system->uptime = 0;
}

void
btp_operating_system_free(struct btp_operating_system *operating_system)
{
    if (!operating_system)
        return;

    free(operating_system->name);
    free(operating_system->version);
    free(operating_system->architecture);
    free(operating_system);
}

char *
btp_operating_system_to_json(struct btp_operating_system *operating_system)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();

    if (operating_system->name)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"name\": \"%s\"\n",
                               operating_system->name);
    }

    if (operating_system->version)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"version\": \"%s\"\n",
                               operating_system->version);
    }

    if (operating_system->architecture)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"architecture\": \"%s\"\n",
                               operating_system->architecture);
    }

    btp_strbuf_append_strf(strbuf,
                           ",   \"uptime\": %"PRIu64"\n",
                           operating_system->uptime);

    strbuf->buf[0] = '{';
    btp_strbuf_append_str(strbuf, "}");
    return btp_strbuf_free_nobuf(strbuf);
}

struct btp_operating_system *
btp_operating_system_from_abrt_dir(const char *directory,
                                   char **error_message)
{
    return NULL;
}

bool
btp_operating_system_parse_etc_system_release(const char *etc_system_release,
                                              char **name,
                                              char **version)
{
    const char *release = strstr(etc_system_release, " release ");
    if (!release)
        return false;

    *name = btp_strndup(etc_system_release, release - etc_system_release);

    if (0 == strlen(*name))
        return false;

    const char *version_begin = release + strlen(" release ");
    const char *version_end = version_begin;
    while (isdigit(*version_end) || *version_end == '.')
        ++version_end;

    // Fallback when parsing of version fails.
    ptrdiff_t version_len = version_end - version_begin;
    if (0 == version_len)
        version_end = version_begin + strlen(version_begin);

    *version = btp_strndup(version_begin, version_end - version_begin);
    return true;
}
