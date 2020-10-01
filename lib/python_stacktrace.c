/*
    python_stacktrace.c

    Copyright (C) 2012  Red Hat, Inc.

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
#include "python/stacktrace.h"
#include "python/frame.h"
#include "location.h"
#include "utils.h"
#include "json.h"
#include "sha1.h"
#include "report_type.h"
#include "generic_stacktrace.h"
#include "generic_thread.h"
#include "internal_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Method tables */

static void
python_append_bthash_text(struct sr_python_stacktrace *stacktrace, enum sr_bthash_flags flags,
                          GString *strbuf);

DEFINE_FRAMES_FUNC(python_frames, struct sr_python_stacktrace)
DEFINE_SET_FRAMES_FUNC(python_set_frames, struct sr_python_stacktrace)
DEFINE_PARSE_WRAPPER_FUNC(python_parse, SR_REPORT_PYTHON)

struct thread_methods python_thread_methods =
{
    .frames = (frames_fn_t) python_frames,
    .set_frames = (set_frames_fn_t) python_set_frames,
    .cmp = (thread_cmp_fn_t) NULL,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) thread_no_next_thread,
    .set_next = (set_next_thread_fn_t) NULL,
    .thread_append_bthash_text =
        (thread_append_bthash_text_fn_t) thread_no_bthash_text,
    .thread_free = (thread_free_fn_t) sr_python_stacktrace_free,
    .remove_frame = (remove_frame_fn_t) thread_remove_frame,
    .remove_frames_above =
        (remove_frames_above_fn_t) thread_remove_frames_above,
    .thread_dup = (thread_dup_fn_t) sr_python_stacktrace_dup,
    .normalize = (normalize_fn_t) thread_no_normalization,
};

struct stacktrace_methods python_stacktrace_methods =
{
    .parse = (parse_fn_t) python_parse,
    .parse_location = (parse_location_fn_t) sr_python_stacktrace_parse,
    .to_short_text = (to_short_text_fn_t) stacktrace_to_short_text,
    .to_json = (to_json_fn_t) sr_python_stacktrace_to_json,
    .from_json = (from_json_fn_t) sr_python_stacktrace_from_json,
    .get_reason = (get_reason_fn_t) sr_python_stacktrace_get_reason,
    .find_crash_thread = (find_crash_thread_fn_t) stacktrace_one_thread_only,
    .threads = (threads_fn_t) stacktrace_one_thread_only,
    .set_threads = (set_threads_fn_t) NULL,
    .stacktrace_free = (stacktrace_free_fn_t) sr_python_stacktrace_free,
    .stacktrace_append_bthash_text =
        (stacktrace_append_bthash_text_fn_t) python_append_bthash_text,
};

/* Public functions */

struct sr_python_stacktrace *
sr_python_stacktrace_new()
{
    struct sr_python_stacktrace *stacktrace =
        sr_malloc(sizeof(struct sr_python_stacktrace));

    sr_python_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_python_stacktrace_init(struct sr_python_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct sr_python_stacktrace));
    stacktrace->type = SR_REPORT_PYTHON;
}

void
sr_python_stacktrace_free(struct sr_python_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->frames)
    {
        struct sr_python_frame *frame = stacktrace->frames;
        stacktrace->frames = frame->next;
        sr_python_frame_free(frame);
    }

    free(stacktrace->exception_name);
    free(stacktrace);
}

struct sr_python_stacktrace *
sr_python_stacktrace_dup(struct sr_python_stacktrace *stacktrace)
{
    struct sr_python_stacktrace *result = sr_python_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_python_stacktrace));

    if (result->exception_name)
        result->exception_name = sr_strdup(result->exception_name);

    if (result->frames)
        result->frames = sr_python_frame_dup(result->frames, true);

    return result;
}

struct sr_python_stacktrace *
sr_python_stacktrace_parse(const char **input,
                           struct sr_location *location)
{
    const char *local_input = *input;

    /* Parse the header. */
    if (sr_skip_char(&local_input, '\n'))
        location->column += 1;

    const char *HEADER = "Traceback (most recent call last):\n";
    local_input = sr_strstr_location(local_input,
                                     HEADER,
                                     &location->line,
                                     &location->column);

    if (!local_input)
    {
        /* SyntaxError stack trace of an exception thrown in the executed file
         * conforms to the following template:
         * invalid syntax ($file, line $number)
         *
         *    File "$file", line $number
         *       $code
         *         ^
         * SyntaxError: invalid syntax
         *
         * for exceptions thrown from imported files, the stack trace has the
         * regular form, except the last frame has no function name and is
         * followed by the pointer line (^).
         */
        HEADER = "invalid syntax (",
        local_input = sr_strstr_location(*input,
                                     HEADER,
                                     &location->line,
                                     &location->column);

        if (!local_input)
        {
            location->message = "Traceback header not found.";
            return NULL;
        }

        local_input = sr_strstr_location(local_input,
                           "  File \"",
                           &location->line,
                           &location->column);

        if (!local_input)
        {
            location->message = "Frame with invalid line not found.";
            return NULL;
        }
    }
    else
    {
        local_input += strlen(HEADER);
        location->line += 2;
        location->column = 0;
    }

    struct sr_python_stacktrace *stacktrace = sr_python_stacktrace_new();

    /* Read the frames. */
    struct sr_python_frame *frame;
    struct sr_location frame_location;
    sr_location_init(&frame_location);
    while ((frame = sr_python_frame_parse(&local_input, &frame_location)))
    {
        /*
         * Python stacktraces are in reverse order than other types - we
         * reverse it here by prepending each frame to the list instead of
         * appending it.
         */
        frame->next = stacktrace->frames;
        stacktrace->frames = frame;

        sr_location_add(location,
                        frame_location.line,
                        frame_location.column);
    }

    if (!stacktrace->frames)
    {
        location->message = frame_location.message;
        sr_python_stacktrace_free(stacktrace);
        return NULL;
    }

    bool invalid_syntax_pointer = true;
    const char *tmp_input = local_input;
    while (*tmp_input != '\n' && *tmp_input != '\0')
    {
        if (*tmp_input != ' ' && *tmp_input != '^')
        {
            invalid_syntax_pointer = false;
            break;
        }
        ++tmp_input;
    }

    if (invalid_syntax_pointer)
    {
        /* Skip line "   ^" pointing to the invalid code */
        sr_skip_char_cspan(&local_input, "\n");
        ++local_input;
        ++location->line;
        location->column = 1;
    }

    /* Parse exception name. */
    if (!sr_parse_char_cspan(&local_input, ":\n", &stacktrace->exception_name))
    {

        location->message = "Unable to find the ':\\n' characters "
                            "identifying the end of exception name.";
        sr_python_stacktrace_free(stacktrace);
        return NULL;

    }

    *input = local_input;
    return stacktrace;
}

char *
sr_python_stacktrace_to_json(struct sr_python_stacktrace *stacktrace)
{
    GString *strbuf = g_string_new(NULL);

    /* Exception class name. */
    if (stacktrace->exception_name)
    {
        g_string_append(strbuf, ",   \"exception_name\": ");
        sr_json_append_escaped(strbuf, stacktrace->exception_name);
        g_string_append(strbuf, "\n");
    }

    /* Frames. */
    if (stacktrace->frames)
    {
        struct sr_python_frame *frame = stacktrace->frames;
        g_string_append(strbuf, ",   \"stacktrace\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                g_string_append(strbuf, "      [ ");
            else
                g_string_append(strbuf, "      , ");

            char *frame_json = sr_python_frame_to_json(frame);
            char *indented_frame_json = sr_indent_except_first_line(frame_json, 8);
            g_string_append(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                g_string_append(strbuf, "\n");
        }

        g_string_append(strbuf, " ]\n");
    }

    if (strbuf->len > 0)
        strbuf->str[0] = '{';
    else
        g_string_append_c(strbuf, '{');

    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
}

struct sr_python_stacktrace *
sr_python_stacktrace_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "stacktrace", error_message))
        return NULL;

    struct sr_python_stacktrace *result = sr_python_stacktrace_new();

    /* Exception name. */
    if (!JSON_READ_STRING(root, "exception_name", &result->exception_name))
        goto fail;

    /* Frames. */
    json_object *stacktrace;
    if (json_object_object_get_ex(root, "stacktrace", &stacktrace))
    {
        size_t array_length;

        if (!json_check_type(stacktrace, json_type_array, "stacktrace", error_message))
            goto fail;

        array_length = json_object_array_length(stacktrace);

        for (size_t i = 0; i < array_length; i++)
        {
            json_object *frame_json;
            struct sr_python_frame *frame;

            frame_json = json_object_array_get_idx(stacktrace, i);
            frame = sr_python_frame_from_json(frame_json, error_message);

            if (!frame)
                goto fail;

            result->frames = sr_python_frame_append(result->frames, frame);
        }
    }

    return result;

fail:
    sr_python_stacktrace_free(result);
    return NULL;
}

char *
sr_python_stacktrace_get_reason(struct sr_python_stacktrace *stacktrace)
{
    char *exc = "Unknown error";
    char *file = "<unknown>";
    uint32_t line = 0;

    struct sr_python_frame *frame = stacktrace->frames;
    while (frame && frame->next)
        frame = frame->next;

    if (frame)
    {
        file = frame->file_name;
        line = frame->file_line;
    }

    if (stacktrace->exception_name)
        exc = stacktrace->exception_name;

    return sr_asprintf("%s in %s:%"PRIu32, exc, file, line);
}

static void
python_append_bthash_text(struct sr_python_stacktrace *stacktrace, enum sr_bthash_flags flags,
                          GString *strbuf)
{
    g_string_append_printf(strbuf, "Exception: %s\n", OR_UNKNOWN(stacktrace->exception_name));
    g_string_append_c(strbuf, '\n');
}
