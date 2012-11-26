/*
    python_frame.c

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
#include "python_frame.h"
#include "utils.h"
#include <string.h>

struct btp_python_frame *
btp_python_frame_new()
{
    struct btp_python_frame *frame =
        btp_malloc(sizeof(struct btp_python_frame));

    btp_python_frame_init(frame);
    return frame;
}

void
btp_python_frame_init(struct btp_python_frame *frame)
{
    memset(frame, 0, sizeof(struct btp_python_frame));
}

void
btp_python_frame_free(struct btp_python_frame *frame)
{
    if (!frame)
        return;

    free(frame->file_name);
    free(frame->function_name);
    free(frame->line);
    free(frame);
}

struct btp_python_frame *
btp_python_frame_dup(struct btp_python_frame *frame, bool siblings)
{
    struct btp_python_frame *result = btp_python_frame_new();
    memcpy(result, frame, sizeof(struct btp_python_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = btp_python_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->file_name)
        result->file_name = btp_strdup(result->file_name);

    if (result->function_name)
        result->function_name = btp_strdup(result->function_name);

    if (result->line)
        result->line = btp_strdup(result->line);

    return result;
}
