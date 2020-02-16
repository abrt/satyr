#include <glib.h>

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/thread/gdb/remove-frame", );
    g_test_add_func("/thread/gdb/remove-frames-above", );
    g_test_add_func("/thread/gdb/remove-frames-below-n", );
    g_test_add_func("/thread/gdb/parse", );
    g_test_add_func("/thread/gdb/parse-locations", );
    g_test_add_func("/thread/gdb/parse-lwp", );

    return g_test_run();
}
