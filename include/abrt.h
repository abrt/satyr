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
#ifndef SATYR_ABRT_H
#define SATYR_ABRT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "report_type.h"
#include <stdbool.h>
#include <sys/types.h>

bool
sr_abrt_print_report_from_dir(const char *directory,
                              char **error_message);

/* NOTE: the hash_fingerprints argument has no effect because fingerprint
 * generation is disabled. */
bool
sr_abrt_create_core_stacktrace(const char *directory,
                               bool hash_fingerprints,
                               char **error_message);
bool
sr_abrt_create_core_stacktrace_from_gdb(const char *directory,
                                        const char *gdb_output,
                                        bool hash_fingerprints,
                                        char **error_message);

char *
sr_abrt_get_core_stacktrace_from_core_hook(pid_t thread_id,
                                           const char *executable,
                                           int signum,
                                           char **error_message);

bool
sr_abrt_create_core_stacktrace_from_core_hook(const char *directory,
                                              pid_t thread_id,
                                              const char *executable,
                                              int signum,
                                              char **error_message);

struct sr_rpm_package *
sr_abrt_parse_dso_list(const char *text);

struct sr_rpm_package *
sr_abrt_rpm_packages_from_dir(const char *directory,
                              char **error_message);

struct sr_operating_system *
sr_abrt_operating_system_from_dir(const char *directory,
                                  char **error_message);

struct sr_report *
sr_abrt_report_from_dir(const char *directory,
                        char **error_message);

/* Deprecated: use sr_report_type_from_type() instead */
enum sr_report_type
sr_abrt_type_from_analyzer(const char *analyzer);

enum sr_report_type
sr_abrt_type_from_type(const char *type);

#ifdef __cplusplus
}
#endif

#endif
