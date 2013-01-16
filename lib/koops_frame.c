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
#include "koops_frame.h"
#include "utils.h"
#include "strbuf.h"
#include "json.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

struct btp_koops_frame *
btp_koops_frame_new()
{
    struct btp_koops_frame *frame =
        btp_malloc(sizeof(struct btp_koops_frame));

    btp_koops_frame_init(frame);
    return frame;
}

void
btp_koops_frame_init(struct btp_koops_frame *frame)
{
    memset(frame, 0, sizeof(struct btp_koops_frame));
}

void
btp_koops_frame_free(struct btp_koops_frame *frame)
{
    if (!frame)
        return;

    free(frame->function_name);
    free(frame->module_name);
    free(frame->from_function_name);
    free(frame->from_module_name);
    free(frame);
}

struct btp_koops_frame *
btp_koops_frame_dup(struct btp_koops_frame *frame, bool siblings)
{
    struct btp_koops_frame *result = btp_koops_frame_new();
    memcpy(result, frame, sizeof(struct btp_koops_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_koops_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->function_name)
        result->function_name = btp_strdup(result->function_name);

    if (result->module_name)
        result->module_name = btp_strdup(result->module_name);

    if (result->from_function_name)
        result->from_function_name = btp_strdup(result->from_function_name);

    if (result->from_module_name)
        result->from_module_name = btp_strdup(result->from_module_name);

    return result;
}

int
btp_koops_frame_cmp(struct btp_koops_frame *frame1,
                    struct btp_koops_frame *frame2)
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
    int function_name = btp_strcmp0(frame1->function_name,
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

    return 0;
}

int
btp_koops_frame_cmp_distance(struct btp_koops_frame *frame1,
                             struct btp_koops_frame *frame2)
{
    /* Function. */
    int function_name = btp_strcmp0(frame1->function_name,
                                    frame2->function_name);
    if (function_name != 0)
        return function_name;

    return 0;
}

struct btp_koops_frame *
btp_koops_frame_append(struct btp_koops_frame *dest,
                       struct btp_koops_frame *item)
{
    if (!dest)
        return item;

    struct btp_koops_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

struct btp_koops_frame *
btp_koops_frame_parse(const char **input)
{
    const char *local_input = *input;
    btp_skip_char_span(&local_input, " \t");

    /* Skip timestamp if it's present. */
    btp_koops_skip_timestamp(&local_input);
    btp_skip_char_span(&local_input, " \t");

    struct btp_koops_frame *frame = btp_koops_frame_new();

    if (!btp_koops_parse_address(&local_input, &frame->address))
    {
        int len = btp_parse_hexadecimal_0xuint64(&local_input,
                                                 &frame->address);

        if (len > 0)
            goto done;
    }

    btp_skip_char_span(&local_input, " \t");

    /* Question mark means unreliable */
    frame->reliable = btp_skip_char(&local_input, '?') != true;

    btp_skip_char_span(&local_input, " \t");

    if (!btp_koops_parse_function(&local_input,
                                  &frame->function_name,
                                  &frame->function_offset,
                                  &frame->function_length,
                                  &frame->module_name))
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

        if (!btp_koops_parse_function(&local_input,
                                      &frame->from_function_name,
                                      &frame->from_function_offset,
                                      &frame->from_function_length,
                                      &frame->from_module_name))
        {
            btp_koops_frame_free(frame);
            return NULL;
        }

        btp_skip_char_span(&local_input, " \t");
    }

    if (!frame->module_name && btp_koops_parse_module_name(&local_input,
                                                           &frame->module_name))
    {
        btp_skip_char_span(&local_input, " \t");
    }

done:
    if (*local_input != '\0' && !btp_skip_char(&local_input, '\n'))
    {
        btp_koops_frame_free(frame);
        return NULL;
    }

    *input = local_input;
    return frame;
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
        *module_name = NULL;
        return false;
    }

    *input = local_input;
    return true;
}

bool
btp_koops_parse_function(const char **input,
                         char **function_name,
                         uint64_t *function_offset,
                         uint64_t *function_length,
                         char **module_name)
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
            *function_name = NULL;
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

    btp_skip_char_span(&local_input, " \t");

    bool has_module = btp_koops_parse_module_name(&local_input, module_name);

    btp_skip_char_span(&local_input, " \t");

    if (parenthesis && !btp_skip_char(&local_input, ')'))
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
btp_koops_frame_to_json(struct btp_koops_frame *frame)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();

    if (frame->address != 0)
    {
        btp_strbuf_append_strf(strbuf,
                               "{   \"address\": %"PRIu64"\n",
                               frame->address);
    }

    btp_strbuf_append_strf(strbuf,
                           "%s   \"reliable\": %s\n",
                           frame->address == 0 ? "{" : ",",
                           frame->reliable ? "true" : "false");

    if (frame->function_name)
    {
        char *escaped = btp_json_escape(frame->function_name);
        btp_strbuf_append_strf(strbuf,
                               ",   \"function_name\": \"%s\"\n",
                               escaped);

        free(escaped);
    }

    btp_strbuf_append_strf(strbuf,
                           ",   \"function_offset\": %"PRIu64"\n",
                           frame->function_offset);

    btp_strbuf_append_strf(strbuf,
                           ",   \"function_length\": %"PRIu64"\n",
                           frame->function_length);

    if (frame->module_name)
    {
        char *escaped = btp_json_escape(frame->module_name);
        btp_strbuf_append_strf(strbuf,
                               ",   \"module_name\": \"%s\"\n",
                               escaped);

        free(escaped);
    }

    if (frame->from_address != 0)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"from_address\": %"PRIu64"\n",
                               frame->from_address);
    }

    if (frame->from_function_name)
    {
        char *escaped = btp_json_escape(frame->from_function_name);
        btp_strbuf_append_strf(strbuf,
                               ",   \"from_function_name\": \"%s\"\n",
                               escaped);

        free(escaped);
    }

    btp_strbuf_append_strf(strbuf,
                           ",   \"from_function_offset\": %"PRIu64"\n",
                           frame->from_function_offset);

    btp_strbuf_append_strf(strbuf,
                           ",   \"from_function_length\": %"PRIu64"\n",
                           frame->from_function_length);

    if (frame->from_module_name)
    {
        char *escaped = btp_json_escape(frame->from_module_name);
        btp_strbuf_append_strf(strbuf,
                               ",   \"from_module_name\": \"%s\"\n",
                               escaped);

        free(escaped);
    }

    btp_strbuf_append_str(strbuf, "}");
    return btp_strbuf_free_nobuf(strbuf);
}
