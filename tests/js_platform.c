#include "js/platform.h"
#include "js/frame.h"
#include "json.h"
#include "location.h"
#include "utils.h"
#include <stdio.h>
#include <glib.h>

#define check1(a_engine, a_expected) \
    do { \
        const char *value = sr_js_engine_to_string(a_engine); \
        if (g_strcmp0(value, a_expected) != 0) { \
            fprintf(stderr, "%s != %s\n", value, a_expected); \
            g_assert_true(!"sr_js_engine_to_string(" #a_engine ") != " #a_expected); \
        } \
    } while (0)

static void
test_js_engine_to_string(void)
{
    check1(0, NULL);
    check1(SR_JS_ENGINE_V8, "V8");
    check1(_SR_JS_ENGINE_UPPER_BOUND, NULL);
    check1(_SR_JS_ENGINE_UNINIT, NULL);
}

#define check2(a_engine_str, a_expected) \
    do { \
        enum sr_js_engine t_value = sr_js_engine_from_string(a_engine_str); \
        if (t_value != a_expected) { \
            fprintf(stderr, "%d != %d\n", t_value, a_expected); \
            g_assert_true(!"sr_js_engine_from_string(" #a_engine_str ") != " #a_expected); \
        }\
    } while (0)
    
static void
test_js_engine_from_string(void)
{
    check2("foo", 0);
    check2("V8", SR_JS_ENGINE_V8);
}

#define check3(a_runtime, a_expected) \
    do { \
        const char *t_value = sr_js_runtime_to_string(a_runtime); \
        if (g_strcmp0(t_value, a_expected) != 0) { \
            fprintf(stderr, "%s != %s\n", t_value, a_expected); \
            g_assert_true(!"sr_js_runtime_to_string(" #a_runtime ") != " #a_expected); \
        } \
    } while (0)

static void
test_js_runtime_to_string(void)
{
    check3(0, NULL);
    check3(SR_JS_RUNTIME_NODEJS, "Node.js");
    check3(_SR_JS_RUNTIME_UPPER_BOUND, NULL);
    check3(_SR_JS_RUNTIME_UNINIT, NULL);
}

#define check4(a_runtime_str, a_expected) \
    do { \
        enum sr_js_runtime t_value = sr_js_runtime_from_string(a_runtime_str); \
        if (t_value != a_expected) { \
            fprintf(stderr, "%d != %d\n", t_value, a_expected); \
            g_assert_true(!"sr_js_runtime_from_string(" #a_runtime_str ") != " #a_expected); \
        }\
    } while (0)

static void
test_js_runtime_from_string(void)
{
    check4("foo", 0);
    check4("Node.js", SR_JS_RUNTIME_NODEJS);
}

static void
test_js_platform_basics(void)
{
    sr_js_platform_t platform;
    g_assert_true(platform = sr_js_platform_new());

    sr_js_platform_init(platform, SR_JS_RUNTIME_NODEJS, SR_JS_ENGINE_V8);

    g_assert_true(sr_js_platform_engine(platform) == SR_JS_ENGINE_V8);
    g_assert_true(sr_js_platform_runtime(platform) == SR_JS_RUNTIME_NODEJS);

    sr_js_platform_t another_platform;
    g_assert_true(another_platform = sr_js_platform_dup(platform));

    /* In the future we might need also the above test but now the test would
     * fail because sr_js_platform_t is a pure integer.
     *
     * g_assert_false(another_platform == platform);
     */

    g_assert_true(sr_js_platform_engine(platform) == sr_js_platform_engine(another_platform));
    g_assert_true(sr_js_platform_runtime(platform) == sr_js_platform_runtime(another_platform));

    sr_js_platform_free(another_platform);
}

#define check5(i_runtime_str, i_version_str, e_bool, e_error_message, e_runtime, e_engine) \
    do { \
        char *r_error_message = NULL; \
        sr_js_platform_t r_platform = sr_js_platform_from_string(i_runtime_str, \
                                                                 i_version_str, \
                                                                 &r_error_message); \
        g_assert_true((!!r_platform) == e_bool || !"Should be to "#e_bool); \
        if (r_platform) { \
            if (r_error_message != NULL) { \
                fprintf(stderr, "Error message: %s\n", r_error_message); \
                g_assert_true(!"Got error message: sr_js_platform_from_string(" \
                        #i_runtime_str "," #i_version_str ")"); \
            } \
            if (sr_js_platform_engine(r_platform) != e_engine) { \
                fprintf(stderr, "Engine: %d != %d\n", \
                                sr_js_platform_engine(r_platform), \
                                e_engine); \
                g_assert_true(!"sr_js_platform_engine(sr_js_platform_from_string(" \
                        #i_runtime_str "," #i_version_str ")) != " #e_engine); \
            } \
            if (sr_js_platform_runtime(r_platform) != e_runtime) { \
                fprintf(stderr, "Runtime: %d != %d\n", \
                                sr_js_platform_runtime(r_platform), \
                                e_runtime); \
                g_assert_true(!"sr_js_platform_runtime(sr_js_platform_from_string(" \
                        #i_runtime_str "," #i_version_str ")) != " #e_runtime); \
            } \
        } \
        else { \
            if (g_strcmp0(r_error_message, e_error_message) != 0) { \
                fprintf(stderr, "%s != %s\n", r_error_message, e_error_message); \
                g_assert_true(!"Got invalid error message: sr_js_platform_from_string(" \
                        #i_runtime_str "," #i_version_str ")"); \
            } \
            free(r_error_message); \
        } \
    } while (0)

static void
test_js_platform_from_string(void)
{
    check5("foo",
        NULL,
        false,
        "No known JavaScript platform with runtime 'foo'",
        0,
        0);

    check5("foo",
        "blah",
        false,
        "No known JavaScript platform with runtime 'foo'",
        0,
        0);

    check5(sr_js_runtime_to_string(SR_JS_RUNTIME_NODEJS),
        NULL,
        true,
        NULL,
        SR_JS_RUNTIME_NODEJS,
        SR_JS_ENGINE_V8);

    /* Version is ignored. */
    check5(sr_js_runtime_to_string(SR_JS_RUNTIME_NODEJS),
        "foo",
        true,
        NULL,
        SR_JS_RUNTIME_NODEJS,
        SR_JS_ENGINE_V8);
}

#define check6(i_runtime, i_engine, e_json) \
    do { \
        sr_js_platform_t platform = sr_js_platform_new(); \
        sr_js_platform_init(platform, i_runtime, i_engine); \
        char *r_json = sr_js_platform_to_json(platform); \
        g_assert_nonnull(r_json); \
        if (g_strcmp0(r_json, e_json) != 0) { \
            fprintf(stderr, "%s\n!=\n%s\n", r_json, e_json); \
            g_assert_true(!"sr_js_platform_to_json(("#i_runtime","#i_engine"))");\
        } \
        free(r_json); \
    } while (0)

static void
test_js_platform_to_json(void)
{
    check6(0, 0,
        "{    \"engine\": \"<unknown>\"\n" \
        ",    \"runtime\": \"<unknown>\"\n" \
        "}\n");

    check6(_SR_JS_RUNTIME_UPPER_BOUND, _SR_JS_ENGINE_UPPER_BOUND,
        "{    \"engine\": \"<unknown>\"\n" \
        ",    \"runtime\": \"<unknown>\"\n" \
        "}\n");

    check6(_SR_JS_RUNTIME_UNINIT, _SR_JS_ENGINE_UNINIT,
        "{    \"engine\": \"<unknown>\"\n" \
        ",    \"runtime\": \"<unknown>\"\n" \
        "}\n");

    check6(SR_JS_RUNTIME_NODEJS, 0,
        "{    \"engine\": \"<unknown>\"\n" \
        ",    \"runtime\": \"Node.js\"\n" \
        "}\n");

    check6(0, SR_JS_ENGINE_V8,
        "{    \"engine\": \"V8\"\n" \
        ",    \"runtime\": \"<unknown>\"\n" \
        "}\n");

    check6(SR_JS_RUNTIME_NODEJS, SR_JS_ENGINE_V8,
        "{    \"engine\": \"V8\"\n" \
        ",    \"runtime\": \"Node.js\"\n" \
        "}\n");
}

#define check7(i_json_str, e_bool, e_runtime, e_engine, e_error_message) \
    do { \
        char *r_error_message = NULL; \
        enum json_tokener_error error; \
        json_object *i_json = json_tokener_parse_verbose(i_json_str, &error); \
        if (NULL == i_json) \
        { \
            r_error_message = (char *)json_tokener_error_desc(error); \
        } \
        g_assert_true(r_error_message == NULL || !"Failed to parse test JSON"); \
        sr_js_platform_t r_platform = sr_js_platform_from_json(i_json, &r_error_message); \
        json_object_put(i_json); \
        g_assert_true((!!r_platform) == e_bool || !"Should be to "#e_bool); \
        if (r_platform) { \
            if (r_error_message != NULL) { \
                fprintf(stderr, "Error message: %s\n", r_error_message); \
                g_assert_true(!"Got error message: sr_js_platform_from_json(" #i_json_str ")"); \
            } \
            if (sr_js_platform_engine(r_platform) != e_engine) { \
                fprintf(stderr, "Engine: %d != %d\n", \
                                sr_js_platform_engine(r_platform), \
                                e_engine); \
                g_assert_true(!"sr_js_platform_engine(sr_js_platform_from_json(" \
                        #i_json_str ")) != " #e_engine); \
            } \
            if (sr_js_platform_runtime(r_platform) != e_runtime) { \
                fprintf(stderr, "Runtime: %d != %d\n", \
                                sr_js_platform_runtime(r_platform), \
                                e_runtime); \
                g_assert_true(!"sr_js_platform_runtime(sr_js_platform_from_json(" \
                        #i_json_str ")) != " #e_runtime); \
            } \
        } \
        else{ \
            if (g_strcmp0(r_error_message, e_error_message) != 0) { \
                fprintf(stderr, "%s != %s\n", r_error_message, e_error_message); \
                g_assert_true(!"Got invalid error message: sr_js_platform_from_json(" \
                        #i_json_str ")"); \
            } \
            free(r_error_message); \
        } \
    } while (0)

static void
test_js_platform_from_json(void)
{
    check7("{}",
        false, 0, 0,
        "No 'engine' member");

    check7("{ \"engine\": \"foo\" }",
        false, 0, 0,
        "Unknown JavaScript engine 'foo'");

    check7("{ \"engine\": \"V8\" }",
        false, 0, 0,
        "No 'runtime' member");

    check7("{ \"runtime\": \"blah\" }",
        false, 0, 0,
        "No 'engine' member");

    check7("{ \"runtime\": \"Node.js\" }",
        false, 0, 0,
        "No 'engine' member");

    check7("{ \"engine\": \"foo\", \"runtime\": \"blah\" }",
        false, 0, 0,
        "Unknown JavaScript engine 'foo'");

    check7("{ \"engine\": \"V8\", \"runtime\": \"blah\" }",
        false, 0, 0,
        "Unknown JavaScript runtime 'blah'");

    check7("{ \"engine\": \"foo\", \"runtime\": \"Node.js\" }",
        false, 0, 0,
        "Unknown JavaScript engine 'foo'");

    check7("{ \"engine\": \"V8\", \"runtime\": \"Node.js\" }",
        true, SR_JS_RUNTIME_NODEJS, SR_JS_ENGINE_V8, "No error expected");
}

#define check_invalid(i_engine, i_runtime) \
    do { \
        sr_js_platform_t r_platform = sr_js_platform_new(); \
        sr_js_platform_init(r_platform, i_runtime, i_engine); \
        struct sr_js_frame *r_frame = sr_js_platform_parse_frame(r_platform, \
                                                                 NULL, \
                                                                 NULL); \
        g_assert_true(r_frame == NULL || !"Succeeded with " #i_engine ", " #i_runtime); \
    } while (0)

#define check_valid(i_engine, i_runtime, i_frame_str, e_frame) \
    do { \
        sr_js_platform_t r_platform = sr_js_platform_new(); \
        sr_js_platform_init(r_platform, i_runtime, i_engine); \
        struct sr_location r_location; \
        const char *input = i_frame_str; \
        struct sr_js_frame *r_frame = sr_js_platform_parse_frame(r_platform, \
                                                                 &input, \
                                                                 &r_location); \
        g_assert_true(r_frame != NULL || !"Failed with " #i_engine ", " #i_runtime); \
        if (sr_js_frame_cmp(r_frame, e_frame) != 0) { \
            GString *buf = g_string_new(NULL); \
            sr_js_frame_append_to_str(r_frame, buf); \
            fprintf(stderr, "%s\n", buf->str); \
            g_string_free(buf, TRUE); \
            g_assert_true(!"Failed with " #i_engine ", " #i_runtime ", " #i_frame_str); \
        } \
        sr_js_frame_free(r_frame); \
    } while (0)

static void
test_js_platform_parse_frame(void)
{
    check_invalid(0,                         0);
    check_invalid(_SR_JS_ENGINE_UPPER_BOUND, 0);
    check_invalid(_SR_JS_ENGINE_UNINIT,      0);
    check_invalid(SR_JS_ENGINE_V8,           0);

    check_invalid(0,                         _SR_JS_RUNTIME_UPPER_BOUND);
    check_invalid(_SR_JS_ENGINE_UPPER_BOUND, _SR_JS_RUNTIME_UPPER_BOUND);
    check_invalid(_SR_JS_ENGINE_UNINIT,      _SR_JS_RUNTIME_UPPER_BOUND);
    check_invalid(SR_JS_ENGINE_V8,           _SR_JS_RUNTIME_UPPER_BOUND);

    check_invalid(0,                         _SR_JS_RUNTIME_UNINIT);
    check_invalid(_SR_JS_ENGINE_UPPER_BOUND, _SR_JS_RUNTIME_UNINIT);
    check_invalid(_SR_JS_ENGINE_UNINIT,      _SR_JS_RUNTIME_UNINIT);
    check_invalid(SR_JS_ENGINE_V8,           _SR_JS_RUNTIME_UNINIT);

    check_invalid(0,                         SR_JS_RUNTIME_NODEJS);
    check_invalid(_SR_JS_ENGINE_UPPER_BOUND, SR_JS_RUNTIME_NODEJS);
    check_invalid(_SR_JS_ENGINE_UNINIT,      SR_JS_RUNTIME_NODEJS);

    struct sr_js_frame frame = {
        .type = SR_REPORT_JAVASCRIPT,
        .file_name = (char *)"bar.js",
        .file_line = 1,
        .line_column = 2,
        .function_name = (char *)"foo",
        .next = NULL,
    };

    check_valid(SR_JS_ENGINE_V8, SR_JS_RUNTIME_NODEJS, "at foo (bar.js:1:2)", &frame);
}


int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/js/engine-to-string", test_js_engine_to_string);
    g_test_add_func("/js/engine-from-string", test_js_engine_from_string);
    g_test_add_func("/js/runtime-to-string", test_js_runtime_to_string);
    g_test_add_func("/js/runtime-from-string", test_js_runtime_from_string);
    g_test_add_func("/js/platform-basics", test_js_platform_basics);
    g_test_add_func("/js/platform-from-string", test_js_platform_from_string);
    g_test_add_func("/js/platform-to-json", test_js_platform_to_json);
    g_test_add_func("/js/platform-from-json", test_js_platform_from_json);
    g_test_add_func("/js/platform-parse-frame", test_js_platform_parse_frame);

    return g_test_run();
}
