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
#include "internal_utils.h"
#include <string.h>
#include <ctype.h>
#include <stddef.h>


struct sr_operating_system *
sr_operating_system_new()
{
    struct sr_operating_system *operating_system = g_malloc(sizeof(*operating_system));
    sr_operating_system_init(operating_system);
    return operating_system;
}

void
sr_operating_system_init(struct sr_operating_system *operating_system)
{
    operating_system->name = NULL;
    operating_system->version = NULL;
    operating_system->architecture = NULL;
    operating_system->cpe = NULL;
    operating_system->desktop = NULL;
    operating_system->variant = NULL;
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
    free(operating_system->cpe);
    free(operating_system->desktop);
    free(operating_system->variant);
    free(operating_system);
}

char *
sr_operating_system_to_json(struct sr_operating_system *operating_system)
{
    GString *strbuf = g_string_new(NULL);

    if (operating_system->name)
    {
        g_string_append(strbuf, ",   \"name\": ");
        sr_json_append_escaped(strbuf, operating_system->name);
        g_string_append(strbuf, "\n");
    }

    if (operating_system->version)
    {
        g_string_append(strbuf, ",   \"version\": ");
        sr_json_append_escaped(strbuf, operating_system->version);
        g_string_append(strbuf, "\n");
    }

    if (operating_system->architecture)
    {
        g_string_append(strbuf, ",   \"architecture\": ");
        sr_json_append_escaped(strbuf, operating_system->architecture);
        g_string_append(strbuf, "\n");
    }

    if (operating_system->cpe)
    {
        g_string_append(strbuf, ",   \"cpe\": ");
        sr_json_append_escaped(strbuf, operating_system->cpe);
        g_string_append(strbuf, "\n");
    }

    if (operating_system->desktop)
    {
        g_string_append(strbuf, ",   \"desktop\": ");
        sr_json_append_escaped(strbuf, operating_system->desktop);
        g_string_append(strbuf, "\n");
    }

    if (operating_system->variant)
    {
        g_string_append(strbuf, ",   \"variant\": ");
        sr_json_append_escaped(strbuf, operating_system->variant);
        g_string_append(strbuf, "\n");
    }

    if (operating_system->uptime > 0)
    {
        g_string_append_printf(strbuf,
                              ",   \"uptime\": %"PRIu64"\n",
                              operating_system->uptime);
    }

    if (strbuf->len > 0)
        strbuf->str[0] = '{';
    else
        g_string_append_c(strbuf, '{');

    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
}

struct sr_operating_system *
sr_operating_system_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "operating system", error_message))
        return NULL;

    struct sr_operating_system *result = sr_operating_system_new();

    bool success =
        JSON_READ_STRING(root, "name", &result->name) &&
        JSON_READ_STRING(root, "version", &result->version) &&
        JSON_READ_STRING(root, "architecture", &result->architecture) &&
        JSON_READ_UINT64(root, "uptime", &result->uptime);

    /* desktop is optional - failure is not fatal */
    JSON_READ_STRING(root, "desktop", &result->desktop);
    /* variant is optional - failure is not fatal */
    JSON_READ_STRING(root, "variant", &result->variant);

    if (!success)
    {
        sr_operating_system_free(result);
        return NULL;
    }

    return result;
}

bool
sr_operating_system_parse_etc_system_release(const char *etc_system_release,
                                             char **name,
                                             char **version)
{
    const char *release = strstr(etc_system_release, " release ");
    if (!release)
        return false;

    /* Normal form of Red Hat Enterprise Linux's name is "rhel" */
    if (strncasecmp("Red Hat Enterprise Linux", etc_system_release, strlen("Red Hat Enterprise Linux")) == 0)
        *name = g_strndup("rhel", strlen("rhel"));
    else
    {
        *name = g_strndup(etc_system_release, release - etc_system_release);

        if (0 == strlen(*name))
            return false;

        /* make the name all lower case */
        char *a = *name;
        while (*a)
        {
            *a = tolower(*a);
            a++;
        }
    }

    const char *version_begin = release + strlen(" release ");
    const char *version_end = version_begin;
    while (isdigit(*version_end) || *version_end == '.')
        ++version_end;

    // Fallback when parsing of version fails.
    ptrdiff_t version_len = version_end - version_begin;
    if (0 == version_len)
        version_end = version_begin + strlen(version_begin);

    *version = g_strndup(version_begin, version_end - version_begin);
    return true;
}

static void
os_release_callback(char *key, char *value, void *data)
{
    struct sr_operating_system *operating_system =
        (struct sr_operating_system *)data;

    if (0 == strcmp(key, "ID"))
    {
        operating_system->name = value;
    }
    else if (0 == strcmp(key, "VERSION_ID"))
    {
        if (operating_system->version == NULL)
            operating_system->version = value;
    }
    /* fedora rawhide workaround */
    else if (0 == strcmp(key, "VERSION") && strstr(value, "(Rawhide)"))
    {
        if (operating_system->version)
            free(operating_system->version);
        operating_system->version = sr_strdup("rawhide");
        free(value);
    }
    else if (0 == strcmp(key, "CPE_NAME"))
    {
        operating_system->cpe = value;
    }
    else if (0 == strcmp(key, "VARIANT_ID"))
    {
        operating_system->variant = value;
    }
    else
    {
        free(value);
    }
    free(key);
}

bool
sr_operating_system_parse_etc_os_release(const char *etc_os_release,
                                         struct sr_operating_system *operating_system)
{
    sr_parse_os_release(etc_os_release, os_release_callback, (void*)operating_system);

    return (operating_system->name && operating_system->version);
}
