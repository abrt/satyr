#include <abrt.h>
#include <rpm.h>

#include <glib.h>

void
test_abrt_parse_dso_list(void)
{
    char *dso_list =
        "/lib64/libbz2.so.1.0.4 bzip2-libs-1.0.5-7.el6_0.x86_64 (Scientific Linux) 1342032228\n"
        "/usr/lib64/libnssutil3.so nss-util-3.13.5-1.el6_3.x86_64 (Scientific Linux) 1342664268\n"
        "/usr/lib64/libunwind-coredump.so.0.0.0 libunwind-1.1-1.fc19.x86_64 (None) 1350312182\n"
        "/usr/lib64/librpm.so.1.0.0 rpm-libs-4.8.0-27.el6.x86_64 (Scientific Linux) 1348232255\n"
        "/lib64/libacl.so.1.1.0 libacl-2.2.49-6.el6.x86_64 (Scientific Linux) 1342032227\n"
        "/usr/lib64/libelf-0.152.so elfutils-libelf-0.152-1.el6.x86_64 (Scientific Linux) 1342032254\n"
        "/lib64/libnspr4.so nspr-4.9.1-2.el6_3.x86_64 (Scientific Linux) 1342664267\n"
        "/usr/lib64/libnss3.so nss-3.13.5-1.el6_3.x86_64 (Scientific Linux) 1342664269\n"
        "/lib64/libm-2.12.so glibc-2.12-1.80.el6_3.5.x86_64 (Scientific Linux) 1346116822\n"
        "/lib64/libplc4.so nspr-4.9.1-2.el6_3.x86_64 (Scientific Linux) 1342664267\n"
        "/lib64/libcap.so.2.16 libcap-2.16-5.5.el6.x86_64 (Scientific Linux) 1342032219\n"
        "/lib64/libdl-2.12.so glibc-2.12-1.80.el6_3.5.x86_64 (Scientific Linux) 1346116822\n"
        "/lib64/libdb-4.7.so db4-4.7.25-17.el6.x86_64 (Scientific Linux) 1348232155\n"
        "/lib64/libplds4.so nspr-4.9.1-2.el6_3.x86_64 (Scientific Linux) 1342664267\n"
        "/usr/lib64/liblua-5.1.so lua-5.1.4-4.1.el6.x86_64 (Scientific Linux) 1342032244\n"
        "/usr/lib64/librpmio.so.1.0.0 rpm-libs-4.8.0-27.el6.x86_64 (Scientific Linux) 1348232255\n"
        "/lib64/libpopt.so.0.0.0 popt-1.13-7.el6.x86_64 (Scientific Linux) 1342032221\n"
        "/usr/lib64/liblzma.so.0.0.0 xz-libs-4.999.9-0.3.beta.20091007git.el6.x86_64 (Scientific Linux) 1342032252\n"
        "/lib64/libz.so.1.2.3 zlib-1.2.3-27.el6.x86_64 (Scientific Linux) 1342032220\n"
        "/usr/lib64/libunwind-x86_64.so.8.0.1 libunwind-1.1-1.fc19.x86_64 (None) 1350312182\n"
        "/lib64/libgcc_s-4.4.6-20120305.so.1 libgcc-4.4.6-4.el6.x86_64 (Scientific Linux) 1348232150\n"
        "/lib64/ld-2.12.so glibc-2.12-1.80.el6_3.5.x86_64 (Scientific Linux) 1346116822\n"
        "/lib64/libattr.so.1.1.0 libattr-2.4.44-7.el6.x86_64 (Scientific Linux) 1342032219\n"
        "/lib64/librt-2.12.so glibc-2.12-1.80.el6_3.5.x86_64 (Scientific Linux) 1346116822\n"
        "/lib64/libc-2.12.so glibc-2.12-1.80.el6_3.5.x86_64 (Scientific Linux) 1346116822\n"
        "/lib64/libselinux.so.1 libselinux-2.0.94-5.3.el6.x86_64 (Scientific Linux) 1348232151\n"
        "/usr/lib64/libunwind.so.8.0.1 libunwind-1.1-1.fc19.x86_64 (None) 1350312182\n"
        "/lib64/libpthread-2.12.so glibc-2.12-1.80.el6_3.5.x86_64 (Scientific Linux) 1346116822\n";
    struct sr_rpm_package *packages;
    struct sr_rpm_package *p;

    packages = sr_abrt_parse_dso_list(dso_list);
    p = packages;

    g_assert_nonnull(packages);

    g_assert_cmpstr(packages->name, ==, "bzip2-libs");
    g_assert_cmpstr(packages->version, ==, "1.0.5");
    g_assert_cmpstr(packages->release, ==, "7.el6_0");
    g_assert_cmpstr(packages->architecture, ==, "x86_64");
    g_assert_cmpuint(packages->epoch, ==, 0);
    g_assert_cmpuint(packages->install_time, ==, 1342032228);

    /* last package */
    while (NULL != p->next)
    {
        p = p->next;
    }

    g_assert_cmpstr(p->name, ==, "glibc");
    g_assert_cmpstr(p->version, ==, "2.12");
    g_assert_cmpstr(p->release, ==, "1.80.el6_3.5");
    g_assert_cmpstr(p->architecture, ==, "x86_64");
    g_assert_cmpuint(p->epoch, ==, 0);
    g_assert_cmpuint(p->install_time, ==, 1346116822);

    sr_rpm_package_free(packages, true);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/abrt/parse-dso-list", test_abrt_parse_dso_list);

    return g_test_run();
}
