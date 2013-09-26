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
#include "core/unwind.h"
#include "core/stacktrace.h"
#include "core/fingerprint.h"
#include "python/stacktrace.h"
#include "koops/stacktrace.h"
#include "java/stacktrace.h"
#include "json.h"
#include "location.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char*
file_contents(const char *directory, const char *file, char **error_message)
{
    char *path = sr_build_path(directory, file, NULL);
    char *contents = sr_file_to_string(path, error_message);

    free(path);
    return contents;
}

bool
sr_abrt_print_report_from_dir(const char *directory,
                               char **error_message)
{
    struct sr_report *report = sr_abrt_report_from_dir(directory,
                                                       error_message);

    if (!report)
        return false;

    char *json = sr_report_to_json(report);
    sr_report_free(report);
    puts(json);
    free(json);
    return true;
}

static bool
create_core_stacktrace(const char *directory, const char *gdb_output,
                       bool hash_fingerprints, char **error_message)
{
    char *executable_contents = file_contents(directory, "executable",
                                              error_message);
    if (!executable_contents)
        return NULL;

    char *coredump_filename = sr_build_path(directory, "coredump", NULL);

    struct sr_core_stacktrace *core_stacktrace;

    if (gdb_output)
        core_stacktrace = sr_core_stacktrace_from_gdb(gdb_output,
                coredump_filename, executable_contents, error_message);
    else
        core_stacktrace = sr_parse_coredump(coredump_filename,
                executable_contents, error_message);

    free(executable_contents);
    free(coredump_filename);
    if (!core_stacktrace)
        return false;

    sr_core_fingerprint_generate(core_stacktrace,
                                 error_message);

    if (hash_fingerprints)
        sr_core_fingerprint_hash(core_stacktrace);

    char *json = sr_core_stacktrace_to_json(core_stacktrace);

    // Add newline to the end of core stacktrace file to make text
    // editors happy.
    json = sr_realloc(json, strlen(json) + 2);
    strcat(json, "\n");

    char *core_backtrace_filename = sr_build_path(directory, "core_backtrace", NULL);
    bool success = sr_string_to_file(core_backtrace_filename,
                                    json,
                                    error_message);

    free(core_backtrace_filename);
    free(json);
    sr_core_stacktrace_free(core_stacktrace);
    return success;
}

bool
sr_abrt_create_core_stacktrace_from_gdb(const char *directory,
                                        const char *gdb_output,
                                        bool hash_fingerprints,
                                        char **error_message)
{
    if (!gdb_output)
    {
        *error_message = strdup("GDB output cannot be NULL");
        return false;
    }

    return create_core_stacktrace(directory, gdb_output, hash_fingerprints,
                                  error_message);
}

bool
sr_abrt_create_core_stacktrace(const char *directory,
                               bool hash_fingerprints,
                               char **error_message)
{
    return create_core_stacktrace(directory, NULL, hash_fingerprints,
                                  error_message);
}

struct sr_rpm_package *
sr_abrt_parse_dso_list(const char *text)
{
    struct sr_rpm_package *packages = NULL;
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
        char *nevra = sr_strndup(pos, end - pos);
        struct sr_rpm_package *dso_package = sr_rpm_package_new();
        bool success = sr_rpm_package_parse_nevra(nevra,
                                                  &dso_package->name,
                                                  &dso_package->epoch,
                                                  &dso_package->version,
                                                  &dso_package->release,
                                                  &dso_package->architecture);

        free(nevra);

        // If parsing failed, move to the next line.
        if (!success)
        {
            sr_rpm_package_free(dso_package, true);
            pos = strchr(pos, '\n');
            continue;
        }

        // Find the package install time.
        char *eol = strchr(pos, '\n');
        if (!eol)
        {
            sr_rpm_package_free(dso_package, true);
            break;
        }

        char *line = sr_strndup(pos, eol - pos);
        pos = strrchr(line, ' ');
        if (!pos)
        {
            pos = eol;
            sr_rpm_package_free(dso_package, true);
            free(line);
            continue;
        }

        ++pos;

        // Parse the package install time.
        int len = sr_parse_uint64((const char**)&pos,
                                  &dso_package->install_time);
        free(line);

        if (len <= 0)
        {
            pos = eol;
            sr_rpm_package_free(dso_package, true);
            continue;
        }

        // Append the package to the list.
        packages = sr_rpm_package_append(packages, dso_package);
        pos = eol;
    }

    return packages;
}

static void
strip_newline(char *str)
{
    char *n = strchr(str, '\n');
    if (n)
        *n = '\0';
}

struct sr_rpm_package *
sr_abrt_rpm_packages_from_dir(const char *directory,
                              char **error_message)
{

    char *epoch_str = file_contents(directory, "pkg_epoch", error_message);
    if (!epoch_str)
    {
        return NULL;
    }
    unsigned long epoch = strtoul(epoch_str, NULL, 10);
    if (epoch == ULONG_MAX)
    {
        *error_message = sr_asprintf("Epoch '%s' is not a number", epoch_str);
        return NULL;
    }
    free(epoch_str);

    struct sr_rpm_package *packages = sr_rpm_package_new();

    packages->epoch = (uint32_t)epoch;
    packages->name = file_contents(directory, "pkg_name", error_message);
    packages->version = file_contents(directory, "pkg_version", error_message);
    packages->release = file_contents(directory, "pkg_release", error_message);
    packages->architecture = file_contents(directory, "pkg_arch", error_message);
    packages->role = SR_ROLE_AFFECTED;

    if (!(packages->name && packages->version && packages->release &&
          packages->architecture))
    {
        sr_rpm_package_free(packages, false);
        return NULL;
    }

    /* Workaround abrt-action-save-kernel-data appending trailing \n for
     * koopses. */
    strip_newline(packages->name);
    strip_newline(packages->version);
    strip_newline(packages->release);
    strip_newline(packages->architecture);

    char *dso_list_contents = file_contents(directory, "dso_list",
                                            error_message);
    if (dso_list_contents)
    {
        struct sr_rpm_package *dso_packages =
            sr_abrt_parse_dso_list(dso_list_contents);

        if (dso_packages)
        {
            packages = sr_rpm_package_append(packages, dso_packages);
            packages = sr_rpm_package_sort(packages);
            packages = sr_rpm_package_uniq(packages);
        }

        free(dso_list_contents);
    }

    return packages;
}

struct sr_operating_system *
sr_abrt_operating_system_from_dir(const char *directory,
                                  char **error_message)
{
    bool success = false;
    struct sr_operating_system *os = sr_operating_system_new();

    char *osinfo_contents = file_contents(directory, "os_info", error_message);
    if (osinfo_contents)
    {
        success = sr_operating_system_parse_etc_os_release(osinfo_contents, os);
        free(osinfo_contents);
    }

    /* fall back to os_release if parsing os_info fails */
    if (!success)
    {
        char *release_contents = file_contents(directory, "os_release",
                                               error_message);
        if (release_contents)
        {
            success = sr_operating_system_parse_etc_system_release(release_contents,
                                                                   &os->name,
                                                                   &os->version);
            free(release_contents);
        }
    }

    if (!success)
    {
        sr_operating_system_free(os);
        *error_message = sr_strdup("Failed to parse operating system release string");
        return NULL;
    }

    os->architecture = file_contents(directory, "architecture", error_message);
    if (!os->architecture)
    {
        sr_operating_system_free(os);
        return NULL;
    }

    return os;
}

struct sr_report *
sr_abrt_report_from_dir(const char *directory,
                        char **error_message)
{
    struct sr_report *report = sr_report_new();

    /* Report type. */
    char *analyzer_contents = file_contents(directory, "analyzer", error_message);
    if (!analyzer_contents)
    {
        sr_report_free(report);
        return NULL;
    }

    report->report_type = sr_abrt_type_from_analyzer(analyzer_contents);
    free(analyzer_contents);

    /* Operating system. */
    report->operating_system = sr_abrt_operating_system_from_dir(
        directory, error_message);

    if (!report->operating_system)
    {
        sr_report_free(report);
        return NULL;
    }

    /* Component name. */
    report->component_name = file_contents(directory, "component", error_message);

    /* RPM packages. */
    report->rpm_packages = sr_abrt_rpm_packages_from_dir(
        directory, error_message);

    if (!report->rpm_packages)
    {
        sr_report_free(report);
        return NULL;
    }

    /* Core stacktrace. */
    if (report->report_type == SR_REPORT_CORE)
    {
        char *core_backtrace_contents = file_contents(directory, "core_backtrace",
                                                      error_message);
        if (!core_backtrace_contents)
        {
            sr_report_free(report);
            return NULL;
        }

        report->stacktrace = (struct sr_stacktrace *)sr_core_stacktrace_from_json_text(
                core_backtrace_contents, error_message);

        free(core_backtrace_contents);
        if (!report->stacktrace)
        {
            sr_report_free(report);
            return NULL;
        }
    }

    /* Python stacktrace. */
    if (report->report_type == SR_REPORT_PYTHON)
    {
        char *backtrace_contents = file_contents(directory, "backtrace",
                                                 error_message);
        if (!backtrace_contents)
        {
            sr_report_free(report);
            return NULL;
        }

        /* Parse the Python stacktrace. */
        struct sr_location location;
        sr_location_init(&location);
        const char *contents_pointer = backtrace_contents;
        report->stacktrace = (struct sr_stacktrace *)sr_python_stacktrace_parse(
            &contents_pointer,
            &location);

        free(backtrace_contents);
        if (!report->stacktrace)
        {
            *error_message = sr_location_to_string(&location);
            sr_report_free(report);
            return NULL;
        }
    }

    /* Kerneloops stacktrace. */
    if (report->report_type == SR_REPORT_KERNELOOPS)
    {
        /* Determine kernel version */
        char *kernel_contents = file_contents(directory, "kernel",
                                              error_message);
        if (!kernel_contents)
        {
            sr_report_free(report);
            return NULL;
        }

        /* Load the Kerneloops stacktrace */
        char *backtrace_contents = file_contents(directory, "backtrace", error_message);
        if (!backtrace_contents)
        {
            sr_report_free(report);
            return NULL;
        }

        /* Parse the Kerneloops stacktrace. */
        struct sr_location location;
        sr_location_init(&location);
        const char *contents_pointer = backtrace_contents;
        struct sr_koops_stacktrace *stacktrace = sr_koops_stacktrace_parse(
            &contents_pointer,
            &location);

        stacktrace->version = kernel_contents;
        report->stacktrace = (struct sr_stacktrace *)stacktrace;

        free(backtrace_contents);
        if (!report->stacktrace)
        {
            *error_message = sr_location_to_string(&location);
            sr_report_free(report);
            return NULL;
        }
    }

    /* Java stacktrace. */
    if (report->report_type == SR_REPORT_JAVA)
    {
        char *backtrace_contents = file_contents(directory, "backtrace", error_message);
        if (!backtrace_contents)
        {
            sr_report_free(report);
            return NULL;
        }

        /* Parse the Java stacktrace. */
        struct sr_location location;
        sr_location_init(&location);
        const char *contents_pointer = backtrace_contents;
        report->stacktrace = (struct sr_stacktrace *)sr_java_stacktrace_parse(
            &contents_pointer,
            &location);

        free(backtrace_contents);
        if (!report->stacktrace)
        {
            *error_message = sr_location_to_string(&location);
            sr_report_free(report);
            return NULL;
        }
    }

    return report;
}

enum sr_report_type
sr_abrt_type_from_analyzer(const char *analyzer)
{
    if (0 == strncmp(analyzer, "CCpp", 4))
        return SR_REPORT_CORE;
    else if (0 == strncmp(analyzer, "Python", 6))
        return SR_REPORT_PYTHON;
    else if (0 == strncmp(analyzer, "Kerneloops", 10))
        return SR_REPORT_KERNELOOPS;
    else if (0 == strncmp(analyzer, "Java", 4))
        return SR_REPORT_JAVA;

    return SR_REPORT_INVALID;
}
