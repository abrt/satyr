/*
    operating_system.h

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
#ifndef SATYR_OPERATING_SYSTEM_H
#define SATYR_OPERATING_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

struct sr_json_value;

struct sr_operating_system
{
    char *name;
    char *version;
    char *architecture;
    char *cpe;
    /* Uptime in seconds. */
    uint64_t uptime;
    char *desktop;
    char *variant;
};

struct sr_operating_system *
sr_operating_system_new(void);

void
sr_operating_system_init(struct sr_operating_system *operating_system);

void
sr_operating_system_free(struct sr_operating_system *operating_system);

char *
sr_operating_system_to_json(struct sr_operating_system *operating_system);

struct sr_operating_system *
sr_operating_system_from_json(struct sr_json_value *root, char **error_message);

bool
sr_operating_system_parse_etc_system_release(const char *etc_system_release,
                                             char **name,
                                             char **version);
bool
sr_operating_system_parse_etc_os_release(const char *etc_os_release,
                                         struct sr_operating_system *operating_system);

#ifdef __cplusplus
}
#endif

#endif
