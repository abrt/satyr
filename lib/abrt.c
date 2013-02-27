/*
    abrt.c

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
#include "abrt.h"
#include "report.h"
#include "utils.h"
#include "rpm.h"
#include "operating_system.h"
#include "core_unwind.h"
#include "core_stacktrace.h"
#include "core_fingerprint.h"
#include "python_stacktrace.h"
#include "koops_stacktrace.h"
#include "java_stacktrace.h"
#include "json.h"
#include "location.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
btp_abrt_print_report_from_dir(const char *directory,
                               char **error_message)
{
    struct btp_report *report = btp_abrt_report_from_dir(directory,
                                                         error_message);

    if (!report)
        return false;

    char *json = btp_report_to_json(report);
    btp_report_free(report);
    puts(json);
    free(json);
    return true;
}

bool
btp_abrt_create_core_stacktrace(const char *directory,
                                char **error_message)
{
    char *executable_filename = btp_build_path(directory, "executable", NULL);
    char *executable_contents = btp_file_to_string(executable_filename,
                                                   error_message);

    free(executable_filename);
    if (!executable_contents)
        return NULL;

    char *coredump_filename = btp_build_path(directory, "coredump", NULL);
    struct btp_core_stacktrace *core_stacktrace = btp_parse_coredump(
        coredump_filename, executable_contents, error_message);

    free(executable_contents);
    free(coredump_filename);
    if (!core_stacktrace)
        return false;

    bool success = btp_core_fingerprint_generate(core_stacktrace,
                                                 error_message);

    if (!success)
        return false;

    char *json = btp_core_stacktrace_to_json(core_stacktrace);

    // Add newline to the end of core stacktrace file to make text
    // editors happy.
    json = btp_realloc(json, strlen(json) + 2);
    strcat(json, "\n");

    char *core_backtrace_filename = btp_build_path(directory, "core_backtrace", NULL);
    success = btp_string_to_file(core_backtrace_filename,
                                 json,
                                 error_message);

    free(core_backtrace_filename);
    free(json);
    btp_core_stacktrace_free(core_stacktrace);
    return success;
}

struct btp_rpm_package *
btp_abrt_parse_dso_list(const char *text)
{
    struct btp_rpm_package *packages = NULL;
    const char *pos = text;
    while (pos && *pos)
    {
        // Skip dynamic library name.
        pos = strchr(pos, ' ');

        if (!pos)
            continue;

        // Skip the space.
        ++pos;

        // Find the package NEVRA.
        char *end = strchr(pos, ' ');
        if (!end || end - pos <= 1)
        {
            pos = strchr(pos, '\n');
            continue;
        }

        // Parse the package NEVRA.
        char *nevra = btp_strndup(pos, end - pos);
        struct btp_rpm_package *dso_package = btp_rpm_package_new();
        bool success = btp_rpm_package_parse_nevra(nevra,
                                                   &dso_package->name,
                                                   &dso_package->epoch,
                                                   &dso_package->version,
                                                   &dso_package->release,
                                                   &dso_package->architecture);

        free(nevra);

        // If parsing failed, move to the next line.
        if (!success)
        {
            btp_rpm_package_free(dso_package, true);
            pos = strchr(pos, '\n');
            continue;
        }

        // Find the package install time.
        char *eol = strchr(pos, '\n');
        if (!eol)
        {
            btp_rpm_package_free(dso_package, true);
            break;
        }

        pos = strrchr(pos, ' ');
        if (!pos)
        {
            pos = eol;
            btp_rpm_package_free(dso_package, true);
            continue;
        }

        ++pos;

        // Parse the package install time.
        int len = btp_parse_uint64((const char**)&pos,
                                   &dso_package->install_time);

        if (len <= 0)
        {
            pos = eol;
            btp_rpm_package_free(dso_package, true);
            continue;
        }

        // Append the package to the list.
        packages = btp_rpm_package_append(packages, dso_package);
        pos = eol;
    }

    return packages;
}

struct btp_rpm_package *
btp_abrt_rpm_packages_from_dir(const char *directory,
                               char **error_message)
{
    char *package_filename = btp_build_path(directory, "package", NULL);
    char *package_contents = btp_file_to_string(package_filename,
                                                error_message);

    free(package_filename);
    if (!package_contents)
        return NULL;

    struct btp_rpm_package *packages = btp_rpm_package_new();
    bool success = btp_rpm_package_parse_nvr(package_contents,
                                             &packages->name,
                                             &packages->version,
                                             &packages->release);

    if (!success)
    {
        btp_rpm_package_free(packages, true);
        *error_message = btp_asprintf("Failed to parse \"%s\".",
                                      package_contents);

        free(package_contents);
        return NULL;
    }

    free(package_contents);

    char *dso_list_filename = btp_build_path(directory, "dso_list", NULL);
    char *dso_list_contents = btp_file_to_string(dso_list_filename, error_message);
    free(dso_list_filename);
    if (!dso_list_contents)
    {
        btp_rpm_package_free(packages, true);
        return NULL;
    }

    struct btp_rpm_package *dso_packages =
        btp_abrt_parse_dso_list(dso_list_contents);

    if (dso_packages)
    {
        packages = btp_rpm_package_append(packages, dso_packages);
        packages = btp_rpm_package_sort(packages);
        packages = btp_rpm_package_uniq(packages);
    }

    free(dso_list_contents);
    return packages;
}

struct btp_operating_system *
btp_abrt_operating_system_from_dir(const char *directory,
                                   char **error_message)
{
    char *release_filename = btp_build_path(directory, "os_release", NULL);
    char *release_contents = btp_file_to_string(release_filename, error_message);
    free(release_filename);
    if (!release_contents)
        return NULL;

    struct btp_operating_system *os = btp_operating_system_new();
    bool success = btp_operating_system_parse_etc_system_release(release_contents,
                                                                 &os->name,
                                                                 &os->version);

    free(release_contents);
    if (!success)
    {
        btp_operating_system_free(os);
        *error_message = btp_strdup("Failed to parse operating system release string");
        return NULL;
    }

    char *arch_filename = btp_build_path(directory, "architecture", NULL);
    os->architecture = btp_file_to_string(arch_filename, error_message);
    free(arch_filename);
    if (!os->architecture)
    {
        btp_operating_system_free(os);
        return NULL;
    }

    return os;
}

struct btp_report *
btp_abrt_report_from_dir(const char *directory,
                         char **error_message)
{
    struct btp_report *report = btp_report_new();

    /* Report type. */
    char *analyzer_filename = btp_build_path(directory, "analyzer", NULL);
    char *analyzer_contents = btp_file_to_string(analyzer_filename, error_message);
    free(analyzer_filename);
    if (!analyzer_contents)
    {
        btp_report_free(report);
        return NULL;
    }

    if (0 == strncmp(analyzer_contents, "CCpp", 4))
        report->report_type = BTP_REPORT_CORE;
    else if (0 == strncmp(analyzer_contents, "Python", 6))
        report->report_type = BTP_REPORT_PYTHON;
    else if (0 == strncmp(analyzer_contents, "Kerneloops", 10))
        report->report_type = BTP_REPORT_KERNELOOPS;
    else if (0 == strncmp(analyzer_contents, "Java", 4))
        report->report_type = BTP_REPORT_JAVA;

    free(analyzer_contents);

    /* Operating system. */
    report->operating_system = btp_abrt_operating_system_from_dir(
        directory, error_message);

    if (!report->operating_system)
    {
        btp_report_free(report);
        return NULL;
    }

    /* Component name. */
    char *component_filename = btp_build_path(directory, "component", NULL);
    report->component_name = btp_file_to_string(component_filename, error_message);
    free(component_filename);

    /* RPM packages. */
    report->rpm_packages = btp_abrt_rpm_packages_from_dir(
        directory, error_message);

    if (!report->rpm_packages)
    {
        btp_report_free(report);
        return NULL;
    }

    /* Core stacktrace. */
    if (report->report_type == BTP_REPORT_CORE)
    {
        char *core_backtrace_filename = btp_build_path(directory, "core_backtrace", NULL);
        char *core_backtrace_contents = btp_file_to_string(core_backtrace_filename,
                                                           error_message);

        free(core_backtrace_filename);
        if (!core_backtrace_contents)
        {
            btp_report_free(report);
            return NULL;
        }

        struct btp_json_settings settings;
        memset(&settings, 0, sizeof(struct btp_json_settings));
        struct btp_location location;
        btp_location_init(&location);
        struct btp_json_value *json_root = btp_json_parse_ex(&settings,
                                                             core_backtrace_contents,
                                                             &location);

        free(core_backtrace_contents);
        if (!json_root)
        {
            *error_message = btp_location_to_string(&location);
            btp_report_free(report);
            return NULL;
        }

        report->core_stacktrace = btp_core_stacktrace_from_json(json_root,
                                                                error_message);

        btp_json_value_free(json_root);
        if (!report->core_stacktrace)
        {
            btp_report_free(report);
            return NULL;
        }
    }

    /* Python stacktrace. */
    if (report->report_type == BTP_REPORT_PYTHON)
    {
        char *backtrace_filename = btp_build_path(directory,
                                                  "backtrace",
                                                  NULL);

        char *backtrace_contents = btp_file_to_string(backtrace_filename,
                                                      error_message);

        free(backtrace_filename);
        if (!backtrace_contents)
        {
            btp_report_free(report);
            return NULL;
        }

        /* Parse the Python stacktrace. */
        struct btp_location location;
        btp_location_init(&location);
        const char *contents_pointer = backtrace_contents;
        report->python_stacktrace = btp_python_stacktrace_parse(
            &contents_pointer,
            &location);

        free(backtrace_contents);
        if (!report->python_stacktrace)
        {
            *error_message = btp_location_to_string(&location);
            btp_report_free(report);
            return NULL;
        }
    }

    /* Kerneloops stacktrace. */
    if (report->report_type == BTP_REPORT_KERNELOOPS)
    {
        char *backtrace_filename = btp_build_path(directory,
                                                  "backtrace",
                                                  NULL);

        char *backtrace_contents = btp_file_to_string(backtrace_filename,
                                                      error_message);

        free(backtrace_filename);
        if (!backtrace_contents)
        {
            btp_report_free(report);
            return NULL;
        }

        /* Parse the Kerneloops stacktrace. */
        struct btp_location location;
        btp_location_init(&location);
        const char *contents_pointer = backtrace_contents;
        report->koops_stacktrace = btp_koops_stacktrace_parse(
            &contents_pointer,
            &location);

        free(backtrace_contents);
        if (!report->koops_stacktrace)
        {
            *error_message = btp_location_to_string(&location);
            btp_report_free(report);
            return NULL;
        }
    }

    /* Java stacktrace. */
    if (report->report_type == BTP_REPORT_JAVA)
    {
        char *backtrace_filename = btp_build_path(directory,
                                                  "backtrace",
                                                  NULL);

        char *backtrace_contents = btp_file_to_string(backtrace_filename,
                                                      error_message);

        free(backtrace_filename);
        if (!backtrace_contents)
        {
            btp_report_free(report);
            return NULL;
        }

        /* Parse the Java stacktrace. */
        struct btp_location location;
        btp_location_init(&location);
        const char *contents_pointer = backtrace_contents;
        report->java_stacktrace = btp_java_stacktrace_parse(
            &contents_pointer,
            &location);

        free(backtrace_contents);
        if (!report->java_stacktrace)
        {
            *error_message = btp_location_to_string(&location);
            btp_report_free(report);
            return NULL;
        }
    }

    return report;
}
