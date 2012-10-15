/*
    koops_stacktrace.c

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
#include "koops_stacktrace.h"
#include "koops_frame.h"
#include "utils.h"
#include <string.h>

struct btp_koops_stacktrace *
btp_koops_stacktrace_new()
{
    struct btp_koops_stacktrace *stacktrace =
        btp_malloc(sizeof(struct btp_koops_stacktrace));

    btp_koops_stacktrace_init(stacktrace);
    return stacktrace;
}

void
btp_koops_stacktrace_init(struct btp_koops_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct btp_koops_stacktrace));
}

void
btp_koops_stacktrace_free(struct btp_koops_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->frames)
    {
        struct btp_koops_frame *frame = stacktrace->frames;
        stacktrace->frames = frame->next;
        btp_koops_frame_free(frame);
    }

    free(stacktrace->version);
    free(stacktrace);
}

struct btp_koops_stacktrace *
btp_koops_stacktrace_dup(struct btp_koops_stacktrace *stacktrace)
{
    struct btp_koops_stacktrace *result = btp_koops_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct btp_koops_stacktrace));

    if (result->frames)
        result->frames = btp_koops_frame_dup(result->frames, true);

    if (result->version)
        result->version = btp_strdup(result->version);

    return result;
}

int
btp_koops_stacktrace_get_frame_count(struct btp_koops_stacktrace *stacktrace)
{
    struct btp_koops_frame *frame = stacktrace->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }
    return count;
}

bool
btp_koops_stacktrace_remove_frame(struct btp_koops_stacktrace *stacktrace,
                                  struct btp_koops_frame *frame)
{
    struct btp_koops_frame *loop_frame = stacktrace->frames,
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                stacktrace->frames = loop_frame->next;

            btp_koops_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }

    return false;

}

struct btp_koops_stacktrace *
btp_koops_stacktrace_parse(const char **input,
                           struct btp_location *location)
{
    const char *local_input = *input;

    struct btp_koops_stacktrace *stacktrace = btp_koops_stacktrace_new();

    while (*local_input)
    {
        struct btp_koops_frame *frame = btp_koops_frame_parse(&local_input);

        if (frame)
        {
            stacktrace->frames = btp_koops_frame_append(stacktrace->frames, frame);
            btp_skip_char(&local_input, '\n');
            continue;
        }

        btp_skip_char_cspan(&local_input, "\n");
        btp_skip_char(&local_input, '\n');
    }

    *input = local_input;
    return stacktrace;
}

char **
btp_koops_stacktrace_parse_modules(const char **input)
{
    const char *local_input = *input;
    btp_skip_char_span(&local_input, " \t");

    /* Skip timestamp if it's present. */
    btp_koops_skip_timestamp(&local_input);
    btp_skip_char_span(&local_input, " \t");

    if (!btp_skip_string(&local_input, "Modules linked in:"))
        return NULL;

    btp_skip_char_span(&local_input, " \t");

    int result_size = 20, result_offset = 0;
    char **result = btp_malloc(result_size * sizeof(char*));

    char *module;
    bool success;
    while ((success = btp_parse_char_cspan(&local_input, " \t\n[", &module)))
    {
        // result_size is lowered by 1 because we need to terminate
        // the list by a NULL pointer.
        if (result_offset == result_size - 1)
        {
            result_size *= 2;
            result = btp_realloc(result,
                                 result_size * sizeof(char*));
        }

        result[result_offset] = module;
        ++result_offset;
        btp_skip_char_span(&local_input, " \t");
    }

    result[result_offset] = NULL;
    *input = local_input;
    return result;
}
