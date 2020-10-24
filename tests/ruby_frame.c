#include <frame.h>
#include <glib.h>
#include <location.h>
#include <ruby/frame.h>

typedef struct
{
    const char *input;

    const char *expected_file;
    size_t expected_line;
    const char *expected_function;
    bool special;
    size_t expected_block_level;
    size_t expected_rescue_level;
} ParseTestData;

static void
test_ruby_frame_parse(const void *user_data)
{
    struct sr_location location;
    ParseTestData *test_data;
    struct sr_ruby_frame *frame;

    sr_location_init(&location);

    test_data = (ParseTestData *)user_data;
    frame = sr_ruby_frame_parse(&test_data->input, &location);

    g_assert_nonnull(frame);

    g_assert_cmpstr(frame->file_name, ==, test_data->expected_file);
    g_assert_cmpuint(frame->file_line, ==, test_data->expected_line);
    g_assert_cmpstr(frame->function_name, ==, test_data->expected_function);
    g_assert_cmpint(frame->special_function, ==, test_data->special);
    g_assert_cmpuint(frame->block_level, ==, test_data->expected_block_level);
    g_assert_cmpuint(frame->rescue_level, ==, test_data->expected_rescue_level);

    g_assert_cmpint(*test_data->input, ==, '\0');

    sr_ruby_frame_free(frame);
}

static void
test_ruby_frame_parse_fail(void)
{
    struct sr_location location;
    const char *input = "i;dikasdfxc";
    struct sr_ruby_frame *frame;

    sr_location_init(&location);

    frame = sr_ruby_frame_parse(&input, &location);

    g_assert_null(frame);
    g_assert_nonnull(location.message);
    g_free((void *)location.message);
}

static void
test_ruby_frame_cmp(void)
{
    struct sr_location location;
    const char *line = "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'";
    const char *input;
    struct sr_ruby_frame *frames[2];

    sr_location_init(&location);

    input = line;
    frames[0] = sr_ruby_frame_parse(&input, &location);

    g_assert_nonnull(frames[0]);

    sr_location_init(&location);

    input = line;
    frames[1] = sr_ruby_frame_parse(&input, &location);

    g_assert_nonnull(frames[1]);

    g_assert_false(frames[0] == frames[1]);
    g_assert_cmpint(sr_ruby_frame_cmp(frames[0], frames[1]), ==, 0);

    frames[1]->file_line = 9000;

    g_assert_cmpint(sr_ruby_frame_cmp(frames[0], frames[1]), !=, 0);

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        sr_ruby_frame_free(frames[i]);
    }
}

static void
test_ruby_frame_dup(void)
{
    struct sr_location location;
    const char *line = "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'";
    struct sr_ruby_frame *frames[2];

    sr_location_init(&location);

    frames[0] = sr_ruby_frame_parse(&line, &location);

    g_assert_nonnull(frames[0]);

    frames[1] = sr_ruby_frame_dup(frames[0], false);

    g_assert_nonnull(frames[1]);

    g_assert_cmpint(sr_ruby_frame_cmp(frames[0], frames[1]), ==, 0);
    g_assert_false(frames[0] == frames[1]);
    g_assert_false(frames[0]->function_name == frames[1]->function_name);

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        sr_ruby_frame_free(frames[i]);
    }
}

static void
test_ruby_frame_append(void)
{
    struct sr_ruby_frame *frames[2];

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        frames[i] = sr_ruby_frame_new();
    }

    g_assert_null(frames[0]->next);
    g_assert_true(frames[0]->next == frames[1]->next);

    sr_ruby_frame_append(frames[0], frames[1]);

    g_assert_true(frames[0]->next == frames[1]);

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        sr_ruby_frame_free(frames[i]);
    }
}

static void
test_ruby_frame_append_to_str(void)
{
    struct sr_location location;
    const char *line = "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'";
    struct sr_ruby_frame *frame;
    GString *strbuf;
    g_autofree char *result = NULL;

    sr_location_init(&location);

    frame = sr_ruby_frame_parse(&line, &location);

    g_assert_nonnull(frame);

    strbuf = g_string_new(NULL);

    sr_ruby_frame_append_to_str(frame, strbuf);

    result = g_string_free(strbuf, FALSE);

    g_assert_cmpstr(result, ==, "rescue in block (2 levels) in func in /usr/share/ruby/vendor_ruby/will_crash.rb:13");

    sr_ruby_frame_free(frame);
}

static void
test_ruby_frame_to_json(void)
{
    struct sr_location location;
    const char *line = "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'";
    struct sr_ruby_frame *frame;
    const char *expected =
        "{   \"file_name\": \"/usr/share/ruby/vendor_ruby/will_crash.rb\"\n"
        ",   \"file_line\": 13\n"
        ",   \"function_name\": \"func\"\n"
        ",   \"block_level\": 2\n"
        ",   \"rescue_level\": 1\n"
        "}";
    g_autofree char *json = NULL;

    sr_location_init(&location);

    frame = sr_ruby_frame_parse(&line, &location);

    g_assert_nonnull(frame);

    json = sr_ruby_frame_to_json(frame);

    g_assert_cmpstr(json, ==, expected);

    sr_ruby_frame_free(frame);
}

static void
test_ruby_frame_from_json(const void *user_data)
{
    struct sr_location location;
    struct sr_ruby_frame *frames[2];
    g_autofree char *json = NULL;
    enum json_tokener_error error;
    json_object *root;
    char *error_message = NULL;

    sr_location_init(&location);

    frames[0] = sr_ruby_frame_parse((const char **)&user_data, &location);

    g_assert_nonnull(frames[0]);

    json = sr_ruby_frame_to_json(frames[0]);
    root = json_tokener_parse_verbose(json, &error);

    g_assert_nonnull(root);

    frames[1] = sr_ruby_frame_from_json(root, &error_message);

    g_assert_nonnull(frames[1]);

    g_assert_cmpint(sr_ruby_frame_cmp(frames[0], frames[1]), ==, 0);

    json_object_put(root);

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        sr_ruby_frame_free(frames[i]);
    }
}

static void
test_ruby_frame_generic_functions(void)
{
    struct sr_location location;
    const char *line = "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'";
    struct sr_ruby_frame *frame;
    GString *strbuf;
    g_autofree char *result = NULL;

    sr_location_init(&location);

    frame = sr_ruby_frame_parse(&line, &location);

    g_assert_nonnull(frame);
    g_assert_null(sr_frame_next((struct sr_frame *)frame));

    strbuf = g_string_new(NULL);

    sr_frame_append_to_str((struct sr_frame*)frame, strbuf);

    result = g_string_free(strbuf, FALSE);

    g_assert_cmpstr(result, ==, "rescue in block (2 levels) in func in /usr/share/ruby/vendor_ruby/will_crash.rb:13");

    sr_frame_free((struct sr_frame *)frame);
}

int
main(int    argc,
     char **argv)
{
    ParseTestData parse_test_data[3] =
    {
        {
            "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'",
            "/usr/share/ruby/vendor_ruby/will_crash.rb",
            13,
            "func",
            false,
            2, 1,
        },
        {
            "/home/u/work/will:crash/will_crash.rb:30:in `block in <class:WillClass>'",
            "/home/anonymized/work/will:crash/will_crash.rb",
            30,
            "class:WillClass",
            true,
            1, 0,
        },
        {
            "./will_ruby_raise:8:in `<main>'",
            "./will_ruby_raise",
            8,
            "main",
            true,
            0, 0,
        },
    };

    g_test_init(&argc, &argv, NULL);

    g_test_add_data_func("/frame/ruby/parse/1", &parse_test_data[0],
                         test_ruby_frame_parse);
    g_test_add_data_func("/frame/ruby/parse/2", &parse_test_data[1],
                         test_ruby_frame_parse);
    g_test_add_data_func("/frame/ruby/parse/3", &parse_test_data[2],
                         test_ruby_frame_parse);
    g_test_add_func("/frame/ruby/parse/fail", test_ruby_frame_parse_fail);
    g_test_add_func("/frame/ruby/cmp", test_ruby_frame_cmp);
    g_test_add_func("/frame/ruby/dup", test_ruby_frame_dup);
    g_test_add_func("/frame/ruby/append", test_ruby_frame_append);
    g_test_add_func("/frame/ruby/append-to-str", test_ruby_frame_append_to_str);
    g_test_add_func("/frame/ruby/to-json", test_ruby_frame_to_json);
    g_test_add_data_func("/frame/ruby/from-json/1",
                         "/usr/share/ruby/vendor_ruby/will_crash.rb:13:in `rescue in block (2 levels) in func'",
                         test_ruby_frame_from_json);
    g_test_add_data_func("/frame/ruby/from-json/2",
                         "/usr/share/ruby/vendor:ruby/will_crash.rb:13:in `block (22 levels) in <func>'",
                         test_ruby_frame_from_json);
    g_test_add_func("/frame/ruby/generic-functions", test_ruby_frame_generic_functions);

    return g_test_run();
}
