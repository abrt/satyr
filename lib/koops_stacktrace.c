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
