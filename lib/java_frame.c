/*
    java_frame.c

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
#include "java_frame.h"
#include "strbuf.h"
#include "location.h"
#include "utils.h"
#include <string.h>

struct btp_java_frame *
btp_java_frame_new()
{
    struct btp_java_frame *frame =
        btp_malloc(sizeof(*frame));

    btp_java_frame_init(frame);
    return frame;
}

void
btp_java_frame_init(struct btp_java_frame *frame)
{
    memset(frame, 0, sizeof(*frame));
}

void
btp_java_frame_free(struct btp_java_frame *frame)
{
    if (!frame)
        return;

    free(frame->file_name);
    free(frame->function_name);
    free(frame->class_path);
    free(frame);
}

struct btp_java_frame *
btp_java_frame_dup(struct btp_java_frame *frame, bool siblings)
{
    struct btp_java_frame *result = btp_java_frame_new();
    memcpy(result, frame, sizeof(*result));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_java_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->file_name)
        result->file_name = btp_strdup(result->file_name);

    if (result->function_name)
        result->function_name = btp_strdup(result->function_name);

    if (result->class_path)
        result->class_path = btp_strdup(result->class_path);

    return result;
}

int
btp_java_frame_cmp(struct btp_java_frame *frame1,
                   struct btp_java_frame *frame2,
                   bool compare_number)
{
    int res = btp_strcmp0(frame1->class_path, frame2->class_path);
    if (res != 0)
        return res;

    res = btp_strcmp0(frame1->file_name, frame2->file_name);
    if (res != 0)
        return res;

    res = btp_strcmp0(frame1->function_name, frame2->function_name);
    if (res != 0)
        return res;

    return frame1->file_line - frame2->file_line;
}

void
btp_java_frame_append_to_str(struct btp_java_frame *frame,
                             struct btp_strbuf *dest)
{

    btp_strbuf_append_strf(dest, "\tat %s(%s",
                           frame->function_name ? frame->function_name : "",
                           frame->file_name ? frame->file_name : "");
    if (frame->file_line)
        btp_strbuf_append_strf(dest, ":%d", frame->file_line);

    btp_strbuf_append_str(dest, ")");
}

struct btp_java_frame *
btp_java_frame_parse(const char **input,
                     struct btp_location *location)
{
    const char *mark = *input;
    int lines, columns;
    /*      at SimpleTest.throwNullPointerException(SimpleTest.java:36) */
    const char *cursor = btp_strstr_location(mark, "at", &lines, &columns);

    if (!cursor)
    {
        location->message = "Frame expected";
        return NULL;
    }

    /*  SimpleTest.throwNullPointerException(SimpleTest.java:36) */
    cursor = mark = cursor + 2;
    location->line += (lines - 1);
    location->column = columns + 2;

    /* SimpleTest.throwNullPointerException(SimpleTest.java:36) */
    cursor = btp_skip_whitespace(cursor);
    location->column += cursor - mark;
    mark = cursor;

    while (*cursor != '(' && *cursor != '\n' && *cursor != '\0')
    {
        ++cursor;
        ++location->column;
    }

    struct btp_java_frame *frame = btp_java_frame_new();

    if (cursor != mark)
        frame->function_name = btp_strndup(mark, cursor - mark);

    /* (SimpleTest.java:36) */
    if (*cursor == '(')
    {
        ++cursor;
        ++location->column;
        mark = cursor;

        while (*cursor != ':' && *cursor != ')' && *cursor != '\n' && *cursor != '\0')
        {
            ++cursor;
            ++location->column;
        }

        if (mark != cursor)
            frame->file_name = btp_strndup(mark, cursor - mark);

        if (*cursor == ':')
        {
            ++cursor;
            ++location->column;
            mark = cursor;

            btp_parse_uint32(&cursor, &(frame->file_line));

            location->column += cursor - mark;
        }
    }

    mark = cursor;
    cursor = strchrnul(cursor, '\n');
    *input = cursor;

    if (*cursor == '\n')
    {
        ++*(input);
        ++location->line;
        location->column = 0;
    }
    else
        /* don't take \0 Byte into account */
        location->column += ((cursor - mark) - 1);

    return frame;
}

