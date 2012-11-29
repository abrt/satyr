/*
    report.h

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
#ifndef BTPARSER_REPORT_H
#define BTPARSER_REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

enum btp_user_type
{
    BTP_USER_INVALID = 0,
    BTP_USER_ROOT,
    BTP_USER_NOLOGIN,
    BTP_USER_LOCAL,
    BTP_USER_REMOTE
};

struct btp_operating_system
{
    char *name;
    char *version;
    char *architecture;
    uint64_t uptime;
};

enum btp_report_type
{
    BTP_REPORT_INVALID = 0,
    BTP_REPORT_CORE,
    BTP_REPORT_PYTHON,
    BTP_REPORT_KERNELOOPS,
    BTP_REPORT_JAVA
};

struct btp_report
{
    uint32_t report_version;
    enum btp_report_type report_type;

    char *reporter_name;
    char *reporter_version;

    enum btp_user_type user_type;
    struct btp_operating_system operating_system;

    char *component_name;
    struct btp_rpm_package *rpm_packages;
    struct btp_deb_package *deb_packages;

    struct btp_python_stacktrace *python_stacktrace;
    struct btp_koops_stacktrace *koops_stacktrace;
    struct btp_core_stacktrace *core_stacktrace;
};

struct btp_report *
btp_report_new();

void
btp_report_init(struct btp_report *report);

void
btp_report_free(struct btp_report *report);

char *
btp_report_to_json(struct btp_report *report);

struct btp_report *
btp_report_from_abrt_dir(const char *directory,
                         char **error_message);

#ifdef __cplusplus
}
#endif

#endif
