#include "js/frame.h"
#include "frame.h"
#include "utils.h"
#include "location.h"
#include <stdio.h>
#include <glib.h>

#define check_valid(i_input, e_function, e_file, e_line, e_column, e_loc_column) \
    do { \
        const char *r_input = i_input; \
        struct sr_location r_location; \
        sr_location_init(&r_location); \
        struct sr_js_frame *r_frame; \
        r_frame = sr_js_frame_parse_v8(&r_input, &r_location); \
        if (r_frame == NULL) { \
            fprintf(stderr, "unexpected error -> '%s'\n", r_location.message); \
            g_assert_null(i_input); \
        } \
        if (    ( e_function && (g_strcmp0(r_frame->function_name, e_function) != 0)) \
             || (!e_function && r_frame->function_name)) { \
            fprintf(stderr, "function_name -> '%s' != '%s'\n", r_frame->function_name, e_function); \
            g_assert_null(i_input); \
        } \
        if (    ( e_file && (g_strcmp0(r_frame->file_name, e_file) != 0)) \
             || (!e_file && r_frame->file_name)) { \
            fprintf(stderr, "file_name -> '%s' != '%s'\n", r_frame->file_name, e_file); \
            g_assert_null(i_input); \
        } \
        if (r_frame->file_line != e_line) { \
            fprintf(stderr, "file_line -> %d != %d\n", r_frame->file_line, e_line); \
            g_assert_null(i_input); \
        } \
        if (r_frame->line_column != e_column) { \
            fprintf(stderr, "line_column -> %d != %d\n", r_frame->line_column, e_column); \
            g_assert_null(i_input); \
        } \
        if (r_location.line != 1) { \
            fprintf(stderr, "line -> %d != 0\n", r_location.line); \
            g_assert_null(i_input); \
        } \
        if (r_location.column != e_loc_column) { \
            fprintf(stderr, "column -> %d != %d\n", r_location.column, e_loc_column); \
            g_assert_null(i_input); \
        } \
        if (*r_input != '\0') { \
            fprintf(stderr, "*r_input -> %c != \\0\n", *r_input); \
            g_assert_null(i_input); \
        } \
        sr_js_frame_free(r_frame); \
    } while (0)


#define check_invalid(i_input, e_message, e_line, e_column) \
    do { \
        const char *r_input = i_input; \
        struct sr_location r_location; \
        sr_location_init(&r_location); \
        struct sr_js_frame *r_frame = sr_js_frame_parse_v8(&r_input, &r_location); \
        if (r_frame != NULL) { \
            GString *buf = g_string_new(NULL); \
            sr_js_frame_append_to_str(r_frame, buf); \
            fprintf(stderr, "NULL != '%s'\n", buf->str); \
            g_string_free(buf, TRUE); \
            g_assert_null(i_input); \
        } \
        g_assert_true(r_location.message || !i_input); \
        if (0 != strcmp(e_message, r_location.message)) {  \
            fprintf(stderr, "error_message -> '%s' != '%s'\n", r_location.message, e_message); \
            g_assert_null(i_input); \
        } \
        if (r_location.line != e_line) { \
            fprintf(stderr, "line -> %d != %d\n", r_location.line, e_line); \
            g_assert_null(i_input); \
        } \
        if (r_location.column != e_column) { \
            fprintf(stderr, "column -> %d != %d\n", r_location.column, e_column); \
            g_assert_null(i_input); \
        } \
    } while (0)

static void
test_js_frame_parse_v8(void)
{
    check_valid("    at ContextifyScript.Script.runInThisContext (vm.js:25:33)",
                "ContextifyScript.Script.runInThisContext",
                "vm.js",
                25,
                33,
                61);

    check_valid("at    ContextifyScript.Script.runInThisContext    (vm.js:25:33)",
                "ContextifyScript.Script.runInThisContext",
                "vm.js",
                25,
                33,
                63);

    check_valid("at    ContextifyScript.Script.runInThisContext    (/home/user/vm.js:25:33)",
                "ContextifyScript.Script.runInThisContext",
                "/home/anonymized/vm.js",
                25,
                33,
                74);

    check_valid("at    ContextifyScript.Script.runInThisContext    (vm.js:25:33)  ",
                "ContextifyScript.Script.runInThisContext",
                "vm.js",
                25,
                33,
                65);

    check_valid("    at PipeConnectWrap.afterConnect [as oncomplete] (net.js:1068:10)",
                "PipeConnectWrap.afterConnect",
                "net.js",
                1068,
                10,
                68);

    check_valid("    at bootstrap_node.js:357:29",
                NULL,
                "bootstrap_node.js",
                357,
                29,
                31);

    check_valid("at    bootstrap_node.js:357:29",
                NULL,
                "bootstrap_node.js",
                357,
                29,
                30);

    check_valid("at    bootstrap_node.js:357:29  ",
                NULL,
                "bootstrap_node.js",
                357,
                29,
                32);

    check_valid("    at (b[c]d):e.js:2:1",
                NULL,
                "(b[c]d):e.js",
                2,
                1,
                23);

    check_valid("at _combinedTickCallback (a(b[c]d):e.js:67:7)",
                "_combinedTickCallback",
                "a(b[c]d):e.js",
                67,
                7,
                45);

    check_invalid("    at ContextifyScript.Script.runInThisContext (vm.js:25:foo)",
                  "Line column contains ilegal symbol.",
                  1,
                  61);

    check_invalid("    at ContextifyScript.Script.runInThisContext (vm.js:25:33) at",
                  "Line column contains ilegal symbol.",
                  1,
                  64);

    check_invalid("    at ContextifyScript.Script.runInThisContext (vm.js:foo:33)",
                  "File line contains ilegal symbol.",
                  1,
                  58);

    check_invalid(" bootstrap_node.js:357:29",
                  "Expected frame beginning.",
                  1,
                  1);

    check_invalid("at bootstrap_node.js:357:29)",
                  "White space right behind function name not found.",
                  1,
                  3);

    check_invalid("at bootstrap_node.js:357:29 )",
                  "Opening brace following function name not found.",
                  1,
                  27);

    check_invalid(" at Foo (1234)",
                  "Unable to locate line column.",
                  1,
                  9);

    check_invalid(" at vm.js:25:",
                  "Failed to parse line column.",
                  1,
                  13);

    check_invalid(" at Foo (1234:25)",
                  "Unable to locate file line.",
                  1,
                  9);

    check_invalid(" at vm.js::25",
                  "Failed to parse file line.",
                  1,
                  10);
}

static void
test_js_frame_cmp(void)
{
    struct sr_location location;
    const char *line = "at process._tickCallback (internal/process/next_tick.js:98:9)";
    const char *input;

    sr_location_init(&location);
    input = line;
    struct sr_js_frame *frame1 = sr_js_frame_parse(&input, &location);
    g_assert_nonnull(frame1);

    sr_location_init(&location);
    input = line;
    struct sr_js_frame *frame2 = sr_js_frame_parse(&input, &location);
    g_assert_nonnull(frame1);

    g_assert_true(0 == sr_js_frame_cmp(frame1, frame2));
    g_assert_false(frame1 == frame2);

    frame2->file_line = 9000;
    g_assert_true(0 != sr_js_frame_cmp(frame1, frame2) || !"file_line");

    frame2->file_line = frame1->file_line;
    frame2->line_column = 8000;
    g_assert_true(0 != sr_js_frame_cmp(frame1, frame2) || !"line_column");

    frame2->line_column = frame1->line_column;

    free(frame2->function_name);
    frame2->function_name = NULL;
    g_assert_true(0 != sr_js_frame_cmp(frame1, frame2) || !"function_name - NULL");

    frame2->function_name = g_strdup("foo_blah");
    g_assert_true(0 != sr_js_frame_cmp(frame1, frame2) || !"function_name");

    free(frame2->function_name);
    frame2->function_name = g_strdup(frame1->function_name);

    free(frame2->file_name);
    frame2->file_name = NULL;
    g_assert_true(0 != sr_js_frame_cmp(frame1, frame2) || !"file_name - NULL");

    frame2->file_name = g_strdup("blah_foo");
    g_assert_true(0 != sr_js_frame_cmp(frame1, frame2) || !"file_name");

    sr_js_frame_free(frame1);
    sr_js_frame_free(frame2);
}

static void
test_js_frame_cmp_distance(void)
{
    struct sr_location location;
    const char *line = "at process._tickCallback (internal/process/next_tick.js:98:9)";
    const char *input;

    sr_location_init(&location);
    input = line;
    struct sr_js_frame *frame1 = sr_js_frame_parse(&input, &location);
    g_assert_nonnull(frame1);

    sr_location_init(&location);
    input = line;
    struct sr_js_frame *frame2 = sr_js_frame_parse(&input, &location);
    g_assert_nonnull(frame1);

    g_assert(0 == sr_js_frame_cmp_distance(frame1, frame2));
    g_assert_false(frame1 == frame2);

    frame2->file_line = 9000;
    g_assert_true(0 != sr_js_frame_cmp_distance(frame1, frame2) || !"file_line");
    frame2->file_line = frame1->file_line;

    frame2->line_column = 8000;
    /* Expected no effect */
    g_assert_true(0 == sr_js_frame_cmp_distance(frame1, frame2) || !"line_column");
    frame2->line_column = frame1->line_column;

    free(frame2->function_name);
    frame2->function_name = NULL;
    g_assert_true(0 != sr_js_frame_cmp_distance(frame1, frame2) || !"function_name - NULL");

    frame2->function_name = g_strdup("foo_blah");
    g_assert_true(0 != sr_js_frame_cmp_distance(frame1, frame2) || !"function_name");

    free(frame2->function_name);
    frame2->function_name = g_strdup(frame1->function_name);

    free(frame2->file_name);
    frame2->file_name = NULL;
    g_assert_true(0 != sr_js_frame_cmp_distance(frame1, frame2) || !"file_name - NULL");

    frame2->file_name = g_strdup("blah_foo");
    g_assert_true(0 != sr_js_frame_cmp_distance(frame1, frame2) || !"file_name");

    sr_js_frame_free(frame1);
    sr_js_frame_free(frame2);
}

static void
test_js_frame_dup(void)
{
    struct sr_location location;
    const char *line = "at process._tickCallback (internal/process/next_tick.js:98:9)";

    sr_location_init(&location);
    struct sr_js_frame *frame1 = sr_js_frame_parse(&line, &location);
    g_assert_nonnull(frame1);

    struct sr_js_frame *frame2 = sr_js_frame_dup(frame1, false);

    g_assert_true(0 == sr_js_frame_cmp(frame1, frame2));
    g_assert_false(frame1 == frame2);
    g_assert_false(frame1->function_name == frame2->function_name);
    g_assert_false(frame1->file_name == frame2->file_name);
}

static void
test_js_frame_append(void)
{
    struct sr_js_frame *frame1 = sr_js_frame_new();
    struct sr_js_frame *frame2 = sr_js_frame_new();

    g_assert_true(frame1->next == frame2->next);
    g_assert_null(frame1->next);

    sr_js_frame_append(frame1, frame2);
    g_assert_true(frame1->next == frame2);

    sr_js_frame_free(frame1);
    sr_js_frame_free(frame2);
}

static void
test_js_frame_to_json(void)
{
    struct sr_location location;
    const char *line = "at process._tickCallback (internal/process/next_tick.js:98:9)";

    sr_location_init(&location);
    struct sr_js_frame *frame1 = sr_js_frame_parse(&line, &location);
    g_assert_nonnull(frame1);

    const char *expected =
        "{   \"file_name\": \"internal/process/next_tick.js\"\n"
        ",   \"file_line\": 98\n"
        ",   \"line_column\": 9\n"
        ",   \"function_name\": \"process._tickCallback\"\n"
        "}";

    char *json = sr_js_frame_to_json(frame1);
    if (0 != strcmp(json, expected)) {
      fprintf(stderr, "%s\n!=\n%s\n", json, expected);
      g_assert_false("Invalid JSON for JavaScript frame");
    }

    free(json);
}

static void
test_js_frame_from_json(void)
{
    const char *input = "at process._tickCallback (internal/process/next_tick.js:98:9)";
    struct sr_location location;
    sr_location_init(&location);

    struct sr_js_frame *frame1 = sr_js_frame_parse(&input, &location);
    g_assert_nonnull(frame1);

    char *json = sr_js_frame_to_json(frame1);
    char *error_message = NULL;

    enum json_tokener_error error;
    json_object *root = json_tokener_parse_verbose(json, &error);
    if (NULL == root)
    {
        error_message = (char *)json_tokener_error_desc(error);
    }
    g_assert_nonnull(root);
    struct sr_js_frame *frame2 = sr_js_frame_from_json(root, &error_message);
    g_assert_nonnull(frame2);
    g_assert_true(0 == sr_js_frame_cmp(frame1, frame2));

    sr_js_frame_free(frame2);
    json_object_put(root);
    free(json);
    sr_js_frame_free(frame1);
}

static void
test_js_frame_append_to_str(void)
{
    struct sr_location location;
    const char *line = "at process._tickCallback (internal/process/next_tick.js:98:9)";
    const char *input = line;

    sr_location_init(&location);
    struct sr_js_frame *frame1 = sr_js_frame_parse(&input, &location);
    g_assert_nonnull(frame1);

    GString *strbuf = g_string_new(NULL);
    sr_js_frame_append_to_str(frame1, strbuf);
    char *result = g_string_free(strbuf, FALSE);

    if (g_strcmp0(result, line) != 0) {
        fprintf(stderr, "'%s'\n  !=\n'%s'\n", result, line);
        g_assert_false("Failed to format JavaScript frame to string.");
    }

    free(result);
    sr_js_frame_free(frame1);
}

static void
test_js_frame_generic_functions(void)
{
    const char *line = "at process._tickCallback (internal/process/next_tick.js:98:9)";

    struct sr_location location;
    sr_location_init(&location);
    struct sr_js_frame *frame1 = sr_js_frame_parse(&line, &location);
    g_assert_nonnull(frame1);

    g_assert_null(sr_frame_next((struct sr_frame *)frame1));

    GString *strbuf = g_string_new(NULL);
    sr_frame_append_to_str((struct sr_frame*)frame1, strbuf);
    char *result = g_string_free(strbuf, FALSE);

    g_assert_cmpstr(result, ==, "at process._tickCallback (internal/process/next_tick.js:98:9)");

    sr_frame_free((struct sr_frame*)frame1);
}


int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/js/parse-v8", test_js_frame_parse_v8);
    g_test_add_func("/frame/js/cmp", test_js_frame_cmp);
    g_test_add_func("/frame/js/cmp-distance", test_js_frame_cmp_distance);
    g_test_add_func("/frame/js/dup", test_js_frame_dup);
    g_test_add_func("/frame/js/append", test_js_frame_append);
    g_test_add_func("/frame/js/to-json", test_js_frame_to_json);
    g_test_add_func("/frame/js/from-json", test_js_frame_from_json);
    g_test_add_func("/frame/js/append-to-str", test_js_frame_append_to_str);
    g_test_add_func("/frame/js/generic-functions", test_js_frame_generic_functions);

    return g_test_run();
}
