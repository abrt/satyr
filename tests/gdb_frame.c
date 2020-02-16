#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/gdb/dup", );
    g_test_add_func("/frame/gdb/parse-frame-start", );
    g_test_add_func("/frame/gdb/parseadd-operator", );
    g_test_add_func("/frame/gdb/parse-function-name", );
    g_test_add_func("/frame/gdb/parse-function-name-template-args", );
    g_test_add_func("/frame/gdb/skip-function-args", );
    g_test_add_func("/frame/gdb/parse-function-call", );
    g_test_add_func("/frame/gdb/parse-address-in-function", );
    g_test_add_func("/frame/gdb/parse-file-location", );
    g_test_add_func("/frame/gdb/parse-header", );
    g_test_add_func("/frame/gdb/parse", );

    return g_test_run();
}
