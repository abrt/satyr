#include <core/frame.h>
#include <frame.h>
#include <strbuf.h>
#include <utils.h>

#include <glib.h>

static char const *test_json =
    "{   \"address\": 68719476721\n"
     ",   \"build_id\": \"aabbccddeeff1\"\n"
     ",   \"build_id_offset\": 2561\n"
     ",   \"function_name\": \"test1\"\n"
     ",   \"file_name\": \"executable1\"\n"
     ",   \"fingerprint\": \"ab\"\n"
     "}";

static void
test_core_frame_dup(void)
{
    struct sr_core_frame *frames[2];
    struct sr_core_frame *frame;

    frames[0] = sr_core_frame_new();
    frames[1] = sr_core_frame_new();

    frames[0]->address = 0xffffffff1;
    frames[0]->build_id = sr_strdup("aabbccddeeff1");
    frames[0]->build_id_offset = 2561;
    frames[0]->function_name = sr_strdup("test1");
    frames[0]->file_name = sr_strdup("executable1");
    frames[0]->fingerprint = sr_strdup("ab");

    frames[1]->address = 0xffffffff0;
    frames[1]->build_id = sr_strdup("aabbccddeeff0");
    frames[1]->build_id_offset = 2560;
    frames[1]->function_name = sr_strdup("test0");
    frames[1]->file_name = sr_strdup("executable0");
    frames[1]->fingerprint = sr_strdup("ab");
    frames[1]->next = frames[0];

    /* Test duplication without siblings. */
    frame = sr_core_frame_dup(frames[1], false);

    g_assert_null(frame->next);
    g_assert_false(frame->build_id == frames[1]->build_id);
    g_assert_false(frame->function_name == frames[1]->function_name);
    g_assert_false(frame->file_name == frames[1]->file_name);
    g_assert_cmpint(sr_core_frame_cmp(frame, frames[1]), ==, 0);

    sr_core_frame_free(frame);

    /* Test duplication with siblings. */
    frame = sr_core_frame_dup(frames[1], true);

    g_assert_false(frame->build_id == frames[1]->build_id);
    g_assert_false(frame->function_name == frames[1]->function_name);
    g_assert_false(frame->file_name == frames[1]->file_name);
    g_assert_cmpint(sr_core_frame_cmp(frame, frames[1]), ==, 0);
    g_assert_false(frame->next == frames[0]);
    g_assert_cmpint(sr_core_frame_cmp(frame->next, frames[0]), ==, 0);

    sr_core_frame_free(frame->next);
    sr_core_frame_free(frame);

    sr_core_frame_free(frames[1]);
    sr_core_frame_free(frames[0]);
}

static void
test_core_frame_to_json(void)
{
    struct sr_core_frame *frame;
    g_autofree char *json = NULL;

    frame = sr_core_frame_new();

    frame->address = 0xffffffff1;
    frame->build_id = sr_strdup("aabbccddeeff1");
    frame->build_id_offset = 2561;
    frame->function_name = sr_strdup("test1");
    frame->file_name = sr_strdup("executable1");
    frame->fingerprint = sr_strdup("ab");

    json = sr_core_frame_to_json(frame);

    g_assert_cmpstr(json, ==, test_json);

    sr_core_frame_free(frame);
}

static void
test_core_frame_abstract_functions(void)
{
    struct sr_core_frame *frame;
    struct sr_strbuf *strbuf;
    g_autofree char *res = NULL;

    frame = sr_core_frame_new();
    strbuf = sr_strbuf_new();

    frame->address = 0xffffffff1;
    frame->build_id = sr_strdup("aabbccddeeff1");
    frame->build_id_offset = 2561;
    frame->function_name = sr_strdup("test1");
    frame->file_name = sr_strdup("executable1");
    frame->fingerprint = sr_strdup("ab");
    frame->next = (struct sr_core_frame *)0xdeadbeef;

    g_assert_true(sr_frame_next((struct sr_frame*)frame) == (void*)0xdeadbeef);

    sr_frame_append_to_str((struct sr_frame*)frame, strbuf);

    res = sr_strbuf_free_nobuf(strbuf);

    g_assert_cmpstr(res, ==, "[executable1] test1");
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/core/dup", test_core_frame_dup);
    g_test_add_func("/frame/core/to-json", test_core_frame_to_json);
    g_test_add_func("/frame/core/abstract-functions", test_core_frame_abstract_functions);

    return g_test_run();
}
