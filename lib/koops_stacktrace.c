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
#include "koops/stacktrace.h"
#include "koops/frame.h"
#include "utils.h"
#include "json.h"
#include "strbuf.h"
#include "normalize.h"
#include <string.h>
#include <stddef.h>

/* http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/plain/kernel/panic.c?id=HEAD */

#define FLAG_OFFSET(flag) offsetof(struct sr_koops_stacktrace, taint_ ## flag)
#define FLAG(letter, name)  { letter, FLAG_OFFSET(name), #name }

struct sr_taint_flag sr_flags[] = {
    FLAG('P', module_proprietary),
    FLAG('F', forced_module),
    FLAG('S', smp_unsafe),
    FLAG('R', forced_removal),
    FLAG('M', mce),
    FLAG('B', page_release),
    FLAG('U', userspace),
    FLAG('D', died_recently),
    FLAG('A', acpi_overridden),
    FLAG('W', warning),
    FLAG('C', staging_driver),
    FLAG('I', firmware_workaround),
    FLAG('O', module_out_of_tree),
    { '\0', 0, NULL} /* sentinel */
};

#undef FLAG
#undef FLAG_OFFSET

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
    stacktrace->type = SR_REPORT_KERNELOOPS;
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

/* Based on function kernel_tainted_short from abrt/src/lib/kernel.c */
static bool
parse_taint_flags(const char *input, struct sr_koops_stacktrace *stacktrace)
{
    /* example of flags: 'Tainted: G    B       ' */
    const char *tainted = strstr(input, "Tainted: ");
    if (!tainted)
        return false;

    tainted += strlen("Tainted: ");

    for (;;)
    {
        if (tainted[0] >= 'A' && tainted[0] <= 'Z')
        {
            /* set the appropriate flag */
            struct sr_taint_flag *f;
            for (f = sr_flags; f->letter; f++)
            {
                if (tainted[0] == f->letter)
                {
                    *(bool *)((void *)stacktrace + f->member_offset) = true;
                    break;
                }
            }
        }
        else if (tainted[0] != ' ')
            break;

        ++tainted;
    }

    return true;
}

/* a special line with instruction pointer may be present in the format
 * RIP: 0010:[<ffffffff811c6ed5>]  [<ffffffff811c6ed5>] __block_write_full_page+0xa5/0x350
 * (for x86_64), or
 * EIP: [<f8e40765>] wdev_priv.part.8+0x3/0x5 [wl] SS:ESP 0068:f180dbf8
 * (for i386, where it is AFTER the trace)
 */
static struct sr_koops_frame*
parse_IP(const char **input)
{
    struct sr_koops_frame *frame;
    const char *local_input = *input;
    sr_skip_char_span(&local_input, " \t");

    /* Skip timestamp if it's present. */
    sr_koops_skip_timestamp(&local_input);
    sr_skip_char_span(&local_input, " \t");

    if (sr_skip_string(&local_input, "RIP:"))
    {
        if (!sr_skip_char_span(&local_input, " \t"))
            return NULL;

        /* The address is there twice, skip the first one */
        if (!sr_skip_char_cspan(&local_input, " \t"))
            return NULL;

        if (!sr_skip_char_span(&local_input, " \t"))
            return NULL;

        frame = sr_koops_frame_parse(&local_input);
        if (!frame)
            return NULL;
    }
    else if(sr_skip_string(&local_input, "EIP:"))
    {
        if (!sr_skip_char_span(&local_input, " \t"))
            return NULL;

        frame = sr_koops_frame_new();

        if (!sr_koops_parse_address(&local_input, &frame->address))
        {
            sr_koops_frame_free(frame);
            return NULL;
        }

        sr_skip_char_span(&local_input, " \t");

        /* Question mark means unreliable */
        frame->reliable = sr_skip_char(&local_input, '?') != true;

        sr_skip_char_span(&local_input, " \t");

        if (!sr_koops_parse_function(&local_input,
                                     &frame->function_name,
                                     &frame->function_offset,
                                     &frame->function_length,
                                     &frame->module_name))
        {
            sr_koops_frame_free(frame);
            return NULL;
        }
    }
    else
        return NULL;

    sr_skip_char_cspan(&local_input, "\n");
    *input = local_input;
    return frame;
}

struct sr_koops_stacktrace *
sr_koops_stacktrace_parse(const char **input,
                          struct sr_location *location)
{
    const char *local_input = *input;

    struct sr_koops_stacktrace *stacktrace = sr_koops_stacktrace_new();
    struct sr_koops_frame *frame;
    bool parsed_ip = false;

    /* Looks for the "Tainted: " line in the whole input */
    parse_taint_flags(local_input, stacktrace);

    while (*local_input)
    {
        if (!stacktrace->modules &&
            (stacktrace->modules = sr_koops_stacktrace_parse_modules(&local_input)))
        {
        }
        else if (!parsed_ip &&
                 (frame = parse_IP(&local_input)))
        {
            /* this is the very first frame (even though for i386 is at the
             * end, we need to prepend it */
            stacktrace->frames = sr_koops_frame_prepend(stacktrace->frames, frame);
            parsed_ip = true;
        }
        else if((frame = sr_koops_frame_parse(&local_input)))
        {
            stacktrace->frames = sr_koops_frame_append(stacktrace->frames, frame);
        }
        else
            sr_skip_char_cspan(&local_input, "\n");

        sr_skip_char(&local_input, '\n');
    }

    *input = local_input;
    return stacktrace;
}

static bool
module_list_continues(const char *input)
{
    /* There should be no timestamp in the continuation */
    if (sr_koops_skip_timestamp(&input))
        return false;

    /* Usually follows the module list */
    if (sr_skip_string(&input, "Pid: "))
        return false;

    /* See tests/kerneloopses/rhbz-865695-2-notime */
    if (sr_skip_string(&input, "CPU ") && sr_skip_char_span(&input, "0123456789"))
        return false;

    /* Other conditions may need to be added */

    return true;
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

    int ws = sr_skip_char_span(&local_input, " \t");

    int result_size = 20, result_offset = 0;
    char **result = sr_malloc(result_size * sizeof(char*));

    char *module;
    while (true)
    {
        if (sr_parse_char_cspan(&local_input, " \t\n[", &module))
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
            ws = sr_skip_char_span(&local_input, " \t");
        }
        else if(*local_input == '\n')
        {
            /* We're at the end of the line. Check whether the next line might
             * be a continuation of the module list. */
            if (!module_list_continues(local_input + 1))
                break;

            local_input++;

            /* If the next line does not start with space and there wasn't
             * any space before the newline either, then the last module was
             * split into two parts and we need to read the rest */
            if (*local_input != ' ' && ws == 0 && result_offset > 0)
            {
                char *therest;
                if (!sr_parse_char_cspan(&local_input, " \t\n[", &therest))
                {
                    break; /* wtf? */
                }

                char *tmp = sr_asprintf("%s%s", result[result_offset-1], therest);
                free(result[result_offset-1]);
                result[result_offset-1] = tmp;
            }

            ws = sr_skip_char_span(&local_input, " \t");
        }
        else
            break;
    }

    result[result_offset] = NULL;
    *input = local_input;
    return result;
}

static char *
taint_flags_to_json(struct sr_koops_stacktrace *stacktrace)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    struct sr_taint_flag *f;
    for (f = sr_flags; f->letter; f++)
    {
        bool val = *(bool *)((void *)stacktrace + f->member_offset);
        if (val == true)
        {
            sr_strbuf_append_strf(strbuf, ", \"%s\"\n", f->name);
        }
    }

    if (strbuf->len == 0)
        return sr_strdup("[]");

    sr_strbuf_append_char(strbuf, ']');
    char *result = sr_strbuf_free_nobuf(strbuf);
    result[0] = '[';
    result[strlen(result) - 2] = ' '; /* erase the last newline */
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
    char *taint_flags = taint_flags_to_json(stacktrace);
    char *indented_taint_flags = sr_indent_except_first_line(taint_flags, strlen(",   \"taint_flags\": "));
    free(taint_flags);
    sr_strbuf_append_strf(strbuf, ",   \"taint_flags\": %s\n", indented_taint_flags);
    free(indented_taint_flags);

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

char *
sr_koops_stacktrace_get_reason(struct sr_koops_stacktrace *stacktrace)
{
    char *func = "<unknown>";
    char *result;

    struct sr_koops_stacktrace *copy = sr_koops_stacktrace_dup(stacktrace);
    sr_normalize_koops_stacktrace(copy);

    if (copy->frames && copy->frames->function_name)
        func = copy->frames->function_name;

    if (copy->frames && copy->frames->module_name)
    {
        result = sr_asprintf("Kernel oops in %s [%s]", func,
                copy->frames->module_name);
    }
    else
        result = sr_asprintf("Kernel oops in %s", func);

    sr_koops_stacktrace_free(copy);

    return result;
}
