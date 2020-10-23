#include "gdb/thread.h"
#include "gdb/frame.h"
#include "location.h"
#include "utils.h"
#include <stdio.h>
#include <glib.h>

static void
test_gdb_thread_remove_frame(void)
{
    struct sr_gdb_frame *frame2 = sr_gdb_frame_new();
    frame2->function_name = g_strdup("write");
    frame2->number = 2;
    frame2->source_file = g_strdup("gfileutils.c");
    frame2->source_line = 120;
    frame2->address = 0x322160e7fdULL;

    struct sr_gdb_frame *frame1 = sr_gdb_frame_new();
    frame1->function_name = g_strdup("write_to_temp_file");
    frame1->number = 1;
    frame1->source_file = g_strdup("gfileutils.c");
    frame1->source_line = 980;
    frame1->address = 0x322160e7fdULL;
    frame1->next = frame2;

    struct sr_gdb_frame *frame0 = sr_gdb_frame_new();
    frame0->function_name = g_strdup("fsync");
    frame0->number = 0;
    frame0->source_file = g_strdup("../sysdeps/unix/syscall-template.S");
    frame0->source_line = 82;
    frame0->address = 0x322160e7fdULL;
    frame0->next = frame1;

    struct sr_gdb_thread thread;
    sr_gdb_thread_init(&thread);
    thread.number = 3;
    thread.frames = frame0;

    /* Remove the frame from the top of the stack. */
    g_assert_true(sr_gdb_thread_remove_frame(&thread, frame0));
    g_assert_true(frame1 == thread.frames);
    g_assert_true(frame2 == thread.frames->next);

    /* Remove the frame from the bottom of the stack. */
    g_assert_true(sr_gdb_thread_remove_frame(&thread, frame2));
    g_assert_true(frame1 == thread.frames);
    g_assert_null(thread.frames->next);

    /* Remove the last remaining frame. */
    g_assert_true(sr_gdb_thread_remove_frame(&thread, frame1));
    g_assert_null(thread.frames);

    /* Remove nonexistant frame -> should return false. */
    g_assert_false(sr_gdb_thread_remove_frame(&thread, frame0));
    g_assert_null(thread.frames);
}

static void
test_gdb_thread_remove_frames_above(void)
{
    struct sr_gdb_frame *frame2 = sr_gdb_frame_new();
    frame2->function_name = g_strdup("write");
    frame2->number = 2;
    frame2->source_file = g_strdup("gfileutils.c");
    frame2->source_line = 120;
    frame2->address = 0x322160e7fdULL;

    struct sr_gdb_frame *frame1 = sr_gdb_frame_new();
    frame1->function_name = g_strdup("write_to_temp_file");
    frame1->number = 1;
    frame1->source_file = g_strdup("gfileutils.c");
    frame1->source_line = 980;
    frame1->address = 0x322160e7fdULL;
    frame1->next = frame2;

    struct sr_gdb_frame *frame0 = sr_gdb_frame_new();
    frame0->function_name = g_strdup("fsync");
    frame0->number = 0;
    frame0->source_file = g_strdup("../sysdeps/unix/syscall-template.S");
    frame0->source_line = 82;
    frame0->address = 0x322160e7fdULL;
    frame0->next = frame1;

    struct sr_gdb_thread thread;
    sr_gdb_thread_init(&thread);
    thread.number = 3;
    thread.frames = frame0;

    /* Remove the frames above the top of the stack. */
    g_assert_true(sr_gdb_thread_remove_frames_above(&thread, frame0));
    g_assert_true(frame0 == thread.frames);
    g_assert_true(frame1 == thread.frames->next);

    /* Remove the frames above the second frame from the top of the stack. */
    g_assert_true(sr_gdb_thread_remove_frames_above(&thread, frame1));
    g_assert_true(frame1 == thread.frames);
    g_assert_true(frame2 == thread.frames->next);

    /* Remove the frames above the bottom of the stack. */
    g_assert_true(sr_gdb_thread_remove_frames_above(&thread, frame2));
    g_assert_true(frame2 == thread.frames);
    g_assert_null(thread.frames->next);

    /* Remove frames above a nonexistant frame -> should return false. */
    g_assert_false(sr_gdb_thread_remove_frames_above(&thread, frame0));
    g_assert_true(frame2 == thread.frames);
}

static void
test_gdb_thread_remove_frames_below_n(void)
{
    struct sr_gdb_frame *frame2 = sr_gdb_frame_new();
    frame2->function_name = g_strdup("write");
    frame2->number = 2;
    frame2->source_file = g_strdup("gfileutils.c");
    frame2->source_line = 120;
    frame2->address = 0x322160e7fdULL;

    struct sr_gdb_frame *frame1 = sr_gdb_frame_new();
    frame1->function_name = g_strdup("write_to_temp_file");
    frame1->number = 1;
    frame1->source_file = g_strdup("gfileutils.c");
    frame1->source_line = 980;
    frame1->address = 0x322160e7fdULL;
    frame1->next = frame2;

    struct sr_gdb_frame *frame0 = sr_gdb_frame_new();
    frame0->function_name = g_strdup("fsync");
    frame0->number = 0;
    frame0->source_file = g_strdup("../sysdeps/unix/syscall-template.S");
    frame0->source_line = 82;
    frame0->address = 0x322160e7fdULL;
    frame0->next = frame1;

    struct sr_gdb_thread thread;
    sr_gdb_thread_init(&thread);
    thread.number = 3;
    thread.frames = frame0;

    /* Remove no frame as n is too large. */
    sr_gdb_thread_remove_frames_below_n(&thread, 5);
    g_assert_true(frame0 == thread.frames);
    g_assert_true(frame1 == thread.frames->next);
    g_assert_true(frame2 == thread.frames->next->next);
    g_assert_null(thread.frames->next->next->next);

    /* Remove the frames below 1. */
    sr_gdb_thread_remove_frames_below_n(&thread, 1);
    g_assert_true(frame0 == thread.frames);
    g_assert_null(thread.frames->next);

    /* Remove the frames below 0. */
    sr_gdb_thread_remove_frames_below_n(&thread, 0);
    g_assert_null(thread.frames);

    /* Try to remove frames when no frame is present. */
    sr_gdb_thread_remove_frames_below_n(&thread, 4);
    g_assert_null(thread.frames);
}

static void
test_gdb_thread_parse(void)
{
    struct test_data
    {
        char *input;
        struct sr_gdb_thread *expected_thread;
    };

    /* Basic test. */
    struct sr_gdb_frame frame0, frame1;
    sr_gdb_frame_init(&frame0);
    frame0.function_name = "fsync";
    frame0.number = 0;
    frame0.source_file = "../sysdeps/unix/syscall-template.S";
    frame0.source_line = 82;
    frame0.address = 0x322160e7fdULL;
    frame0.next = &frame1;

    sr_gdb_frame_init(&frame1);
    frame1.function_name = "write_to_temp_file";
    frame1.number = 1;
    frame1.source_file = "gfileutils.c";
    frame1.source_line = 980;
    frame1.address = 0x322160e7fdULL;

    struct sr_gdb_thread thread1;
    sr_gdb_thread_init(&thread1);
    thread1.number = 3;
    thread1.frames = &frame0;

    struct sr_gdb_thread thread2;
    sr_gdb_thread_init(&thread2);
    thread2.number = 3;
    thread2.frames = &frame0;
    thread2.tid = 6357;

    struct sr_gdb_thread thread3;
    sr_gdb_thread_init(&thread3);
    thread3.number = 3;
    thread3.frames = &frame0;
    thread3.tid = 635;

    struct test_data t[] = {
        {"Thread 3 (Thread 11917):\n"
         "#0  0x000000322160e7fd in fsync () at ../sysdeps/unix/syscall-template.S:82\n"
         "No locals.\n"
         "#1  0x000000322222987a in write_to_temp_file (\n"
         "    filename=0x18971b0 \"/home/jfclere/.recently-used.xbel\", \n"
         "    contents=<value optimized out>, length=29917, error=0x7fff3cbe4110)\n"
         "    at gfileutils.c:980\n"
         "        statbuf = {st_dev = 64768, st_ino = 13709, st_nlink = 1, \n"
         "          st_mode = 33152, st_uid = 500, st_gid = 500, __pad0 = 0, \n"
         "          st_rdev = 0, st_size = 29917, st_blksize = 4096, st_blocks = 64, \n"
         "          st_atim = {tv_sec = 1273231242, tv_nsec = 73521863}, st_mtim = {\n"
         "            tv_sec = 1273231242, tv_nsec = 82521015}, st_ctim = {\n"
         "            tv_sec = 1273231242, tv_nsec = 190522021}, __unused = {0, 0, 0}}\n",
         &thread1},
        {"Thread 3 (Thread 0xdd119170 (LWP 6357)):\n"
         "#0  0x000000322160e7fd in fsync () at ../sysdeps/unix/syscall-template.S:82\n"
         "No locals.\n"
         "#1  0x000000322222987a in write_to_temp_file () at gfileutils.c:980\n"
         "No locals.\n",
         &thread2},
        {"Thread 3 (LWP 635):\n"
         "#0  0x000000322160e7fd in fsync () at ../sysdeps/unix/syscall-template.S:82\n"
         "No locals.\n"
         "#1  0x000000322222987a in write_to_temp_file () at gfileutils.c:980\n"
         "No locals.\n",
         &thread3},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        struct sr_location location;
        sr_location_init(&location);
        struct sr_gdb_thread *thread = sr_gdb_thread_parse((const char **)&t[i].input, &location);
        g_assert_true(!t[i].expected_thread || thread);
        if (thread)
        {
            g_assert_cmpint(*t[i].input, ==, '\0');
            g_assert_cmpint(sr_gdb_thread_cmp(thread, t[i].expected_thread), ==, 0);
            sr_gdb_thread_free(thread);
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
            g_assert_null(t[i].expected_thread);
        }
    }
}

static void
test_gdb_thread_parse_locations(void)
{
    struct test_data
    {
        char *input;
        int expected_line;
        int expected_column;
    };

    struct test_data t[] = {
        /* Thread keyword */
        {"Thraed 3 (Thread 11917):\n", 1, 0},

        /* Spaces after the thread keyword. */
        {"Thread3 (Thread 11917):\n", 1, 6},

        /* Thread number. */
        {"Thread  a (Thread 11917):\n", 1, 8},

        /* Spaces after the thread number. */
        {"Thread 8(Thread 11917):\n", 1, 8},

        /* A parenthesis. */
        {"Thread 8 (11917):\n", 1, 9},

        /* Second number. */
        {"Thread 8 (Thread ffff):\n", 1, 17},

        /* Semicolon missing. */
        {"Thread 8 (Thread 1)\n", 1, 18},

        /* Not a single frame. */
        {"Thread 3 (Thread 11917):\n\n", 2, 0},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        struct sr_location location;
        sr_location_init(&location);
        g_assert_null(sr_gdb_thread_parse((const char **)&t[i].input, &location));

        /* Check that the pointer is not moved. */
        g_assert_true(old_input == t[i].input);

        /* Check the location. */
        printf("location %d:%d '%s', expected %d:%d\n",
               location.line, location.column, location.message,
               t[i].expected_line, t[i].expected_column);

        g_assert_cmpuint(location.line, ==, t[i].expected_line);
        g_assert_cmpuint(location.column, ==, t[i].expected_column);
        g_assert_nonnull(location.message);
        g_assert_cmpint(location.message[0], !=, '\0');
    }
}

static void
test_gdb_thread_parse_lwp(void)
{
    struct test_data
    {
        char *input;
        int expected_count;
        uint32_t expected_tid;
    };

    struct test_data t[] = {
        {"(LWP 20)", 8, 20},
        {"(LWP 20)10", 8, 20},
        {"(LWP 3454343450)", 16, 3454343450},
        {"(LWP 2454434523)  ", 16, 2454434523},
        {"", 0, -1},
        {" ", 0, -1},
        {" (LWP 20)", 0, -1},
        {"(LWP", 0, -1},
        {"(LWP 20", 0, -1},
        {"(LWP )", 0, -1},
        {"(LWP 20()", 0, -1},
        {"(LWP 0x0)", 0, -1},
        {"(LWP 20(", 0, -1},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        uint32_t tid = -1;
        g_assert_cmpuint(t[i].expected_count, ==, sr_gdb_thread_parse_lwp((const char **)&t[i].input, &tid));
        g_assert_cmpuint(t[i].input - old_input, ==, t[i].expected_count);
        g_assert_cmpuint(tid, ==, t[i].expected_tid);
    }
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/thread/gdb/remove-frame", test_gdb_thread_remove_frame);
    g_test_add_func("/thread/gdb/remove-frames-above", test_gdb_thread_remove_frames_above);
    g_test_add_func("/thread/gdb/remove-frames-below-n", test_gdb_thread_remove_frames_below_n);
    g_test_add_func("/thread/gdb/parse", test_gdb_thread_parse);
    g_test_add_func("/thread/gdb/parse-locations", test_gdb_thread_parse_locations);
    g_test_add_func("/thread/gdb/parse-lwp", test_gdb_thread_parse_lwp);

    return g_test_run();
}
