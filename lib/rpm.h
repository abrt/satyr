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

#include <stdbool.h>
#include <inttypes.h>

struct btp_rpm_consistency
{
    char *path;
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
btp_rpm_package_new();

void
btp_rpm_package_init(struct btp_rpm_package *package);

void
btp_rpm_package_free(struct btp_rpm_package *package,
                     bool recursive);

/**
 * Compares two packages.
 * @param package1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param package2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * Returns 0 if the packages are same.  Returns negative number if
 * package1 is found to be 'less' than package2.  Returns positive
 * number if package1 is found to be 'greater' than package2.
 */
int
btp_rpm_package_cmp(struct btp_rpm_package *package1,
                    struct btp_rpm_package *package2);

/**
 * Compares two packages by their name, version and release.
 * @param package1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param package2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * Returns 0 if the packages are same.  Returns negative number if
 * package1 is found to be 'less' than package2.  Returns positive
 * number if package1 is found to be 'greater' than package2.
 */
int
btp_rpm_package_cmp_nvr(struct btp_rpm_package *package1,
                        struct btp_rpm_package *package2);

/**
 * Compares two packages by their name, epoch, version, release and
 * architecture.
 * @param package1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param package2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * Returns 0 if the packages are same.  Returns negative number if
 * package1 is found to be 'less' than package2.  Returns positive
 * number if package1 is found to be 'greater' than package2.
 */
int
btp_rpm_package_cmp_nevra(struct btp_rpm_package *package1,
                          struct btp_rpm_package *package2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' package.  If 'dest' is NULL, it
 * returns the 'item' package.
 */
struct btp_rpm_package *
btp_rpm_package_append(struct btp_rpm_package *dest,
                       struct btp_rpm_package *item);

/**
 * Returns the number of packages in the list.
 */
int
btp_rpm_package_count(struct btp_rpm_package *packages);

struct btp_rpm_package *
btp_rpm_package_sort(struct btp_rpm_package *packages);

struct btp_rpm_package *
btp_rpm_package_uniq(struct btp_rpm_package *packages);

struct btp_rpm_package *
btp_rpm_package_get_by_name(const char *name,
                            char **error_message);

struct btp_rpm_package *
btp_rpm_package_get_by_path(const char *path,
                            char **error_message);

char *
btp_rpm_package_to_json(struct btp_rpm_package *package,
                        bool recursive);

bool
btp_rpm_package_parse_nvr(const char *text,
                          char **name,
                          char **version,
                          char **release);

bool
btp_rpm_package_parse_nevra(const char *text,
                            char **name,
                            uint32_t *epoch,
                            char **version,
                            char **release,
                            char **architecture);

struct btp_rpm_consistency *
btp_rpm_consistency_new();

void
btp_rpm_consistency_init(struct btp_rpm_consistency *consistency);

void
btp_rpm_consistency_free(struct btp_rpm_consistency *consistency,
                         bool recursive);

int
btp_rpm_consistency_cmp(struct btp_rpm_consistency *consistency1,
                        struct btp_rpm_consistency *consistency2);

int
btp_rpm_consistency_cmp_recursive(struct btp_rpm_consistency *consistency1,
                                  struct btp_rpm_consistency *consistency2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' consistency info.  If 'dest' is
 * NULL, it returns the 'item' consistency info.
 */
struct btp_rpm_consistency *
btp_rpm_consistency_append(struct btp_rpm_consistency *dest,
                           struct btp_rpm_consistency *item);

#ifdef __cplusplus
}
#endif

#endif
