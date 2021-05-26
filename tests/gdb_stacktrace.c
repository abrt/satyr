#include "stacktrace.h"
#include "thread.h"
#include "gdb/frame.h"
#include "gdb/thread.h"
#include "gdb/stacktrace.h"
#include "location.h"
#include "utils.h"
#include <stdio.h>
#include <glib.h>

static void
test_gdb_stacktrace_remove_threads_except_one(void)
{
    /* Delete the thread except the middle one. */
    struct sr_gdb_thread *thread2 = sr_gdb_thread_new();
    struct sr_gdb_thread *thread1 = sr_gdb_thread_new();
    thread1->next = thread2;
    struct sr_gdb_thread *thread0 = sr_gdb_thread_new();
    thread0->next = thread1;
    struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_new();
    stacktrace->threads = thread0;
    sr_gdb_stacktrace_remove_threads_except_one(stacktrace, thread1);
    g_assert_true(stacktrace->threads == thread1);
    g_assert_null(stacktrace->threads->next);

    /* Delete all threads except one when there is only one. */
    sr_gdb_stacktrace_remove_threads_except_one(stacktrace, thread1);
    g_assert_true(stacktrace->threads == thread1);
    g_assert_null(stacktrace->threads->next);

    /* Delete all threads except the first one. */
    thread0 = sr_gdb_thread_new();
    stacktrace->threads = thread0;
    thread0->next = thread1; // already exists
    thread2 = sr_gdb_thread_new();
    thread1->next = thread2;
    sr_gdb_stacktrace_remove_threads_except_one(stacktrace, thread0);
    g_assert_true(stacktrace->threads == thread0);
    g_assert_null(stacktrace->threads->next);

    /* Delete all threads except the last one. */
    thread1 = sr_gdb_thread_new();
    thread0->next = thread1;
    thread2 = sr_gdb_thread_new();
    thread1->next = thread2;
    sr_gdb_stacktrace_remove_threads_except_one(stacktrace, thread2);
    g_assert_true(stacktrace->threads == thread2);
    g_assert_null(stacktrace->threads->next);

    sr_gdb_stacktrace_free(stacktrace);
}

static void
test_gdb_stacktrace_find_crash_thread(void)
{
    struct test_data
    {
        const char *path;
        const char *first_thread_function_name;
        uint32_t crash_tid;
    };

    struct test_data t[] = {
        /* Test the stacktrace from Red Hat Bugzilla bug #621492. */
        {"gdb_stacktraces/rhbz-621492", "raise", -1},
        /* Test the stacktrace from Red Hat Bugzilla bug #803600. */
        {"gdb_stacktraces/rhbz-803600", "validate_row", -1},
        /* Test the stacktrace from Red Hat Bugzilla bug #1032472. */
        {"gdb_stacktraces/rhbz-1032472", "g_str_hash", -1},
        /* A casual gnome-shell stack trace that has the crash frame header but does
         * not have a thread with that frame. The stacktrace has been sent by
         * a proactive user to ABRT developers
         */
        {"gdb_stacktraces/no-crash-frame-found", "process_event", 11113},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        /* Load the stacktrace from a file. */
        struct sr_location location;
        sr_location_init(&location);
        char *error_message;
        char *full_input = sr_file_to_string(t[i].path, &error_message);
        g_assert_nonnull(full_input);
        char *input = full_input;
        struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_parse((const char **)&input, &location);
        g_assert_nonnull(stacktrace);
        if (t[i].crash_tid != -1)
            sr_gdb_stacktrace_set_crash_tid(stacktrace, t[i].crash_tid);

        /* Check that the crash thread is found. */
        struct sr_gdb_thread *crash_thread = sr_gdb_stacktrace_find_crash_thread(stacktrace);
        g_assert_nonnull(crash_thread);
        g_assert_cmpstr(crash_thread->frames->function_name, ==, t[i].first_thread_function_name);

        struct sr_gdb_thread *crash_thread2 = (struct sr_gdb_thread *)sr_stacktrace_find_crash_thread((struct sr_stacktrace *)stacktrace);
        g_assert_nonnull(crash_thread2);
        g_assert_true(crash_thread == crash_thread2);

        sr_gdb_stacktrace_free(stacktrace);
        g_free(full_input);
    }
}

static void
test_gdb_stacktrace_limit_frame_depth(void)
{
    /* Load the stacktrace from Red Hat Bugzilla bug #621492. */
    struct sr_location location;
    sr_location_init(&location);
    char *error_message;
    g_autofree char *full_input = sr_file_to_string("gdb_stacktraces/rhbz-621492", &error_message);
    g_assert_nonnull(full_input);
    char *input = full_input;
    struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_parse((const char **)&input, &location);
    g_assert_nonnull(stacktrace);

    /* Check the frame depth limit. */
    sr_gdb_stacktrace_limit_frame_depth(stacktrace, 5);
    g_assert_cmpuint(sr_gdb_stacktrace_get_thread_count(stacktrace), ==, 11);
    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
      g_assert_cmpuint(sr_thread_frame_count((struct sr_thread*) thread), ==, 5);
      thread = thread->next;
    }

    sr_gdb_stacktrace_free(stacktrace);
}

static void
test_gdb_stacktrace_quality_complex(void)
{
    struct test_data
    {
        const char *path;
        const float expected_quality;
    };

    struct test_data t[] = {
        /* Test the stacktrace from Red Hat Bugzilla bug #621492. */
        {"gdb_stacktraces/rhbz-621492", 0.994703f},
        /* 100% pure BIO quality backtrace */
        {"gdb_stacktraces/quality_100", 1.0f},
        /* 89% genetically modified quality backtrace */
        {"gdb_stacktraces/quality_89", 0.89f},
        /* Rotten RHBZ#1239318 backtrace */
        {"gdb_stacktraces/rhbz-1239318", 0.0f},
        /* RHBZ#1239318 backtrace modified to have more SO files end in ".so" */
        {"gdb_stacktraces/rhbz-1239318_modified", 0.0f},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        struct sr_location location;
        sr_location_init(&location);
        char *error_message;
        g_autofree char *full_input = sr_file_to_string(t[i].path, &error_message);
        g_assert_nonnull(full_input);
        char *input = full_input;
        struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_parse((const char **)&input, &location);
        g_assert_nonnull(stacktrace);
        float quality = sr_gdb_stacktrace_quality_complex(stacktrace);
        printf("Expected quality: %f got: %f\n", t[i].expected_quality, quality);
        /* Be a little tolerant */
        g_assert_true(t[i].expected_quality > quality - 0.00001f && t[i].expected_quality < quality + 0.00001f);
        sr_gdb_stacktrace_free(stacktrace);
    }
}

static void
test_gdb_stacktrace_get_crash_frame(void)
{
    /* Check the crash frame of stacktrace from Red Hat Bugzilla bug #803600. */
    struct sr_location location;
    sr_location_init(&location);
    char *error_message;
    g_autofree char *full_input = sr_file_to_string("gdb_stacktraces/rhbz-803600", &error_message);
    g_assert_nonnull(full_input);
    char *input = full_input;
    struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_parse((const char **)&input, &location);
    g_assert_nonnull(stacktrace);
    struct sr_gdb_frame *crash_frame = sr_gdb_stacktrace_get_crash_frame(stacktrace);
    puts(crash_frame->function_name);
    g_assert_cmpstr(crash_frame->function_name, ==, "validate_row");
    sr_gdb_frame_free(crash_frame);
    sr_gdb_stacktrace_free(stacktrace);
}

static void
test_gdb_stacktrace_parse_no_thread_header(void)
{
    /* Check that satyr is able to parse backtrace with missing Thread header */
    struct sr_location location;
    sr_location_init(&location);
    char *error_message;
    g_autofree char *full_input = sr_file_to_string("gdb_stacktraces/no-thread-header", &error_message);
    g_assert_nonnull(full_input);
    char *input = full_input;
    struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_parse((const char **)&input, &location);
    g_assert_nonnull(stacktrace);
    g_assert_cmpuint(sr_gdb_stacktrace_get_thread_count(stacktrace), ==, 1);
    struct sr_gdb_thread *crash_thread = sr_gdb_stacktrace_find_crash_thread(stacktrace);
    g_assert_cmpuint(sr_thread_frame_count((struct sr_thread *)crash_thread), ==, 6);
    g_assert_cmpint(crash_thread->number, ==, 0);
    struct sr_gdb_frame *crash_frame = sr_gdb_stacktrace_get_crash_frame(stacktrace);
    puts(crash_frame->function_name);
    g_assert_cmpstr(crash_frame->function_name, ==, "write");
    sr_gdb_frame_free(crash_frame);
    sr_gdb_stacktrace_free(stacktrace);
}

static void
test_gdb_stacktrace_parse_ppc64(void)
{
    /* Check that satyr is able to parse backtrace with missing Thread header */
    struct sr_location location;
    sr_location_init(&location);
    char *error_message;
    g_autofree char *full_input = sr_file_to_string("gdb_stacktraces/rhbz-1119072", &error_message);
    g_assert_nonnull(full_input);
    char *input = full_input;
    struct sr_gdb_stacktrace *stacktrace = sr_gdb_stacktrace_parse((const char **)&input, &location);
    g_assert_nonnull(stacktrace);
    g_assert_cmpuint(sr_gdb_stacktrace_get_thread_count(stacktrace), ==, 5);
    struct sr_gdb_thread *thread = stacktrace->threads;
    g_assert_cmpuint(sr_thread_frame_count((struct sr_thread *)thread), ==, 7);
    struct sr_gdb_frame *frame = thread->frames;
    g_assert_cmpstr(frame->function_name, ==, ".pthread_cond_timedwait");
    sr_gdb_stacktrace_free(stacktrace);
}


int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/gdb/remove-threads-except-one", test_gdb_stacktrace_remove_threads_except_one);
    g_test_add_func("/stacktrace/gdb/find-crash-thread", test_gdb_stacktrace_find_crash_thread);
    g_test_add_func("/stacktrace/gdb/limit-frame-depth", test_gdb_stacktrace_limit_frame_depth);
    g_test_add_func("/stacktrace/gdb/quality-complex", test_gdb_stacktrace_quality_complex);
    g_test_add_func("/stacktrace/gdb/get-crash-frame", test_gdb_stacktrace_get_crash_frame);
    g_test_add_func("/stacktrace/gdb/parse-no-thread-header", test_gdb_stacktrace_parse_no_thread_header);
    g_test_add_func("/stacktrace/gdb/parse-ppc64", test_gdb_stacktrace_parse_ppc64);

    return g_test_run();
}
