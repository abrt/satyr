#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/js/parse-v8", );
    g_test_add_func("/stacktrace/js/dup", );
    g_test_add_func("/stacktrace/js/get-reason", );
    g_test_add_func("/stacktrace/js/to-json", );
    g_test_add_func("/stacktrace/js/from-json", );

    return g_test_run();
}
