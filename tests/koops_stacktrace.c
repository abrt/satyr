#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/koops/parse-modules", );
    g_test_add_func("/stacktrace/koops/parse", );
    g_test_add_func("/stacktrace/koops/to-json", );
    g_test_add_func("/thread/get-duphash", );
    g_test_add_func("/stacktrace/koops/get-reason", );

    return g_test_run();
}
