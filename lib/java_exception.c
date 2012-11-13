/*
    java_exception.c

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
#include "java_exception.h"
#include "java_frame.h"
#include "location.h"
#include "utils.h"
#include "strbuf.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct btp_java_exception *
btp_java_exception_new()
{
    struct btp_java_exception *exception =
        btp_malloc(sizeof(*exception));
    btp_java_exception_init(exception);
    return exception;
}

void
btp_java_exception_init(struct btp_java_exception *exception)
{
    exception->name = NULL;
    exception->message = NULL;
    exception->frames = NULL;
    exception->inner = NULL;
}

static void
btp_java_exception_free_frames(struct btp_java_exception *exception)
{
    while (exception->frames)
    {
        struct btp_java_frame *frame = exception->frames;
        exception->frames = frame->next;
        btp_java_frame_free(frame);
    }
}

bool
btp_java_exception_pop(struct btp_java_exception *exception)
{
    if (!exception->inner)
        return false;

    btp_java_exception_free_frames(exception);
    free(exception->name);
    free(exception->message);

    struct btp_java_exception *inner = exception->inner;

    exception->frames  = inner->frames;
    exception->inner   = inner->inner;
    exception->name    = inner->name;
    exception->message = inner->message;

    free(inner);

    return true;
}

void
btp_java_exception_free(struct btp_java_exception *exception)
{
    if (!exception)
        return;

    btp_java_exception_free_frames(exception);
    btp_java_exception_free(exception->inner);
    free(exception->name);
    free(exception->message);
    free(exception);
}

struct btp_java_exception *
btp_java_exception_dup(struct btp_java_exception *exception, bool deep)
{
    struct btp_java_exception *result = btp_java_exception_new();
    memcpy(result, exception, sizeof(*result));

    /* Handle deep. */
    if (deep)
    {
        if (result->inner)
            result->inner = btp_java_exception_dup(result->inner, true);
    }
    else
        result->inner = NULL; /* Do not copy that. */

    if (result->frames)
        result->frames = btp_java_frame_dup(result->frames, true);

    if (result->name)
        result->name = btp_strdup(result->name);

    if (result->message)
        result->message = btp_strdup(result->message);

    return result;
}

int
btp_java_exception_cmp(struct btp_java_exception *exception1,
                       struct btp_java_exception *exception2,
                       bool deep)
{
    const int ret = btp_strcmp0(exception1->name, exception2->name);
    if (ret)
        return ret;

    struct btp_java_frame *frame1 = exception1->frames;
    struct btp_java_frame *frame2 = exception2->frames;

    do
    {
        if ( frame1 && !frame2)
            return 1;
        if (!frame1 &&  frame2)
            return -1;
        if ( frame1 &&  frame2)
        {
            int frames = btp_java_frame_cmp(frame1, frame2);
            if (frames != 0)
                return frames;
            frame1 = frame1->next;
            frame2 = frame2->next;
        }
    }
    while (frame1 || frame2);

    if (deep)
    {
        struct btp_java_exception *inner1 = exception1->inner;
        struct btp_java_exception *inner2 = exception2->inner;

        if ( inner1 && !inner2)
            return 1;
        if (!inner1 && inner2)
            return -1;
        if ( inner1 && inner2)
            return btp_java_exception_cmp(inner1, inner2, deep);
    }

    return 0;
}

int
btp_java_exception_get_frame_count(struct btp_java_exception *exception, bool deep)
{
    struct btp_java_frame *frame = exception->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }

    if (deep && exception->inner)
        count += btp_java_exception_get_frame_count(exception->inner, deep);

    return count;
}

void
btp_java_exception_quality_counts(struct btp_java_exception *exception,
                                  int *ok_count,
                                  int *all_count,
                                  bool deep)
{
    struct btp_java_frame *frame = exception->frames;
    while (frame)
    {
        ++(*all_count);

        /* Functions with unknown file names are NOT OK */
        if (frame->is_native || frame->file_name)
            ++(*ok_count);

        frame = frame->next;
    }

    if (deep && exception->inner)
        btp_java_exception_quality_counts(exception->inner, ok_count, all_count, deep);
}

float
btp_java_exception_quality(struct btp_java_exception *exception, bool deep)
{
    int ok_count = 0, all_count = 0;
    btp_java_exception_quality_counts(exception, &ok_count, &all_count, deep);
    if (0 == all_count)
        return 1;

    return ok_count / (float)all_count;
}

bool
btp_java_exception_remove_frame(struct btp_java_exception *exception,
                                struct btp_java_frame *frame,
                                bool deep)
{
    struct btp_java_frame *loop_frame = exception->frames,
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                exception->frames = loop_frame->next;

            btp_java_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }

    if (deep && exception->inner)
        return btp_java_exception_remove_frame(exception->inner, frame, deep);

    return false;
}

bool
btp_java_exception_remove_frames_above(struct btp_java_exception *exception,
                                       struct btp_java_frame *frame,
                                       bool deep)
{
    /* Check that the frame is present in the exception. */
    struct btp_java_frame *loop_frame = exception->frames;
    while (loop_frame && loop_frame != frame)
        loop_frame = loop_frame->next;

    if (loop_frame)
    {
        /* Delete all the frames up to the frame. */
        while (exception->frames != frame)
        {
            loop_frame = exception->frames->next;
            btp_java_frame_free(exception->frames);
            exception->frames = loop_frame;
        }

        /* Stack trace of inner exception is above */
        btp_java_exception_free(exception->inner);
        exception->inner = NULL;

        return true;
    }
    else if (deep && exception->inner)
        return btp_java_exception_remove_frames_above(exception->inner, frame, deep);

    return false;
}

unsigned
btp_java_exception_remove_frames_below_n(struct btp_java_exception *exception,
                                         unsigned n,
                                         bool deep)
{
    if (deep && exception->inner)
        n = btp_java_exception_remove_frames_below_n(exception->inner, n, deep);

    if (n == 0)
    {
        /* if not deep then remove all frames immediately */
        /* if deep then try pop and if it fails then remove all frames */
        if (!deep || !btp_java_exception_pop(exception))
            btp_java_exception_free_frames(exception);

        return 0;
    }

    struct btp_java_frame *frame = exception->frames,
        *last_frame = NULL;

    while (frame && n)
    {
        last_frame = frame;
        frame = frame->next;
        --n;
    }

    /* Delete the remaining frames. */
    if (last_frame)
        last_frame->next = NULL;
    else
        exception->frames = NULL;

    while (frame)
    {
        struct btp_java_frame *delete_frame = frame;
        frame = frame->next;
        btp_java_frame_free(delete_frame);
    }

    return n;
}

void
btp_java_exception_append_to_str(struct btp_java_exception *exception,
                                 struct btp_strbuf *dest)
{
    btp_strbuf_append_str(dest, exception->name);

    if (exception->message)
        btp_strbuf_append_strf(dest, ": %s", exception->message);

    btp_strbuf_append_char(dest, '\n');

    struct btp_java_frame *frame = exception->frames;
    while (frame)
    {
        btp_java_frame_append_to_str(frame, dest);
        btp_strbuf_append_char(dest, '\n');

        frame = frame->next;
    }
}

struct btp_java_exception *
btp_java_exception_parse(const char **input,
                         struct btp_location *location)
{
    /* java.lang.NullPointerException: foo */
    const char *cursor = btp_skip_whitespace(*input);
    btp_location_add(location, 0, cursor - *input);
    const char *mark = cursor;

    btp_location_add(location, 0, btp_skip_char_cspan(&cursor, ":\n"));

    if (mark == cursor)
    {
        location->message = "Expected exception name";
        return NULL;
    }

    struct btp_java_exception *exception = btp_java_exception_new();
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

    if (*cursor == '\n')
    {
        ++cursor;
        btp_location_add(location, 2, 0);
    }
    /* else *cursor == '\0' */

    struct btp_java_frame *frame = NULL;
    /* iterate line by line
       best effort - continue on error */
    while (*cursor != '\0')
    {
        cursor = btp_skip_whitespace(mark);
        btp_location_add(location, 0, cursor - mark);

        if (strncmp("... ", cursor, strlen("... ")) == 0)
            goto current_exception_done;

        if (strncmp("Caused by: ", cursor, strlen("Caused by: ")) == 0)
            goto parse_inner_exception;

        struct btp_java_frame *parsed = btp_java_frame_parse(&cursor, location);

        if (parsed == NULL)
        {
            btp_java_exception_free(exception);
            return NULL;
        }

        mark = cursor;

        if (exception->frames == NULL)
            exception->frames = parsed;
        else
            frame->next = parsed;

        frame = parsed;

        btp_location_add(location, 2, 0);
    }
    goto exception_parsing_successful;

current_exception_done:
    btp_location_add(location, 0, btp_skip_char_cspan(&cursor, "\n"));

    if (*cursor == '\n')
    {
        ++cursor;
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

        exception->inner = btp_java_exception_parse(&cursor, location);
        if (exception->inner == NULL)
        {
            btp_java_exception_free(exception);
            return NULL;
        }
    }

exception_parsing_successful:
    *input = cursor;

    return exception;
}
