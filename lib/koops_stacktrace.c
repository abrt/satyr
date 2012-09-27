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

struct btp_koops_stacktrace *
btp_koops_stacktrace_parse(const char **input,
                           struct btp_location *location)
{
    const char *line = *input;

    while (line)
        btp_koops_parse_frame(&line);

    return NULL;
}

bool
btp_koops_skip_timestamp(const char **input)
{
    const char *local_input = *input;
    if (!btp_skip_char(&local_input, '['))
        return false;

    btp_skip_char_span(&local_input, BTP_space);

    int num = btp_skip_uint(&local_input);
    if (0 == num)
        return false;

    if (!btp_skip_char(&local_input, '.'))
        return false;

    num = btp_skip_uint(&local_input);
    if (0 == num)
        return false;

    if (!btp_skip_char(&local_input, ']'))
        return false;

    *input = local_input;
    return true;
}

bool
btp_koops_parse_address(const char **input, uint64_t *address)
{
    const char *local_input = *input;

    if (!btp_skip_string(&local_input, "[<"))
        return false;

    int len = btp_parse_hexadecimal_uint64(&local_input, address);
    if (!len)
        return false;

    if (!btp_skip_string(&local_input, ">]"))
        return false;

    *input = local_input;
    return true;
}

bool
btp_koops_parse_module_name(const char **input,
                            char **module_name)
{
    const char *local_input = *input;

    if (!btp_skip_char(&local_input, '['))
        return false;

    if (!btp_parse_char_cspan(&local_input, " \t\n]",
                              module_name))
    {
        return false;
    }

    if (!btp_skip_char(&local_input, ']'))
    {
        free(*module_name);
        return false;
    }

    *input = local_input;
    return true;
}

bool
btp_koops_parse_function_name(const char **input,
                              char **function_name,
                              uint64_t *function_offset,
                              uint64_t *function_length)
{
    const char *local_input = *input;

    bool parenthesis = btp_skip_char(&local_input, '(');

    if (!btp_parse_char_cspan(&local_input, " \t)+",
                              function_name))
    {
        return false;
    }

    if (btp_skip_char(&local_input, '+'))
    {
        btp_parse_hexadecimal_0xuint64(&local_input,
                                       function_offset);

        if (!btp_skip_char(&local_input, '/'))
        {
            free(*function_name);
            return false;
        }

        btp_parse_hexadecimal_0xuint64(&local_input,
                                       function_length);
    }
    else
    {
        *function_offset = -1;
        *function_length = -1;
    }

    if (parenthesis && !btp_skip_char(&local_input, ')'))
    {
        free(*function_name);
        return false;
    }

    *input = local_input;
    return true;
}

struct btp_koops_frame *
btp_koops_parse_frame(const char **input)
{
    const char *local_input = *input;

    btp_skip_char_span(&local_input, " \t");

    /* Skip timestamp if it's present. */
    btp_koops_skip_timestamp(&local_input);

    btp_skip_char_span(&local_input, " \t");

    uint64_t address;
    if (!btp_koops_parse_address(&local_input, &address))
        return false;

    btp_skip_char_span(&local_input, " \t");

    /* Question mark? */
    bool reliable = (0 != btp_skip_char(&local_input, '?'));

    btp_skip_char_span(&local_input, " \t");

    struct btp_koops_frame *frame = btp_koops_frame_new();
    frame->address = address;
    frame->reliable = reliable;
    if (!btp_koops_parse_function_name(&local_input,
                                       &frame->function_name,
                                       &frame->function_offset,
                                       &frame->function_length))
    {
        btp_koops_frame_free(frame);
        return NULL;
    }

    btp_skip_char_span(&local_input, " \t");

    if (btp_skip_string(&local_input, "from"))
    {
        btp_skip_char_span(&local_input, " \t");

        if (!btp_koops_parse_address(&local_input,
                                     &frame->from_address))
        {
            btp_koops_frame_free(frame);
            return NULL;
        }

        btp_skip_char_span(&local_input, " \t");

        if (!btp_koops_parse_function_name(&local_input,
                                           &frame->from_function_name,
                                           &frame->from_function_offset,
                                           &frame->from_function_length))
        {
            btp_koops_frame_free(frame);
            return NULL;
        }

        btp_skip_char_span(&local_input, " \t");
    }


    if (btp_koops_parse_module_name(&local_input, &frame->module_name))
    {
        btp_skip_char_span(&local_input, " \t");
    }

    if (*local_input != '\0' && !btp_skip_char(&local_input, '\n'))
    {
        btp_koops_frame_free(frame);
        return NULL;
    }

    *input = local_input;
    return frame;
}
