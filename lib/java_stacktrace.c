/*
    java_stacktrace.c

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
#include "java_stacktrace.h"
#include "java_frame.h"
#include "core_stacktrace.h"
#include "core_thread.h"
#include "core_frame.h"
#include "utils.h"
#include "sha1.h"
#include <stdio.h>
#include <string.h>

struct btp_java_stacktrace *
btp_java_stacktrace_new()
{
    struct btp_java_stacktrace *stacktrace =
        btp_malloc(sizeof(*stacktrace));

    btp_java_stacktrace_init(stacktrace);
    return stacktrace;
}

void
btp_java_stacktrace_init(struct btp_java_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(*stacktrace));
}

void
btp_java_stacktrace_free(struct btp_java_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->threads)
    {
        struct btp_java_thread *thread = stacktrace->threads;
        stacktrace->threads = thread->next;
        btp_java_thread_free(frame);
    }

    free(stacktrace);
}

struct btp_java_stacktrace *
btp_java_stacktrace_dup(struct btp_java_stacktrace *stacktrace)
{
    struct btp_java_stacktrace *result = btp_java_stacktrace_new();
    memcpy(result, stacktrace, sizeof(*result));

    if (result->frames)
        result->frames = btp_java_frame_dup(result->frames, true);

    return result;
}


/*
Example input:

----- BEGIN -----
Exception in thread "main" java.lang.NullPointerException
        at SimpleTest.throwNullPointerException(SimpleTest.java:36)
        at SimpleTest.throwAndDontCatchException(SimpleTest.java:70)
        at SimpleTest.main(SimpleTest.java:82)
----- END -----

relevant lines:
*/

struct btp_java_stacktrace *
btp_java_parse_stacktrace(const char *input, btp_location *location)
{
    const char *mark = input;
    /* Exception in thread "main" java.lang.NullPointerException: foo */
    int chars = btp_skip_string(&mark, "Exception in thread ");
    location->column += chars;
    j
    if (!chars)
    {
        location->message = "\"Thread\" header expected";
        return NULL;
    }

    /* "main" java.lang.NullPointerException: foo */
    if (*mark != '"')
    {
        location->message = "\"Thread\" name start expected";
        return NULL;
    }

    ++mark;
    ++location->column;

    const char *cursor = mark;
    while (*cursor != '"' && *cursor != '\n' && *cursor != '\0')
    {
        ++cursor;
        ++location->column;
    }

    /* " java.lang.NullPointerException: foo */
    if (*cursor != '"')
    {
        location->message = "\"Thread\" name end expected";
        return NULL;
    }

    struct btp_java_stacktrace *stacktrace = btp_java_stacktrace_new();
    struct btp_java_thread *thread =
                    stacktrace->threads = btp_java_thread_new();

    thread->name = btp_strndup(mark, cursor - mark);

    /*  java.lang.NullPointerException: foo */
    ++cursor;
    ++location->column;
    mark = cursor;

    /* java.lang.NullPointerException: foo */
    btp_skip_whitespace(&cursor);
    location->column += cursor - mark;
    mark = cursor;

    while (*cursor != ':' && *cursor != '\n' && *cursor != '\0')
    {
        ++cursor;
        ++location->column;
    }

    if (mark != cursor)
        thread->exception_name = btp_strndup(mark, cursor - mark);

    /* : foo */
    if (*cursor == ':')
    {
        ++cursor;
        ++location->column;
        mark = cursor;

        /* foo */
        btp_skip_whitespace(&cursor);
        location->column += cursor - mark;
        mark = cursor;

        while (*cursor != '\n' && *cursor != '\0')
        {
            ++cursor;
            ++location->column;
        }

        if (mark != cursor)
            thread->exception_message = btp_strndup(mark, cursor - mark);
    }

    if (*cursor == '\n')
    {
        ++cursor;
        ++location->line;
        location->column = 1;
    }
    else /* if (*cursor == '\0') */
    {
        location->message = "Empty stack trace";
        goto stacktrace_parse_finish;
    }

    btp_java_frame *frame = NULL;
    /* iterate line by line
       best effort - continue on error */
    while (cursor && *cursor != '\0')
    {
        /*       at SimpleTest.throwNullPointerException(SimpleTest.java:36) */
        mark = cursor;
        int lines, columns;
        cursor = btp_strstr_location(&cursor, "at", &lines, &columns);

        if (!cursor)
        {
            location->message = "Frame expected";
            break;
        }

        /* SimpleTest.throwNullPointerException(SimpleTest.java:36) */
        cursor = mark = cursor + 2;
        location->line += (lines - 1);
        location->column = columns + 2;

        btp_skip_whitespace(&cursor);
        location->column += cursor - mark;
        mark = cursor;

        while (*cursor != '(' && *cursor != '\n' && *cursor != '\0')
        {
            ++cursor;
            ++location->column;
        }

        if (frame == NULL)
        {
            frame = btp_java_frame_new();
            thread->frame = frame;
        }
        else
        {
            frame->next = btp_java_frame_new();
            frame = frame->next;
        }

        if (cursor != mark)
        {
            frame->function_name = btp_strndup(mark, cursor - mark);
        }

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
            }
        }

        mark = cursor;
        cursor = strchr(cursor, '\n');
        if (cursor)
            ++cursor;
    }

stacktrace_parse_finish:
    return stacktrace;
}
