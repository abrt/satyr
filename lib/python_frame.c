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
#include "python_frame.h"
#include "utils.h"
#include "location.h"
#include <string.h>

struct btp_python_frame *
btp_python_frame_new()
{
    struct btp_python_frame *frame =
        btp_malloc(sizeof(struct btp_python_frame));

    btp_python_frame_init(frame);
    return frame;
}

void
btp_python_frame_init(struct btp_python_frame *frame)
{
    memset(frame, 0, sizeof(struct btp_python_frame));
}

void
btp_python_frame_free(struct btp_python_frame *frame)
{
    if (!frame)
        return;

    free(frame->file_name);
    free(frame->function_name);
    free(frame->line);
    free(frame);
}

struct btp_python_frame *
btp_python_frame_dup(struct btp_python_frame *frame, bool siblings)
{
    struct btp_python_frame *result = btp_python_frame_new();
    memcpy(result, frame, sizeof(struct btp_python_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_python_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->file_name)
        result->file_name = btp_strdup(result->file_name);

    if (result->function_name)
        result->function_name = btp_strdup(result->function_name);

    if (result->line)
        result->line = btp_strdup(result->line);

    return result;
}

struct btp_python_frame *
btp_python_frame_append(struct btp_python_frame *dest,
                        struct btp_python_frame *item)
{
    if (!dest)
        return item;

    struct btp_python_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

struct btp_python_frame *
btp_python_frame_parse(const char **input,
                       struct btp_location *location)
{
    const char *local_input = *input;

    if (0 == btp_skip_string(&local_input, "  File \""))
    {
        location->message = btp_asprintf("Frame header not found.");
        return NULL;
    }

    location->column += strlen("  File \"");
    struct btp_python_frame *frame = btp_python_frame_new();

    /* Parse file name */
    if (!btp_parse_char_cspan(&local_input, "\"", &frame->file_name))
    {
        btp_python_frame_free(frame);
        location->message = btp_asprintf("Unable to find the '\"' character "
                "identifying the beginning of file name.");

        return NULL;
    }

    location->column += strlen(frame->file_name);

    if (0 == btp_skip_string(&local_input, "\", line "))
    {
        location->message = btp_asprintf("Line separator not found.");
        return NULL;
    }

    location->column += strlen("\", line ");

    /* Parse line number */
    int length = btp_parse_uint32(&local_input, &frame->file_line);
    if (0 == length)
    {
        location->message = btp_asprintf("Line number not found.");
        return NULL;
    }

    location->column += length;

    if (0 == btp_skip_string(&local_input, ", in "))
    {
        location->message = btp_asprintf("Function name separator not found.");
        return NULL;
    }

    location->column += strlen(", in ");

    /* Parse function name */
    if (!btp_parse_char_cspan(&local_input, "\n", &frame->function_name))
    {
        btp_python_frame_free(frame);
        location->message = btp_asprintf("Unable to find the '\"' character "
                "identifying the beginning of file name.");

        return NULL;
    }

    location->column += strlen(frame->function_name);

    if (0 == strcmp(frame->function_name, "<module>"))
    {
        frame->is_module = true;
        free(frame->function_name);
        frame->function_name = NULL;
    }

    btp_skip_char(&local_input, '\n');
    btp_location_add(&location, 1, 0);

    /* Parse source code line (optional). */
    if (4 == btp_skip_string(&local_input, "    "))
    {
        btp_skip_char_cspan(&local_input, "\n");
        btp_skip_char(&local_input, '\n');
        btp_location_add(&location, 1, 0);
    }

    *input = local_input;
    return frame;
}
