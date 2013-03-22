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
#include "json.h"
#include "strbuf.h"
#include <string.h>
#include <ctype.h>
#include <stddef.h>


struct sr_operating_system *
sr_operating_system_new()
{
    struct sr_operating_system *operating_system = sr_malloc(sizeof(struct sr_operating_system));
    sr_operating_system_init(operating_system);
    return operating_system;
}

void
sr_operating_system_init(struct sr_operating_system *operating_system)
{
    operating_system->name = NULL;
    operating_system->version = NULL;
    operating_system->architecture = NULL;
    operating_system->uptime = 0;
}

void
sr_operating_system_free(struct sr_operating_system *operating_system)
{
    if (!operating_system)
        return;

    free(operating_system->name);
    free(operating_system->version);
    free(operating_system->architecture);
    free(operating_system);
}

char *
sr_operating_system_to_json(struct sr_operating_system *operating_system)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    if (operating_system->name)
    {
        sr_strbuf_append_str(strbuf, ",   \"name\": ");
        sr_json_append_escaped(strbuf, operating_system->name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (operating_system->version)
    {
        sr_strbuf_append_str(strbuf, ",   \"version\": ");
        sr_json_append_escaped(strbuf, operating_system->version);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (operating_system->architecture)
    {
        sr_strbuf_append_str(strbuf, ",   \"architecture\": ");
        sr_json_append_escaped(strbuf, operating_system->architecture);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (operating_system->uptime > 0)
    {
        sr_strbuf_append_strf(strbuf,
                              ",   \"uptime\": %"PRIu64"\n",
                              operating_system->uptime);
    }

    if (strbuf->len > 0)
        strbuf->buf[0] = '{';
    else
        sr_strbuf_append_char(strbuf, '{');

    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}

bool
sr_operating_system_parse_etc_system_release(const char *etc_system_release,
                                             char **name,
                                             char **version)
{
    const char *release = strstr(etc_system_release, " release ");
    if (!release)
        return false;

    *name = sr_strndup(etc_system_release, release - etc_system_release);

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

    *version = sr_strndup(version_begin, version_end - version_begin);
    return true;
}
