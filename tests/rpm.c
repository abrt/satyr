#include <rpm.h>
#include <utils.h>

#include <glib.h>

#include <stdbool.h>
#include <stdint.h>

static void
test_rpm_package_cmp(void)
{
    struct sr_rpm_package *p1;
    struct sr_rpm_package *p2;

    p1 = sr_rpm_package_new();
    p2 = sr_rpm_package_new();

    p1->name = g_strdup("coreutils");
    p1->version = g_strdup("8.32");
    p1->release = g_strdup("19.fc33");
    p1->architecture = g_strdup("x86_64");
    p1->install_time = 1627139000;
    p1->next = p2;

    p2->name = g_strdup("coreutils");
    p2->version = g_strdup("8.32");
    p2->release = g_strdup("21.fc33");
    p2->architecture = g_strdup("x86_64");
    p2->install_time = 1627139000;
    p2->next = NULL;

    g_assert_cmpint(sr_rpm_package_cmp(p1, p2), <, 0);

    g_free(p1->release);
    p1->release = g_strdup(p2->release);
    p1->install_time += 3600;

    g_assert_cmpint(sr_rpm_package_cmp(p1, p2), >, 0);

    p1->install_time = p2->install_time;

    g_assert_cmpint(sr_rpm_package_cmp(p1, p2), ==, 0);

    sr_rpm_package_free(p1, true);
}

static void
test_rpm_package_parse_nvr(void)
{
    g_autofree char *name = NULL;
    g_autofree char *version = NULL;
    g_autofree char *release = NULL;
    bool success;

    success = sr_rpm_package_parse_nvr("coreutils-8.4-19.el6",
                                       &name,
                                       &version,
                                       &release);

    g_assert_true(success);
    g_assert_cmpstr(name, ==, "coreutils");
    g_assert_cmpstr(version, ==, "8.4");
    g_assert_cmpstr(release, ==, "19.el6");
}

static void
test_rpm_package_parse_nevra(void)
{
    g_autofree char *name = NULL;
    g_autofree char *version = NULL;
    g_autofree char *release = NULL;
    g_autofree char *architecture = NULL;
    uint32_t epoch;
    bool success;

    success = sr_rpm_package_parse_nevra("glibc-2.12-1.80.el6_3.5.x86_64",
                                         &name,
                                         &epoch,
                                         &version,
                                         &release,
                                         &architecture);

    g_assert_true(success);
    g_assert_cmpstr(name, ==, "glibc");
    g_assert_cmpuint(epoch, ==, 0);
    g_assert_cmpstr(version, ==, "2.12");
    g_assert_cmpstr(release, ==, "1.80.el6_3.5");
    g_assert_cmpstr(architecture, ==, "x86_64");
}

void
test_rpm_package_uniq_1(void)
{
    struct sr_rpm_package *p1;
    struct sr_rpm_package *p2;
    struct sr_rpm_package *p3;
    struct sr_rpm_package *packages;

    p1 = sr_rpm_package_new();
    p2 = sr_rpm_package_new();
    p3 = sr_rpm_package_new();

    p1->name = g_strdup("coreutils");
    p1->version = g_strdup("8.15");
    p1->release = g_strdup("10.fc17");
    p1->role = SR_ROLE_AFFECTED;
    p1->next = p2;

    p2->name = g_strdup("coreutils");
    p2->version = g_strdup("8.15");
    p2->release = g_strdup("10.fc17");
    p2->architecture = g_strdup("x86_64");
    p2->install_time = 1363628422;
    p2->next = p3;

    p3->name = g_strdup("glibc");
    p3->version = g_strdup("2.15");
    p3->release = g_strdup("58.fc17");
    p3->architecture = g_strdup("x86_64");
    p3->install_time = 1363628422;

    packages = sr_rpm_package_uniq(p1);

    g_assert_cmpstr(packages->name, ==, "coreutils");
    g_assert_cmpstr(packages->version, ==, "8.15");
    g_assert_cmpstr(packages->release, ==, "10.fc17");
    g_assert_cmpstr(packages->architecture, ==, "x86_64");
    g_assert_cmpuint(packages->install_time, ==, 1363628422);
    g_assert_cmpint(packages->role, ==, SR_ROLE_AFFECTED);

    g_assert_cmpstr(packages->next->name, ==, "glibc");
    g_assert_cmpstr(packages->next->version, ==, "2.15");
    g_assert_cmpstr(packages->next->release, ==, "58.fc17");
    g_assert_cmpstr(packages->next->architecture, ==, "x86_64");
    g_assert_cmpuint(packages->next->install_time, ==, 1363628422);
    g_assert_cmpint(packages->next->role, ==, SR_ROLE_UNKNOWN);

    g_assert_null(packages->next->next);

    sr_rpm_package_free(packages, true);
}

void
test_rpm_package_uniq_2(void)
{
    struct sr_rpm_package *p1;
    struct sr_rpm_package *p2;
    struct sr_rpm_package *p3;
    struct sr_rpm_package *packages;

    p1 = sr_rpm_package_new();
    p2 = sr_rpm_package_new();
    p3 = sr_rpm_package_new();

    p1->name = g_strdup("glibc");
    p1->version = g_strdup("2.15");
    p1->release = g_strdup("58.fc17");
    p1->architecture = g_strdup("x86_64");
    p1->install_time = 1363628422;
    p1->next = p2;

    p2->name = g_strdup("coreutils");
    p2->version = g_strdup("8.15");
    p2->release = g_strdup("10.fc17");
    p2->next = p3;

    p3->name = g_strdup("coreutils");
    p3->version = g_strdup("8.15");
    p3->release = g_strdup("10.fc17");
    p3->architecture = g_strdup("x86_64");
    p3->install_time = 1363628422;
    p3->role = SR_ROLE_AFFECTED;

    packages = sr_rpm_package_uniq(p1);

    g_assert_cmpstr(packages->name, ==, "glibc");
    g_assert_cmpstr(packages->version, ==, "2.15");
    g_assert_cmpstr(packages->release, ==, "58.fc17");
    g_assert_cmpstr(packages->architecture, ==, "x86_64");
    g_assert_cmpuint(packages->install_time, ==, 1363628422);
    g_assert_cmpint(packages->role, ==, SR_ROLE_UNKNOWN);

    g_assert_cmpstr(packages->next->name, ==, "coreutils");
    g_assert_cmpstr(packages->next->version, ==, "8.15");
    g_assert_cmpstr(packages->next->release, ==, "10.fc17");
    g_assert_cmpstr(packages->next->architecture, ==, "x86_64");
    g_assert_cmpuint(packages->next->install_time, ==, 1363628422);
    g_assert_cmpint(packages->next->role, ==, SR_ROLE_AFFECTED);

    g_assert_null(packages->next->next);

    sr_rpm_package_free(packages, true);
}

void
test_rpm_package_uniq_3(void)
{
    struct sr_rpm_package *p1;
    struct sr_rpm_package *p2;
    struct sr_rpm_package *p3;
    struct sr_rpm_package *packages;

    p1 = sr_rpm_package_new();
    p2 = sr_rpm_package_new();
    p3 = sr_rpm_package_new();

    p1->name = g_strdup("pango");
    p1->version = g_strdup("1.32.3");
    p1->release = g_strdup("1.fc18");
    p1->architecture = g_strdup("x86_64");
    p2->install_time = 1363628422;
    p1->next = p2;

    p2->name = g_strdup("pango");
    p2->version = g_strdup("1.32.3");
    p2->release = g_strdup("1.fc18");
    p2->architecture = g_strdup("x86_64");
    p2->install_time = 1363628422;
    p2->next = p3;

    p3->name = g_strdup("pango");
    p3->version = g_strdup("1.32.3");
    p3->release = g_strdup("1.fc18");
    p3->architecture = g_strdup("x86_64");
    p3->install_time = 1363628422;

    packages = sr_rpm_package_uniq(p1);

    g_assert_nonnull(packages);
    g_assert_null(packages->next);

    sr_rpm_package_free(packages, true);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/rpm/package-cmp", test_rpm_package_cmp);
    g_test_add_func("/rpm/package-parse-nvr", test_rpm_package_parse_nvr);
    g_test_add_func("/rpm/package-parse-nevra", test_rpm_package_parse_nevra);

    g_test_add_func("/rpm/package-uniq-1", test_rpm_package_uniq_1);
    g_test_add_func("/rpm/package-uniq-2", test_rpm_package_uniq_2);
    g_test_add_func("/rpm/package-uniq-3", test_rpm_package_uniq_3);

    return g_test_run();
}
