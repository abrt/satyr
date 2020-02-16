#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/js/engine-to-string", );
    g_test_add_func("/js/engine-from-string", );
    g_test_add_func("/js/runtime-to-string", );
    g_test_add_func("/js/runtime-from-string", );
    g_test_add_func("/js/platform-basics", );
    g_test_add_func("/js/platform-from-string", );
    g_test_add_func("/js/platform-to-json", );
    g_test_add_func("/js/platform-from-json", );
    g_test_add_func("/js/platform-parse-frame", );

    return g_test_run();
}
