#include <gdb/sharedlib.h>
#include <utils.h>

#include <glib.h>

static void
test_gdb_sharedlib_parse(void)
{
    g_autofree char *error = NULL;
    g_autofree char *stack_trace = NULL;
    struct sr_gdb_sharedlib *libraries;

    stack_trace = sr_file_to_string("gdb_stacktraces/rhbz-621492", &error);
    libraries = sr_gdb_sharedlib_parse(stack_trace);

    g_assert_true(libraries->from == 0x3848c05640);
    g_assert_true(libraries->to == 0x3848c10e48);
    g_assert_true(libraries->symbols == SYMS_OK);
    g_assert_cmpstr("/lib64/libpthread.so.0", ==, libraries->soname);

    sr_gdb_sharedlib_free(libraries);
}

static void
test_gdb_sharedlib_count(void)
{
    g_autofree char *error = NULL;
    g_autofree char *stack_trace = NULL;
    struct sr_gdb_sharedlib *libraries;
    int count;

    stack_trace = sr_file_to_string("gdb_stacktraces/rhbz-621492", &error);
    libraries = sr_gdb_sharedlib_parse(stack_trace);
    count = sr_gdb_sharedlib_count(libraries);

    g_assert_cmpint(count, ==, 185);

    sr_gdb_sharedlib_free(libraries);
}

static void
test_gdb_sharedlib_append(void)
{
    struct sr_gdb_sharedlib *libraries[2];
    int count;

    libraries[0] = sr_gdb_sharedlib_new();
    libraries[1] = sr_gdb_sharedlib_new();
    count = sr_gdb_sharedlib_count(libraries[0]);
    g_assert_cmpint(count, ==, 1);

    count = sr_gdb_sharedlib_count(libraries[1]);
    g_assert_cmpint(count, ==, 1);

    sr_gdb_sharedlib_append(libraries[0], libraries[1]);

    count = sr_gdb_sharedlib_count(libraries[0]);
    g_assert_cmpint(count, ==, 2);

    count = sr_gdb_sharedlib_count(libraries[1]);
    g_assert_cmpint(count, ==, 1);

    for (size_t i = 0; i < G_N_ELEMENTS(libraries); i++)
    {
        sr_gdb_sharedlib_free(libraries[i]);
    }
}

static void
test_gdb_sharedlib_find_address(void)
{
    char *error;
    char *stack_trace;
    struct sr_gdb_sharedlib *libraries;

    stack_trace = sr_file_to_string("gdb_stacktraces/rhbz-621492", &error);
    libraries = sr_gdb_sharedlib_parse(stack_trace);

    g_assert_nonnull(sr_gdb_sharedlib_find_address(libraries, 0x3848c08000));
    g_assert_nonnull(sr_gdb_sharedlib_find_address(libraries, 0x7f907d3f0000));
    g_assert_null(sr_gdb_sharedlib_find_address(libraries, 0));
    g_assert_null(sr_gdb_sharedlib_find_address(libraries, 0xffff00000000ffff));

    sr_gdb_sharedlib_free(libraries);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gdb_sharedlib/parse", test_gdb_sharedlib_parse);
    g_test_add_func("/gdb_sharedlib/count", test_gdb_sharedlib_count);
    g_test_add_func("/gdb_sharedlib/append", test_gdb_sharedlib_append);
    g_test_add_func("/gdb_sharedlib/find-address", test_gdb_sharedlib_find_address);

    return g_test_run();
}
