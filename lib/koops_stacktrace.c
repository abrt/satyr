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
    stacktrace->version = NULL;
    stacktrace->taint = NULL;
    stacktrace->frames = NULL;
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
    free(stacktrace->taint);
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

    if (result->taint)
        result->taint = btp_strdup(result->taint);

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
parse_frame_line(const char *line)
{
    /* Skip timestamp if it's present. */
    skip_timestamp(&line);

    btp_skip_char_span(&line, BTP_space);

    uint64_t address;
    if (!parse_address(&line, &address))
        return false;
/*
    // function name
    char *funcname = strchr(line, ' ');
    if (!funcname)
    {
        btp_backtrace_entry_free(frame);
        fprintf(stderr, "Unable to find functon name: '%s'\n", line);
        line = nextline;
        continue;
    }
    ++funcname;

    while (*funcname && !isalpha(*funcname))
    {
        // threre is no correct place for the '?' flag in
        // struct backtrace_entry. took function_initial_loc
        // because it was unused
        if (*funcname == '?')
            frame->function_initial_loc = -1;
        ++funcname;
    }

    if (!*funcname)
    {
        btp_backtrace_entry_free(frame);
        fprintf(stderr, "Unable to find functon name: '%s'\n", line);
        line = nextline;
        continue;
    }

    char *splitter = strchr(funcname, '+');
    if (!splitter)
    {
        btp_backtrace_entry_free(frame);
        fprintf(stderr, "Unable to find offset & function length: '%s'\n", line);
        line = nextline;
        continue;
    }
    *splitter = '\0';
    frame->symbol = btp_strdup(funcname);
    *splitter = '+';
    ++splitter;

    // offset, function legth
    if (sscanf(splitter, "0x%x/0x%x", &frame->build_id_offset, &frame->function_length) != 2)
    {
        btp_backtrace_entry_free(frame);
        fprintf(stderr, "Unable to read offset & function length: '%s'\n", line);
        line = nextline;
        continue;
    }

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

    if (!frame->filename)
        frame->filename = btp_strdup("vmlinux");

    // build-id
    // there is no actual "build-id" for kernel
    if (!frame->build_id && kernelver)
        frame->build_id = btp_strdup(kernelver);

    result = g_list_append(result, frame);
    line = nextline;
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
