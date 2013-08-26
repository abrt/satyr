/*
    report.c

    Copyright (C) 2012  Red Hat, Inc.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301 USA.
*/
#include "report.h"
#include "config.h"
#include "utils.h"
#include "json.h"
#include "koops/stacktrace.h"
#include "core/stacktrace.h"
#include "python/stacktrace.h"
#include "java/stacktrace.h"
#include "operating_system.h"
#include "rpm.h"
#include "internal_utils.h"
#include "strbuf.h"
#include <string.h>
#include <assert.h>

static char *report_types[] =
{
    [SR_REPORT_INVALID] = "invalid",
    [SR_REPORT_CORE] = "core",
    [SR_REPORT_PYTHON] = "python",
    [SR_REPORT_KERNELOOPS] = "kerneloops",
    [SR_REPORT_JAVA] = "java",
    [SR_REPORT_GDB] = "gdb",
    NULL
};

struct sr_report *
sr_report_new()
{
    struct sr_report *report = sr_malloc(sizeof(struct sr_report));
    sr_report_init(report);
    return report;
}

void
sr_report_init(struct sr_report *report)
{
    report->report_version = 2;
    report->report_type = SR_REPORT_INVALID;
    report->reporter_name = PACKAGE_NAME;
    report->reporter_version = PACKAGE_VERSION;
    report->user_root = false;
    report->user_local = true;
    report->operating_system = NULL;
    report->component_name = NULL;
    report->rpm_packages = NULL;
    report->python_stacktrace = NULL;
    report->koops_stacktrace = NULL;
    report->core_stacktrace = NULL;
    report->java_stacktrace = NULL;
}

void
sr_report_free(struct sr_report *report)
{
    free(report->component_name);
    sr_operating_system_free(report->operating_system);
    sr_rpm_package_free(report->rpm_packages, true);
    sr_python_stacktrace_free(report->python_stacktrace);
    sr_koops_stacktrace_free(report->koops_stacktrace);
    sr_core_stacktrace_free(report->core_stacktrace);
    sr_java_stacktrace_free(report->java_stacktrace);
    free(report);
}

/* The object has to be non-empty, i.e. contain at least one key-value pair. */
static void
dismantle_object(char *obj)
{
    assert(obj);

    size_t last = strlen(obj) - 1;

    assert(obj[0] == '{');
    assert(obj[last] == '}');

    obj[0] = ',';
    obj[last] = '\0';
}

static char *
problem_object_string(struct sr_report *report, const char *report_type)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Report type. */
    assert(report_type);
    sr_strbuf_append_str(strbuf, "{   \"type\": ");
    sr_json_append_escaped(strbuf, report_type);
    sr_strbuf_append_str(strbuf, "\n");

    /* Component name. */
    if (report->component_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"component\": ");
        sr_json_append_escaped(strbuf, report->component_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (report->report_type != SR_REPORT_KERNELOOPS)
    {
        /* User type (not applicable to koopses). */
        sr_strbuf_append_strf(strbuf, ",   \"user\": {   \"root\": %s\n"  \
                                      "            ,   \"local\": %s\n" \
                                      "            }\n",
                              report->user_root ? "true" : "false",
                              report->user_local ? "true" : "false");
    }

    /* Stacktrace. */

    /* Core stacktrace. */
    if (report->core_stacktrace)
    {
        char *stacktrace = sr_core_stacktrace_to_json(report->core_stacktrace);
        dismantle_object(stacktrace);
        sr_strbuf_append_str(strbuf, stacktrace);
        free(stacktrace);
    }

    /* Python stacktrace. */
    if (report->python_stacktrace)
    {
        char *stacktrace = sr_python_stacktrace_to_json(report->python_stacktrace);
        dismantle_object(stacktrace);
        sr_strbuf_append_str(strbuf, stacktrace);
        free(stacktrace);
    }

    /* Koops stacktrace. */
    if (report->koops_stacktrace)
    {
        char *stacktrace = sr_koops_stacktrace_to_json(report->koops_stacktrace);
        dismantle_object(stacktrace);
        sr_strbuf_append_str(strbuf, stacktrace);
        free(stacktrace);
    }

    /* Java stacktrace. */
    if (report->java_stacktrace)
    {
        char *stacktrace = sr_java_stacktrace_to_json(report->java_stacktrace);
        dismantle_object(stacktrace);
        sr_strbuf_append_strf(strbuf, stacktrace);
        free(stacktrace);
    }

    sr_strbuf_append_str(strbuf, "}");

    return sr_strbuf_free_nobuf(strbuf);
}

char *
sr_report_to_json(struct sr_report *report)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Report version. */
    sr_strbuf_append_strf(strbuf,
                          "{   \"ureport_version\": %"PRIu32"\n",
                          report->report_version);

    /* Report type. */
    const char *report_type;
    char *reason;
    switch (report->report_type)
    {
    default:
    case SR_REPORT_INVALID:
    case SR_REPORT_GDB: /* gdb ureports are not supported */
        report_type = "invalid";
        reason = sr_strdup("invalid");
        break;
    case SR_REPORT_CORE:
        report_type = "core";
        reason = sr_core_stacktrace_get_reason(report->core_stacktrace);
        break;
    case SR_REPORT_PYTHON:
        report_type = "python";
        reason = sr_python_stacktrace_get_reason(report->python_stacktrace);
        break;
    case SR_REPORT_KERNELOOPS:
        report_type = "kerneloops";
        reason = sr_koops_stacktrace_get_reason(report->koops_stacktrace);
        break;
    case SR_REPORT_JAVA:
        report_type = "java";
        reason = sr_java_stacktrace_get_reason(report->java_stacktrace);
        break;
    }

    sr_strbuf_append_str(strbuf, ",   \"reason\": ");
    sr_json_append_escaped(strbuf, reason);
    sr_strbuf_append_str(strbuf, "\n");
    free(reason);

    /* Reporter name and version. */
    assert(report->reporter_name);
    assert(report->reporter_version);

    char *reporter = sr_asprintf("{   \"name\": \"%s\"\n,   \"version\": \"%s\"\n}",
                                 report->reporter_name,
                                 report->reporter_version);
    char *reporter_indented = sr_indent_except_first_line(reporter, strlen(",   \"reporter\": "));
    free(reporter);
    sr_strbuf_append_strf(strbuf,
                          ",   \"reporter\": %s\n",
                          reporter_indented);
    free(reporter_indented);

    /* Operating system. */
    if (report->operating_system)
    {
        char *opsys_str = sr_operating_system_to_json(report->operating_system);
        char *opsys_str_indented = sr_indent_except_first_line(opsys_str, strlen(",   \"os\": "));
        free(opsys_str);
        sr_strbuf_append_strf(strbuf,
                              ",   \"os\": %s\n",
                              opsys_str_indented);

        free(opsys_str_indented);
    }

    /* Problem section - stacktrace + other info. */
    char *problem = problem_object_string(report, report_type);
    char *problem_indented = sr_indent_except_first_line(problem, strlen(",   \"problem\": "));
    free(problem);
    sr_strbuf_append_strf(strbuf,
                          ",   \"problem\": %s\n",
                          problem_indented);
    free(problem_indented);

    /* Packages. (Only RPM supported so far.) */
    if (report->rpm_packages)
    {
        char *rpms_str = sr_rpm_package_to_json(report->rpm_packages, true);
        char *rpms_str_indented = sr_indent_except_first_line(rpms_str, strlen(",   \"packages\": "));
        free(rpms_str);
        sr_strbuf_append_strf(strbuf,
                              ",   \"packages\": %s\n",
                              rpms_str_indented);

        free(rpms_str_indented);
    }

    sr_strbuf_append_str(strbuf, "}");
    return sr_strbuf_free_nobuf(strbuf);
}

enum sr_report_type
sr_report_type_from_string(const char *report_type_str)
{
    for (int i = SR_REPORT_INVALID; i < SR_REPORT_NUM; i++)
    {
        if (0 == sr_strcmp0(report_types[i], report_type_str))
            return i;
    }

    return SR_REPORT_INVALID;
}

char *
sr_report_type_to_string(enum sr_report_type report_type)
{
    if (report_type < SR_REPORT_INVALID || report_type >= SR_REPORT_NUM)
        return sr_strdup("invalid");

    return sr_strdup(report_types[report_type]);
}

struct sr_report *
sr_report_from_json(struct sr_json_value *root, char **error_message)
{
    if (!JSON_CHECK_TYPE(root, SR_JSON_OBJECT, "root value"))
        return NULL;

    bool success;
    struct sr_report *report = sr_report_new();

    /* Report version. */
    if (!JSON_READ_UINT32(root, "ureport_version", &report->report_version))
        goto fail;

    /* Reporter name and version. */
    struct sr_json_value *reporter = json_element(root, "reporter");
    if (reporter)
    {
        success =
            JSON_CHECK_TYPE(reporter, SR_JSON_OBJECT, "reporter") &&
            JSON_READ_STRING(reporter, "name", &report->reporter_name) &&
            JSON_READ_STRING(reporter, "version", &report->reporter_version);

        if (!success)
            goto fail;
    }

    /* Operating system. */
    struct sr_json_value *os = json_element(root, "os");
    if (os)
    {
        report->operating_system = sr_operating_system_from_json(os, error_message);
        if (!report->operating_system)
            goto fail;
    }

    /* Packages. */
    struct sr_json_value *packages = json_element(root, "packages");
    if (packages)
    {
        /* In the future, we'll choose the parsing function according to OS here. */
        report->rpm_packages = sr_rpm_package_from_json(packages, true, error_message);
        if (!report->rpm_packages)
            goto fail;
    }

    /* Problem. */
    struct sr_json_value *problem = json_element(root, "problem");
    if (problem)
    {
        char *report_type;

        success =
            JSON_CHECK_TYPE(problem, SR_JSON_OBJECT, "problem") &&
            JSON_READ_STRING(problem, "type", &report_type) &&
            JSON_READ_STRING(problem, "component", &report->component_name);

        if (!success)
            goto fail;

        report->report_type = sr_report_type_from_string(report_type);

        /* User. */
        struct sr_json_value *user = json_element(root, "user");
        if (user)
        {
            success =
                JSON_CHECK_TYPE(user, SR_JSON_OBJECT, "user") &&
                JSON_READ_BOOL(user, "root", &report->user_root) &&
                JSON_READ_BOOL(user, "local", &report->user_local);

            if (!success)
                goto fail;
        }

        /* Stacktrace. */
        switch (report->report_type)
        {
        case SR_REPORT_CORE:
            report->core_stacktrace = sr_core_stacktrace_from_json(problem, error_message);
            break;
        case SR_REPORT_PYTHON:
            report->python_stacktrace = sr_python_stacktrace_from_json(problem, error_message);
            break;
        case SR_REPORT_KERNELOOPS:
            report->koops_stacktrace = sr_koops_stacktrace_from_json(problem, error_message);
            break;
        case SR_REPORT_JAVA:
            report->java_stacktrace = sr_java_stacktrace_from_json(problem, error_message);
            break;
        default:
            /* Invalid report type -> no stacktrace. */
            break;
        }

    }

    return report;

fail:
    sr_report_free(report);
    return NULL;
}

struct sr_report *
sr_report_from_json_text(const char *report, char **error_message)
{
    struct sr_json_value *json_root = sr_json_parse(report, error_message);

    if (!json_root)
        return NULL;

    struct sr_report *result = sr_report_from_json(json_root, error_message);

    sr_json_value_free(json_root);
    return result;
}
