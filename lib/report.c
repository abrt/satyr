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
#include "stacktrace.h"
#include "koops/stacktrace.h"
#include "core/stacktrace.h"
#include "python/stacktrace.h"
#include "java/stacktrace.h"
#include "operating_system.h"
#include "rpm.h"
#include "internal_utils.h"
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
    [SR_REPORT_RUBY] = "ruby",
    [SR_REPORT_JAVASCRIPT] = "javascript",
    NULL
};

struct sr_report *
sr_report_new()
{
    struct sr_report *report = g_malloc(sizeof(*report));
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
    report->stacktrace = NULL;
    report->auth_entries = NULL;
    report->serial = 0;
}

void
sr_report_free(struct sr_report *report)
{
    free(report->component_name);
    sr_operating_system_free(report->operating_system);
    sr_rpm_package_free(report->rpm_packages, true);
    sr_stacktrace_free(report->stacktrace);

    struct sr_report_custom_entry *iter = report->auth_entries;
    while (iter)
    {
        struct sr_report_custom_entry *tmp = iter->next;

        free(iter->value);
        free(iter->key);
        free(iter);

        iter = tmp;
    }

    free(report);
}

void
sr_report_add_auth(struct sr_report *report, const char *key, const char *value)
{
    struct sr_report_custom_entry *new_entry = g_malloc(sizeof(*new_entry));
    new_entry->key = g_strdup(key);
    new_entry->value = g_strdup(value);

    /* prepend the new value
     * it is much faster and easier
     * we can do it because we do not to preserve the order(?) */
    new_entry->next = report->auth_entries;
    report->auth_entries = new_entry;
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
    GString *strbuf = g_string_new(NULL);

    /* Report type. */
    assert(report_type);
    g_string_append(strbuf, "{   \"type\": ");
    sr_json_append_escaped(strbuf, report_type);
    g_string_append(strbuf, "\n");

    /* Component name. */
    if (report->component_name)
    {
        g_string_append(strbuf, ",   \"component\": ");
        sr_json_append_escaped(strbuf, report->component_name);
        g_string_append(strbuf, "\n");
    }

    if (report->report_type != SR_REPORT_KERNELOOPS)
    {
        /* User type (not applicable to koopses). */
        g_string_append_printf(strbuf, ",   \"user\": {   \"root\": %s\n"  \
                                      "            ,   \"local\": %s\n" \
                                      "            }\n",
                              report->user_root ? "true" : "false",
                              report->user_local ? "true" : "false");
    }

    g_string_append_printf(strbuf, ",   \"serial\": %"PRIu32"\n", report->serial);

    /* Stacktrace. */
    if (report->stacktrace)
    {
        char *stacktrace = sr_stacktrace_to_json(report->stacktrace);
        dismantle_object(stacktrace);
        g_string_append(strbuf, stacktrace);
        free(stacktrace);
    }

    g_string_append(strbuf, "}");

    return g_string_free(strbuf, FALSE);
}

char *
sr_report_to_json(struct sr_report *report)
{
    GString *strbuf = g_string_new(NULL);

    /* Report version. */
    g_string_append_printf(strbuf,
                          "{   \"ureport_version\": %"PRIu32"\n",
                          report->report_version);

    /* Report type. */
    char *report_type;
    char *reason;
    switch (report->report_type)
    {
    default:
    case SR_REPORT_INVALID:
    case SR_REPORT_GDB: /* gdb ureports are not supported */
        report_type = g_strdup("invalid");
        reason = g_strdup("invalid");
        break;
    case SR_REPORT_CORE:
    case SR_REPORT_PYTHON:
    case SR_REPORT_KERNELOOPS:
    case SR_REPORT_JAVA:
    case SR_REPORT_RUBY:
    case SR_REPORT_JAVASCRIPT:
        report_type = sr_report_type_to_string(report->report_type);
        reason = sr_stacktrace_get_reason(report->stacktrace);
        break;
    }

    g_string_append(strbuf, ",   \"reason\": ");
    sr_json_append_escaped(strbuf, reason);
    g_string_append(strbuf, "\n");
    free(reason);

    /* Reporter name and version. */
    assert(report->reporter_name);
    assert(report->reporter_version);

    char *reporter = g_strdup_printf("{   \"name\": \"%s\"\n,   \"version\": \"%s\"\n}",
                                 report->reporter_name,
                                 report->reporter_version);
    char *reporter_indented = sr_indent_except_first_line(reporter, strlen(",   \"reporter\": "));
    free(reporter);
    g_string_append_printf(strbuf,
                          ",   \"reporter\": %s\n",
                          reporter_indented);
    free(reporter_indented);

    /* Operating system. */
    if (report->operating_system)
    {
        char *opsys_str = sr_operating_system_to_json(report->operating_system);
        char *opsys_str_indented = sr_indent_except_first_line(opsys_str, strlen(",   \"os\": "));
        free(opsys_str);
        g_string_append_printf(strbuf,
                              ",   \"os\": %s\n",
                              opsys_str_indented);

        free(opsys_str_indented);
    }

    /* Problem section - stacktrace + other info. */
    char *problem = problem_object_string(report, report_type);
    char *problem_indented = sr_indent_except_first_line(problem, strlen(",   \"problem\": "));
    free(problem);
    free(report_type);
    g_string_append_printf(strbuf,
                          ",   \"problem\": %s\n",
                          problem_indented);
    free(problem_indented);

    /* Packages. (Only RPM supported so far.) */
    if (report->rpm_packages)
    {
        char *rpms_str = sr_rpm_package_to_json(report->rpm_packages, true);
        char *rpms_str_indented = sr_indent_except_first_line(rpms_str, strlen(",   \"packages\": "));
        free(rpms_str);
        g_string_append_printf(strbuf,
                              ",   \"packages\": %s\n",
                              rpms_str_indented);

        free(rpms_str_indented);
    }
    /* If there is no package, attach empty list (packages is a mandatory field) */
    else
        g_string_append_printf(strbuf, ",   \"packages\": []\n");

    /* Custom entries.
     *    "auth" : {   "foo": "blah"
     *             ,   "one": "two"
     *             }
     */
    struct sr_report_custom_entry *iter = report->auth_entries;
    if (iter)
    {
        g_string_append_printf(strbuf, ",   \"auth\": {   ");
        sr_json_append_escaped(strbuf, iter->key);
        g_string_append(strbuf, ": ");
        sr_json_append_escaped(strbuf, iter->value);
        g_string_append(strbuf, "\n");

        /* the first entry is prefix with '{', see lines above */
        iter = iter->next;
        while (iter)
        {
            g_string_append_printf(strbuf, "            ,   ");
            sr_json_append_escaped(strbuf, iter->key);
            g_string_append(strbuf, ": ");
            sr_json_append_escaped(strbuf, iter->value);
            g_string_append(strbuf, "\n");
            iter = iter->next;
        }
        g_string_append(strbuf, "            } ");
    }

    g_string_append(strbuf, "}");
    return g_string_free(strbuf, FALSE);
}

enum sr_report_type
sr_report_type_from_string(const char *report_type_str)
{
    for (int i = SR_REPORT_INVALID; i < SR_REPORT_NUM; i++)
    {
        if (0 == g_strcmp0(report_types[i], report_type_str))
            return i;
    }

    return SR_REPORT_INVALID;
}

char *
sr_report_type_to_string(enum sr_report_type report_type)
{
    if (report_type < SR_REPORT_INVALID || report_type >= SR_REPORT_NUM)
        return g_strdup("invalid");

    return g_strdup(report_types[report_type]);
}

struct sr_report *
sr_report_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "root value", error_message))
        return NULL;

    bool success;
    struct sr_report *report = sr_report_new();

    /* Report version. */
    if (!JSON_READ_UINT32(root, "ureport_version", &report->report_version))
        goto fail;

    /* Reporter name and version. */
    json_object *reporter;

    if (json_object_object_get_ex(root, "reporter", &reporter))
    {
        success =
            json_check_type(reporter, json_type_object, "reporter", error_message) &&
            JSON_READ_STRING(reporter, "name", &report->reporter_name) &&
            JSON_READ_STRING(reporter, "version", &report->reporter_version);

        if (!success)
            goto fail;
    }

    /* Operating system. */
    json_object *os;

    if (json_object_object_get_ex(root, "os", &os))
    {
        report->operating_system = sr_operating_system_from_json(os, error_message);
        if (!report->operating_system)
            goto fail;
    }

    /* Packages. */
    json_object *packages;

    if (json_object_object_get_ex(root, "packages", &packages))
    {
        /* In the future, we'll choose the parsing function according to OS here. */
        if (sr_rpm_package_from_json(&report->rpm_packages, packages, true, error_message) < 0)
            goto fail;
    }

    /* Problem. */
    json_object *problem;

    if (json_object_object_get_ex(root, "problem", &problem))
    {
        g_autofree char *report_type = NULL;

        success =
            json_check_type(problem, json_type_object, "problem", error_message) &&
            JSON_READ_STRING(problem, "type", &report_type) &&
            JSON_READ_STRING(problem, "component", &report->component_name);

        if (!success)
            goto fail;

        report->report_type = sr_report_type_from_string(report_type);

        /* User. */
        json_object *user;

        if (json_object_object_get_ex(root, "user", &user))
        {
            success =
                json_check_type(user, json_type_object, "user", error_message) &&
                JSON_READ_BOOL(user, "root", &report->user_root) &&
                JSON_READ_BOOL(user, "local", &report->user_local);

            if (!success)
                goto fail;
        }

        /* Serial. */
        if (!JSON_READ_UINT32(problem, "serial", &report->serial))
            goto fail;

        /* Stacktrace. */
        switch (report->report_type)
        {
        case SR_REPORT_CORE:
        case SR_REPORT_PYTHON:
        case SR_REPORT_KERNELOOPS:
        case SR_REPORT_JAVA:
        case SR_REPORT_RUBY:
            report->stacktrace = sr_stacktrace_from_json(report->report_type, problem, error_message);
            break;
        default:
            /* Invalid report type -> no stacktrace. */
            break;
        }

    }

    /* Authentication entries. */
    json_object *extra;
    if (json_object_object_get_ex(root, "auth", &extra))
    {
        lh_table *object;

        if (!json_check_type(extra, json_type_object, "auth", error_message))
            goto fail;

        /* from the last children down to the first for easier testing :)
         * keep it as it is as long as sr_report_add_auth() does LIFO */
        object = json_object_get_object(extra);
        for (struct lh_entry *entry = object->tail; NULL != entry; entry = entry->prev)
        {
            const char *child_name;
            json_object *child_value;
            const char *string;

            child_name = lh_entry_k(entry);
            child_value = lh_entry_v(entry);

            if (!json_check_type(child_value, json_type_string, child_name, error_message))
                continue;

            string = json_object_get_string(child_value);

            sr_report_add_auth(report, child_name, string);
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
    enum json_tokener_error error;
    json_object *json_root = json_tokener_parse_verbose(report, &error);
    if (!json_root)
    {
        if (NULL != error_message)
        {
            const char *description;

            description = json_tokener_error_desc(error);

            *error_message = g_strdup(description);
        }
        return NULL;
    }

    struct sr_report *result = sr_report_from_json(json_root, error_message);

    json_object_put(json_root);
    return result;
}
