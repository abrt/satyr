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

struct btp_python_stacktrace *
btp_python_stacktrace_new()
{
    struct btp_python_stacktrace *stacktrace =
        btp_malloc(sizeof(struct btp_python_stacktrace));

    btp_python_stacktrace_init(stacktrace);
    return stacktrace;
}

void
btp_python_stacktrace_init(struct btp_python_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct btp_python_stacktrace));
}

void
btp_python_stacktrace_free(struct btp_python_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    free(stacktrace);
}

struct btp_python_stacktrace *
btp_python_stacktrace_dup(struct btp_python_stacktrace *stacktrace)
{
    struct btp_python_stacktrace *result = btp_python_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct btp_python_stacktrace));

    if (result->file_name)
        result->file_name = btp_strdup(result->file_name);

    if (result->exception_name)
        result->exception_name = btp_strdup(result->exception_name);

    if (result->frames)
        result->frames = btp_python_frame_dup(result->frames, true);

    return result;
}

int
btp_python_stacktrace_get_frame_count(struct btp_python_stacktrace *stacktrace)
{
    struct btp_python_frame *frame = stacktrace->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }

    return count;
}

struct btp_python_stacktrace *
btp_python_stacktrace_parse(const char **input,
                            struct btp_location *location)
{
    const char *local_input = *input;

    /* Parse the header. */
    const char *HEADER = "\nTraceback (most recent call last):\n";
    local_input = btp_strstr_location(local_input,
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

    struct btp_python_stacktrace *stacktrace = btp_python_stacktrace_new();

    /* Read the frames. */
    struct btp_python_frame *frame, *prevframe = NULL;
    struct btp_location frame_location;
    btp_location_init(&frame_location);
    while ((frame = btp_python_frame_parse(&local_input, &frame_location)))
    {
        if (prevframe)
        {
            btp_python_frame_append(prevframe, frame);
            prevframe = frame;
        }
        else
            stacktrace->frames = prevframe = frame;

        btp_location_add(location,
                         frame_location.line,
                         frame_location.column);
    }

    if (!stacktrace->frames)
    {
        location->message = frame_location.message;
        btp_python_stacktrace_free(stacktrace);
        return NULL;
    }

    /* Parse exception name. */
    btp_parse_char_cspan(&local_input,
                         ":\n",
                         &stacktrace->exception_name);

    *input = local_input;
    return stacktrace;
}

char *
btp_python_stacktrace_to_json(struct btp_python_stacktrace *stacktrace)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();

    /* Exception file name. */
    if (stacktrace->file_name)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"file_name\": \"%s\"\n",
                               stacktrace->file_name);
    }

    /* Exception file line. */
    if (stacktrace->file_line > 0)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"file_line\": %"PRIu32"\n",
                               stacktrace->file_line);
    }

    /* Exception class name. */
    if (stacktrace->exception_name)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"exception_name\": \"%s\"\n",
                               stacktrace->exception_name);
    }

    /* Frames. */
    if (stacktrace->frames)
    {
        struct btp_python_frame *frame = stacktrace->frames;
        btp_strbuf_append_str(strbuf, ",   \"frames\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                btp_strbuf_append_str(strbuf, "      [ ");
            else
                btp_strbuf_append_str(strbuf, "      , ");

            char *frame_json = btp_python_frame_to_json(frame);
            char *indented_frame_json = btp_indent_except_first_line(frame_json, 8);
            btp_strbuf_append_str(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                btp_strbuf_append_str(strbuf, "\n");
        }

        btp_strbuf_append_str(strbuf, " ]\n");
    }

    if (strbuf->len > 0)
        strbuf->buf[0] = '{';
    else
        btp_strbuf_append_char(strbuf, '{');

    btp_strbuf_append_char(strbuf, '}');
    return btp_strbuf_free_nobuf(strbuf);
}
