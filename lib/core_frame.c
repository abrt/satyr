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
#include "core/frame.h"
#include "utils.h"
#include "strbuf.h"
#include "json.h"
#include "generic_frame.h"
#include "internal_utils.h"
#include <string.h>

/* Method table */

DEFINE_NEXT_FUNC(core_next, struct sr_frame, struct sr_core_frame)
DEFINE_SET_NEXT_FUNC(core_set_next, struct sr_frame, struct sr_core_frame)

struct frame_methods core_frame_methods =
{
    .append_to_str = (append_to_str_fn_t) sr_core_frame_append_to_str,
    .next = (next_frame_fn_t) core_next,
    .set_next = (set_next_frame_fn_t) core_set_next,
    .cmp = (frame_cmp_fn_t) sr_core_frame_cmp,
    .cmp_distance = (frame_cmp_fn_t) sr_core_frame_cmp_distance,
};

/* Public functions */

struct sr_core_frame *
sr_core_frame_new()
{
    struct sr_core_frame *frame = sr_malloc(sizeof(struct sr_core_frame));
    sr_core_frame_init(frame);
    return frame;
}

void
sr_core_frame_init(struct sr_core_frame *frame)
{
    frame->address = -1;
    frame->build_id = NULL;
    frame->build_id_offset = -1;
    frame->function_name = NULL;
    frame->file_name = NULL;
    frame->fingerprint = NULL;
    frame->fingerprint_hashed = false;
    frame->next = NULL;
    frame->type = SR_REPORT_CORE;
}

void
sr_core_frame_free(struct sr_core_frame *frame)
{
    if (!frame)
        return;

    free(frame->build_id);
    free(frame->function_name);
    free(frame->file_name);
    free(frame->fingerprint);
    free(frame);
}

struct sr_core_frame *
sr_core_frame_dup(struct sr_core_frame *frame, bool siblings)
{
    struct sr_core_frame *result = sr_core_frame_new();
    memcpy(result, frame, sizeof(struct sr_core_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_core_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings if the copy is not shallow. */
    if (result->build_id)
        result->build_id = sr_strdup(result->build_id);
    if (result->function_name)
        result->function_name = sr_strdup(result->function_name);
    if (result->file_name)
        result->file_name = sr_strdup(result->file_name);
    if (result->fingerprint)
        result->fingerprint = sr_strdup(result->fingerprint);

    return result;
}

bool
sr_core_frame_calls_func(struct sr_core_frame *frame,
                          const char *function_name,
                          ...)
{
    if (!frame->function_name ||
        0 != strcmp(frame->function_name, function_name))
    {
        return false;
    }

    va_list file_names;
    va_start(file_names, function_name);
    bool success = false;
    bool empty = true;

    while (true)
    {
        char *file_name = va_arg(file_names, char*);
        if (!file_name)
            break;

        empty = false;

        if (frame->file_name &&
            NULL != strstr(frame->file_name, file_name))
        {
            success = true;
            break;
        }
    }

   va_end(file_names);
   return success || empty;
}

int
sr_core_frame_cmp(struct sr_core_frame *frame1,
                  struct sr_core_frame *frame2)
{
    /* Build ID. */
    int build_id = sr_strcmp0(frame1->build_id,
                              frame2->build_id);
    if (build_id != 0)
        return build_id;

    /* Build ID offset */
    int build_id_offset = frame1->build_id_offset - frame2->build_id_offset;
    if (build_id_offset != 0)
        return build_id_offset;

    /* Function name. */
    int function_name = sr_strcmp0(frame1->function_name,
                                    frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* File name */
    int file_name = sr_strcmp0(frame1->file_name,
                                frame2->file_name);
    if (file_name != 0)
        return file_name;

    /* Fingerprint. */
    int fingerprint = sr_strcmp0(frame1->fingerprint,
                                  frame2->fingerprint);
    if (fingerprint != 0)
        return fingerprint;

    return 0;
}

int
sr_core_frame_cmp_distance(struct sr_core_frame *frame1,
                           struct sr_core_frame *frame2)
{
    /* TODO: we should not rely on function name and we definitely should use
     * build id */
    /* Function name. */
    int function_name = sr_strcmp0(frame1->function_name,
                                    frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* Fingerprint. */
    int fingerprint = sr_strcmp0(frame1->fingerprint,
                                  frame2->fingerprint);
    if (fingerprint != 0)
        return fingerprint;

    return 0;
}

struct sr_core_frame *
sr_core_frame_append(struct sr_core_frame *dest,
                     struct sr_core_frame *item)
{
    if (!dest)
        return item;

    struct sr_core_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

struct sr_core_frame *
sr_core_frame_from_json(struct sr_json_value *root,
                        char **error_message)
{
    if (root->type != SR_JSON_OBJECT)
    {
        *error_message = sr_strdup("Invalid type of root value; object expected.");
        return NULL;
    }

    struct sr_core_frame *result = sr_core_frame_new();

    /* Read address. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("address", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_INTEGER)
        {
            *error_message = sr_strdup("Invalid type of \"address\"; integer expected.");
            sr_core_frame_free(result);
            return NULL;
        }

        result->address = root->u.object.values[i].value->u.integer;
        break;
    }

    /* Read build id. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("build_id", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_STRING)
        {
            *error_message = sr_strdup("Invalid type of \"build_id\"; string expected.");
            sr_core_frame_free(result);
            return NULL;
        }

        result->build_id = sr_strdup(root->u.object.values[i].value->u.string.ptr);
        break;
    }

    /* Read build id offset. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("build_id_offset", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_INTEGER)
        {
            *error_message = sr_strdup("Invalid type of \"build_id_offset\"; integer expected.");
            sr_core_frame_free(result);
            return NULL;
        }

        result->build_id_offset = root->u.object.values[i].value->u.integer;
        break;
    }

    /* Read function name. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("function_name", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_STRING)
        {
            *error_message = sr_strdup("Invalid type of \"function_name\"; string expected.");
            sr_core_frame_free(result);
            return NULL;
        }

        result->function_name = sr_strdup(root->u.object.values[i].value->u.string.ptr);
        break;
    }

    /* Read file name. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("file_name", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_STRING)
        {
            *error_message = sr_strdup("Invalid type of \"file_name\"; string expected.");
            sr_core_frame_free(result);
            return NULL;
        }

        result->file_name = sr_strdup(root->u.object.values[i].value->u.string.ptr);
        break;
    }

    /* Read fingerprint. */
    for (unsigned i = 0; i < root->u.object.length; ++i)
    {
        if (0 != strcmp("fingerprint", root->u.object.values[i].name))
            continue;

        if (root->u.object.values[i].value->type != SR_JSON_STRING)
        {
            *error_message = sr_strdup("Invalid type of \"fingerprint\"; string expected.");
            sr_core_frame_free(result);
            return NULL;
        }

        result->fingerprint = sr_strdup(root->u.object.values[i].value->u.string.ptr);
        break;
    }

    return result;
}

char *
sr_core_frame_to_json(struct sr_core_frame *frame)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();
    sr_strbuf_append_strf(strbuf,
                          "{   \"address\": %"PRIu64"\n",
                          frame->address);

    if (frame->build_id)
    {
        sr_strbuf_append_str(strbuf, ",   \"build_id\": ");
        sr_json_append_escaped(strbuf, frame->build_id);
        sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_strf(strbuf,
                          ",   \"build_id_offset\": %"PRIu64"\n",
                          frame->build_id_offset);

    if (frame->function_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"function_name\": ");
        sr_json_append_escaped(strbuf, frame->function_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (frame->file_name)
    {
        sr_strbuf_append_str(strbuf, ",   \"file_name\": ");
        sr_json_append_escaped(strbuf, frame->file_name);
        sr_strbuf_append_str(strbuf, "\n");
    }

    if (frame->fingerprint)
    {
        sr_strbuf_append_str(strbuf, ",   \"fingerprint\": ");
        sr_json_append_escaped(strbuf, frame->fingerprint);
        sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}

void
sr_core_frame_append_to_str(struct sr_core_frame *frame,
                            struct sr_strbuf *dest)
{
    if (frame->function_name)
    {
        sr_strbuf_append_str(dest, frame->function_name);
    }
    else
    {
        sr_strbuf_append_strf(dest, "%s+%"PRIu64, frame->build_id,
                              frame->build_id_offset);
    }

    if (frame->file_name)
    {
        sr_strbuf_append_strf(dest, " in %s", frame->file_name);
    }
}
