/*
    core_frame.c

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
#include "core_frame.h"
#include "utils.h"
#include "strbuf.h"
#include <string.h>

struct btp_core_frame *
btp_core_frame_new()
{
    struct btp_core_frame *frame = btp_malloc(sizeof(struct btp_core_frame));
    btp_core_frame_init(frame);
    return frame;
}

void
btp_core_frame_init(struct btp_core_frame *frame)
{
    frame->address = -1;
    frame->build_id = NULL;
    frame->build_id_offset = -1;
    frame->function_name = NULL;
    frame->file_name = NULL;
    frame->fingerprint = NULL;
    frame->next = NULL;
}

void
btp_core_frame_free(struct btp_core_frame *frame)
{
    if (!frame)
        return;

    free(frame->build_id);
    free(frame->function_name);
    free(frame->file_name);
    free(frame->fingerprint);
    free(frame);
}

struct btp_core_frame *
btp_core_frame_dup(struct btp_core_frame *frame, bool siblings)
{
    struct btp_core_frame *result = btp_core_frame_new();
    memcpy(result, frame, sizeof(struct btp_core_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_core_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings if the copy is not shallow. */
    if (result->build_id)
        result->build_id = btp_strdup(result->build_id);
    if (result->function_name)
        result->function_name = btp_strdup(result->function_name);
    if (result->file_name)
        result->file_name = btp_strdup(result->file_name);
    if (result->fingerprint)
        result->fingerprint = btp_strdup(result->fingerprint);

    return result;
}

int
btp_core_frame_cmp(struct btp_core_frame *frame1,
                   struct btp_core_frame *frame2)
{
    /* Build ID. */
    int build_id = btp_strcmp0(frame1->build_id,
                               frame2->build_id);
    if (build_id != 0)
        return build_id;

    /* Build ID offset */
    int build_id_offset = frame1->build_id_offset - frame2->build_id_offset;
    if (build_id_offset != 0)
        return build_id_offset;

    /* Function name. */
    int function_name = btp_strcmp0(frame1->function_name,
                                    frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* File name */
    int file_name = btp_strcmp0(frame1->file_name,
                                frame2->file_name);
    if (file_name != 0)
        return file_name;

    /* Fingerprint. */
    int fingerprint = btp_strcmp0(frame1->fingerprint,
                                  frame2->fingerprint);
    if (fingerprint != 0)
        return fingerprint;

    return 0;
}

struct btp_core_frame *
btp_core_frame_append(struct btp_core_frame *dest,
                      struct btp_core_frame *item)
{
    if (!dest)
        return item;

    struct btp_core_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

char *
btp_core_frame_to_json(struct btp_core_frame *frame)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();
    btp_strbuf_append_strf(strbuf,
                           "{   \"address\": %"PRIu64"\n",
                           frame->address);
    if (frame->build_id)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"build_id\": \"%s\"\n",
                               frame->build_id);
    }

    btp_strbuf_append_strf(strbuf,
                           ",   \"build_id_offset\": %"PRIu64"\n",
                           frame->build_id_offset);

    if (frame->function_name)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"function_name\": \"%s\"\n",
                               frame->function_name);
    }

    if (frame->file_name)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"file_name\": \"%s\"\n",
                               frame->file_name);
    }

    if (frame->fingerprint)
    {
        btp_strbuf_append_strf(strbuf,
                               ",   \"fingerprint\": \"%s\"\n",
                               frame->fingerprint);
    }

    btp_strbuf_append_str(strbuf, "}");
    return btp_strbuf_free_nobuf(strbuf);
}
