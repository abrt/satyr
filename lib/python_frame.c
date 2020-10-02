/*
    python_frame.c

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
#include "python/frame.h"
#include "utils.h"
#include "location.h"
#include "json.h"
#include "generic_frame.h"
#include "thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <string.h>
#include <inttypes.h>

/* Method table */

static void
python_append_bthash_text(struct sr_python_frame *frame, enum sr_bthash_flags flags,
                          GString *strbuf);
static void
python_append_duphash_text(struct sr_python_frame *frame, enum sr_duphash_flags flags,
                           GString *strbuf);

DEFINE_NEXT_FUNC(python_next, struct sr_frame, struct sr_python_frame)
DEFINE_SET_NEXT_FUNC(python_set_next, struct sr_frame, struct sr_python_frame)

struct frame_methods python_frame_methods =
{
    .append_to_str = (append_to_str_fn_t) sr_python_frame_append_to_str,
    .next = (next_frame_fn_t) python_next,
    .set_next = (set_next_frame_fn_t) python_set_next,
    .cmp = (frame_cmp_fn_t) sr_python_frame_cmp,
    .cmp_distance = (frame_cmp_fn_t) sr_python_frame_cmp_distance,
    .frame_append_bthash_text =
        (frame_append_bthash_text_fn_t) python_append_bthash_text,
    .frame_append_duphash_text =
        (frame_append_duphash_text_fn_t) python_append_duphash_text,
    .frame_free = (frame_free_fn_t) sr_python_frame_free,
};

/* Public functions */

struct sr_python_frame *
sr_python_frame_new()
{
    struct sr_python_frame *frame =
        g_malloc(sizeof(*frame));

    sr_python_frame_init(frame);
    return frame;
}

void
sr_python_frame_init(struct sr_python_frame *frame)
{
    memset(frame, 0, sizeof(struct sr_python_frame));
    frame->type = SR_REPORT_PYTHON;
}

void
sr_python_frame_free(struct sr_python_frame *frame)
{
    if (!frame)
        return;

    free(frame->file_name);
    free(frame->function_name);
    free(frame->line_contents);
    free(frame);
}

struct sr_python_frame *
sr_python_frame_dup(struct sr_python_frame *frame, bool siblings)
{
    struct sr_python_frame *result = sr_python_frame_new();
    memcpy(result, frame, sizeof(struct sr_python_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_python_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->file_name)
        result->file_name = sr_strdup(result->file_name);

    if (result->function_name)
        result->function_name = sr_strdup(result->function_name);

    if (result->line_contents)
        result->line_contents = sr_strdup(result->line_contents);

    return result;
}

int
sr_python_frame_cmp(struct sr_python_frame *frame1,
                    struct sr_python_frame *frame2)
{
    /* function_name */
    int function_name = sr_strcmp0(frame1->function_name,
                                   frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* file_name */
    int file_name = sr_strcmp0(frame1->file_name,
                               frame2->file_name);
    if (file_name != 0)
        return file_name;

    /* file_line */
    int file_line = frame1->file_line - frame2->file_line;
    if (file_line != 0)
        return file_line;

    /* special_function */
    int special_function = frame1->special_function - frame2->special_function;

    if (special_function != 0)
        return special_function;

    /* special_file */
    int special_file = frame1->special_file - frame2->special_file;

    if (special_file != 0)
        return special_file;

    /* line_contents */
    int line_contents = sr_strcmp0(frame1->line_contents,
                                   frame2->line_contents);
    if (line_contents != 0)
        return line_contents;

    return 0;
}

int
sr_python_frame_cmp_distance(struct sr_python_frame *frame1,
                             struct sr_python_frame *frame2)
{
    /* function_name */
    int function_name = sr_strcmp0(frame1->function_name,
                                   frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* file_name */
    int file_name = sr_strcmp0(frame1->file_name,
                               frame2->file_name);
    if (file_name != 0)
        return file_name;

    /* special_function */
    int special_function = frame1->special_function - frame2->special_function;

    if (special_function != 0)
        return special_function;

    /* special_file */
    int special_file = frame1->special_file - frame2->special_file;

    if (special_file != 0)
        return special_file;

    return 0;
}

struct sr_python_frame *
sr_python_frame_append(struct sr_python_frame *dest,
                       struct sr_python_frame *item)
{
    if (!dest)
        return item;

    struct sr_python_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

struct sr_python_frame *
sr_python_frame_parse(const char **input,
                      struct sr_location *location)
{
    const char *local_input = *input;

    if (0 == sr_skip_string(&local_input, "  File \""))
    {
        location->message = g_strdup_printf("Frame header not found.");
        return NULL;
    }

    location->column += strlen("  File \"");
    struct sr_python_frame *frame = sr_python_frame_new();

    /* Parse file name */
    if (!sr_parse_char_cspan(&local_input, "\"", &frame->file_name))
    {
        location->message = g_strdup_printf("Unable to find the '\"' character "
                "identifying the beginning of file name.");

        goto fail;
    }

    if (strlen(frame->file_name) > 0 &&
        frame->file_name[0] == '<' &&
        frame->file_name[strlen(frame->file_name)-1] == '>')
    {
        frame->special_file = true;
        frame->file_name[strlen(frame->file_name)-1] = '\0';
        char *inside = sr_strdup(frame->file_name + 1);
        free(frame->file_name);
        frame->file_name = inside;
    }

    frame->file_name = anonymize_path(frame->file_name);

    location->column += strlen(frame->file_name);

    if (0 == sr_skip_string(&local_input, "\", line "))
    {
        location->message = g_strdup_printf("Line separator not found.");
        goto fail;
    }

    location->column += strlen("\", line ");

    /* Parse line number */
    int length = sr_parse_uint32(&local_input, &frame->file_line);
    if (0 == length)
    {
        location->message = g_strdup_printf("Line number not found.");
        goto fail;
    }

    location->column += length;

    if (0 == sr_skip_string(&local_input, ", in "))
    {
        if (local_input[0] != '\n')
        {
            location->message = g_strdup_printf("Function name separator not found.");
            goto fail;
        }

        /* The last frame of SyntaxError stack trace does not have
         * function name on its line. For the sake of simplicity, we will
         * believe that we are dealing with such a frame now.
         */
        frame->function_name = sr_strdup("syntax");
        frame->special_function = true;
    }
    else
    {
        location->column += strlen(", in ");

        /* Parse function name */
        if (!sr_parse_char_cspan(&local_input, "\n", &frame->function_name))
        {
            location->message = g_strdup_printf("Unable to find the newline character "
                    "identifying the end of function name.");

            goto fail;
        }

        location->column += strlen(frame->function_name);

        if (strlen(frame->function_name) > 0 &&
            frame->function_name[0] == '<' &&
            frame->function_name[strlen(frame->function_name)-1] == '>')
        {
            frame->special_function = true;
            frame->function_name[strlen(frame->function_name)-1] = '\0';
            char *inside = sr_strdup(frame->function_name + 1);
            free(frame->function_name);
            frame->function_name = inside;
        }
    }

    if (sr_skip_char(&local_input, '\n'))
        sr_location_add(location, 1, 0);

    /* Parse source code line (optional). */
    if (4 == sr_skip_string(&local_input, "    "))
    {
        if (sr_parse_char_cspan(&local_input, "\n", &frame->line_contents)
                && sr_skip_char(&local_input, '\n'))
            sr_location_add(location, 1, 0);
    }

    *input = local_input;
    return frame;

fail:
    sr_python_frame_free(frame);
    return NULL;
}

char *
sr_python_frame_to_json(struct sr_python_frame *frame)
{
    GString *strbuf = g_string_new(NULL);

    /* Source file name / special file. */
    if (frame->file_name)
    {
        if (frame->special_file)
            g_string_append(strbuf, ",   \"special_file\": ");
        else
            g_string_append(strbuf, ",   \"file_name\": ");

        sr_json_append_escaped(strbuf, frame->file_name);
        g_string_append(strbuf, "\n");
    }

    /* Source file line. */
    if (frame->file_line)
    {
        g_string_append_printf(strbuf,
                              ",   \"file_line\": %"PRIu32"\n",
                              frame->file_line);
    }

    /* Function name / special function. */
    if (frame->function_name)
    {
        if (frame->special_function)
            g_string_append(strbuf, ",   \"special_function\": ");
        else
            g_string_append(strbuf, ",   \"function_name\": ");

        sr_json_append_escaped(strbuf, frame->function_name);
        g_string_append(strbuf, "\n");
    }

    /* Line contents. */
    if (frame->line_contents)
    {
        g_string_append(strbuf, ",   \"line_contents\": ");
        sr_json_append_escaped(strbuf, frame->line_contents);
        g_string_append(strbuf, "\n");
    }

    strbuf->str[0] = '{';
    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
}

struct sr_python_frame *
sr_python_frame_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "frame", error_message))
        return NULL;

    struct sr_python_frame *result = sr_python_frame_new();
    json_object *val;

    /* Source file name / special file */
    if (json_object_object_get_ex(root, "file_name", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "file_name", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->special_file = false;
        result->file_name = sr_strdup(string);
    }
    else if (json_object_object_get_ex(root, "special_file", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "special_file", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->special_file = true;
        result->file_name = sr_strdup(string);
    }

    /* Function name / special function. */
    if (json_object_object_get_ex(root, "function_name", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "function_name", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->special_function = false;
        result->function_name = sr_strdup(string);
    }
    else if (json_object_object_get_ex(root, "special_function", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "special_function", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->special_function = true;
        result->function_name = sr_strdup(string);
    }

    bool success =
        JSON_READ_STRING(root, "line_contents", &result->line_contents) &&
        JSON_READ_UINT32(root, "file_line", &result->file_line);

    if (!success)
        goto fail;

    return result;

fail:
    sr_python_frame_free(result);
    return NULL;
}

void
sr_python_frame_append_to_str(struct sr_python_frame *frame,
                              GString *dest)
{
    if (frame->file_name)
    {
        g_string_append_printf(dest, "[%s%s%s",
                              (frame->special_file ? "<" : ""),
                              frame->file_name,
                              (frame->special_file ? ">" : ""));

        if (frame->file_line)
        {
            g_string_append_printf(dest, ":%"PRIu32, frame->file_line);
        }

        g_string_append(dest, "]");
    }

    g_string_append_printf(dest, " %s%s%s",
                          (frame->special_function ? "<" : ""),
                          (frame->function_name ? frame->function_name : "??"),
                          (frame->special_function ? ">" : ""));
}

static void
python_append_bthash_text(struct sr_python_frame *frame, enum sr_bthash_flags flags,
                          GString *strbuf)
{
    g_string_append_printf(strbuf,
                          "%s, %d, %"PRIu32", %s, %d, %s\n",
                          OR_UNKNOWN(frame->file_name),
                          frame->special_file,
                          frame->file_line,
                          OR_UNKNOWN(frame->function_name),
                          frame->special_function,
                          OR_UNKNOWN(frame->line_contents));
}

static void
python_append_duphash_text(struct sr_python_frame *frame, enum sr_duphash_flags flags,
                          GString *strbuf)
{
    /* filename:line */
    g_string_append_printf(strbuf, "%s:%"PRIu32"\n",
                          OR_UNKNOWN(frame->file_name),
                          frame->file_line);
}
