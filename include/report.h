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
#ifndef SATYR_REPORT_H
#define SATYR_REPORT_H

/**
 * @file
 * @brief Report
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "report_type.h"
#include <inttypes.h>
#include <stdbool.h>

struct sr_json_value;
struct sr_stacktrace;

struct sr_report_custom_entry
{
    char *key;
    char *value;
    struct sr_report_custom_entry *next;
};

struct sr_report
{
    uint32_t report_version;
    enum sr_report_type report_type;

    char *reporter_name;
    char *reporter_version;

    // This is the real user id, not effective.
    bool user_root;
    bool user_local;

    struct sr_operating_system *operating_system;

    char *component_name;
    struct sr_rpm_package *rpm_packages;

    struct sr_stacktrace *stacktrace;

    struct sr_report_custom_entry *auth_entries;

    uint32_t serial;
};

struct sr_report *
sr_report_new(void);

void
sr_report_init(struct sr_report *report);

void
sr_report_free(struct sr_report *report);


/* @brief Adds a new entry to 'auth' object
 *
 * The implementation is LIFO. The resulting list is in reversed.
 */
void
sr_report_add_auth(struct sr_report *report, const char *key, const char *value);

char *
sr_report_to_json(struct sr_report *report);

struct sr_report *
sr_report_from_json(struct sr_json_value *root, char **error_message);

struct sr_report *
sr_report_from_json_text(const char *text, char **error_message);

#ifdef __cplusplus
}
#endif

#endif
