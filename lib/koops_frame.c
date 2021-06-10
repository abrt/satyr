/*
    koops_frame.c

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
#include "koops/frame.h"
#include "utils.h"
#include "json.h"
#include "generic_frame.h"
#include "thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* Method table */

static void
koops_append_bthash_text(struct sr_koops_frame *frame, enum sr_bthash_flags flags,
                         GString *strbuf);
static void
koops_append_duphash_text(struct sr_koops_frame *frame, enum sr_duphash_flags flags,
                          GString *strbuf);

DEFINE_NEXT_FUNC(koops_next, struct sr_frame, struct sr_koops_frame)
DEFINE_SET_NEXT_FUNC(koops_set_next, struct sr_frame, struct sr_koops_frame)

struct frame_methods koops_frame_methods =
{
    .append_to_str = (append_to_str_fn_t) sr_koops_frame_append_to_str,
    .next = (next_frame_fn_t) koops_next,
    .set_next = (set_next_frame_fn_t) koops_set_next,
    .cmp = (frame_cmp_fn_t) sr_koops_frame_cmp,
    .cmp_distance = (frame_cmp_fn_t) sr_koops_frame_cmp_distance,
    .frame_append_bthash_text =
        (frame_append_bthash_text_fn_t) koops_append_bthash_text,
    .frame_append_duphash_text =
        (frame_append_duphash_text_fn_t) koops_append_duphash_text,
    .frame_free = (frame_free_fn_t) sr_koops_frame_free,
};

/* Public functions */

struct sr_koops_frame *
sr_koops_frame_new()
{
    struct sr_koops_frame *frame =
        g_malloc(sizeof(*frame));

    sr_koops_frame_init(frame);
    return frame;
}

void
sr_koops_frame_init(struct sr_koops_frame *frame)
{
    memset(frame, 0, sizeof(struct sr_koops_frame));
    frame->type = SR_REPORT_KERNELOOPS;
}

void
sr_koops_frame_free(struct sr_koops_frame *frame)
{
    if (!frame)
        return;

    g_free(frame->function_name);
    g_free(frame->module_name);
    g_free(frame->from_function_name);
    g_free(frame->from_module_name);
    g_free(frame->special_stack);
    g_free(frame);
}

struct sr_koops_frame *
sr_koops_frame_dup(struct sr_koops_frame *frame, bool siblings)
{
    struct sr_koops_frame *result = sr_koops_frame_new();
    memcpy(result, frame, sizeof(struct sr_koops_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_koops_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->function_name)
        result->function_name = g_strdup(result->function_name);

    if (result->module_name)
        result->module_name = g_strdup(result->module_name);

    if (result->from_function_name)
        result->from_function_name = g_strdup(result->from_function_name);

    if (result->from_module_name)
        result->from_module_name = g_strdup(result->from_module_name);

    if (result->special_stack)
        result->special_stack = g_strdup(result->special_stack);

    return result;
}

int
sr_koops_frame_cmp(struct sr_koops_frame *frame1,
                   struct sr_koops_frame *frame2)
{
    /* Address. */
    int address = frame2->address - frame1->address;
    if (address != 0)
        return address;

    /* Reliable. */
    int reliable = (int)frame2->reliable - (int)frame1->reliable;
    if (reliable != 0)
        return reliable;

    /* Function name. */
    int function_name = g_strcmp0(frame1->function_name,
                                   frame2->function_name);

    if (function_name != 0)
        return function_name;

    /* Function offset. */
    int64_t function_offset = frame2->function_offset - frame1->function_offset;
    if (function_offset != 0)
        return function_offset;

    /* Function length. */
    int64_t function_length = frame2->function_length - frame1->function_length;
    if (function_length != 0)
        return function_length;

    /* Module name. */
    int module_name = g_strcmp0(frame1->module_name,
                                 frame2->module_name);

    if (module_name != 0)
        return module_name;

    /* From address. */
    int from_address = frame2->from_address - frame1->from_address;
    if (from_address != 0)
        return from_address;

    /* From function name. */
    int from_function_name = g_strcmp0(frame1->from_function_name,
                                        frame2->from_function_name);

    if (from_function_name != 0)
        return from_function_name;

    /* From function offset. */
    int64_t from_function_offset = frame2->from_function_offset - frame1->from_function_offset;
    if (from_function_offset != 0)
        return from_function_offset;

    /* From function length. */
    int64_t from_function_length = frame2->from_function_length - frame1->from_function_length;
    if (from_function_length != 0)
        return from_function_length;

    /* From module name. */
    int from_module_name = g_strcmp0(frame1->from_module_name,
                                      frame2->from_module_name);

    if (from_module_name != 0)
        return from_module_name;

    /* Special stack. */
    int special_stack = g_strcmp0(frame1->special_stack,
                                   frame2->special_stack);

    if (special_stack != 0)
        return special_stack;

    return 0;
}

int
sr_koops_frame_cmp_distance(struct sr_koops_frame *frame1,
                            struct sr_koops_frame *frame2)
{
    /* Function. */
    int function_name = g_strcmp0(frame1->function_name,
                                    frame2->function_name);
    if (function_name != 0)
        return function_name;

    return 0;
}

struct sr_koops_frame *
sr_koops_frame_append(struct sr_koops_frame *dest,
                      struct sr_koops_frame *item)
{
    if (!dest)
        return item;

    struct sr_koops_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

struct sr_koops_frame *
sr_koops_frame_prepend(struct sr_koops_frame *dest,
                       struct sr_koops_frame *item)
{
    if (!dest)
        return item;

    item->next = dest;
    return item;
}

static struct sr_koops_frame *
koops_frame_parse_ppc(const char **input)
{
    const char *local_input = *input;
    sr_skip_char_span(&local_input, " \t");

    struct sr_koops_frame *frame = sr_koops_frame_new();

    uint64_t sp; /* unused */

    if (!sr_koops_parse_address(&local_input, &sp))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    if (!sr_koops_parse_address(&local_input, &frame->address))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    /* strip the leading dots */
    sr_skip_char(&local_input, '.');

    if (!sr_koops_parse_function(&local_input,
                                 &frame->function_name,
                                 &frame->function_offset,
                                 &frame->function_length,
                                 &frame->module_name))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    frame->reliable = (sr_skip_string(&local_input, "(unreliable)") == 0);

    sr_skip_char_span(&local_input, " \t");

    if (*local_input != '\0' && !sr_skip_char(&local_input, '\n'))
        goto fail;

    *input = local_input;
    return frame;

fail:
    sr_koops_frame_free(frame);
    return NULL;
}

static struct sr_koops_frame *
koops_frame_parse_arm_reduced(const char **input)
{
    const char *local_input = *input;
    sr_skip_char_span(&local_input, " \t");

    struct sr_koops_frame *frame = sr_koops_frame_new();

    if (!sr_koops_parse_address(&local_input, &frame->address))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    /* Question mark means unreliable */
    frame->reliable = sr_skip_char(&local_input, '?') != true;

    sr_skip_char_span(&local_input, " \t");

    if (!sr_skip_char(&local_input, '('))
        goto fail;

    if (!sr_parse_char_cspan(&local_input, " \t\n)+<", &frame->function_name))
        goto fail;

    if (!sr_skip_char(&local_input, ')'))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    if (!sr_skip_string(&local_input, "from"))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    if (!sr_koops_parse_address(&local_input, &frame->from_address))
        goto fail;

    sr_skip_char_span(&local_input, " \t");

    if (!sr_koops_parse_function(&local_input,
                                 &frame->from_function_name,
                                 &frame->from_function_offset,
                                 &frame->from_function_length,
                                 &frame->from_module_name))
        goto fail;

    *input = local_input;
    return frame;

fail:
    sr_koops_frame_free(frame);
    return NULL;
}

struct sr_koops_frame *
sr_koops_frame_parse(const char **input)
{
    struct sr_koops_frame *ppc_frame = koops_frame_parse_ppc(input);
    if (ppc_frame)
        return ppc_frame;

    struct sr_koops_frame *arm_frame = koops_frame_parse_arm_reduced(input);
    if (arm_frame)
        return arm_frame;

    const char *local_input = *input;
    sr_skip_char_span(&local_input, " \t");

    struct sr_koops_frame *frame = sr_koops_frame_new();

    bool parenthesis = sr_skip_char(&local_input, '(');

    if (!sr_koops_parse_address(&local_input, &frame->address))
    {
        int len = sr_parse_hexadecimal_0xuint64(&local_input,
                                                 &frame->address);

        if (len > 0)
            goto done;
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

    sr_skip_char_span(&local_input, " \t");

    if (sr_skip_string(&local_input, "from"))
    {
        sr_skip_char_span(&local_input, " \t");

        if (!sr_koops_parse_address(&local_input,
                                    &frame->from_address))
        {
            sr_koops_frame_free(frame);
            return NULL;
        }

        sr_skip_char_span(&local_input, " \t");

        if (!sr_koops_parse_function(&local_input,
                                     &frame->from_function_name,
                                     &frame->from_function_offset,
                                     &frame->from_function_length,
                                     &frame->from_module_name))
        {
            sr_koops_frame_free(frame);
            return NULL;
        }

        sr_skip_char_span(&local_input, " \t");
    }

    if (!frame->module_name && sr_koops_parse_module_name(&local_input,
                                                          &frame->module_name))
    {
        sr_skip_char_span(&local_input, " \t");
    }

    if (parenthesis)
    {
        if (!sr_skip_char(&local_input, ')'))
        {
            sr_koops_frame_free(frame);
            return NULL;
        }
        else
            sr_skip_char_span(&local_input, " \t");
    }

done:
    if (*local_input != '\0' && !sr_skip_char(&local_input, '\n'))
    {
        sr_koops_frame_free(frame);
        return NULL;
    }

    *input = local_input;
    return frame;
}


bool
sr_koops_skip_timestamp(const char **input)
{
    const char *local_input = *input;
    if (!sr_skip_char(&local_input, '['))
        return false;

    sr_skip_char_span(&local_input, SR_space);

    int num = sr_skip_uint(&local_input);
    if (0 == num)
        return false;

    if (!sr_skip_char(&local_input, '.'))
        return false;

    num = sr_skip_uint(&local_input);
    if (0 == num)
        return false;

    if (!sr_skip_char(&local_input, ']'))
        return false;

    *input = local_input;
    return true;
}

bool
sr_koops_parse_address(const char **input, uint64_t *address)
{
    const char *local_input = *input;

    if (!sr_skip_char(&local_input, '['))
        return false;

    bool angle = sr_skip_char(&local_input, '<');

    int len = sr_parse_hexadecimal_uint64(&local_input, address);
    if (!len)
        return false;

    if (angle && !sr_skip_char(&local_input, '>'))
        return false;

    if (!sr_skip_char(&local_input, ']'))
        return false;

    *input = local_input;
    return true;
}

bool
sr_koops_parse_module_name(const char **input,
                           char **module_name)
{
    const char *local_input = *input;

    if (!sr_skip_char(&local_input, '['))
        return false;

    if (!sr_parse_char_cspan(&local_input, " \t\n]",
                             module_name))
    {
        return false;
    }

    if (!sr_skip_char(&local_input, ']'))
    {
        g_free(*module_name);
        *module_name = NULL;
        return false;
    }

    *input = local_input;
    return true;
}

bool
sr_koops_parse_function(const char **input,
                        char **function_name,
                        uint64_t *function_offset,
                        uint64_t *function_length,
                        char **module_name)
{
    const char *local_input = *input;
    uint64_t unused;
    bool parenthesis = sr_skip_char(&local_input, '(');

    /* Might be just address - see e.g. kerneloopses/gitlog-09
     * or rhbz-1040900-s390x-1.
     */
    if (sr_parse_hexadecimal_0xuint64(&local_input, &unused))
    {
        if (parenthesis && !sr_skip_char(&local_input, ')'))
            return false;

        *input = local_input;
        return true;
    }

    if (!sr_parse_char_cspan(&local_input, " \t\n)+<",
                             function_name))
    {
        return false;
    }

    if (sr_skip_char(&local_input, '+'))
    {
        sr_parse_hexadecimal_0xuint64(&local_input,
                                      function_offset);

        if (!sr_skip_char(&local_input, '/'))
        {
            g_free(*function_name);
            *function_name = NULL;
            return false;
        }

        sr_parse_hexadecimal_0xuint64(&local_input,
                                      function_length);
    }
    else
    {
        return false;
    }

    sr_skip_char_span(&local_input, " \t");

    bool has_module = sr_koops_parse_module_name(&local_input, module_name);

    sr_skip_char_span(&local_input, " \t");

    if (parenthesis && !sr_skip_char(&local_input, ')'))
    {
        g_free(*function_name);
        *function_name = NULL;
        if (has_module)
        {
            g_free(*module_name);
            *module_name = NULL;
        }

        return false;
    }

    *input = local_input;
    return true;
}

char *
sr_koops_frame_to_json(struct sr_koops_frame *frame)
{
    GString *strbuf = g_string_new(NULL);

    if (frame->address != 0)
    {
        g_string_append_printf(strbuf,
                              "{   \"address\": %"PRIu64"\n",
                              frame->address);
    }

    g_string_append_printf(strbuf,
                          "%s   \"reliable\": %s\n",
                          frame->address == 0 ? "{" : ",",
                          frame->reliable ? "true" : "false");

    if (frame->function_name)
    {
        g_string_append(strbuf, ",   \"function_name\": ");
        sr_json_append_escaped(strbuf, frame->function_name);
        g_string_append(strbuf, "\n");
    }

    g_string_append_printf(strbuf,
                          ",   \"function_offset\": %"PRIu64"\n",
                          frame->function_offset);

    g_string_append_printf(strbuf,
                          ",   \"function_length\": %"PRIu64"\n",
                          frame->function_length);

    if (frame->module_name)
    {
        g_string_append(strbuf, ",   \"module_name\": ");
        sr_json_append_escaped(strbuf, frame->module_name);
        g_string_append(strbuf, "\n");
    }

    if (frame->from_address != 0)
    {
        g_string_append_printf(strbuf,
                              ",   \"from_address\": %"PRIu64"\n",
                              frame->from_address);
    }

    if (frame->from_function_name)
    {
        g_string_append(strbuf, ",   \"from_function_name\": ");
        sr_json_append_escaped(strbuf, frame->from_function_name);
        g_string_append(strbuf, "\n");
    }

    g_string_append_printf(strbuf,
                          ",   \"from_function_offset\": %"PRIu64"\n",
                          frame->from_function_offset);

    g_string_append_printf(strbuf,
                          ",   \"from_function_length\": %"PRIu64"\n",
                          frame->from_function_length);

    if (frame->from_module_name)
    {
        g_string_append(strbuf, ",   \"from_module_name\": ");
        sr_json_append_escaped(strbuf, frame->from_module_name);
        g_string_append(strbuf, "\n");
    }

    if (frame->special_stack)
    {
        g_string_append(strbuf, ",   \"special_stack\": ");
        sr_json_append_escaped(strbuf, frame->special_stack);
        g_string_append(strbuf, "\n");
    }

    g_string_append(strbuf, "}");
    return g_string_free(strbuf, FALSE);
}

struct sr_koops_frame *
sr_koops_frame_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "frame", error_message))
        return NULL;

    struct sr_koops_frame *result = sr_koops_frame_new();

    bool success =
        JSON_READ_UINT64(root, "address", &result->address) &&
        JSON_READ_BOOL(root, "reliable", &result->reliable) &&
        JSON_READ_STRING(root, "function_name", &result->function_name) &&
        JSON_READ_UINT64(root, "function_offset", &result->function_offset) &&
        JSON_READ_UINT64(root, "function_length", &result->function_length) &&
        JSON_READ_STRING(root, "module_name", &result->module_name) &&
        JSON_READ_UINT64(root, "from_address", &result->from_address) &&
        JSON_READ_STRING(root, "from_function_name", &result->from_function_name) &&
        JSON_READ_UINT64(root, "from_function_offset", &result->from_function_offset) &&
        JSON_READ_UINT64(root, "from_function_length", &result->from_function_length) &&
        JSON_READ_STRING(root, "from_module_name", &result->from_module_name) &&
        JSON_READ_STRING(root, "special_stack", &result->special_stack);

    if (!success)
    {
        sr_koops_frame_free(result);
        return NULL;
    }

    return result;
}

void
sr_koops_frame_append_to_str(struct sr_koops_frame *frame,
                             GString *str)
{
    if (frame->special_stack)
        g_string_append_printf(str, "[%s] ", frame->special_stack);

    g_string_append_printf(str, "%s%s",
                          (frame->reliable ? "" : "? "),
                          (frame->function_name ? frame->function_name : "??"));

    if (frame->module_name)
        g_string_append_printf(str, " in %s", frame->module_name);
}

static void
koops_append_bthash_text(struct sr_koops_frame *frame, enum sr_bthash_flags flags,
                         GString *strbuf)
{
    g_string_append_printf(strbuf,
                          "0x%"PRIx64", %d, %s, 0x%"PRIx64", 0x%"PRIx64", %s, "
                          "0x%"PRIx64", %s, 0x%"PRIx64", 0x%"PRIx64", %s\n",
                          frame->address,
                          frame->reliable,
                          OR_UNKNOWN(frame->function_name),
                          frame->function_offset,
                          frame->function_length,
                          OR_UNKNOWN(frame->module_name),
                          frame->from_address,
                          OR_UNKNOWN(frame->from_function_name),
                          frame->from_function_offset,
                          frame->from_function_length,
                          OR_UNKNOWN(frame->from_module_name));
}

static void
koops_append_duphash_text(struct sr_koops_frame *frame, enum sr_duphash_flags flags,
                         GString *strbuf)
{
    /* ABRT's koops hashing skipped unreliable frames entirely */
    if ((flags & SR_DUPHASH_KOOPS_COMPAT) && !frame->reliable)
        return;

    if (frame->function_name)
        g_string_append_printf(strbuf, "%s\n", frame->function_name);
    else
        g_string_append_printf(strbuf, "0x%"PRIx64"\n", frame->address);
}
