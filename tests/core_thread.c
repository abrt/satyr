#include <core/frame.h>
#include <core/thread.h>
#include <thread.h>
#include <utils.h>

#include <glib.h>

static const char *test_json =
    "{   \"frames\":\n"
    "      [ {   \"address\": 68719476721\n"
    "        ,   \"build_id\": \"aabbccddeeff1\"\n"
    "        ,   \"build_id_offset\": 2561\n"
    "        ,   \"function_name\": \"test1\"\n"
    "        ,   \"file_name\": \"executable1\"\n"
    "        ,   \"fingerprint\": \"ab\"\n"
    "        }\n"
    "      , {   \"address\": 68719476720\n"
    "        ,   \"build_id\": \"aabbccddeeff0\"\n"
    "        ,   \"build_id_offset\": 2560\n"
    "        ,   \"function_name\": \"test0\"\n"
    "        ,   \"file_name\": \"executable0\"\n"
    "        } ]\n"
    "}";

static void
test_core_thread_to_json(void)
{
    struct sr_core_frame *frames[2];
    struct sr_core_thread *thread;
    g_autofree char *json = NULL;

    frames[0] = sr_core_frame_new();
    frames[1] = sr_core_frame_new();
    thread = sr_core_thread_new();

    frames[0]->address = 0xffffffff0;
    frames[0]->build_id = sr_strdup("aabbccddeeff0");
    frames[0]->build_id_offset = 2560;
    frames[0]->function_name = sr_strdup("test0");
    frames[0]->file_name = sr_strdup("executable0");

    frames[1]->address = 0xffffffff1;
    frames[1]->build_id = sr_strdup("aabbccddeeff1");
    frames[1]->build_id_offset = 2561;
    frames[1]->function_name = sr_strdup("test1");
    frames[1]->file_name = sr_strdup("executable1");
    frames[1]->fingerprint = sr_strdup("ab");
    frames[1]->next = frames[0];

    thread->frames = frames[1];

    json = sr_core_thread_to_json(thread, false);

    g_assert_cmpstr(json, ==, test_json);

    sr_core_thread_free(thread);
}

static void
test_core_thread_abstract_functions(void)
{
    struct sr_core_frame *frames[2];
    struct sr_core_thread *thread;
    struct sr_frame *thread_frames;
    struct sr_thread *next_thread;
    int frame_count;

    frames[0] = sr_core_frame_new();
    frames[1] = sr_core_frame_new();
    thread = sr_core_thread_new();

    frames[0]->address = 0xffffffff0;
    frames[0]->build_id = sr_strdup("aabbccddeeff0");
    frames[0]->build_id_offset = 2560;
    frames[0]->function_name = sr_strdup("test0");
    frames[0]->file_name = sr_strdup("executable0");

    frames[1]->address = 0xffffffff1;
    frames[1]->build_id = sr_strdup("aabbccddeeff1");
    frames[1]->build_id_offset = 2561;
    frames[1]->function_name = sr_strdup("test1");
    frames[1]->file_name = sr_strdup("executable1");
    frames[1]->fingerprint = sr_strdup("ab");
    frames[1]->next = frames[0];

    thread->frames = frames[1];

    thread_frames = sr_thread_frames((struct sr_thread*) thread);
    next_thread = sr_thread_next((struct sr_thread*) thread);
    frame_count = sr_thread_frame_count((struct sr_thread*) thread);

    g_assert_true(thread_frames == (struct sr_frame *)thread->frames);
    g_assert_true(next_thread == (struct sr_thread *)thread->next);
    g_assert_cmpint(frame_count, ==, 2);

    sr_core_thread_free(thread);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/thread/core/to-json", test_core_thread_to_json);
    g_test_add_func("/thread/core/abstract-functions", test_core_thread_abstract_functions);

    return g_test_run();
}
