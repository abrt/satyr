#include <gdb/frame.h>
#include <gdb/thread.h>
#include <normalize.h>
#include <utils.h>

#include <glib.h>

static struct sr_gdb_thread *
create_thread(int count,
              ...)
{
    struct sr_gdb_thread *thread;
    va_list argp;

    thread = sr_gdb_thread_new();

    if (0 == count)
    {
       return thread;
    }

    va_start(argp, count);

    for (int i = 0; i < count; i++)
    {
        struct sr_gdb_frame *frame;
        const char *fname;

        fname = va_arg(argp, const char*);
        frame = sr_gdb_frame_new();

        frame->function_name = g_strdup(fname);
        frame->source_file = g_strdup("");

        if (NULL == thread->frames)
        {
            thread->frames = frame;
        }
        else
        {
            sr_gdb_frame_append(thread->frames, frame);
        }
    }

    va_end(argp);

    return thread;
}

static void
test_normalize_gdb_thread(void)
{
    struct sr_gdb_thread *threads[3];
    struct sr_gdb_frame *frame;

    threads[0] = create_thread(5, "aa", "abort", "raise", "__assert_fail_base", "ee");

    threads[1] = create_thread(5, "aa", "abort", "bb", "cc", "dd");
    g_free(threads[1]->frames->next->source_file);
    threads[1]->frames->next->source_file = g_strdup("abort.c");

    threads[2] = create_thread(5, "aa", "abort", "raise", "dd", "ee");
    g_free(threads[2]->frames->next->source_file);
    threads[2]->frames->next->source_file = g_strdup("abort.c");
    g_free(threads[2]->frames->next->next->source_file);
    threads[2]->frames->next->next->source_file = g_strdup("libc.so");

    sr_normalize_gdb_thread(threads[0]);
    sr_normalize_gdb_thread(threads[1]);
    sr_normalize_gdb_thread(threads[2]);

    g_assert_cmpstr(threads[0]->frames->function_name, ==, "ee");
    g_assert_cmpstr(threads[1]->frames->function_name, ==, "bb");
    g_assert_cmpstr(threads[2]->frames->function_name, ==, "dd");

    sr_gdb_thread_free(threads[0]);
    sr_gdb_thread_free(threads[1]);
    sr_gdb_thread_free(threads[2]);
}

static void
test_normalize_gdb_thread_remove_zeroes(void)
{
    struct sr_gdb_frame *frames[3];
    struct sr_gdb_thread thread;

    frames[0] = sr_gdb_frame_new();
    frames[1] = sr_gdb_frame_new();
    frames[2] = sr_gdb_frame_new();

    frames[0]->function_name = g_strdup("??");
    frames[0]->number = 2;
    frames[0]->address = 0x0000;

    frames[1]->function_name = g_strdup("write_to_temp_file");
    frames[1]->number = 1;
    frames[1]->source_file = g_strdup("gfileutils.c");
    frames[1]->source_line = 980;
    frames[1]->address = 0x322160e7fdULL;
    frames[1]->next = frames[0];

    frames[2]->function_name = g_strdup("??");
    frames[2]->number = 0;
    frames[2]->address = 0x0000;
    frames[2]->next = frames[1];

    sr_gdb_thread_init(&thread);

    thread.number = 0;
    thread.frames = frames[2];

    sr_normalize_gdb_thread(&thread);

    g_assert_true(thread.frames == frames[1]);
    g_assert_null(thread.frames->next);

    sr_gdb_frame_free(frames[1]);
}

static void
test_normalize_gdb_paired_unknown_function_names_1(void)
{
    struct sr_gdb_thread *threads[6];

    threads[0] = create_thread(5, "aa", "??", "??", "??", "??");
    threads[1] = create_thread(5, "bb", "aa", "??", "??", "??");
    threads[2] = create_thread(5, "aa", "??", "??", "??", "??");
    threads[3] = create_thread(5, "bb", "aa", "??", "??", "??");
    threads[4] = sr_gdb_thread_dup(threads[0], false);
    threads[5] = sr_gdb_thread_dup(threads[1], false);

    sr_normalize_gdb_paired_unknown_function_names(threads[4], threads[5]);

    g_assert_false(sr_gdb_thread_cmp(threads[2], threads[4]));
    g_assert_false(sr_gdb_thread_cmp(threads[3], threads[5]));

    sr_gdb_thread_free(threads[4]);
    sr_gdb_thread_free(threads[5]);

    threads[4] = sr_gdb_thread_dup(threads[0], false);
    threads[5] = sr_gdb_thread_dup(threads[1], false);

    sr_normalize_gdb_paired_unknown_function_names(threads[5], threads[4]);

    g_assert_false(sr_gdb_thread_cmp(threads[2], threads[4]));
    g_assert_false(sr_gdb_thread_cmp(threads[3], threads[5]));

    sr_gdb_thread_free(threads[0]);
    sr_gdb_thread_free(threads[1]);
    sr_gdb_thread_free(threads[2]);
    sr_gdb_thread_free(threads[3]);
    sr_gdb_thread_free(threads[4]);
    sr_gdb_thread_free(threads[5]);
}

static void
test_normalize_gdb_paired_unknown_function_names_2(void)
{
    struct sr_gdb_thread *threads[6];

    threads[0] = create_thread(5, "aa", "??", "cc", "cc", "??");
    threads[1] = create_thread(5, "bb", "aa", "??", "cc", "??");
    threads[2] = create_thread(5, "aa", "__unknown_function_0", "cc", "cc", "__unknown_function_1");
    threads[3] = create_thread(5, "bb", "aa","__unknown_function_0", "cc", "__unknown_function_1");
    threads[4] = sr_gdb_thread_dup(threads[0], false);
    threads[5] = sr_gdb_thread_dup(threads[1], false);

    sr_normalize_gdb_paired_unknown_function_names(threads[4], threads[5]);

    g_assert_false(sr_gdb_thread_cmp(threads[2], threads[4]));
    g_assert_false(sr_gdb_thread_cmp(threads[3], threads[5]));

    sr_gdb_thread_free(threads[0]);
    sr_gdb_thread_free(threads[1]);
    sr_gdb_thread_free(threads[2]);
    sr_gdb_thread_free(threads[3]);
    sr_gdb_thread_free(threads[4]);
    sr_gdb_thread_free(threads[5]);
}

static void
test_normalize_gdb_paired_unknown_function_names_3(void)
{
    struct sr_gdb_thread *threads[6];

    threads[0] = create_thread(5, "aa", "??", "cc", "??", "dd");
    threads[1] = create_thread(5, "bb", "??", "cc", "??", "dd");
    threads[2] = create_thread(5, "aa", "??", "cc", "__unknown_function_0", "dd");
    threads[3] = create_thread(5, "bb", "??", "cc", "__unknown_function_0", "dd");
    threads[4] = sr_gdb_thread_dup(threads[0], false);
    threads[5] = sr_gdb_thread_dup(threads[1], false);

    sr_normalize_gdb_paired_unknown_function_names(threads[4], threads[5]);

    g_assert_false(sr_gdb_thread_cmp(threads[2], threads[4]));
    g_assert_false(sr_gdb_thread_cmp(threads[3], threads[5]));

    sr_gdb_thread_free(threads[0]);
    sr_gdb_thread_free(threads[1]);
    sr_gdb_thread_free(threads[2]);
    sr_gdb_thread_free(threads[3]);
    sr_gdb_thread_free(threads[4]);
    sr_gdb_thread_free(threads[5]);
}

static void
test_normalize_gdb_paired_unknown_function_names_4(void)
{
    struct sr_gdb_thread *threads[6];

    threads[0] = create_thread(5, "aa", "cc", "??", "??", "??");
    threads[1] = create_thread(5, "bb", "??", "??", "??", "??");
    threads[2] = create_thread(5, "aa", "cc", "??", "??", "??");
    threads[3] = create_thread(5, "bb", "??", "??", "??", "??");
    threads[4] = sr_gdb_thread_dup(threads[0], false);
    threads[5] = sr_gdb_thread_dup(threads[1], false);

    sr_normalize_gdb_paired_unknown_function_names(threads[4], threads[5]);

    g_assert_false(sr_gdb_thread_cmp(threads[2], threads[4]));
    g_assert_false(sr_gdb_thread_cmp(threads[3], threads[5]));

    sr_gdb_thread_free(threads[4]);
    sr_gdb_thread_free(threads[5]);

    threads[4] = sr_gdb_thread_dup(threads[0], false);
    threads[5] = sr_gdb_thread_dup(threads[1], false);

    sr_normalize_gdb_paired_unknown_function_names(threads[5], threads[4]);

    g_assert_false(sr_gdb_thread_cmp(threads[2], threads[4]));
    g_assert_false(sr_gdb_thread_cmp(threads[3], threads[5]));

    sr_gdb_thread_free(threads[0]);
    sr_gdb_thread_free(threads[1]);
    sr_gdb_thread_free(threads[2]);
    sr_gdb_thread_free(threads[3]);
    sr_gdb_thread_free(threads[4]);
    sr_gdb_thread_free(threads[5]);
}

static void
test_normalize_gdb_thread_java_frames(void)
{
    struct sr_gdb_frame *frames[4];
    struct sr_gdb_thread thread;

    frames[0] = sr_gdb_frame_new();
    frames[1] = sr_gdb_frame_new();
    frames[2] = sr_gdb_frame_new();
    frames[3] = sr_gdb_frame_new();

    frames[0]->function_name = g_strdup("jthread_map_push");
    frames[0]->number = 3;
    frames[0]->source_file = g_strdup("jthread_map.c");
    frames[0]->source_line = 110;
    frames[0]->address = 0x7fda9a03f920ULL;
    frames[0]->next = NULL;

    frames[1]->function_name = g_strdup("JVM_handle_linux_signal");
    frames[1]->number = 2;
    frames[1]->source_file = g_strdup("os_linux_x86.cpp");
    frames[1]->source_line = 531;
    frames[1]->address = 0x7fda9af3106fULL;
    frames[1]->next = frames[0];

    frames[2]->function_name = g_strdup("VMError::report_and_die");
    frames[2]->number = 1;
    frames[2]->source_file = g_strdup("vmError.cpp");
    frames[2]->source_line = 1053;
    frames[2]->address = 0x7fda9b0a83efULL;
    frames[2]->next = frames[1];

    frames[3]->function_name = g_strdup("os::abort");
    frames[3]->number = 0;
    frames[3]->source_file = g_strdup("os_linux.cpp");
    frames[3]->source_line = 1594;
    frames[3]->address = 0x7fda9af29259ULL;
    frames[3]->next = frames[2];

    sr_gdb_thread_init(&thread);

    thread.number = 0;
    thread.frames = frames[3];

    sr_normalize_gdb_thread(&thread);

    g_assert_false(thread.frames == frames[3]);
    g_assert_false(thread.frames == frames[2]);
    g_assert_false(thread.frames == frames[1]);
    g_assert_true(thread.frames == frames[0]);
    g_assert_null(thread.frames->next);

    sr_gdb_frame_free(frames[0]);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/thread/gdb/normalize", test_normalize_gdb_thread);
    g_test_add_func("/thread/gdb/normalize/remove-zeroes", test_normalize_gdb_thread_remove_zeroes);
    g_test_add_func("/thread/gdb/normalize/paired-unknown-function-names-1", test_normalize_gdb_paired_unknown_function_names_1);
    g_test_add_func("/thread/gdb/normalize/paired-unknown-function-names-2", test_normalize_gdb_paired_unknown_function_names_2);
    g_test_add_func("/thread/gdb/normalize/paired-unknown-function-names-3", test_normalize_gdb_paired_unknown_function_names_3);
    g_test_add_func("/thread/gdb/normalize/paired-unknown-function-names-4", test_normalize_gdb_paired_unknown_function_names_4);
    g_test_add_func("/thread/gdb/normalize/jvm-frames", test_normalize_gdb_thread_java_frames);

    return g_test_run();
}
