#include "java_testcases.c"

#include <java/stacktrace.h>
#include <java/thread.h>
#include <glib.h>
#include <location.h>
#include <stacktrace.h>

void
test_java_stacktrace_cmp(void)
{
    struct sr_java_stacktrace *stacktraces[2];

    for (size_t i = 0; i < G_N_ELEMENTS(stacktraces); i++)
    {
        stacktraces[i] = sr_java_stacktrace_new();
        stacktraces[i]->threads = create_real_main_thread_objects();
    }

    g_assert_cmpint(sr_java_stacktrace_cmp(stacktraces[0], stacktraces[1]), ==, 0);

    sr_java_thread_free(stacktraces[1]->threads);

    stacktraces[1]->threads = NULL;

    g_assert_cmpint(sr_java_stacktrace_cmp(stacktraces[0], stacktraces[1]), !=, 0);

    for (size_t i = 0; i < G_N_ELEMENTS(stacktraces); i++)
    {
        sr_java_stacktrace_free(stacktraces[i]);
    }
}

void
test_java_stacktrace_dup(void)
{
    struct sr_java_stacktrace *stacktrace;
    struct sr_java_stacktrace *dupe_stacktrace;

    stacktrace = sr_java_stacktrace_new();

    stacktrace->threads = create_real_main_thread_objects();

    dupe_stacktrace = sr_java_stacktrace_dup(stacktrace);

    g_assert_cmpint(sr_java_stacktrace_cmp(stacktrace, dupe_stacktrace), ==, 0);

    sr_java_stacktrace_free(dupe_stacktrace);
    sr_java_stacktrace_free(stacktrace);
}

static void
test_java_stacktrace_parse(void)
{
    struct sr_java_stacktrace *stacktrace;
    const char *input;
    const char *cursor;
    struct sr_location location;
    struct sr_java_stacktrace *parsed_java_stacktrace;

    stacktrace = sr_java_stacktrace_new();
    input = get_real_thread_stacktrace();
    cursor = input;

    stacktrace->threads = create_real_main_thread_objects();

    sr_location_init(&location);

    parsed_java_stacktrace = sr_java_stacktrace_parse(&cursor, &location);

    g_assert_true(NULL == stacktrace || NULL != parsed_java_stacktrace);

    if (NULL != parsed_java_stacktrace)
    {
        char *error;
        g_autofree char *json1 = NULL;
        struct sr_stacktrace *parsed_stacktrace;
        g_autofree char *json2 = NULL;

        g_assert_cmpint(*cursor, ==, '\0');
        g_assert_cmpint(sr_java_stacktrace_cmp(parsed_java_stacktrace, stacktrace), ==, 0);

        json1 = sr_stacktrace_to_json((struct sr_stacktrace *) parsed_java_stacktrace);
        g_assert_nonnull(json1);

        sr_java_stacktrace_free(parsed_java_stacktrace);

        parsed_stacktrace = sr_stacktrace_parse(SR_REPORT_JAVA, input, &error);
        g_assert_nonnull(parsed_stacktrace);

        json2 = sr_stacktrace_to_json(parsed_stacktrace);
        g_assert_nonnull(json2);

        g_assert_cmpstr(json1, ==, json2);

        sr_stacktrace_free(parsed_stacktrace);

    }
    else
    {
        /* Check that the pointer is not moved. */
        g_assert_true(cursor == input);
        g_assert_nonnull(stacktrace);
    }

    sr_java_stacktrace_free(stacktrace);
}

static void
test_java_stacktrace_reason(void)
{
    char *error = NULL;
    g_autofree char *input = NULL;
    const char *cursor;
    struct sr_location location;
    struct sr_java_stacktrace *stacktrace;
    g_autofree char *reason = NULL;

    input = sr_file_to_string("java_stacktraces/java-01", &error);
    cursor = input;

    g_assert_nonnull(input);
    g_assert_null(error);

    stacktrace = sr_java_stacktrace_parse(&cursor, &location);

    g_assert_nonnull(stacktrace);

    reason = sr_java_stacktrace_get_reason(stacktrace);

    g_assert_nonnull(reason);

    sr_java_stacktrace_free(stacktrace);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/java/cmp", test_java_stacktrace_cmp);
    g_test_add_func("/stacktrace/java/dup", test_java_stacktrace_dup);
    g_test_add_func("/stacktrace/java/parse", test_java_stacktrace_parse);
    g_test_add_func("/stacktrace/java/reason", test_java_stacktrace_reason);

    return g_test_run();
}
