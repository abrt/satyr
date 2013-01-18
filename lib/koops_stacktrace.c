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
#include "strbuf.h"
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
        if (!stacktrace->modules) {
            stacktrace->modules = btp_koops_stacktrace_parse_modules(&local_input);
        }

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

char *
btp_koops_stacktrace_to_json(struct btp_koops_stacktrace *stacktrace)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();

    /* Kernel version. */
    if (stacktrace->version)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"version\": \"%s\"\n",
                               stacktrace->version);
    }

    /* Kernel taint flags. */
    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_module_proprietary\": %s\n",
                           stacktrace->taint_module_proprietary ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_module_gpl\": %s\n",
                           stacktrace->taint_module_gpl ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_module_out_of_tree\": %s\n",
                           stacktrace->taint_module_out_of_tree ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_forced_module\": %s\n",
                           stacktrace->taint_forced_module ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_forced_removal\": %s\n",
                           stacktrace->taint_forced_removal ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_smp_unsafe\": %s\n",
                           stacktrace->taint_smp_unsafe ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_mce\": %s\n",
                           stacktrace->taint_mce ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_page_release\": %s\n",
                           stacktrace->taint_page_release ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_userspace\": %s\n",
                           stacktrace->taint_userspace ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_died_recently\": %s\n",
                           stacktrace->taint_died_recently ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_acpi_overridden\": %s\n",
                           stacktrace->taint_acpi_overridden ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_warning\": %s\n",
                           stacktrace->taint_warning ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_staging_driver\": %s\n",
                           stacktrace->taint_staging_driver ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_firmware_workaround\": %s\n",
                           stacktrace->taint_firmware_workaround ? "true" : "false");

    btp_strbuf_append_strf(strbuf,
                           ",   \"taint_virtual_box\": %s\n",
                           stacktrace->taint_virtual_box ? "true" : "false");

    /* Modules. */
    if (stacktrace->modules)
    {
        btp_strbuf_append_strf(strbuf, ",   \"modules\":\n");
        char **module = stacktrace->modules;
        while (*module)
        {
            if (module == stacktrace->modules)
                btp_strbuf_append_str(strbuf, "      [ ");
            else
                btp_strbuf_append_str(strbuf, "      , ");

            btp_strbuf_append_strf(strbuf, "\"%s\"", *module);
            ++module;
            if (*module)
                btp_strbuf_append_str(strbuf, "\n");
        }

        btp_strbuf_append_str(strbuf, " ]\n");
    }

    /* Frames. */
    if (stacktrace->frames)
    {
        struct btp_koops_frame *frame = stacktrace->frames;
        btp_strbuf_append_str(strbuf, ",   \"frames\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                btp_strbuf_append_str(strbuf, "      [ ");
            else
                btp_strbuf_append_str(strbuf, "      , ");

            char *frame_json = btp_koops_frame_to_json(frame);
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
