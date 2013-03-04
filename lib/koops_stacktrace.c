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

struct sr_koops_stacktrace *
sr_koops_stacktrace_new()
{
    struct sr_koops_stacktrace *stacktrace =
        sr_malloc(sizeof(struct sr_koops_stacktrace));

    sr_koops_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_koops_stacktrace_init(struct sr_koops_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct sr_koops_stacktrace));
}

void
sr_koops_stacktrace_free(struct sr_koops_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->frames)
    {
        struct sr_koops_frame *frame = stacktrace->frames;
        stacktrace->frames = frame->next;
        sr_koops_frame_free(frame);
    }

    free(stacktrace->version);
    free(stacktrace);
}

struct sr_koops_stacktrace *
sr_koops_stacktrace_dup(struct sr_koops_stacktrace *stacktrace)
{
    struct sr_koops_stacktrace *result = sr_koops_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_koops_stacktrace));

    if (result->frames)
        result->frames = sr_koops_frame_dup(result->frames, true);

    if (result->version)
        result->version = sr_strdup(result->version);

    return result;
}

int
sr_koops_stacktrace_get_frame_count(struct sr_koops_stacktrace *stacktrace)
{
    struct sr_koops_frame *frame = stacktrace->frames;
    int count = 0;
    while (frame)
    {
        frame = frame->next;
        ++count;
    }
    return count;
}

bool
sr_koops_stacktrace_remove_frame(struct sr_koops_stacktrace *stacktrace,
                                 struct sr_koops_frame *frame)
{
    struct sr_koops_frame *loop_frame = stacktrace->frames,
        *prev_frame = NULL;

    while (loop_frame)
    {
        if (loop_frame == frame)
        {
            if (prev_frame)
                prev_frame->next = loop_frame->next;
            else
                stacktrace->frames = loop_frame->next;

            sr_koops_frame_free(loop_frame);
            return true;
        }

        prev_frame = loop_frame;
        loop_frame = loop_frame->next;
    }

    return false;

}

struct sr_koops_stacktrace *
sr_koops_stacktrace_parse(const char **input,
                          struct sr_location *location)
{
    const char *local_input = *input;

    struct sr_koops_stacktrace *stacktrace = sr_koops_stacktrace_new();

    while (*local_input)
    {
        struct sr_koops_frame *frame = sr_koops_frame_parse(&local_input);
        if (!stacktrace->modules) {
            stacktrace->modules = sr_koops_stacktrace_parse_modules(&local_input);
        }

        if (frame)
        {
            stacktrace->frames = sr_koops_frame_append(stacktrace->frames, frame);
            sr_skip_char(&local_input, '\n');
            continue;
        }

        sr_skip_char_cspan(&local_input, "\n");
        sr_skip_char(&local_input, '\n');
    }

    *input = local_input;
    return stacktrace;
}

char **
sr_koops_stacktrace_parse_modules(const char **input)
{
    const char *local_input = *input;
    sr_skip_char_span(&local_input, " \t");

    /* Skip timestamp if it's present. */
    sr_koops_skip_timestamp(&local_input);
    sr_skip_char_span(&local_input, " \t");

    if (!sr_skip_string(&local_input, "Modules linked in:"))
        return NULL;

    sr_skip_char_span(&local_input, " \t");

    int result_size = 20, result_offset = 0;
    char **result = sr_malloc(result_size * sizeof(char*));

    char *module;
    bool success;
    while ((success = sr_parse_char_cspan(&local_input, " \t\n[", &module)))
    {
        // result_size is lowered by 1 because we need to terminate
        // the list by a NULL pointer.
        if (result_offset == result_size - 1)
        {
            result_size *= 2;
            result = sr_realloc(result,
                                result_size * sizeof(char*));
        }

        result[result_offset] = module;
        ++result_offset;
        sr_skip_char_span(&local_input, " \t");
    }

    result[result_offset] = NULL;
    *input = local_input;
    return result;
}

char *
sr_koops_stacktrace_to_json(struct sr_koops_stacktrace *stacktrace)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    /* Kernel version. */
    if (stacktrace->version)
    {
        sr_strbuf_append_str(strbuf, ",   \"version\": ");
        sr_json_append_escaped(strbuf, stacktrace->version);
        sr_strbuf_append_str(strbuf, "\n");
    }

    /* Kernel taint flags. */
    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_module_proprietary\": %s\n",
                          stacktrace->taint_module_proprietary ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_module_gpl\": %s\n",
                          stacktrace->taint_module_gpl ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_module_out_of_tree\": %s\n",
                          stacktrace->taint_module_out_of_tree ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_forced_module\": %s\n",
                          stacktrace->taint_forced_module ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_forced_removal\": %s\n",
                          stacktrace->taint_forced_removal ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_smp_unsafe\": %s\n",
                          stacktrace->taint_smp_unsafe ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_mce\": %s\n",
                          stacktrace->taint_mce ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_page_release\": %s\n",
                          stacktrace->taint_page_release ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_userspace\": %s\n",
                          stacktrace->taint_userspace ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_died_recently\": %s\n",
                          stacktrace->taint_died_recently ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_acpi_overridden\": %s\n",
                          stacktrace->taint_acpi_overridden ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_warning\": %s\n",
                          stacktrace->taint_warning ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_staging_driver\": %s\n",
                          stacktrace->taint_staging_driver ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_firmware_workaround\": %s\n",
                          stacktrace->taint_firmware_workaround ? "true" : "false");

    sr_strbuf_append_strf(strbuf,
                          ",   \"taint_virtual_box\": %s\n",
                          stacktrace->taint_virtual_box ? "true" : "false");

    /* Modules. */
    if (stacktrace->modules)
    {
        sr_strbuf_append_strf(strbuf, ",   \"modules\":\n");
        char **module = stacktrace->modules;
        while (*module)
        {
            if (module == stacktrace->modules)
                sr_strbuf_append_str(strbuf, "      [ ");
            else
                sr_strbuf_append_str(strbuf, "      , ");

            sr_json_append_escaped(strbuf, *module);
            ++module;
            if (*module)
                sr_strbuf_append_str(strbuf, "\n");
        }

        sr_strbuf_append_str(strbuf, " ]\n");
    }

    /* Frames. */
    if (stacktrace->frames)
    {
        struct sr_koops_frame *frame = stacktrace->frames;
        sr_strbuf_append_str(strbuf, ",   \"frames\":\n");
        while (frame)
        {
            if (frame == stacktrace->frames)
                sr_strbuf_append_str(strbuf, "      [ ");
            else
                sr_strbuf_append_str(strbuf, "      , ");

            char *frame_json = sr_koops_frame_to_json(frame);
            char *indented_frame_json = sr_indent_except_first_line(frame_json, 8);
            sr_strbuf_append_str(strbuf, indented_frame_json);
            free(indented_frame_json);
            free(frame_json);
            frame = frame->next;
            if (frame)
                sr_strbuf_append_str(strbuf, "\n");
        }

        sr_strbuf_append_str(strbuf, " ]\n");
    }

    if (strbuf->len > 0)
        strbuf->buf[0] = '{';
    else
        sr_strbuf_append_char(strbuf, '{');

    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}
