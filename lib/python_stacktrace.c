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
#include "python_stacktrace.h"
#include "python_frame.h"
#include "location.h"
#include "utils.h"
#include "sha1.h"
#include "strbuf.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

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
}

void
sr_python_stacktrace_free(struct sr_python_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    free(stacktrace);
}

struct sr_python_stacktrace *
sr_python_stacktrace_dup(struct sr_python_stacktrace *stacktrace)
{
    struct sr_python_stacktrace *result = sr_python_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_python_stacktrace));

    if (result->file_name)
        result->file_name = sr_strdup(result->file_name);

    if (result->exception_name)
        result->exception_name = sr_strdup(result->exception_name);

    if (result->frames)
        result->frames = sr_python_frame_dup(result->frames, true);

    return result;
}

int
sr_python_stacktrace_get_frame_count(struct sr_python_stacktrace *stacktrace)
{
    struct sr_python_frame *frame = stacktrace->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }

    return count;
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
        location->message = "Traceback header not found.";
        return NULL;
    }

    local_input += strlen(HEADER);
    location->line += 2;
    location->column = 0;

    struct sr_python_stacktrace *stacktrace = sr_python_stacktrace_new();

    /* Read the frames. */
    struct sr_python_frame *frame, *prevframe = NULL;
    struct sr_location frame_location;
    sr_location_init(&frame_location);
    while ((frame = sr_python_frame_parse(&local_input, &frame_location)))
    {
        if (prevframe)
        {
            sr_python_frame_append(prevframe, frame);
            prevframe = frame;
        }
        else
            stacktrace->frames = prevframe = frame;

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

    /* Parse exception name. */
    sr_parse_char_cspan(&local_input,
                        ":\n",
                        &stacktrace->exception_name);

    *input = local_input;
    return stacktrace;
}

char *
sr_python_stacktrace_to_json(struct sr_python_stacktrace *stacktrace)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Exception file name. */
    if (stacktrace->file_name)
    {
        sr_strbuf_append_strf(strbuf,
                              ",   \"file_name\": \"%s\"\n",
                              stacktrace->file_name);
    }

    /* Exception file line. */
    if (stacktrace->file_line > 0)
    {
        sr_strbuf_append_strf(strbuf,
                              ",   \"file_line\": %"PRIu32"\n",
                              stacktrace->file_line);
    }

    /* Exception class name. */
    if (stacktrace->exception_name)
    {
        sr_strbuf_append_strf(strbuf,
                              ",   \"exception_name\": \"%s\"\n",
                              stacktrace->exception_name);
    }

    /* Frames. */
    if (stacktrace->frames)
    {
        struct sr_python_frame *frame = stacktrace->frames;
        sr_strbuf_append_str(strbuf, ",   \"frames\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                sr_strbuf_append_str(strbuf, "      [ ");
            else
                sr_strbuf_append_str(strbuf, "      , ");

            char *frame_json = sr_python_frame_to_json(frame);
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

    if (strbuf->len > 0)
        strbuf->buf[0] = '{';
    else
        sr_strbuf_append_char(strbuf, '{');

    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}
