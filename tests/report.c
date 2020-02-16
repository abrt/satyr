#include <abrt.h>
#include <glib.h>
#include <operating_system.h>
#include <report.h>
#include <report_type.h>
#include <rpm.h>
#include <utils.h>

static void
test_report_type_to_string(void)
{
    struct
    {
        enum sr_report_type type;
        const char *type_string;
    } types[] =
    {
        { SR_REPORT_INVALID, "invalid", },
        { SR_REPORT_CORE, "core", },
        { SR_REPORT_KERNELOOPS, "kerneloops", },
        { SR_REPORT_GDB, "gdb", },
        { SR_REPORT_RUBY, "ruby", },
        { SR_REPORT_JAVASCRIPT, "javascript", },
        { SR_REPORT_NUM, "invalid", },
        { 5000, "invalid", },
        { -42, "invalid", },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(types); i++)
    {
        g_autofree char *string = NULL;

        string = sr_report_type_to_string(types[i].type);

        g_assert_cmpstr(string, ==, types[i].type_string);
    }
}

static void
test_report_type_from_string(void)
{
    struct
    {
        enum sr_report_type type;
        const char *type_string;
    } types[] =
    {
        { SR_REPORT_INVALID, "invalid", },
        { SR_REPORT_CORE, "core", },
        { SR_REPORT_PYTHON, "python", },
        { SR_REPORT_GDB, "gdb", },
        { SR_REPORT_RUBY, "ruby", },
        { SR_REPORT_JAVASCRIPT, "javascript", },
        { SR_REPORT_INVALID, NULL, },
        { SR_REPORT_INVALID, "visual basic", },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(types); i++)
    {
        enum sr_report_type type;

        type = sr_report_type_from_string(types[i].type_string);

        g_assert_cmpint(type, ==, types[i].type);
    }
}

static void
test_report_add_auth(void)
{
    const char *expected[][2] =
    {
        { "satyr", "wonderful", },
        { "abrt", "awesome", },
        { "foo", "blah", },
    };
    struct sr_report *report;
    struct sr_report_custom_entry *report_iterator;
    g_autofree char *json = NULL;
    char *error = NULL;

    report = sr_report_new();

    sr_report_add_auth(report, "foo", "blah");
    sr_report_add_auth(report, "abrt", "awesome");
    sr_report_add_auth(report, "satyr", "wonderful");

    report_iterator = report->auth_entries;

    for (size_t i = 0;
         i < G_N_ELEMENTS(expected), NULL != report_iterator;
         i++, report_iterator = report_iterator->next)
    {
        g_assert_cmpstr(report_iterator->key, ==, expected[i][0]);
        g_assert_cmpstr(report_iterator->value, ==, expected[i][1]);
    }

    json = sr_report_to_json(report);

    for (size_t i = 0; i < G_N_ELEMENTS(expected); i++)
    {
        g_autofree char *entry = NULL;
        char *needle;

        entry = g_strdup_printf("\"%s\": \"%s\"", expected[i][0], expected[i][1]);
        needle = strstr(json, entry);

        g_assert_nonnull(needle);
    }

    sr_report_free(report);

    report = sr_report_from_json_text(json, &error);

    report_iterator = report->auth_entries;

    for (size_t i = 0;
         i < G_N_ELEMENTS(expected), NULL != report_iterator;
         i++, report_iterator = report_iterator->next)
    {
        g_assert_cmpstr(report_iterator->key, ==, expected[i][0]);
        g_assert_cmpstr(report_iterator->value, ==, expected[i][1]);
    }
}

static void
test_abrt_report_from_dir(void)
{
    char *error_message = NULL;
    struct sr_report *report;
    struct sr_operating_system *os;
    struct sr_rpm_package *package;
    g_autofree char *report_json = NULL;
    g_autofree char *report_json_from_file = NULL;

    report = sr_abrt_report_from_dir("problem_dir", &error_message);

    g_assert_nonnull(report);

    /* set reporter_version because the value depends on build */
    report->reporter_version = "0.20.dirty";

    g_assert_cmpuint(report->report_version, ==, 2);
    g_assert_cmpstr(report->reporter_name, ==, "satyr");
    g_assert_cmpint(report->report_type, ==, SR_REPORT_CORE);
    g_assert_false(report->user_root);
    g_assert_true(report->user_local);
    g_assert_cmpstr(report->component_name, ==, "coreutils");
    g_assert_cmpuint(report->serial, ==, 55);

    os = report->operating_system;

    g_assert_cmpstr(os->name, ==, "fedora");
    g_assert_cmpstr(os->version, ==, "24");
    g_assert_cmpstr(os->architecture, ==, "x86_64");
    g_assert_cmpstr(os->cpe, ==, "cpe:/o:fedoraproject:fedora:24");
    g_assert_cmpstr(os->variant, ==, "workstation");

    package = report->rpm_packages;

    g_assert_cmpstr(package->name, ==, "coreutils");
    g_assert_cmpuint(package->epoch, ==, 0);
    g_assert_cmpstr(package->version, ==, "8.25");
    g_assert_cmpstr(package->release, ==, "5.fc24");
    g_assert_cmpstr(package->architecture, ==, "x86_64");
    g_assert_cmpuint(package->install_time, ==, 1460022612);
    g_assert_cmpint(package->role, ==, SR_ROLE_AFFECTED);

    package = package->next;

    g_assert_cmpstr(package->name, ==, "glibc");
    g_assert_cmpuint(package->epoch, ==, 0);
    g_assert_cmpstr(package->version, ==, "2.23.1");
    g_assert_cmpstr(package->release, ==, "5.fc24");
    g_assert_cmpstr(package->architecture, ==, "x86_64");
    g_assert_cmpuint(package->install_time, ==, 1460022605);

    package = package->next;

    g_assert_cmpstr(package->name, ==, "glibc-all-langpacks");
    g_assert_cmpuint(package->epoch, ==, 0);
    g_assert_cmpstr(package->version, ==, "2.23.1");
    g_assert_cmpstr(package->release, ==, "5.fc24");
    g_assert_cmpstr(package->architecture, ==, "x86_64");
    g_assert_cmpuint(package->install_time, ==, 1460024921);

    report_json = sr_report_to_json(report);
    report_json_from_file = sr_file_to_string("json_files/ureport-from-problem-dir", &error_message);

    g_assert_cmpstr(report_json, ==, report_json_from_file);

    sr_report_free(report);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/report/type/to-string", test_report_type_to_string);
    g_test_add_func("/report/type/from-string", test_report_type_from_string);
    g_test_add_func("/report/add-auth", test_report_add_auth);
    g_test_add_func("/report/abrt/from-dir", test_abrt_report_from_dir);

    return g_test_run();
}
