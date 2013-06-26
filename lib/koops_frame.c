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
#include "strbuf.h"
#include "json.h"
#include "generic_frame.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* Method table */

static void
koops_append_bthash_text(struct sr_koops_frame *frame, enum sr_bthash_flags flags,
                         struct sr_strbuf *strbuf);

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
    .frame_free = (frame_free_fn_t) sr_koops_frame_free,
};

/* Public functions */

struct sr_koops_frame *
sr_koops_frame_new()
{
    struct sr_koops_frame *frame =
        sr_malloc(sizeof(struct sr_koops_frame));

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

    free(frame->function_name);
    free(frame->module_name);
    free(frame->from_function_name);
    free(frame->from_module_name);
    free(frame);
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
        result->function_name = sr_strdup(result->function_name);

    if (result->module_name)
        result->module_name = sr_strdup(result->module_name);

    if (result->from_function_name)
        result->from_function_name = sr_strdup(result->from_function_name);

    if (result->from_module_name)
        result->from_module_name = sr_strdup(result->from_module_name);

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
    int function_name = sr_strcmp0(frame1->function_name,
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
    int module_name = sr_strcmp0(frame1->module_name,
                                 frame2->module_name);

    if (module_name != 0)
        return module_name;

    /* From address. */
    int from_address = frame2->from_address - frame1->from_address;
    if (from_address != 0)
        return from_address;

    /* From function name. */
    int from_function_name = sr_strcmp0(frame1->from_function_name,
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
    int from_module_name = sr_strcmp0(frame1->from_module_name,
                                      frame2->from_module_name);

    if (from_module_name != 0)
        return from_module_name;

    return 0;
}

int
sr_koops_frame_cmp_distance(struct sr_koops_frame *frame1,
                            struct sr_koops_frame *frame2)
{
    /* Function. */
    int function_name = sr_strcmp0(frame1->function_name,
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

struct sr_koops_frame *
sr_koops_frame_parse(const char **input)
{
    const char *local_input = *input;
    sr_skip_char_span(&local_input, " \t");

    /* Skip timestamp if it's present. */
    sr_koops_skip_timestamp(&local_input);
    sr_skip_char_span(&local_input, " \t");

    struct sr_koops_frame *frame = sr_koops_frame_new();

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

    if (!sr_skip_string(&local_input, "[<"))
        return false;

    int len = sr_parse_hexadecimal_uint64(&local_input, address);
    if (!len)
        return false;

    if (!sr_skip_string(&local_input, ">]"))
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
        free(*module_name);
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
    bool parenthesis = sr_skip_char(&local_input, '(');

    if (!sr_parse_char_cspan(&local_input, " \t)+",
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
            free(*function_name);
            *function_name = NULL;
            return false;
        }

        sr_parse_hexadecimal_0xuint64(&local_input,
                                      function_length);
    }
    else
    {
        *function_offset = -1;
        *function_length = -1;
    }

    sr_skip_char_span(&local_input, " \t");

    bool has_module = sr_koops_parse_module_name(&local_input, module_name);

    sr_skip_char_span(&local_input, " \t");

    if (parenthesis && !sr_skip_char(&local_input, ')'))
    {
        free(*function_name);
        *function_name = NULL;
        if (has_module)
        {
            free(*module_name);
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
    struct sr_strbuf *strbuf = sr_strbuf_new();

    if (frame->address != 0)
    {
        sr_strbuf_append_strf(strbuf,
                              "{   \"address\": %"PRIu64"\n",
                              frame->address);
    }

    sr_strbuf_append_strf(strbuf,
                          "%s   \"reliable\": %s\n",
                          frame->address == 0 ? "{" : ",",
                          frame->reliable ? "true" : "false");

    if (frame->function_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"function_name\": ");
        sr_json_append_escaped(strbuf, frame->function_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_strf(strbuf,
                          ",   \"function_offset\": %"PRIu64"\n",
                          frame->function_offset);

    sr_strbuf_append_strf(strbuf,
                          ",   \"function_length\": %"PRIu64"\n",
                          frame->function_length);

    if (frame->module_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"module_name\": ");
        sr_json_append_escaped(strbuf, frame->module_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (frame->from_address != 0)
    {
        sr_strbuf_append_strf(strbuf,
                              ",   \"from_address\": %"PRIu64"\n",
                              frame->from_address);
    }

    if (frame->from_function_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"from_function_name\": ");
        sr_json_append_escaped(strbuf, frame->from_function_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_strf(strbuf,
                          ",   \"from_function_offset\": %"PRIu64"\n",
                          frame->from_function_offset);

    sr_strbuf_append_strf(strbuf,
                          ",   \"from_function_length\": %"PRIu64"\n",
                          frame->from_function_length);

    if (frame->from_module_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"from_module_name\": ");
        sr_json_append_escaped(strbuf, frame->from_module_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_str(strbuf, "}");
    return sr_strbuf_free_nobuf(strbuf);
}

void
sr_koops_frame_append_to_str(struct sr_koops_frame *frame,
                             struct sr_strbuf *str)
{
    sr_strbuf_append_strf(str, "%s%s",
                          (frame->reliable ? "" : "? "),
                          (frame->function_name ? frame->function_name : "??"));

    if (frame->module_name)
        sr_strbuf_append_strf(str, " in %s", frame->module_name);
}

static void
koops_append_bthash_text(struct sr_koops_frame *frame, enum sr_bthash_flags flags,
                         struct sr_strbuf *strbuf)
{
    sr_strbuf_append_strf(strbuf,
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
