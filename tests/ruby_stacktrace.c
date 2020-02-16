#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/ruby/parse", );
    g_test_add_func("/stacktrace/ruby/dup", );
    g_test_add_func("/stacktrace/ruby/get-reason", );
    g_test_add_func("/stacktrace/ruby/to-json", );
    g_test_add_func("/stacktrace/ruby/from-json", );

    return g_test_run();
}
