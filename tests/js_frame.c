#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/js/parse-v8", );
    g_test_add_func("/frame/js/cmp", );
    g_test_add_func("/frame/js/cmp-distance", );
    g_test_add_func("/frame/js/dup", );
    g_test_add_func("/frame/js/append", );
    g_test_add_func("/frame/js/to-json", );
    g_test_add_func("/frame/js/from-json", );
    g_test_add_func("/frame/js/append-to-str", );
    g_test_add_func("/frame/js/generic-functions", );

    return g_test_run();
}
