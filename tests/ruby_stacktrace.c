#include "ruby/stacktrace.h"
#include "ruby/frame.h"
#include "utils.h"
#include "location.h"
#include "stacktrace.h"
#include <glib.h>

static void
test_ruby_stacktrace_parse(void)
{
    struct test_data
    {
        char *filename;
        char *exc_name;
        unsigned frame_count;
        struct sr_ruby_frame *top_frame;
        struct sr_ruby_frame *second_frame;
        struct sr_ruby_frame *bottom_frame;
    };

    struct sr_ruby_frame top = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/share/ruby/vendor_ruby/will_crash.rb",
        .file_line = 13,
        .special_function = false,
        .function_name = "func",
        .block_level = 2,
        .rescue_level = 1
    };
    struct sr_ruby_frame second = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/share/ruby/vendor_ruby/will_crash.rb",
        .file_line = 10,
        .special_function = false,
        .function_name = "func",
        .block_level = 2,
        .rescue_level = 0
    };
    struct sr_ruby_frame bottom = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/bin/will_ruby_raise",
        .file_line = 8,
        .special_function = true,
        .function_name = "main",
        .block_level = 0,
        .rescue_level = 0
    };

    struct sr_ruby_frame top2 = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/share/gems/gems/rdoc-4.1.1/lib/rdoc/class_module.rb",
        .file_line = 173,
        .special_function = false,
        .function_name = "aref_prefix",
        .block_level = 0,
        .rescue_level = 0
    };
    struct sr_ruby_frame second2 = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/share/gems/gems/rdoc-4.1.1/lib/rdoc/class_module.rb",
        .file_line = 181,
        .special_function = false,
        .function_name = "aref",
        .block_level = 0,
        .rescue_level = 0
    };

    struct sr_ruby_frame top4 = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/share/rubygems/rubygems/basic_specification.rb",
        .file_line = 50,
        .special_function = false,
        .function_name = "contains_requirable_file?",
        .block_level = 3,
        .rescue_level = 0
    };
    struct sr_ruby_frame second4 = {
        .type = SR_REPORT_RUBY,
        .file_name = "/usr/share/rubygems/rubygems/basic_specification.rb",
        .file_line = 50,
        .special_function = false,
        .function_name = "each",
        .block_level = 0,
        .rescue_level = 0
    };

    struct test_data t[] = {
        {"ruby_stacktraces/ruby-01", "Wrap::MyException", 21, &top, &second, &bottom},
        {"ruby_stacktraces/ruby-02", "NotImplementedError", 37, &top2, &second2, NULL},
        {"ruby_stacktraces/ruby-03", "RuntimeError", 4, NULL, NULL, NULL},
        {"ruby_stacktraces/ruby-04", "SignalException", 39, &top4, &second4, NULL},
    };

    for (int n = 0; n < sizeof (t) / sizeof (*t); n++)
    {
        char *error_message = NULL;
        const char *file_contents = sr_file_to_string(t[n].filename, &error_message);
        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);

        struct sr_ruby_stacktrace *stacktrace = sr_ruby_stacktrace_parse(&input, &location);
        g_assert_nonnull(stacktrace);
        g_assert_cmpint(*input, ==, '\0');
        g_assert_cmpstr(stacktrace->exception_name, ==, t[n].exc_name);

        struct sr_ruby_frame *frame = stacktrace->frames;
        int i = 0;
        while (frame)
        {
            if (i==0 && t[n].top_frame)
            {
                g_assert_true(sr_ruby_frame_cmp(frame, t[n].top_frame) == 0);
            }
            else if (i == 1 && t[n].second_frame)
            {
                g_assert_true(sr_ruby_frame_cmp(frame, t[n].second_frame) == 0);
            }

            frame = frame->next;
            i++;
        }

        g_assert_true(i == t[n].frame_count);
        if (frame && t[n].bottom_frame)
        {
            g_assert_true(sr_ruby_frame_cmp(frame, t[n].bottom_frame) == 0);
        }

        sr_ruby_stacktrace_free(stacktrace);
        g_free((void *)file_contents);
    }
}

static void
test_ruby_stacktrace_dup(void)
{
    char *filenames[] = {
        "ruby_stacktraces/ruby-01",
        "ruby_stacktraces/ruby-02",
        "ruby_stacktraces/ruby-03",
        "ruby_stacktraces/ruby-04",
    };

    for (int i = 0; i < sizeof (filenames) / sizeof (*filenames); i++)
    {
        char *error_message = NULL;
        const char *file_contents = sr_file_to_string(filenames[i], &error_message);
        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);

        struct sr_ruby_stacktrace *stacktrace1 = sr_ruby_stacktrace_parse(&input, &location);
        struct sr_ruby_stacktrace *stacktrace2 = sr_ruby_stacktrace_dup(stacktrace1);

        g_assert_false(stacktrace1 == stacktrace2);
        g_assert_cmpstr(stacktrace1->exception_name, ==, stacktrace2->exception_name);

        struct sr_ruby_frame *f1 = stacktrace1->frames;
        struct sr_ruby_frame *f2 = stacktrace2->frames;

        while (f1 && f2)
        {
            g_assert_true(0 == sr_ruby_frame_cmp(f1, f2));
            f1 = f1->next;
            f2 = f2->next;
        }
        g_assert_null(f1);
        g_assert_null(f2);

        sr_ruby_stacktrace_free(stacktrace1);
        sr_ruby_stacktrace_free(stacktrace2);
        g_free((void *)file_contents);
    }
}

static void
test_ruby_stacktrace_get_reason(void)
{
    char *error_message = NULL;
    const char *file_contents = sr_file_to_string("ruby_stacktraces/ruby-03", &error_message);
    const char *input = file_contents;
    struct sr_location location;
    sr_location_init(&location);
    struct sr_ruby_stacktrace *stacktrace1 = sr_ruby_stacktrace_parse(&input, &location);

    g_autofree char *reason = sr_ruby_stacktrace_get_reason(stacktrace1);
    char *expected = "RuntimeError in /usr/share/gems/gems/openshift-origin-node-1.18.0.1/lib/openshift-origin-node/utils/tc.rb:103";
    g_assert_cmpstr(reason, ==, expected);

    sr_ruby_stacktrace_free(stacktrace1);
    g_free((void *)file_contents);
}

static void
test_ruby_stacktrace_to_json(void)
{
    struct test_data
    {
        char *filename;
        char *json_filename;
    };

    struct test_data t[] = {
        {"ruby_stacktraces/ruby-01", "ruby_stacktraces/ruby-01-expected-json"},
        {"ruby_stacktraces/ruby-02", "ruby_stacktraces/ruby-02-expected-json"},
        {"ruby_stacktraces/ruby-03", "ruby_stacktraces/ruby-03-expected-json"},
        {"ruby_stacktraces/ruby-04", "ruby_stacktraces/ruby-04-expected-json"},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *error_message = NULL;
        const char *file_contents = sr_file_to_string(t[i].filename, &error_message);
        const char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);

        struct sr_ruby_stacktrace *stacktrace1 = sr_ruby_stacktrace_parse(&input, &location);

        g_autofree char *expected = sr_file_to_string(t[i].json_filename, &error_message);
        char *json = sr_ruby_stacktrace_to_json(stacktrace1);

        g_assert_cmpstr(json, ==, expected);

        sr_ruby_stacktrace_free(stacktrace1);
        g_free(json);
        g_free((void *)file_contents);
    }
}

static void
test_ruby_stacktrace_from_json(void)
{
    char *filenames[] = {
        "ruby_stacktraces/ruby-01",
        "ruby_stacktraces/ruby-02",
        "ruby_stacktraces/ruby-03",
        "ruby_stacktraces/ruby-04",
    };

    for (int i = 0; i < sizeof (filenames) / sizeof (*filenames); i++)
    {
        char *error_message = NULL;
        char *file_contents = sr_file_to_string(filenames[i], &error_message);
        char *input = file_contents;
        struct sr_location location;
        sr_location_init(&location);

        struct sr_ruby_stacktrace *stacktrace1 = sr_ruby_stacktrace_parse((const char **)&input, &location);

        char *json = sr_ruby_stacktrace_to_json(stacktrace1);
        struct sr_ruby_stacktrace *stacktrace2 = (struct sr_ruby_stacktrace *)sr_stacktrace_from_json_text(SR_REPORT_RUBY, json, &error_message);

        g_assert_cmpstr(stacktrace1->exception_name, ==, stacktrace2->exception_name);

        struct sr_ruby_frame *f1 = stacktrace1->frames;
        struct sr_ruby_frame *f2 = stacktrace2->frames;

        while (f1 && f2)
        {
            g_assert_true(0 == sr_ruby_frame_cmp(f1, f2));
            f1 = f1->next;
            f2 = f2->next;
        }
        g_assert_null(f1);
        g_assert_null(f2);

        sr_ruby_stacktrace_free(stacktrace1);
        sr_ruby_stacktrace_free(stacktrace2);
        g_free(json);
        g_free((void *)file_contents);
    }
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/ruby/parse", test_ruby_stacktrace_parse);
    g_test_add_func("/stacktrace/ruby/dup", test_ruby_stacktrace_dup);
    g_test_add_func("/stacktrace/ruby/get-reason", test_ruby_stacktrace_get_reason);
    g_test_add_func("/stacktrace/ruby/to-json", test_ruby_stacktrace_to_json);
    g_test_add_func("/stacktrace/ruby/from-json", test_ruby_stacktrace_from_json);

    return g_test_run();
}
