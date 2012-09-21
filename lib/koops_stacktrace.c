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

/**
 * Timestamp may be present in the oops lines.
 * @example
 * [123456.654321]
 * [   65.470000]
 */
static bool
skip_timestamp(const char **input)
{
    const char *local_input = *input;
    if (!btp_skip_char(&local_input, '['))
        return false;

    btp_skip_char_span(&local_input, BTP_space);

    int num = btp_skip_unsigned_integer(&local_input);
    if (0 == num)
        return false;

    if (!btp_skip_char(&local_input, '.'))
        return false;

    num = btp_skip_unsigned_integer(&local_input);
    if (0 == num)
        return false;

    if (!btp_skip_char(&local_input, ']'))
        return false;

    *input = local_input;
    return true;
}

static bool
parse_address(const char **input, uint64_t *address)
{
    const char *local_input = *input;

    if (!btp_skip_string(&local_input, "[<"))
        return false;

    int len = btp_parse_uint64(&local_input, address);
    if (!len)
        return false;

    if (!btp_skip_string(&local_input, ">]"))
        return false;

    *input = local_input;
    return true;
}

/**
 * @returns
 * True if line was successfully parsed, false if line is in unknown
 * format.
 */
static bool
parse_frame_line(const char **input)
{
    const char *local_input = *input;

    btp_skip_char_span(&local_input, BTP_space);

    /* Skip timestamp if it's present. */
    skip_timestamp(&local_input);

    btp_skip_char_span(&local_input, BTP_space);

    uint64_t address;
    if (!parse_address(&local_input, &address))
        return false;

    btp_skip_char_span(&local_input, BTP_space);

    /* Question mark? */
    bool reliable = (0 != btp_skip_char(&local_input, '?'));

    btp_skip_char_span(&local_input, BTP_space);

    char *function_name;
    if (!btp_parse_char_cspan(&local_input, BTP_space "+",
                              &function_name))
    {
        return false;
    }

    if (btp_skip_char(&local_input, '+'))
    {
        uint64_t function_offset;
        btp_parse_hexadecimal_number(&local_input,
                                     &function_offset);

        if (!btp_skip_char(&local_input, '/'))
        {
            free(function_name);
            return false;
        }

        uint64_t function_length;
        btp_parse_hexadecimal_number(&local_input,
                                     &function_length);
    }

    btp_skip_char_span(&local_input, " \t");

    if (!btp_skip_char(&local_input, '\n'))
    {
        free(function_name);
        return false;
    }

    return true;

/*
    // module
    // in the 2nd example mentioned above, [] also matches
    // the address part of line's 2nd half (callback).
    char *callback = strstr(splitter, "from");
    char *module = strchr(splitter, '[');
    if (module && (!callback || callback > module))
    {
        ++module;
        splitter = strchr(module, ']');
        if (splitter)
        {
            *splitter = '\0';
            frame->filename = btp_strdup(module);
            *splitter = ']';
        }
    }
*/

    return true;
}

struct btp_koops_stacktrace *
btp_koops_stacktrace_parse(const char **input,
                           struct btp_location *location)
{
    const char *line = *input;

    while (line)
        parse_frame_line(line);

    return NULL;
}
