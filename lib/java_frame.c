/*
    java_frame.c

    Copyright (C) 2012  ABRT Team
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

#define BTP_JF_MARK_NATIVE_METHOD "Native Method"
#define BTP_JF_MARK_UNKNOWN_SOURCE "Unknown Source"

struct btp_java_frame *
btp_java_frame_new()
{
    struct btp_java_frame *frame =
        btp_malloc(sizeof(*frame));

    btp_java_frame_init(frame);
    return frame;
}

struct btp_java_frame *
btp_java_frame_new_exception()
{
    struct btp_java_frame *frame =
        btp_malloc(sizeof(*frame));

    btp_java_frame_init(frame);
    frame->is_exception = true;
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
    free(frame->name);
    free(frame->class_path);
    free(frame->message);
    free(frame);
}

void
btp_java_frame_free_full(struct btp_java_frame *frame)
{
    while(frame && frame->next)
    {
        struct btp_java_frame *tmp = frame;
        frame = frame->next;
        btp_java_frame_free(frame);
    }
}

struct btp_java_frame *
btp_java_frame_get_last(struct btp_java_frame *frame)
{
    while(frame && frame->next)
        frame = frame->next;

    return frame;
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

    if (result->name)
        result->name = btp_strdup(result->name);

    if (result->class_path)
        result->class_path = btp_strdup(result->class_path);

    if (result->message)
        result->message = btp_strdup(result->message);

    return result;
}

int
btp_java_frame_cmp(struct btp_java_frame *frame1,
                   struct btp_java_frame *frame2)
{
    if (frame1->is_exception != frame2->is_exception)
        return frame1->is_exception ? 1 : -1;

    int res = btp_strcmp0(frame1->name, frame2->name);
    if (res != 0)
        return res;

    /* Don't compare exception messages because of localization! */
    if (frame1->is_exception)
        return 0;

    /* Method call comparsion */
    res = btp_strcmp0(frame1->class_path, frame2->class_path);
    if (res != 0)
        return res;

    res = btp_strcmp0(frame1->file_name, frame2->file_name);
    if (res != 0)
        return res;

    if (frame1->is_native != frame2->is_native)
        return frame1->is_native ? 1 : -1;

    return frame1->file_line - frame2->file_line;
}

void
btp_java_frame_append_to_str(struct btp_java_frame *frame,
                             struct btp_strbuf *dest)
{
    if (frame->is_exception)
    {
        if (frame->name)
            btp_strbuf_append_str(dest, frame->name);

        if (frame->message)
            btp_strbuf_append_strf(dest, ": %s", frame->message);
    }
    else
    {
        btp_strbuf_append_strf(dest, "\tat %s(",
                               frame->name ? frame->name : "");

        if (frame->is_native)
            btp_strbuf_append_str(dest, BTP_JF_MARK_NATIVE_METHOD);
        else if (!frame->file_name)
            btp_strbuf_append_str(dest, BTP_JF_MARK_UNKNOWN_SOURCE);
        else
            btp_strbuf_append_str(dest, frame->file_name);

        /* YES even if the frame is native method or source is unknown */
        /* WHY? Because it was parsed in this form */
        /* Ooops! Maybe the source file was empty string. Don't care! */
        if (frame->file_line)
            btp_strbuf_append_strf(dest, ":%d", frame->file_line);

        btp_strbuf_append_str(dest, ")");
    }
}

/*
 * We can expect different formats hence these two following helper functions
 */
static bool
btp_java_frame_parse_is_native_method(const char *input_mark)
{
    return 0 == strncmp(input_mark, BTP_JF_MARK_NATIVE_METHOD,
                                    strlen(BTP_JF_MARK_NATIVE_METHOD));
}

static bool
btp_java_frame_parse_is_unknown_source(const char *input_mark)
{
    return 0 == strncmp(input_mark, BTP_JF_MARK_UNKNOWN_SOURCE,
                                    strlen(BTP_JF_MARK_UNKNOWN_SOURCE));
}

struct btp_java_frame *
btp_java_frame_parse_exception(const char **input,
                               struct btp_location *location)
{
    /* java.lang.NullPointerException: foo */
    const char *cursor = btp_skip_whitespace(*input);
    btp_location_add(location, 0, cursor - *input);
    const char *mark = cursor;

    btp_location_add(location, 0, btp_skip_char_cspan(&cursor, ": \t\n"));

    if (mark == cursor)
    {
        location->message = "Expected exception name";
        return NULL;
    }

    struct btp_java_frame *exception = btp_java_frame_new_exception();
    exception->name = btp_strndup(mark, cursor - mark);

    /* : foo */
    if (*cursor == ':')
    {
        ++cursor;
        btp_location_add(location, 0, 1);
        mark = cursor;

        /* foo */
        cursor = btp_skip_whitespace(mark);
        btp_location_add(location, 0, cursor - mark);
        mark = cursor;

        btp_location_add(location, 0, btp_skip_char_cspan(&cursor, "\n"));

        if (mark != cursor)
            exception->message = btp_strndup(mark, cursor - mark);
    }
    else
    {
        /* just to be sure, that we skip white space behind exception name */
        btp_location_add(location, 0, btp_skip_char_cspan(&cursor, "\n"));
    }

    if (*cursor == '\n')
    {
        ++cursor;
        /* this adds one line */
        btp_location_add(location, 2, 0);
    }
    /* else *cursor == '\0' */

    mark = cursor;

    struct btp_java_frame *frame = NULL;
    /* iterate line by line
       best effort - continue on error */
    while (*cursor != '\0')
    {
        cursor = btp_skip_whitespace(mark);
        btp_location_add(location, 0, cursor - mark);

        /* Each inner exception has '...' at its end */
        if (strncmp("... ", cursor, strlen("... ")) == 0)
            goto current_exception_done;

        /* The top most exception does not have '...' at its end */
        if (strncmp("Caused by: ", cursor, strlen("Caused by: ")) == 0)
            goto parse_inner_exception;

        struct btp_java_frame *parsed = btp_java_frame_parse(&cursor, location);

        if (parsed == NULL)
        {
            btp_java_frame_free(exception);
            return NULL;
        }

        mark = cursor;

        if (exception->next == NULL)
            exception->next = parsed;
        else
            frame->next = parsed;

        frame = parsed;
    }
    /* We are done with the top most exception without inner exceptions */
    /* because of no 'Caused by:' and no '...' */
    goto exception_parsing_successful;

current_exception_done:
    btp_location_add(location, 0, btp_skip_char_cspan(&cursor, "\n"));

    if (*cursor == '\n')
    {
        ++cursor;
        /* this adds one line */
        btp_location_add(location, 2, 0);
    }

    mark = cursor;
    cursor = btp_skip_whitespace(mark);
    btp_location_add(location, 0, cursor - mark);

    if (strncmp("Caused by: ", cursor, strlen("Caused by: ")) == 0)
    {
parse_inner_exception:
        cursor += strlen("Caused by: ");
        btp_location_add(location, 0, strlen("Caused by: "));

        struct btp_java_frame *inner = btp_java_frame_parse_exception(&cursor, location);
        if (inner == NULL)
        {
            btp_java_frame_free(exception);
            return NULL;
        }

        struct btp_java_frame *last_inner = btp_java_frame_get_last(inner);
        last_inner->next = exception;
        exception = inner;
    }

exception_parsing_successful:
    *input = cursor;

    return exception;
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
    btp_location_add(location, lines, columns + 2);

    /* SimpleTest.throwNullPointerException(SimpleTest.java:36) */
    cursor = btp_skip_whitespace(cursor);
    btp_location_add(location, 0, cursor - mark);
    mark = cursor;

    btp_location_add(location, 0, btp_skip_char_cspan(&cursor, "(\n"));

    struct btp_java_frame *frame = btp_java_frame_new();

    if (cursor != mark)
        frame->name = btp_strndup(mark, cursor - mark);

    /* (SimpleTest.java:36) */
    if (*cursor == '(')
    {
        ++cursor;
        btp_location_add(location, 0, 1);
        mark = cursor;

        btp_location_add(location, 0, btp_skip_char_cspan(&cursor, ":)\n"));

        if (mark != cursor)
        {
            if (btp_java_frame_parse_is_native_method(mark))
                frame->is_native = true;
            else if (!btp_java_frame_parse_is_unknown_source(mark))
                /* DO NOT set file_name if input says that source isn't known */
                frame->file_name = btp_strndup(mark, cursor - mark);
        }

        if (*cursor == ':')
        {
            ++cursor;
            btp_location_add(location, 0, 1);
            mark = cursor;

            btp_parse_uint32(&cursor, &(frame->file_line));

            btp_location_add(location, 0, cursor - mark);
        }
    }

    mark = cursor;
    cursor = strchrnul(cursor, '\n');

    if (*cursor == '\n')
    {
        *input = cursor + 1;
        btp_location_add(location, 2, 0);
    }
    else
    {
        *input = cursor;
        /* don't take \0 Byte into account */
        btp_location_add(location, 0, (cursor - mark) - 1);
    }

    return frame;
}

