#include <glib.h>
#include <java/thread.h>
#include <location.h>
#include <thread.h>

#include "java_testcases.c"

static void
test_java_thread_cmp(void)
{
    struct sr_java_thread *threads[2];

    threads[0] = create_real_main_thread_objects();
    threads[1] = create_real_main_thread_objects();

    g_assert_cmpint(sr_java_thread_cmp(threads[0], threads[1]), ==, 0);
    g_assert_cmpint(sr_thread_cmp((struct sr_thread *)threads[0],
                                  (struct sr_thread *)threads[1]),
                    ==,
                    0);

    g_clear_pointer(&threads[1]->frames, sr_java_frame_free_full);

    g_assert_cmpint(sr_java_thread_cmp(threads[0], threads[1]), !=, 0);
    g_assert_cmpint(sr_thread_cmp((struct sr_thread *)threads[0],
                                  (struct sr_thread *)threads[1]),
                    !=,
                    0);

    sr_java_thread_free(threads[1]);

    threads[1] = create_real_main_thread_objects();

    free(threads[1]->name);
    threads[1]->name = g_strdup("worker");

    g_assert_cmpint(sr_java_thread_cmp(threads[0], threads[1]), !=, 0);
    g_assert_cmpint(sr_thread_cmp((struct sr_thread *)threads[0],
                                  (struct sr_thread *)threads[1]),
                    !=,
                    0);

    for (size_t i = 0; i < G_N_ELEMENTS(threads); i++)
    {
        sr_java_thread_free(threads[i]);
    }
}

static void
test_java_thread_dup(void)
{
    struct sr_java_thread *threads[2];

    threads[0] = create_real_main_thread_objects();
    threads[1] = sr_java_thread_dup(threads[0], false);

    g_assert_cmpint(sr_java_thread_cmp(threads[0], threads[1]), ==, 0);

    for (size_t i = 0; i < G_N_ELEMENTS(threads); i++)
    {
        sr_java_thread_free(threads[i]);
    }
}

static void
test_java_thread_remove_frame(void)
{
    struct sr_java_thread thread;
    struct sr_java_frame *frame;

    sr_java_thread_init(&thread);

    frame = create_real_stacktrace_objects();

    thread.frames = frame;

    g_assert_true(sr_java_thread_remove_frame(&thread, frame));
    g_assert_false(frame == thread.frames);

    frame = sr_java_frame_get_last(thread.frames);

    g_assert_true(sr_java_thread_remove_frame(&thread, frame));
    g_assert_false(frame == sr_java_frame_get_last(thread.frames));

    frame = thread.frames->next;

    g_assert_true(sr_java_thread_remove_frame(&thread, frame));
    g_assert_false(frame == thread.frames->next);

    g_assert_cmpint(sr_java_thread_remove_frame(&thread, frame), ==, 0);

    sr_java_frame_free_full(thread.frames);
}

static void
test_java_thread_remove_frames_above(void)
{
    struct sr_java_thread thread;
    struct sr_java_frame *top;
    struct sr_java_frame *above;

    sr_java_thread_init(&thread);

    thread.frames = create_real_stacktrace_objects();

    top = thread.frames;

    g_assert_true(sr_java_thread_remove_frames_above(&thread, top));
    g_assert_true(top == thread.frames);

    top = thread.frames->next;

    g_assert_true(sr_java_thread_remove_frames_above(&thread, top));
    g_assert_true(top == thread.frames);

    above = thread.frames;
    top = sr_java_frame_get_last(thread.frames);

    g_assert_true(sr_java_thread_remove_frames_above(&thread, top));
    g_assert_true(top == thread.frames);

    g_assert_cmpint(sr_java_thread_remove_frames_above(&thread, above), ==, 0);

    sr_java_frame_free_full(thread.frames);
}

static void
test_java_thread_remove_frames_below_n(void)
{
    struct sr_java_thread thread;
    struct sr_java_frame *bottom;

    sr_java_thread_init(&thread);

    thread.frames = create_real_stacktrace_objects();

    bottom = sr_java_frame_get_last(thread.frames);

    /* Remove below too large */
    sr_java_thread_remove_frames_below_n(&thread, 100);

    g_assert_false(bottom == thread.frames);
    g_assert_true(bottom == sr_java_frame_get_last(thread.frames));

    bottom = thread.frames->next;

    /* Remove below the second frame */
    sr_java_thread_remove_frames_below_n(&thread, 2);

    g_assert_nonnull(thread.frames);
    g_assert_true(bottom == thread.frames->next);
    g_assert_null(thread.frames->next->next);

    /* Remove below 'over the top' frame */
    sr_java_thread_remove_frames_below_n(&thread, 0);

    g_assert_null(thread.frames);

    sr_java_frame_free_full(thread.frames);
}

static void
test_java_thread_parse(void)
{
    const char *input;
    const char *old_input;
    struct sr_location location;
    struct sr_java_thread *parsed_thread;
    struct sr_java_thread *expected_thread;

    sr_location_init(&location);

    input = get_real_thread_stacktrace();
    old_input = input;
    parsed_thread = sr_java_thread_parse(&input, &location);
    expected_thread = create_real_main_thread_objects();

    if (NULL != parsed_thread)
    {
        GString *output;

        output = g_string_new(NULL);

        sr_java_thread_append_to_str(parsed_thread, output);

        g_string_free(output, TRUE);

        g_assert_cmpint(*input, ==, '\0');
        g_assert_cmpint(sr_java_thread_cmp(parsed_thread, expected_thread), ==, 0);

        sr_java_thread_free(parsed_thread);
        sr_java_thread_free(expected_thread);
    }
    else
    {
        /* Check that the pointer is not moved. */
        g_assert_true(old_input == input);
        g_assert_null(expected_thread);
    }
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/java/thread/cmp", test_java_thread_cmp);
    g_test_add_func("/java/thread/dup", test_java_thread_dup);
    g_test_add_func("/java/thread/remove-frame", test_java_thread_remove_frame);
    g_test_add_func("/java/thread/remove-frames-above",
                    test_java_thread_remove_frames_above);
    g_test_add_func("/java/thread/remove-frames-below-n",
                    test_java_thread_remove_frames_below_n);
    g_test_add_func("/java/thread/parse", test_java_thread_parse);

    return g_test_run();
}
