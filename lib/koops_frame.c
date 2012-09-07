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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    frame->address = 0;
    frame->reliable = false;
    frame->name = NULL;
    frame->offset = 0;
    frame->len = 0;
    frame->module = NULL;
    frame->next = NULL;
}

void
btp_koops_frame_free(struct btp_koops_frame *frame)
{
    if (!frame)
        return;

    free(frame->module);
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
    if (result->module)
        result->module = btp_strdup(result->module);

    return result;
}
