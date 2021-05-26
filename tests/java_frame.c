#include <glib.h>
#include <java/frame.h>
#include <location.h>
#include <utils.h>

static void
test_java_frame_cmp(void)
{
    struct sr_java_frame *frames[2];
    char *tmp;
    struct sr_java_frame *exception_frames[2];

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        frames[i] = sr_java_frame_new();
    }

    frames[0]->name = g_strdup("name.space.test1.ctor");
    frames[0]->file_name = g_strdup("test1.java");
    frames[0]->file_line = 1;
    frames[0]->class_path = g_strdup("/usr/share/jar1");

    frames[1]->name = g_strdup(frames[0]->name);
    frames[1]->file_name = g_strdup(frames[0]->file_name);
    frames[1]->file_line = frames[0]->file_line;
    frames[1]->class_path = g_strdup(frames[0]->class_path);

    g_assert_cmpint(sr_java_frame_cmp(frames[1], frames[0]), ==, 0);

    frames[1]->file_line += 1;

    g_assert_cmpint(sr_java_frame_cmp(frames[1], frames[0]), !=, 0);

    frames[1]->file_line -= 1;

    tmp = frames[1]->name;
    frames[1]->name = NULL;
    g_assert_cmpint(sr_java_frame_cmp(frames[1], frames[0]), !=, 0);

    frames[1]->name = tmp;

    tmp = frames[1]->file_name;
    frames[1]->file_name = NULL;
    g_assert_cmpint(sr_java_frame_cmp(frames[1], frames[0]), !=, 0);

    frames[1]->file_name = tmp;

    tmp = frames[1]->class_path;
    g_free(frames[1]->class_path);
    frames[1]->class_path = NULL;
    g_assert_cmpint(sr_java_frame_cmp(frames[1], frames[0]), !=, 0);

    for (size_t i = 0; i < G_N_ELEMENTS(exception_frames); i++)
    {
        exception_frames[i] = sr_java_frame_new_exception();
    }

    exception_frames[0]->name = g_strdup("com.sun.java.NullPointer");
    exception_frames[0]->message = g_strdup("Null pointer exception");

    exception_frames[1]->name = g_strdup(exception_frames[0]->name);
    exception_frames[1]->message = g_strdup(exception_frames[0]->message);

    g_assert_cmpint(sr_java_frame_cmp(exception_frames[0], exception_frames[1]), ==, 0);
    g_assert_cmpint(sr_java_frame_cmp(frames[1], exception_frames[1]), !=, 0);
    g_assert_cmpint(sr_java_frame_cmp(exception_frames[0], frames[0]), !=, 0);

    g_free(exception_frames[1]->message);
    exception_frames[1]->message = NULL;

    g_assert_cmpint(sr_java_frame_cmp(exception_frames[0], exception_frames[1]), ==, 0);

    g_free(exception_frames[1]->name);
    exception_frames[1]->name = g_strdup("com.sun.java.InvalidOperation");

    g_assert_cmpint(sr_java_frame_cmp(exception_frames[0], exception_frames[1]), !=, 0);

    for (size_t i = 0; i < G_N_ELEMENTS(exception_frames); i++)
    {
        sr_java_frame_free(exception_frames[i]);
    }

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        sr_java_frame_free(frames[i]);
    }
}

static void
test_java_frame_dup(void)
{
    struct sr_java_frame *frames[2];
    struct sr_java_frame *exception_frame;

    frames[0] = sr_java_frame_new();

    frames[0]->name = g_strdup("name.space.test1.ctor");
    frames[0]->file_name = g_strdup("test1.java");
    frames[0]->file_line = 1;
    frames[0]->class_path = g_strdup("/usr/share/jar1");

    frames[1] = sr_java_frame_new();

    frames[1]->name = g_strdup("name.space.test0.ctor");
    frames[1]->file_name = g_strdup("test0.java");
    frames[1]->file_line = 2;
    frames[1]->class_path = g_strdup("/usr/share/jar0");
    frames[1]->next = frames[0];

    /* Test the duplication without siblings. */
    {
        struct sr_java_frame *frame;

        frame = sr_java_frame_dup(frames[1], false);

        g_assert_null(frame->next);

        g_assert_false(frame->name == frames[1]->name);
        g_assert_cmpstr(frame->name, ==, frames[1]->name);

        g_assert_false(frame->file_name == frames[1]->file_name);
        g_assert_cmpstr(frame->file_name, ==, frames[1]->file_name);

        g_assert_cmpuint(frame->file_line, ==, frames[1]->file_line);

        g_assert_false(frame->class_path == frames[1]->class_path);
        g_assert_cmpstr(frame->class_path, ==, frames[1]->class_path);

        g_assert_cmpint(sr_java_frame_cmp(frame, frames[1]), ==, 0);
        sr_java_frame_free(frame);
    }

    /* Test the duplication with the siblings. */
    {
        struct sr_java_frame *frame;

        frame = sr_java_frame_dup(frames[1], true);

        g_assert_false(frame->name == frames[1]->name);
        g_assert_cmpstr(frame->name, ==, frames[1]->name);

        g_assert_false(frame->file_name == frames[1]->file_name);
        g_assert_cmpstr(frame->file_name, ==, frames[1]->file_name);

        g_assert_true(frame->file_line == frames[1]->file_line);

        g_assert_false(frame->class_path == frames[1]->class_path);
        g_assert_cmpstr(frame->class_path, ==, frames[1]->class_path);

        g_assert_false(frame->next == frames[0]);
        g_assert_cmpint(sr_java_frame_cmp(frame->next, frames[0]), ==, 0);

        sr_java_frame_free_full(frame);
    }

    exception_frame = sr_java_frame_new_exception();

    exception_frame->name = g_strdup("com.sun.java.NullPointer");
    exception_frame->message = g_strdup("Null pointer exception");
    exception_frame->next = frames[1];

    {
        struct sr_java_frame *frame;

        frame = sr_java_frame_dup(exception_frame, false);

        g_assert_null(frame->next);

        g_assert_false(frame->name == exception_frame->name);
        g_assert_cmpstr(frame->name, ==, exception_frame->name);

        g_assert_false(frame->message == exception_frame->message);
        g_assert_cmpstr(frame->message, ==, exception_frame->message);

        sr_java_frame_free(frame);
    }

    {
        struct sr_java_frame *frame;

        frame = sr_java_frame_dup(exception_frame, true);

        g_assert_nonnull(frame->next);
        g_assert_false(frame->next == frames[1]);

        g_assert_false(frame->name == exception_frame->name);
        g_assert_cmpstr(frame->name, ==, exception_frame->name);

        g_assert_false(frame->message == exception_frame->message);
        g_assert_cmpstr(frame->message, ==, exception_frame->message);

        g_assert_cmpint(sr_java_frame_cmp(frame->next, frames[1]), ==, 0);
        g_assert_cmpint(sr_java_frame_cmp(frame->next->next, frames[0]), ==, 0);

        sr_java_frame_free_full(frame);
    }

    sr_java_frame_free(exception_frame);

    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        sr_java_frame_free(frames[i]);
    }
}

static void
test_java_frame_append_to_str_check(struct sr_java_frame *frame,
                                    const char           *string)
{
    GString *strbuf;

    strbuf = g_string_new(NULL);

    sr_java_frame_append_to_str(frame, strbuf);

    g_assert_cmpstr(strbuf->str, ==, string);

    g_string_free(strbuf, TRUE);
}

static void
test_java_frame_append_to_str(void)
{
    struct sr_java_frame frame;
    struct sr_java_frame *exception_frame;

    sr_java_frame_init(&frame);

    frame.name = "com.redhat.abrt.duke.nuke";
    frame.file_name = "duke.java";
    frame.file_line = 666;

    test_java_frame_append_to_str_check(&frame,
                                        "\tat com.redhat.abrt.duke.nuke(duke.java:666) [unknown]");

    frame.file_line = 0;
    frame.is_native = true;

    test_java_frame_append_to_str_check(&frame,
                                        "\tat com.redhat.abrt.duke.nuke(Native Method) [unknown]");

    frame.is_native = false;
    frame.file_name = NULL;

    test_java_frame_append_to_str_check(&frame,
                                        "\tat com.redhat.abrt.duke.nuke(Unknown Source) [unknown]");

    frame.class_path = "/usr/lib/java/Foo.class";

    test_java_frame_append_to_str_check(&frame,
                                        "\tat com.redhat.abrt.duke.nuke(Unknown Source) [file:/usr/lib/java/Foo.class]");

    frame.class_path = "http://localhost/Foo.class";

    test_java_frame_append_to_str_check(&frame,
                                        "\tat com.redhat.abrt.duke.nuke(Unknown Source) [http://localhost/Foo.class]");

    exception_frame = sr_java_frame_new_exception();

    exception_frame->name = g_strdup("com.sun.java.NullPointer");
    exception_frame->message = g_strdup("Null pointer exception");

    test_java_frame_append_to_str_check(exception_frame,
                                        "com.sun.java.NullPointer: Null pointer exception");

    g_clear_pointer(&exception_frame->message, g_free);

    test_java_frame_append_to_str_check(exception_frame,
                                        "com.sun.java.NullPointer");

    g_clear_pointer(&exception_frame->name, g_free);

    test_java_frame_append_to_str_check(exception_frame, "");

    exception_frame->message = g_strdup("Null pointer exception");

    test_java_frame_append_to_str_check(exception_frame,
                                        ": Null pointer exception");

    sr_java_frame_free(exception_frame);
}

static void
test_java_frame_parse_check(const char           *input,
                            const char           *expected_input,
                            struct sr_java_frame *expected_frame,
                            struct sr_location   *expected_location)
{
    struct sr_location location;
    struct sr_java_frame *frame;

    sr_location_init(&location);

    frame = sr_java_frame_parse(&input, &location);

    g_assert_nonnull(frame);
    g_assert_true(input == expected_input);
    g_assert_cmpint(sr_java_frame_cmp(frame, expected_frame), ==, 0);
    g_assert_cmpint(sr_location_cmp(&location, expected_location, true), ==, 0);

    sr_java_frame_free(frame);
}

static void
test_java_frame_parse(void)
{
    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:666)";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = g_strdup("com.redhat.abrt.duke.nuke");
        frame.file_name = g_strdup("duke.java");
        frame.file_line = 666;

        location.line = 1;
        location.column = 46;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
        g_free(frame.name);
        g_free(frame.file_name);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:666) [file:/usr/lib/java/foo.class]";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.file_line = 666;
        frame.class_path = "/usr/lib/java/foo.class";

        location.line = 1;
        location.column = 77;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:666) [http://localhost/foo.class]";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.file_line = 666;
        frame.class_path = "http://localhost/foo.class";

        location.line = 1;
        location.column = 75;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:666) [jar:file:/usr/lib/java/foo.jar!/foo.class]";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.file_line = 666;
        frame.class_path = "/usr/lib/java/foo.jar";

        location.line = 1;
        location.column = 90;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:666) [jar:http://localhost/foo.jar!/foo.class]";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.file_line = 666;
        frame.class_path = "http://localhost/foo.jar";

        location.line = 1;
        location.column = 88;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:666) [unknown]";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.file_line = 666;

        location.line = 1;
        location.column = 56;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(Unknown Source:666)\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_line = 666;

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at (duke.java:666)\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.file_name = "duke.java";
        frame.file_line = 666;

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java)\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = g_strdup("com.redhat.abrt.duke.nuke");
        frame.file_name = g_strdup("duke.java");

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
        g_free(frame.name);
        g_free(frame.file_name);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:-1) [file:/usr/lib/java/foo.class]\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.class_path = "/usr/lib/java/foo.class";

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(duke.java:-1) [file:/home/user/lib/java/foo.class]\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.class_path = "/home/anonymized/lib/java/foo.class";

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "    at com.redhat.abrt.duke.nuke(Native Method)\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.is_native = true;

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }

    {
        struct sr_java_frame frame;
        struct sr_location location;
        const char *input = "  at (Unknown Source)\n";

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        location.line = 2;
        location.column = 0;

        test_java_frame_parse_check(input, input + strlen(input), &frame, &location);
    }
}

static void
test_java_frame_parse_failing(void)
{
    const char *original_input;
    const char *input;
    struct sr_location parsed_location;
    struct sr_java_frame *parsed_frame;
    struct sr_location location;

    original_input = " fasdfd";
    input = original_input;

    sr_location_init(&parsed_location);
    sr_location_init(&location);

    location.message = "Frame expected";

    parsed_frame = sr_java_frame_parse(&input, &parsed_location);

    g_assert_null(parsed_frame);
    g_assert_true(input == original_input);
    g_assert_cmpint(sr_location_cmp(&parsed_location, &location, true), ==, 0);
}

static void
test_java_frame_parse_exception_check(const char           *input,
                                      const char           *expected_input,
                                      const char           *expected_exception,
                                      struct sr_java_frame *expected_frame,
                                      struct sr_location   *expected_location)
{
    struct sr_location location;
    struct sr_java_frame *parsed_frame;
    struct sr_java_frame *frame;
    GString *buf;

    sr_location_init(&location);

    parsed_frame = sr_java_frame_parse_exception(&input, &location);

    g_assert_true(input == expected_input);

    buf = g_string_new(NULL);

    sr_java_frame_append_to_str(parsed_frame, buf);

    g_assert_cmpstr(expected_exception, ==, buf->str);

    g_string_free(buf, TRUE);

    for (frame = parsed_frame;
         NULL != frame && NULL != expected_frame;
         frame = frame->next, expected_frame = expected_frame->next)
    {
        g_assert_cmpint(sr_java_frame_cmp(frame, expected_frame), ==, 0);
    }

    g_assert_true(frame == expected_frame);
    g_assert_cmpint(sr_location_cmp(&location, expected_location, true), ==, 0);

    sr_java_frame_free_full(parsed_frame);
}

static void
test_java_frame_parse_exception(void)
{
    {
        struct sr_java_frame *frame;
        struct sr_location location;
        const char *input = "com.sun.java.NullPointer: Null Pointer Exception";

        frame = sr_java_frame_new_exception();

        sr_location_init(&location);

        frame->name = g_strdup("com.sun.java.NullPointer");
        frame->message = g_strdup("Null Pointer Exception");

        location.line = 1;
        location.column = 48;

        test_java_frame_parse_exception_check(input, input + strlen(input),
                                              input, frame, &location);

        sr_java_frame_free(frame);
    }

    {
        struct sr_java_frame *frame;
        struct sr_location location;
        const char *input = "com.sun.java.NullPointer: ";

        frame = sr_java_frame_new_exception();

        sr_location_init(&location);

        frame->name = g_strdup("com.sun.java.NullPointer");

        location.line = 1;
        location.column = 26;

        test_java_frame_parse_exception_check(input, input + strlen(input),
                                              "com.sun.java.NullPointer",
                                              frame, &location);

        sr_java_frame_free(frame);
    }

    {
        struct sr_java_frame *frame;
        struct sr_location location;
        const char *input = "com.sun.java.NullPointer  ";

        frame = sr_java_frame_new_exception();

        sr_location_init(&location);

        frame->name = g_strdup("com.sun.java.NullPointer");

        location.line = 1;
        location.column = 26;

        test_java_frame_parse_exception_check(input, input + strlen(input),
                                              "com.sun.java.NullPointer",
                                              frame, &location);

        sr_java_frame_free(frame);
    }

    {
        struct sr_java_frame frame;
        struct sr_java_frame *exception_frame;
        struct sr_location location;
        const char *input =
            "com.sun.java.NullPointer: Null Pointer Exception\n"
            "\tat com.redhat.abrt.duke.nuke(duke.java:666)";

        exception_frame = sr_java_frame_new_exception();

        sr_java_frame_init(&frame);
        sr_location_init(&location);

        frame.name = "com.redhat.abrt.duke.nuke";
        frame.file_name = "duke.java";
        frame.file_line = 666;

        exception_frame->name = g_strdup("com.sun.java.NullPointer");
        exception_frame->message = g_strdup("Null Pointer Exception");
        exception_frame->next = &frame;

        location.line = 2;
        location.column = 43;

        test_java_frame_parse_exception_check(input, input + strlen(input),
                                              "com.sun.java.NullPointer: Null Pointer Exception",
                                              exception_frame, &location);
        sr_java_frame_free(exception_frame);
    }

    {
        struct sr_java_frame *frame = sr_java_frame_new();
        struct sr_java_frame *frame1;
        struct sr_java_frame *frame2;
        struct sr_java_frame *exception_frames[3];
        struct sr_location location;
        const char *input =
            "com.sun.java.NullPointer: Null Pointer Exception\n"
            "\tat com.redhat.abrt.duke.nuke(duke.java:666)\n"
            "\tCaused by: com.sun.java.InvalidOperation: Invalid Operation\n"
            "\tat com.redhat.abrt.duke.nuke(duke.java:666)\n"
            "\t... 1 more\n"
            "\tCaused by: com.sun.java.InvalidOperation: Invalid Operation\n"
            "\tat com.redhat.abrt.duke.nuke(duke.java:666)\n"
            "\t... 2 more";

        frame->name = "com.redhat.abrt.duke.nuke";
        frame->file_name = "duke.java";
        frame->file_line = 666;

        frame1 = sr_java_frame_dup(frame, false);
        frame2 = sr_java_frame_dup(frame, false);
        exception_frames[0] = sr_java_frame_new_exception();
        exception_frames[1] = sr_java_frame_new_exception();

        sr_location_init(&location);

        exception_frames[0]->name = g_strdup("com.sun.java.NullPointer");
        exception_frames[0]->message = g_strdup("Null Pointer Exception");
        exception_frames[0]->next = frame;

        exception_frames[1]->name = g_strdup("com.sun.java.InvalidOperation");

        exception_frames[2] = sr_java_frame_dup(exception_frames[1], false);

        frame1->next = exception_frames[0];
        exception_frames[1]->next = frame1;
        frame2->next = exception_frames[1];
        exception_frames[2]->next = frame2;

        location.line = 8;
        location.column = 11;

        test_java_frame_parse_exception_check(input, input + strlen(input),
                                              "com.sun.java.InvalidOperation: Invalid Operation",
                                              exception_frames[2], &location);
        sr_java_frame_free(exception_frames[2]);
        sr_java_frame_free(frame2);
        sr_java_frame_free(exception_frames[1]);
        sr_java_frame_free(frame1);
        sr_java_frame_free(exception_frames[0]);
        g_free(frame);
    }
    
}

static void
test_java_frame_parse_exception_failing(void)
{
    struct sr_location location;
    const char *exceptions[] =
    {
        ": Null Pointer Exception",
        "",
    };

    sr_location_init(&location);

    location.line = 1;
    location.column = 0;
    location.message = "Expected exception name";

    for (size_t i = 0; i < G_N_ELEMENTS(exceptions); i++)
    {
        const char *input;
        struct sr_location parsed_location;
        struct sr_java_frame *parsed_frame;

        input = exceptions[i];

        sr_location_init(&parsed_location);

        parsed_frame = sr_java_frame_parse_exception(&input, &parsed_location);

        g_assert_null(parsed_frame);
        g_assert_true(input == exceptions[i]);
        g_assert_cmpint(sr_location_cmp(&parsed_location, &location, true), ==, 0);
    }
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/java/cmp", test_java_frame_cmp);
    g_test_add_func("/frame/java/dup", test_java_frame_dup);
    g_test_add_func("/frame/java/append-to-str", test_java_frame_append_to_str);
    g_test_add_func("/frame/java/parse", test_java_frame_parse);
    g_test_add_func("/frame/java/parse/failing", test_java_frame_parse_failing);
    g_test_add_func("/frame/java/exception/parse", test_java_frame_parse_exception);
    g_test_add_func("/frame/java/exception/parse/failing", test_java_frame_parse_exception_failing);

    return g_test_run();
}
