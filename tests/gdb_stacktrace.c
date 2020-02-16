#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/gdb/remove-threads-except-one", );
    g_test_add_func("/stacktrace/gdb/find-crash-thread", );
    g_test_add_func("/stacktrace/gdb/limit-frame-depth", );
    g_test_add_func("/stacktrace/gdb/quality-complex", );
    g_test_add_func("/stacktrace/gdb/get-crash-frame", );
    g_test_add_func("/stacktrace/gdb/parse-no-thread-header", );
    g_test_add_func("/stacktrace/gdb/parse-ppc64", );

    return g_test_run();
}
