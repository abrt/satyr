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
    memset(frame, 0, sizeof(struct btp_koops_frame));
}

void
btp_koops_frame_free(struct btp_koops_frame *frame)
{
    if (!frame)
        return;

    free(frame->function_name);
    free(frame->from_function_name);
    free(frame->module_name);
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

    if (result->from_function_name)
        result->from_function_name = btp_strdup(result->from_function_name);

    if (result->module_name)
        result->module_name = btp_strdup(result->module_name);

    return result;
}

int
btp_koops_frame_cmp(struct btp_koops_frame *frame1,
                    struct btp_koops_frame *frame2)
{
    /* Function. */
    int function_name = btp_strcmp0(frame1->function_name,
                                    frame2->function_name);
    if (function_name != 0)
        return function_name;

    return 0;
}
