/*
    abrt.h

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
#ifndef BTPARSER_ABRT_H
#define BTPARSER_ABRT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool
btp_abrt_print_report_from_dir(const char *directory,
                               char **error_message);

bool
btp_abrt_create_core_stacktrace(const char *directory,
                                char **error_message);

struct btp_rpm_package *
btp_abrt_parse_dso_list(const char *text);

struct btp_rpm_package *
btp_abrt_rpm_packages_from_dir(const char *directory,
                               char **error_message);

struct btp_operating_system *
btp_abrt_operating_system_from_dir(const char *directory,
                                   char **error_message);

struct btp_report *
btp_abrt_report_from_dir(const char *directory,
                         char **error_message);

#ifdef __cplusplus
}
#endif

#endif
