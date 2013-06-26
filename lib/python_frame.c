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
#include "strbuf.h"
#include "json.h"
#include "generic_frame.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <string.h>
#include <inttypes.h>

/* Method table */

static void
python_append_bthash_text(struct sr_python_frame *frame, enum sr_bthash_flags flags,
                          struct sr_strbuf *strbuf);

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
};

/* Public functions */

struct sr_python_frame *
sr_python_frame_new()
{
    struct sr_python_frame *frame =
        sr_malloc(sizeof(struct sr_python_frame));

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
        location->message = sr_asprintf("Frame header not found.");
        return NULL;
    }

    location->column += strlen("  File \"");
    struct sr_python_frame *frame = sr_python_frame_new();

    /* Parse file name */
    if (!sr_parse_char_cspan(&local_input, "\"", &frame->file_name))
    {
        sr_python_frame_free(frame);
        location->message = sr_asprintf("Unable to find the '\"' character "
                "identifying the beginning of file name.");

        return NULL;
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

    location->column += strlen(frame->file_name);

    if (0 == sr_skip_string(&local_input, "\", line "))
    {
        location->message = sr_asprintf("Line separator not found.");
        return NULL;
    }

    location->column += strlen("\", line ");

    /* Parse line number */
    int length = sr_parse_uint32(&local_input, &frame->file_line);
    if (0 == length)
    {
        location->message = sr_asprintf("Line number not found.");
        return NULL;
    }

    location->column += length;

    if (0 == sr_skip_string(&local_input, ", in "))
    {
        location->message = sr_asprintf("Function name separator not found.");
        return NULL;
    }

    location->column += strlen(", in ");

    /* Parse function name */
    if (!sr_parse_char_cspan(&local_input, "\n", &frame->function_name))
    {
        sr_python_frame_free(frame);
        location->message = sr_asprintf("Unable to find the newline character "
                "identifying the end of function name.");

        return NULL;
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

    sr_skip_char(&local_input, '\n');
    sr_location_add(location, 1, 0);

    /* Parse source code line (optional). */
    if (4 == sr_skip_string(&local_input, "    "))
    {
        sr_parse_char_cspan(&local_input, "\n", &frame->line_contents);
        sr_skip_char(&local_input, '\n');
        sr_location_add(location, 1, 0);
    }

    *input = local_input;
    return frame;
}

char *
sr_python_frame_to_json(struct sr_python_frame *frame)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Source file name / special file. */
    if (frame->file_name)
    {
        if (frame->special_file)
            sr_strbuf_append_str(strbuf, ",   \"special_file\": ");
        else
            sr_strbuf_append_str(strbuf, ",   \"file_name\": ");

        sr_json_append_escaped(strbuf, frame->file_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    /* Source file line. */
    if (frame->file_line)
    {
        sr_strbuf_append_strf(strbuf,
                              ",   \"file_line\": %"PRIu32"\n",
                              frame->file_line);
    }

    /* Function name / special function. */
    if (frame->function_name)
    {
        if (frame->special_function)
            sr_strbuf_append_str(strbuf, ",   \"special_function\": ");
        else
            sr_strbuf_append_str(strbuf, ",   \"function_name\": ");

        sr_json_append_escaped(strbuf, frame->function_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    /* Line contents. */
    if (frame->line_contents)
    {
        sr_strbuf_append_str(strbuf, ",   \"line_contents\": ");
        sr_json_append_escaped(strbuf, frame->line_contents);
        sr_strbuf_append_str(strbuf, "\n");
    }

    strbuf->buf[0] = '{';
    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}

void
sr_python_frame_append_to_str(struct sr_python_frame *frame,
                              struct sr_strbuf *dest)
{
    sr_strbuf_append_strf(dest, "%s%s%s",
                          (frame->special_function ? "<" : ""),
                          (frame->function_name ? frame->function_name : "??"),
                          (frame->special_function ? ">" : ""));

    if (frame->file_name)
    {
        sr_strbuf_append_strf(dest, " in %s%s%s",
                              (frame->special_file ? "<" : ""),
                              frame->file_name,
                              (frame->special_file ? ">" : ""));

        if (frame->file_line)
        {
            sr_strbuf_append_strf(dest, ":%d", frame->file_line);
        }
    }
}

static void
python_append_bthash_text(struct sr_python_frame *frame, enum sr_bthash_flags flags,
                          struct sr_strbuf *strbuf)
{
    sr_strbuf_append_strf(strbuf,
                          "%s, %d, %"PRIu32", %s, %d, %s\n",
                          OR_UNKNOWN(frame->file_name),
                          frame->special_file,
                          frame->file_line,
                          OR_UNKNOWN(frame->function_name),
                          frame->special_function,
                          OR_UNKNOWN(frame->line_contents));
}
