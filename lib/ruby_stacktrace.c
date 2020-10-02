/*
    ruby_stacktrace.c

    Copyright (C) 2015  Red Hat, Inc.

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
#include "ruby/stacktrace.h"
#include "ruby/frame.h"
#include "location.h"
#include "utils.h"
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
ruby_append_bthash_text(struct sr_ruby_stacktrace *stacktrace, enum sr_bthash_flags flags,
                          GString *strbuf);

DEFINE_FRAMES_FUNC(ruby_frames, struct sr_ruby_stacktrace)
DEFINE_SET_FRAMES_FUNC(ruby_set_frames, struct sr_ruby_stacktrace)
DEFINE_PARSE_WRAPPER_FUNC(ruby_parse, SR_REPORT_RUBY)

struct thread_methods ruby_thread_methods =
{
    .frames = (frames_fn_t) ruby_frames,
    .set_frames = (set_frames_fn_t) ruby_set_frames,
    .cmp = (thread_cmp_fn_t) NULL,
    .frame_count = (frame_count_fn_t) thread_frame_count,
    .next = (next_thread_fn_t) thread_no_next_thread,
    .set_next = (set_next_thread_fn_t) NULL,
    .thread_append_bthash_text =
        (thread_append_bthash_text_fn_t) thread_no_bthash_text,
    .thread_free = (thread_free_fn_t) sr_ruby_stacktrace_free,
    .remove_frame = (remove_frame_fn_t) thread_remove_frame,
    .remove_frames_above =
        (remove_frames_above_fn_t) thread_remove_frames_above,
    .thread_dup = (thread_dup_fn_t) sr_ruby_stacktrace_dup,
    .normalize = (normalize_fn_t) thread_no_normalization,
};

struct stacktrace_methods ruby_stacktrace_methods =
{
    .parse = (parse_fn_t) ruby_parse,
    .parse_location = (parse_location_fn_t) sr_ruby_stacktrace_parse,
    .to_short_text = (to_short_text_fn_t) stacktrace_to_short_text,
    .to_json = (to_json_fn_t) sr_ruby_stacktrace_to_json,
    .from_json = (from_json_fn_t) sr_ruby_stacktrace_from_json,
    .get_reason = (get_reason_fn_t) sr_ruby_stacktrace_get_reason,
    .find_crash_thread = (find_crash_thread_fn_t) stacktrace_one_thread_only,
    .threads = (threads_fn_t) stacktrace_one_thread_only,
    .set_threads = (set_threads_fn_t) NULL,
    .stacktrace_free = (stacktrace_free_fn_t) sr_ruby_stacktrace_free,
    .stacktrace_append_bthash_text =
        (stacktrace_append_bthash_text_fn_t) ruby_append_bthash_text,
};

/* Public functions */

struct sr_ruby_stacktrace *
sr_ruby_stacktrace_new()
{
    struct sr_ruby_stacktrace *stacktrace =
        g_malloc(sizeof(*stacktrace));

    sr_ruby_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_ruby_stacktrace_init(struct sr_ruby_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct sr_ruby_stacktrace));
    stacktrace->type = SR_REPORT_RUBY;
}

void
sr_ruby_stacktrace_free(struct sr_ruby_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->frames)
    {
        struct sr_ruby_frame *frame = stacktrace->frames;
        stacktrace->frames = frame->next;
        sr_ruby_frame_free(frame);
    }

    free(stacktrace->exception_name);
    free(stacktrace);
}

struct sr_ruby_stacktrace *
sr_ruby_stacktrace_dup(struct sr_ruby_stacktrace *stacktrace)
{
    struct sr_ruby_stacktrace *result = sr_ruby_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_ruby_stacktrace));

    if (result->exception_name)
        result->exception_name = g_strdup(result->exception_name);

    if (result->frames)
        result->frames = sr_ruby_frame_dup(result->frames, true);

    return result;
}

struct sr_ruby_stacktrace *
sr_ruby_stacktrace_parse(const char **input,
                         struct sr_location *location)
{
    const char *local_input = *input;
    struct sr_ruby_stacktrace *stacktrace = sr_ruby_stacktrace_new();
    char *message_and_class = NULL;

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     */
    stacktrace->frames = sr_ruby_frame_parse(&local_input, location);
    if (!stacktrace->frames)
    {
        location->message = g_strdup_printf("Topmost stacktrace frame not found: %s",
                            location->message ? location->message : "(unknown reason)");
        goto fail;
    }

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                              ^^
     */
    if (!sr_skip_string(&local_input, ": "))
    {
        location->message = g_strdup("Unable to find the colon after first function name.");
        goto fail;
    }
    location->column += strlen(": ");

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     */
    if (!sr_parse_char_cspan(&local_input, "\t", &message_and_class))
    {
        location->message = g_strdup("Unable to find the exception type and message.");
        goto fail;
    }

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                                                                    ^^
     */
    size_t l = strlen(message_and_class);
    location->column += l;

    char *p = message_and_class + l - 1;
    if (p < message_and_class || *p != '\n')
    {
        location->column--;
        location->message = g_strdup("Unable to find the new line character after "
                                      "the end of exception class");
        goto fail;
    }

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                                                                   ^
     */
    p--;
    if (p < message_and_class || *p != ')')
    {
        location->column -= 2;
        location->message = g_strdup("Unable to find the ')' character identifying "
                                      "the end of exception class");
        goto fail;
    }

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                                                   ^^^^^^^^^^^^^^^^
     */
    *p = '\0';
    do {
        p--;
    } while (p >= message_and_class && *p != '(');
    p++;

    if (strlen(p) <= 0)
    {
        location->message = g_strdup("Unable to find the '(' character identifying "
                                      "the beginning of the exception class");
        goto fail;
    }
    stacktrace->exception_name = g_strdup(p);

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                                                  ^
     */
    p--;
    if (p < message_and_class || *p != '(')
    {
        location->message = g_strdup("Unable to find the '(' character identifying "
                                      "the beginning of the exception class");
        goto fail;
    }

    /* Throw away the message, it may contain sensitive data. */
    free(message_and_class);
    message_and_class = p = NULL;

    /* /some/thing.rb:13:in `method': exception message (Exception::Class)\n\tfrom ...
     *                                   we're here now, increment the line ^^
     */
    location->column = 0;
    location->line++;

    struct sr_ruby_frame *last_frame = stacktrace->frames;
    while (*local_input)
    {
        /* The exception message can continue on the lines after the topmost frame
         * - skip those.
         */
        int skipped = sr_skip_string(&local_input, "\tfrom ");
        if (!skipped)
        {
            location->message = g_strdup("Frame header not found.");
            goto fail;
        }
        location->column += skipped;

        last_frame->next = sr_ruby_frame_parse(&local_input, location);
        if (!last_frame->next)
        {
            /* location->message is already set */
            goto fail;
        }

        /* Eat newline (except at the end of file). */
        if (!sr_skip_char(&local_input, '\n') && *local_input != '\0')
        {
            location->message = g_strdup("Expected newline after stacktrace frame.");
            goto fail;
        }
        location->column = 0;
        location->line++;
        last_frame = last_frame->next;
    }

    *input = local_input;
    return stacktrace;

fail:
    sr_ruby_stacktrace_free(stacktrace);
    free(message_and_class);
    return NULL;
}

char *
sr_ruby_stacktrace_to_json(struct sr_ruby_stacktrace *stacktrace)
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
        struct sr_ruby_frame *frame = stacktrace->frames;
        g_string_append(strbuf, ",   \"stacktrace\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                g_string_append(strbuf, "      [ ");
            else
                g_string_append(strbuf, "      , ");

            char *frame_json = sr_ruby_frame_to_json(frame);
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

struct sr_ruby_stacktrace *
sr_ruby_stacktrace_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "stacktrace", error_message))
        return NULL;

    struct sr_ruby_stacktrace *result = sr_ruby_stacktrace_new();

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
            struct sr_ruby_frame *frame;

            frame_json = json_object_array_get_idx(stacktrace, i);
            frame = sr_ruby_frame_from_json(frame_json, error_message);

            if (!frame)
                goto fail;

            result->frames = sr_ruby_frame_append(result->frames, frame);
        }
    }

    return result;

fail:
    sr_ruby_stacktrace_free(result);
    return NULL;
}

char *
sr_ruby_stacktrace_get_reason(struct sr_ruby_stacktrace *stacktrace)
{
    char *exc = "Unknown error";
    char *file = "<unknown>";
    uint32_t line = 0;

    struct sr_ruby_frame *frame = stacktrace->frames;
    if (frame)
    {
        file = frame->file_name;
        line = frame->file_line;
    }

    if (stacktrace->exception_name)
        exc = stacktrace->exception_name;

    return g_strdup_printf("%s in %s:%"PRIu32, exc, file, line);
}

static void
ruby_append_bthash_text(struct sr_ruby_stacktrace *stacktrace, enum sr_bthash_flags flags,
                        GString *strbuf)
{
    g_string_append_printf(strbuf, "Exception: %s\n", OR_UNKNOWN(stacktrace->exception_name));
    g_string_append_c(strbuf, '\n');
}
