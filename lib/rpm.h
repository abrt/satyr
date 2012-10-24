/*
    rpm.h

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
#ifndef BTPARSER_RPM_H
#define BTPARSER_RPM_H

/**
 * @file
 * @brief RPM-related structures and utilities.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct btp_rpm_consistency
{
    char *file_name;

    bool owner_changed;
    bool group_changed;
    bool mode_changed;
    bool md5_mismatch;
    bool size_changed;
    bool major_number_changed;
    bool minor_number_changed;
    bool symlink_changed;
    bool modification_time_changed;

    struct btp_rpm_consistency *next;
};

struct btp_rpm_package
{
    char *name;
    uint32_t epoch;
    char *version;
    char *release;
    char *architecture;
    uint64_t install_time;
    struct btp_rpm_consistency *consistency;

    struct btp_rpm_package *next;
};

struct btp_rpm_package *
btp_rpm_package_get_by_name(const char *name);

struct btp_rpm_package *
btp_rpm_package_get_by_path(const char *path);

void
btp_rpm_package_free(struct btp_rpm_package *package, bool recursive);

#ifdef __cplusplus
}
#endif

#endif
