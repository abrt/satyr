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
#ifndef BTPARSER_OPERATING_SYSTEM_H
#define BTPARSER_OPERATING_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

struct btp_operating_system
{
    char *name;
    char *version;
    char *architecture;
    /* Uptime in seconds. */
    uint64_t uptime;
};

struct btp_operating_system *
btp_operating_system_new();

void
btp_operating_system_init(struct btp_operating_system *operating_system);

void
btp_operating_system_free(struct btp_operating_system *operating_system);

char *
btp_operating_system_to_json(struct btp_operating_system *operating_system);

struct btp_operating_system *
btp_operating_system_from_abrt_dir(const char *directory,
                                   char **error_message);



#ifdef __cplusplus
}
#endif

#endif
