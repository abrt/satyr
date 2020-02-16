#include <operating_system.h>

#include <glib.h>

typedef struct
{
    const char *test_name;

    const char *release;

    const char *expected_name;
    const char *expected_version;
    const char *expected_variant;
} ReleaseTestData;

static const char *fedora_19_os_release =
    "NAME=Fedora\n"
    "VERSION=\"19 (Schrödinger’s Cat)\"\n"
    "ID=fedora\n"
    "VERSION_ID=19\n"
    "PRETTY_NAME=\"Fedora 19 (Schrödinger’s Cat)\"\n"
    "ANSI_COLOR=\"0;34\"\n"
    "CPE_NAME=\"cpe:/o:fedoraproject:fedora:19\"\n";
static const char *fedora_23_os_release =
    "NAME=Fedora\n"
    "VERSION=\"23 (Workstation Edition)\"\n"
    "ID=fedora\n"
    "VERSION_ID=23\n"
    "PRETTY_NAME=\"Fedora 23 (Workstation Edition)\"\n"
    "ANSI_COLOR=\"0;34\"\n"
    "CPE_NAME=\"cpe:/o:fedoraproject:fedora:23\"\n"
    "HOME_URL=\"https://fedoraproject.org/\"\n"
    "BUG_REPORT_URL=\"https://bugzilla.redhat.com/\"\n"
    "REDHAT_BUGZILLA_PRODUCT=\"Fedora\"\n"
    "REDHAT_BUGZILLA_PRODUCT_VERSION=23\n"
    "REDHAT_SUPPORT_PRODUCT=\"Fedora\"\n"
    "REDHAT_SUPPORT_PRODUCT_VERSION=23\n"
    "PRIVACY_POLICY_URL=https://fedoraproject.org/wiki/Legal:PrivacyPolicy\n"
    "VARIANT=\"Workstation Edition\"\n"
    "VARIANT_ID=workstation\n";
static const char *fedora_rawhide_os_release =
    "NAME=Fedora\n"
    "VERSION=\"20 (Rawhide)\"\n"
    "ID=fedora\n"
    "VERSION_ID=20\n"
    "PRETTY_NAME=\"Fedora 20 (Rawhide)\"\n"
    "ANSI_COLOR=\"0;34\"\n"
    "CPE_NAME=\"cpe:/o:fedoraproject:fedora:20\"\n"
    "REDHAT_BUGZILLA_PRODUCT=\"Fedora\"\n"
    "REDHAT_BUGZILLA_PRODUCT_VERSION=Rawhide\n"
    "REDHAT_SUPPORT_PRODUCT=\"Fedora\"\n"
    "REDHAT_SUPPORT_PRODUCT_VERSION=Rawhide\n";
static const char *rhel_7_os_release =
    "NAME=\"Red Hat Enterprise Linux Workstation\"\n"
    "VERSION=\"7.0 (Codename)\"\n"
    "ID=\"rhel\"\n"
    "VERSION_ID=\"7.0\"\n"
    "PRETTY_NAME=\"Red Hat Enterprise Linux Workstation 7.0 (Codename)\"\n"
    "ANSI_COLOR=\"0;31\"\n"
    "CPE_NAME=\"cpe:/o:redhat:enterprise_linux:7.0:beta:workstation\"\n"
    "\n"
    "REDHAT_BUGZILLA_PRODUCT=\"Red Hat Enterprise Linux 7\"\n"
    "REDHAT_BUGZILLA_PRODUCT_VERSION=7.0\n"
    "REDHAT_SUPPORT_PRODUCT=\"Red Hat Enterprise Linux\"\n"
    "REDHAT_SUPPORT_PRODUCT_VERSION=7.0\n";

static void
test_operating_system_parse_etc_system_release(const void *user_data)
{
    const ReleaseTestData *test_data;
    g_autofree char *name = NULL;
    g_autofree char *version = NULL;
    bool success;

    test_data = user_data;
    success = sr_operating_system_parse_etc_system_release(test_data->release,
                                                           &name,
                                                           &version);

    g_assert_true(success);
    g_assert_cmpstr(name, ==, test_data->expected_name);
    g_assert_cmpstr(version, ==, test_data->expected_version);
}

static void
test_operating_system_parse_etc_os_release(const void *user_data)
{
    const ReleaseTestData *test_data;
    struct sr_operating_system *operating_system;
    bool success;

    test_data = user_data;
    operating_system = sr_operating_system_new();
    success = sr_operating_system_parse_etc_os_release(test_data->release,
                                                       operating_system);

    g_assert_true(success);
    g_assert_cmpstr(operating_system->name, ==, test_data->expected_name);
    g_assert_cmpstr(operating_system->version, ==, test_data->expected_version);
    g_assert_cmpstr(operating_system->variant, ==, test_data->expected_variant);

    sr_operating_system_free(operating_system);
}

int
main(int    argc,
     char **argv)
{
    ReleaseTestData system_release_tests[] =
    {
        {
            "/operating-system/parse-etc-system-release/fedora",
            "Fedora release 16 (Verne)",
            "fedora",
            "16",
        },
        {
            "/operating-system/parse-etc-system-release/rhel",
            "Red Hat Enterprise Linux release 6.2",
            "rhel",
            "6.2",
        },
        {
            "/operating-system/parse-etc-system-release/centos",
            "CentOS release 6.5 (Final)",
            "centos",
            "6.5",
        },
    };
    ReleaseTestData os_release_tests[] =
    {
        {
            "/operating-system/parse-etc-os-release/fedora-19",
            fedora_19_os_release,
            "fedora",
            "19",
            NULL,
        },
        {
            "/operating-system/parse-etc-os-release/fedora-23",
            fedora_23_os_release,
            "fedora",
            "23",
            "workstation",
        },
        {
            "/operating-system/parse-etc-os-release/fedora-rawhide",
            fedora_rawhide_os_release,
            "fedora",
            "rawhide",
            NULL,
        },
        {
            "/operating-system/parse-etc-os-release/rhel-7",
            rhel_7_os_release,
            "rhel",
            "7.0",
            NULL,
        },
    };


    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(system_release_tests); i++)
    {
        g_test_add_data_func(system_release_tests[i].test_name,
                             &system_release_tests[i],
                             test_operating_system_parse_etc_system_release);
    }
    for (size_t i = 0; i < G_N_ELEMENTS(os_release_tests); i++)
    {
        g_test_add_data_func(os_release_tests[i].test_name,
                             &os_release_tests[i],
                             test_operating_system_parse_etc_os_release);
    }

    return g_test_run();
}
