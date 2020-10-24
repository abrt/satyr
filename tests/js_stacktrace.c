#include "js/stacktrace.h"
#include "js/frame.h"
#include "stacktrace.h"
#include "utils.h"
#include "location.h"
#include <stdio.h>
#include <glib.h>

void
print_frame(FILE *out, struct sr_js_frame *frame)
{
    GString *buf = g_string_new(NULL);
    sr_js_frame_append_to_str(frame, buf);
    fprintf(out, "%s", buf->str);
    g_string_free(buf, TRUE);
}

void
check_valid(char *filename, char *exc_name, unsigned frame_count,
      struct sr_js_frame *top_frame,
      struct sr_js_frame *second_frame,
      struct sr_js_frame *bottom_frame)
{
    char *error_message = NULL;
    char *file_contents = sr_file_to_string(filename, &error_message);
    const char *input = file_contents;
    struct sr_location location;
    sr_location_init(&location);

    struct sr_js_stacktrace *stacktrace = sr_js_stacktrace_parse_v8(&input, &location);
    g_assert_nonnull(stacktrace);
    g_assert_cmpint(*input, ==, '\0');
    if (0 != g_strcmp0(stacktrace->exception_name, exc_name)) {
        fprintf(stderr, "exception_name -> '%s' != '%s'\n", stacktrace->exception_name, exc_name);
        g_assert_true(!"Parsed exception name");
    }

    struct sr_js_frame *frame = stacktrace->frames;
    int i = 0;
    while (frame)
    {
        if (i==0 && top_frame) {
            if (sr_js_frame_cmp(frame, top_frame) != 0) {
                print_frame(stderr, frame);
                fprintf(stderr, "\n  !=\n");
                print_frame(stderr, top_frame);
                fprintf(stderr, "\n");
                g_assert_true(!"Invalid top frame");
            }
        }
        else if (i == 1 && second_frame) {
            if (sr_js_frame_cmp(frame, second_frame) != 0) {
                print_frame(stderr, frame);
                fprintf(stderr, "\n  !=\n");
                print_frame(stderr, second_frame);
                fprintf(stderr, "\n");
                g_assert_true(!"Invalid second frame");
            }
        }

        frame = frame->next;
        i++;
    }

    g_assert_true(i == frame_count);
    if (frame && bottom_frame)
    {
        if (sr_js_frame_cmp(frame, bottom_frame) != 0) {
            print_frame(stderr, frame);
            fprintf(stderr, "\n  !=\n");
            print_frame(stderr, bottom_frame);
            fprintf(stderr, "\n");
            g_assert_true(!"Invalid bottom frame");
        }
    }

    sr_js_stacktrace_free(stacktrace);
    free(file_contents);
}

void
check_invalid(const char *input, const char *error_message,
              uint32_t loc_line, uint32_t loc_column)
{
    struct sr_location location;
    sr_location_init(&location);

    const char *local_input = input;
    struct sr_js_stacktrace *stacktrace =sr_js_stacktrace_parse_v8(&local_input, &location);

    if (stacktrace) {
        fprintf(stderr, "Parsed '%s'\n", input);
        g_assert_true(!"Parsed invalid stacktrace");
    }
    if (g_strcmp0(error_message, location.message) != 0) {
        fprintf(stderr, "message -> '%s' != '%s'\n", location.message, error_message);
        g_assert_true(!"Error message doe not match");
    }
    if (loc_line != location.line) {
        fprintf(stderr, "line -> %d != %d\n", location.line, loc_line);
        g_assert_true(!"Line does not match");
    }
    if (loc_column != location.column) {
        fprintf(stderr, "column -> %d != %d\n", location.column, loc_column);
        g_assert_true(!"Column does not match");
    }
}

static void
test_js_stacktrace_parse_v8(void)
{
    {
        struct sr_js_frame top = {
            .type = SR_REPORT_JAVASCRIPT,
            .file_name = "/tmp/index.js",
            .file_line = 2,
            .line_column = 1,
            .function_name = "Object.<anonymous>",
        };
        struct sr_js_frame second = {
            .type = SR_REPORT_JAVASCRIPT,
            .file_name = "module.js",
            .file_line = 556,
            .line_column = 32,
            .function_name = "Module._compile",
        };
        struct sr_js_frame bottom = {
            .type = SR_REPORT_JAVASCRIPT,
            .file_name = "bootstrap_node.js",
            .file_line = 509,
            .line_column = 3,
            .function_name = NULL,
        };
        check_valid("js_stacktraces/node-01", "ReferenceError", 10, &top, &second, &bottom);
    }

    {
        struct sr_js_frame top = {
            .type = SR_REPORT_JAVASCRIPT,
            .file_name = "/tmp/index.js",
            .file_line = 16,
            .line_column = 35,
            .function_name = "Socket.<anonymous>",
        };
        struct sr_js_frame second = {
            .type = SR_REPORT_JAVASCRIPT,
            .file_name = "events.js",
            .file_line = 286,
            .line_column = 16,
            .function_name = "Socket.g",
        };
        struct sr_js_frame bottom = {
            .type = SR_REPORT_JAVASCRIPT,
            .file_name = "net.js",
            .file_line = 1068,
            .line_column = 10,
            .function_name = "PipeConnectWrap.afterConnect",
        };
        check_valid("js_stacktraces/node-02", "TypeError", 5, &top, &second, &bottom);
    }

    check_invalid(
        "nonexistentFunc is not defined\n"
        "    at Object.<anonymous> (/tmp/index.js:2:1))",
        "Unable to find the colon right behind exception type.",
        1,
        0);

     check_invalid(
        ": nonexistentFunc is not defined\n"
        "    at Object.<anonymous> (/tmp/index.js:2:1))",
        "Zero length exception type.",
        1,
        0);

     check_invalid(
        "ReferenceError:\n"
        "    at Object.<anonymous> (/tmp/index.js:2:1))",
        "Exception type not followed by white space.",
        1,
        1);

     check_invalid(
        "ReferenceError: nonexistentFunc is not defined",
        "Stack trace does not include any frames.",
        1,
        31);
}

static void
test_js_stacktrace_dup(void)
{
    char *filenames[] = {
        "js_stacktraces/node-01",
        "js_stacktraces/node-02",
    };

    for (int i = 0; i < sizeof (filenames) / sizeof (*filenames); i++)
    {
        char *error_message = NULL;
        char *file_contents = sr_file_to_string(filenames[i], &error_message);
        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);
        
        struct sr_js_stacktrace *stacktrace1 = sr_js_stacktrace_parse(&input, &location);
        struct sr_js_stacktrace *stacktrace2 = sr_js_stacktrace_dup(stacktrace1);
        
        g_assert_false(stacktrace1 == stacktrace2);
        printf("%s == %s\n", stacktrace1->exception_name, stacktrace2->exception_name);
        g_assert_cmpstr(stacktrace1->exception_name, ==, stacktrace2->exception_name);
        
        struct sr_js_frame *f1 = stacktrace1->frames;
        struct sr_js_frame *f2 = stacktrace2->frames;
        
        while (f1 && f2)
        {
            g_assert_cmpint(sr_js_frame_cmp(f1, f2), ==, 0);
            f1 = f1->next;
            f2 = f2->next;
        }
        g_assert_null(f1);
        g_assert_null(f2);
        
        sr_js_stacktrace_free(stacktrace1);
        sr_js_stacktrace_free(stacktrace2);
        free(file_contents);
    }
}

void
check1(struct sr_js_stacktrace *stacktrace, const char *expected)
{
    char *reason = sr_js_stacktrace_get_reason(stacktrace);
    if (g_strcmp0(reason, expected) != 0) {
        fprintf(stderr, "reason -> '%s' != '%s'\n", reason, expected);
        g_assert_true(!"Invalid reason");
    }
    free(reason);
}

static void
test_js_stacktrace_get_reason(void)
{
    {
        char *error_message = NULL;
        char *file_contents = sr_file_to_string("js_stacktraces/node-01", &error_message);
        if (file_contents == NULL) {
            fprintf(stderr, "'%s'\n", error_message);
            g_assert_true(!"Cannot load test file");
        }

        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);

        struct sr_js_stacktrace *stacktrace1 = sr_js_stacktrace_parse(&input, &location);
        if (stacktrace1 == NULL) {
            fprintf(stderr, "'%s'\n", location.message);
            g_assert_true(!"Cannot parse test stacktrace");
        }

        check1(stacktrace1, "ReferenceError at /tmp/index.js:2:1");

        sr_js_stacktrace_free(stacktrace1);
        free(file_contents);
    }
    {
        struct sr_js_stacktrace *stacktrace = sr_js_stacktrace_new();
        check1(stacktrace, "Error at <unknown>:0:0");
        sr_js_stacktrace_free(stacktrace);
    }
}

static void
test_js_stacktrace_to_json(void)
{
    struct test_data
    {
        char *filename;
        char *json_filename;
    };

    struct test_data t[] = {
        {"js_stacktraces/node-01", "js_stacktraces/node-01-expected-json"},
        {"js_stacktraces/node-02", "js_stacktraces/node-02-expected-json"},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *error_message = NULL;
        char *file_contents = sr_file_to_string(t[i].filename, &error_message);
        if (file_contents == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].filename, error_message);
            g_assert_true(!"Cannot load test file");
        }
        
        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);
        
        struct sr_js_stacktrace *stacktrace = sr_js_stacktrace_parse(&input, &location);
        if (stacktrace == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].filename, location.message);
            g_assert_true(!"Cannot parse test stacktrace");
        }
        
        char *expected = sr_file_to_string(t[i].json_filename, &error_message);
        if (expected == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].json_filename, error_message);
            g_assert_true(!"Cannot load json file");
        }
        
        const size_t elen = strlen(expected);
        if (expected[elen - 1] = '\n') {
            expected[elen - 1] = '\0';
        }
        
        char *json = sr_js_stacktrace_to_json(stacktrace);
        if (0 != g_strcmp0(json, expected)) {
            fprintf(stderr, "'%s'\n  !=\n'%s'\n", json, expected);
            abort();
        }
        
        free(json);
        free(expected);
        sr_js_stacktrace_free(stacktrace);
        free(file_contents);
    }
}

static void
test_js_stacktrace_from_json(void)
{
    struct test_data
    {
        char *filename;
        char *json_filename;
    };

    struct test_data t[] = {
        {"js_stacktraces/node-01", "js_stacktraces/node-01-expected-json"},
        {"js_stacktraces/node-02", "js_stacktraces/node-02-expected-json"},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *error_message = NULL;
        char *file_contents = sr_file_to_string(t[i].filename, &error_message);
        if (file_contents == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].filename, error_message);
            g_assert_true(!"Cannot load test file");
        }
        
        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);
        
        struct sr_js_stacktrace *stacktrace1 = sr_js_stacktrace_parse(&input, &location);
        if (stacktrace1 == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].filename, location.message);
            g_assert_true(!"Cannot parse test stacktrace");
        }
        
        char *json_file_contents = sr_file_to_string(t[i].json_filename, &error_message);
        if (json_file_contents == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].json_filename, error_message);
            g_assert_true(!"Cannot load JSON file");
        }
        
        struct sr_js_stacktrace *stacktrace2 = (struct sr_js_stacktrace *)sr_stacktrace_from_json_text(SR_REPORT_JAVASCRIPT, json_file_contents, &error_message);
        if (stacktrace2 == NULL) {
            fprintf(stderr, "'%s' -> '%s'\n", t[i].filename, error_message);
            g_assert_true(!"Cannot parse JSON stacktrace");
        }
        
        if (0 != g_strcmp0(stacktrace1->exception_name, stacktrace2->exception_name)) {
            fprintf(stderr, "'%s': exception_name -> '%s' != '%s'\n", stacktrace1->exception_name, stacktrace2->exception_name);
            g_assert_true(!"Error names do no match");
        }
        
        struct sr_js_frame *f1 = stacktrace1->frames;
        struct sr_js_frame *f2 = stacktrace2->frames;
        
        uint32_t i = 0;
        for (; f1 && f2; ++i) {
            if (0 != sr_js_frame_cmp(f1, f2)) {
                GString *buf1 = g_string_new(NULL);
                sr_js_frame_append_to_str(f1, buf1);
        
                GString *buf2 = g_string_new(NULL);
                sr_js_frame_append_to_str(f2, buf2);
        
                fprintf(stderr, "Frames %i do not match:\n'%s' != '%s'\n", i, buf1->str, buf2->str);
        
                g_assert_false("Frames do not match");
            }
        
            f1 = f1->next;
            f2 = f2->next;
        }
        
        if (f1 || f2) {
            fprintf(stderr, "Frames %i: %p != %p\n", i, f1, f2);
            g_assert_false("Stacktraces do no match");
        }
        
        sr_js_stacktrace_free(stacktrace2);
        free(json_file_contents);
        sr_js_stacktrace_free(stacktrace1);
        free(file_contents);
    }
}


int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/js/parse-v8", test_js_stacktrace_parse_v8);
    g_test_add_func("/stacktrace/js/dup", test_js_stacktrace_dup);
    g_test_add_func("/stacktrace/js/get-reason", test_js_stacktrace_get_reason);
    g_test_add_func("/stacktrace/js/to-json", test_js_stacktrace_to_json);
    g_test_add_func("/stacktrace/js/from-json", test_js_stacktrace_from_json);

    return g_test_run();
}
