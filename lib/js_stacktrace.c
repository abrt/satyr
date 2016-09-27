/*
    js_stacktrace.c

    Copyright (C) 2016  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "js/platform.h"
#include "js/stacktrace.h"
#include "js/frame.h"
#include "location.h"
#include "utils.h"
#include "json.h"
#include "sha1.h"
#include "report_type.h"
#include "strbuf.h"
#include "generic_stacktrace.h"
#include "generic_thread.h"
#include "internal_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Method tables */

static void
js_append_bthash_text(struct sr_js_stacktrace *stacktrace, enum sr_bthash_flags flags,
                          struct sr_strbuf *strbuf);

DEFINE_FRAMES_FUNC(js_frames, struct sr_js_stacktrace)
DEFINE_SET_FRAMES_FUNC(js_set_frames, struct sr_js_stacktrace)
DEFINE_PARSE_WRAPPER_FUNC(js_parse, SR_REPORT_JAVASCRIPT)

struct thread_methods js_thread_methods =
{
    .frames = (frames_fn_t) js_frames,
    .set_frames = (set_frames_fn_t) js_set_frames,
    .cmp = (thread_cmp_fn_t) NULL,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) thread_no_next_thread,
    .set_next = (set_next_thread_fn_t) NULL,
    .thread_append_bthash_text =
        (thread_append_bthash_text_fn_t) thread_no_bthash_text,
    .thread_free = (thread_free_fn_t) sr_js_stacktrace_free,
    .remove_frame = (remove_frame_fn_t) thread_remove_frame,
    .remove_frames_above =
        (remove_frames_above_fn_t) thread_remove_frames_above,
    .thread_dup = (thread_dup_fn_t) sr_js_stacktrace_dup,
    .normalize = (normalize_fn_t) thread_no_normalization,
};

struct stacktrace_methods js_stacktrace_methods =
{
    .parse = (parse_fn_t) js_parse,
    .parse_location = (parse_location_fn_t) sr_js_stacktrace_parse,
    .to_short_text = (to_short_text_fn_t) stacktrace_to_short_text,
    .to_json = (to_json_fn_t) sr_js_stacktrace_to_json,
    .from_json = (from_json_fn_t) sr_js_stacktrace_from_json,
    .get_reason = (get_reason_fn_t) sr_js_stacktrace_get_reason,
    .find_crash_thread = (find_crash_thread_fn_t) stacktrace_one_thread_only,
    .threads = (threads_fn_t) stacktrace_one_thread_only,
    .set_threads = (set_threads_fn_t) NULL,
    .stacktrace_free = (stacktrace_free_fn_t) sr_js_stacktrace_free,
    .stacktrace_append_bthash_text =
        (stacktrace_append_bthash_text_fn_t) js_append_bthash_text,
};

/* Public functions */

struct sr_js_stacktrace *
sr_js_stacktrace_new()
{
    struct sr_js_stacktrace *stacktrace =
        sr_malloc(sizeof(struct sr_js_stacktrace));

    sr_js_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_js_stacktrace_init(struct sr_js_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct sr_js_stacktrace));
    stacktrace->type = SR_REPORT_JAVASCRIPT;
}

void
sr_js_stacktrace_free(struct sr_js_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->frames)
    {
        struct sr_js_frame *frame = stacktrace->frames;
        stacktrace->frames = frame->next;
        sr_js_frame_free(frame);
    }

    free(stacktrace->exception_name);
    free(stacktrace);
}

struct sr_js_stacktrace *
sr_js_stacktrace_dup(struct sr_js_stacktrace *stacktrace)
{
    struct sr_js_stacktrace *result = sr_js_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_js_stacktrace));

    if (result->exception_name)
        result->exception_name = sr_strdup(result->exception_name);

    if (result->frames)
        result->frames = sr_js_frame_dup(result->frames, true);

    if (result->platform)
        result->platform = sr_js_platform_dup(result->platform);

    return result;
}

struct sr_js_stacktrace *
sr_js_stacktrace_parse_v8(const char **input,
                          struct sr_location *location)
{
    const char *local_input = *input;
    struct sr_js_stacktrace *stacktrace = sr_js_stacktrace_new();

    /*
     * ReferenceError: nonexistentFunc is not defined
     * ^^^^^^^^^^^^^^
     */
    const int columns = sr_parse_char_cspan(&local_input, ":\n", &stacktrace->exception_name);
    if (local_input[0] != ':')
    {
        location->message = "Unable to find the colon right behind exception type.";
        goto fail;
    }

    if (columns == 0)
    {
        location->message = "Zero length exception type.";
        goto fail;
    }

    sr_location_add(location, 0, columns);

    /*
     * ReferenceError: nonexistentFunc is not defined
     *               ^^
     */
    if (!sr_skip_string(&local_input, ": "))
    {
        location->message = "Exception type not followed by white space.";
        goto fail;
    }

    /*
     * ReferenceError: nonexistentFunc is not defined
     *                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     *
     * Ignore error message because it may contain security sensitive data.
     */
    if (!sr_skip_to_next_line_location(&local_input, &location->line, &location->column))
    {
        location->message = "Stack trace does not include any frames.";
        goto fail;
    }

    struct sr_js_frame *last_frame = NULL;
    while (*local_input != '\0')
    {
        struct sr_js_frame *current_frame = sr_js_frame_parse_v8(&local_input,
                                                                 location);
        if (current_frame == NULL)
            goto fail;

        if (stacktrace->frames == NULL)
            stacktrace->frames = current_frame;
        else
            last_frame->next = current_frame;

        /* Eat newline (except at the end of file). */
        if (!sr_skip_char(&local_input, '\n') && *local_input != '\0')
        {
            location->message = "Expected newline after stacktrace frame.";
            goto fail;
        }

        location->column = 0;
        location->line++;

        last_frame = current_frame;
    }

    *input = local_input;
    return stacktrace;

fail:
    sr_js_stacktrace_free(stacktrace);
    return NULL;
}

struct sr_js_stacktrace *
sr_js_stacktrace_parse(const char **input,
                       struct sr_location *location)
{
    struct sr_js_stacktrace *stacktrace = NULL;

    /* In the future, iterate over all platforms and
     * call the sr_js_platform_parse_stacktrace() function.
     * Why?
     * Because we want to know the platform where the stacktrace was caught.
     */
    stacktrace = sr_js_stacktrace_parse_v8(input, location);
    if (stacktrace != NULL)
    {
        stacktrace->platform = sr_js_platform_new();
        sr_js_platform_init(stacktrace->platform, 0, SR_JS_ENGINE_V8);

        return stacktrace;
    }

    location->message = "The stacktrace does not match any JavaScript dialect";
    return NULL;
}

char *
sr_js_stacktrace_to_json(struct sr_js_stacktrace *stacktrace)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Exception class name. */
    if (stacktrace->exception_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"exception_name\": ");
        sr_json_append_escaped(strbuf, stacktrace->exception_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    /* Frames. */
    if (stacktrace->frames)
    {
        struct sr_js_frame *frame = stacktrace->frames;
        sr_strbuf_append_str(strbuf, ",   \"stacktrace\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                sr_strbuf_append_str(strbuf, "      [ ");
            else
                sr_strbuf_append_str(strbuf, "      , ");

            char *frame_json = sr_js_frame_to_json(frame);
            char *indented_frame_json = sr_indent_except_first_line(frame_json, 8);
            sr_strbuf_append_str(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                sr_strbuf_append_str(strbuf, "\n");
        }

        sr_strbuf_append_str(strbuf, " ]\n");
    }

    /* Platform.*/
    if (stacktrace->platform)
    {
        sr_strbuf_append_str(strbuf, ",   \"platform\":\n        ");
        char *platform_json = sr_js_platform_to_json(stacktrace->platform);
        char *indented_platform_json = sr_indent_except_first_line(platform_json, 8);
        sr_strbuf_append_str(strbuf, indented_platform_json);
        free(indented_platform_json);
        free(platform_json);
    }

    if (strbuf->len > 0)
        strbuf->buf[0] = '{';
    else
        sr_strbuf_append_char(strbuf, '{');

    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}

struct sr_js_stacktrace *
sr_js_stacktrace_from_json(struct sr_json_value *root, char **error_message)
{
    if (!JSON_CHECK_TYPE(root, SR_JSON_OBJECT, "stacktrace"))
        return NULL;

    struct sr_js_stacktrace *result = sr_js_stacktrace_new();

    /* Exception name. */
    if (!JSON_READ_STRING(root, "exception_name", &result->exception_name))
        goto fail;

    /* Frames. */
    struct sr_json_value *stacktrace = json_element(root, "stacktrace");
    if (stacktrace)
    {
        if (!JSON_CHECK_TYPE(stacktrace, SR_JSON_ARRAY, "stacktrace"))
            goto fail;

        struct sr_json_value *frame_json;
        FOR_JSON_ARRAY(stacktrace, frame_json)
        {
            struct sr_js_frame *frame = sr_js_frame_from_json(frame_json,
                error_message);

            if (!frame)
                goto fail;

            result->frames = sr_js_frame_append(result->frames, frame);
        }
    }

    /* Platform. */
    struct sr_json_value *platform = json_element(root, "platform");
    if (platform)
    {
        result->platform = sr_js_platform_from_json(platform, error_message);
        if (*error_message)
            goto fail;
    }

    return result;

fail:
    sr_js_stacktrace_free(result);
    return NULL;
}

char *
sr_js_stacktrace_get_reason(struct sr_js_stacktrace *stacktrace)
{
    char *exc = "Error";
    char *file = "<unknown>";
    uint32_t line = 0, column = 0;

    struct sr_js_frame *frame = stacktrace->frames;
    if (frame)
    {
        file = frame->file_name;
        line = frame->file_line;
        column = frame->line_column;
    }

    if (stacktrace->exception_name)
        exc = stacktrace->exception_name;

    return sr_asprintf("%s at %s:%"PRIu32":%"PRIu32, exc, file, line, column);
}

static void
js_append_bthash_text(struct sr_js_stacktrace *stacktrace, enum sr_bthash_flags flags,
                        struct sr_strbuf *strbuf)
{
    sr_strbuf_append_strf(strbuf, "Error: %s\n", OR_UNKNOWN(stacktrace->exception_name));
    sr_strbuf_append_char(strbuf, '\n');
}
