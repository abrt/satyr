/*
    ruby_frame.c

    Copyright (C) 2015  Red Hat, Inc.

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
#include "ruby/frame.h"
#include "utils.h"
#include "location.h"
#include "json.h"
#include "generic_frame.h"
#include "thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

/* Method table */

static void
ruby_append_bthash_text(struct sr_ruby_frame *frame, enum sr_bthash_flags flags,
                          GString *strbuf);
static void
ruby_append_duphash_text(struct sr_ruby_frame *frame, enum sr_duphash_flags flags,
                           GString *strbuf);

DEFINE_NEXT_FUNC(ruby_next, struct sr_frame, struct sr_ruby_frame)
DEFINE_SET_NEXT_FUNC(ruby_set_next, struct sr_frame, struct sr_ruby_frame)

struct frame_methods ruby_frame_methods =
{
    .append_to_str = (append_to_str_fn_t) sr_ruby_frame_append_to_str,
    .next = (next_frame_fn_t) ruby_next,
    .set_next = (set_next_frame_fn_t) ruby_set_next,
    .cmp = (frame_cmp_fn_t) sr_ruby_frame_cmp,
    .cmp_distance = (frame_cmp_fn_t) sr_ruby_frame_cmp_distance,
    .frame_append_bthash_text =
        (frame_append_bthash_text_fn_t) ruby_append_bthash_text,
    .frame_append_duphash_text =
        (frame_append_duphash_text_fn_t) ruby_append_duphash_text,
    .frame_free = (frame_free_fn_t) sr_ruby_frame_free,
};

/* Public functions */

struct sr_ruby_frame *
sr_ruby_frame_new()
{
    struct sr_ruby_frame *frame =
        g_malloc(sizeof(*frame));

    sr_ruby_frame_init(frame);
    return frame;
}

void
sr_ruby_frame_init(struct sr_ruby_frame *frame)
{
    memset(frame, 0, sizeof(struct sr_ruby_frame));
    frame->type = SR_REPORT_RUBY;
}

void
sr_ruby_frame_free(struct sr_ruby_frame *frame)
{
    if (!frame)
        return;

    g_free(frame->file_name);
    g_free(frame->function_name);
    g_free(frame);
}

struct sr_ruby_frame *
sr_ruby_frame_dup(struct sr_ruby_frame *frame, bool siblings)
{
    struct sr_ruby_frame *result = sr_ruby_frame_new();
    memcpy(result, frame, sizeof(struct sr_ruby_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_ruby_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->file_name)
        result->file_name = g_strdup(result->file_name);

    if (result->function_name)
        result->function_name = g_strdup(result->function_name);

    return result;
}

int
sr_ruby_frame_cmp(struct sr_ruby_frame *frame1,
                  struct sr_ruby_frame *frame2)
{
    /* function_name */
    int function_name = g_strcmp0(frame1->function_name,
                                   frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* file_name */
    int file_name = g_strcmp0(frame1->file_name,
                               frame2->file_name);
    if (file_name != 0)
        return file_name;

    /* file_line */
    int file_line = frame1->file_line - frame2->file_line;
    if (file_line != 0)
        return file_line;

    /* special_function */
    int special_function = frame1->special_function - frame2->special_function;
    if (special_function != 0)
        return special_function;

    /* block_level */
    int block_level = frame1->block_level - frame2->block_level;
    if (block_level != 0)
            return block_level;

    /* rescue_level */
    int rescue_level = frame1->rescue_level - frame2->rescue_level;
    if (rescue_level != 0)
            return rescue_level;

    return 0;
}

int
sr_ruby_frame_cmp_distance(struct sr_ruby_frame *frame1,
                           struct sr_ruby_frame *frame2)
{
    /* function_name */
    int function_name = g_strcmp0(frame1->function_name,
                                   frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* file_name */
    int file_name = g_strcmp0(frame1->file_name,
                               frame2->file_name);
    if (file_name != 0)
        return file_name;

    /* special_function */
    int special_function = frame1->special_function - frame2->special_function;
    if (special_function != 0)
        return special_function;

    return 0;
}

struct sr_ruby_frame *
sr_ruby_frame_append(struct sr_ruby_frame *dest,
                     struct sr_ruby_frame *item)
{
    if (!dest)
        return item;

    struct sr_ruby_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

struct sr_ruby_frame *
sr_ruby_frame_parse(const char **input,
                    struct sr_location *location)
{
    const char *local_input = *input;
    struct sr_ruby_frame *frame = sr_ruby_frame_new();

    /* take everything before the backtick
     * /usr/share/rubygems/rubygems/core_ext/kernel_require.rb:55:in `require'
     * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     */
    char *filename_lineno_in = NULL;
    if (!sr_parse_char_cspan(&local_input, "`", &filename_lineno_in))
    {
        location->message = g_strdup("Unable to find the '`' character "
                                      "identifying the beginning of function name.");
        goto fail;
    }

    size_t l = strlen(filename_lineno_in);
    location->column += l;

    char *p = filename_lineno_in + l;

    /* /usr/share/rubygems/rubygems/core_ext/kernel_require.rb:55:in `require'
     *                                                           ^^^^
     */
    p -= strlen(":in ");
    if (p < filename_lineno_in || 0 != strcmp(":in ", p))
    {
        location->column -= strlen(":in ");
        location->message = g_strdup("Unable to find ':in ' preceding the "
                                      "backtick character.");
        goto fail;
    }

    /* Find the beginning of the line number. */
    *p = '\0';
    do {
        p--;
    } while (p >= filename_lineno_in && isdigit(*p));
    p++;

    /* /usr/share/rubygems/rubygems/core_ext/kernel_require.rb:55:in `require'
     *                                                         ^^
     */
    char *p_copy = p;
    int lineno_len = sr_parse_uint32((const char **)&p_copy, &frame->file_line);
    if (lineno_len <= 0)
    {
        location->message = g_strdup("Unable to find line number before ':in '");
        goto fail;
    }

    /* /usr/share/rubygems/rubygems/core_ext/kernel_require.rb:55:in `require'
     *                                                        ^
     */
    p--;
    if (p < filename_lineno_in || *p != ':')
    {
        location->column -= lineno_len;
        location->message = g_strdup("Unable to fin the ':' character "
                                      "preceding the line number");
        goto fail;
    }

    /* Everything before the colon is the file name. */
    *p = '\0';
    frame->file_name = filename_lineno_in;
    frame->file_name = anonymize_path(frame->file_name);

    filename_lineno_in = NULL;

    if(!sr_skip_char(&local_input, '`'))
    {
        location->message = g_strdup("Unable to find the '`' character "
                                      "identifying the beginning of function name.");
        goto fail;
    }

    location->column++;

    /* The part in quotes can look like:
     * `rescue in rescue in block (3 levels) in func'
     * parse the number of rescues and blocks
     */
    while (sr_skip_string(&local_input, "rescue in "))
    {
        frame->rescue_level++;
        location->column += strlen("rescue in ");
    }

    if (sr_skip_string(&local_input, "block in "))
    {
        frame->block_level = 1;
        location->column += strlen("block in");
    }
    else if(sr_skip_string(&local_input, "block ("))
    {
        location->column += strlen("block (");

        int len = sr_parse_uint32(&local_input, &frame->block_level);
        if (len == 0 || !sr_skip_string(&local_input, " levels) in "))
        {
            location->message = g_strdup("Unable to parse block depth.");
            goto fail;
        }
        location->column += len + strlen(" levels) in ");
    }

    if (sr_skip_char(&local_input, '<'))
    {
        location->column++;
        frame->special_function = true;
    }

    if (!sr_parse_char_cspan(&local_input, "'>", &frame->function_name))
    {
        location->message = g_strdup("Unable to find the \"'\" character "
                                      "delimiting the function name.");
        goto fail;
    }
    location->column += strlen(frame->function_name);

    if (frame->special_function)
    {
        if (!sr_skip_char(&local_input, '>'))
        {
            location->message = g_strdup("Unable to find the \">\" character "
                                          "delimiting the function name.");
            goto fail;
        }
        location->column++;
    }

    if (!sr_skip_char(&local_input, '\''))
    {
        location->message = g_strdup("Unable to find the \"'\" character "
                                      "delimiting the function name.");
        goto fail;
    }
    location->column++;

    *input = local_input;
    return frame;

fail:
    sr_ruby_frame_free(frame);
    g_free(filename_lineno_in);
    return NULL;
}

char *
sr_ruby_frame_to_json(struct sr_ruby_frame *frame)
{
    GString *strbuf = g_string_new(NULL);

    /* Source file name. */
    if (frame->file_name)
    {
        g_string_append(strbuf, ",   \"file_name\": ");
        sr_json_append_escaped(strbuf, frame->file_name);
        g_string_append(strbuf, "\n");
    }

    /* Source file line. */
    if (frame->file_line)
    {
        g_string_append_printf(strbuf,
                              ",   \"file_line\": %"PRIu32"\n",
                              frame->file_line);
    }

    /* Function name / special function. */
    if (frame->function_name)
    {
        if (frame->special_function)
            g_string_append(strbuf, ",   \"special_function\": ");
        else
            g_string_append(strbuf, ",   \"function_name\": ");

        sr_json_append_escaped(strbuf, frame->function_name);
        g_string_append(strbuf, "\n");
    }

    /* Block level. */
    if (frame->block_level > 0)
    {
        g_string_append_printf(strbuf,
                              ",   \"block_level\": %"PRIu32"\n",
                              frame->block_level);
    }

    /* Rescue level. */
    if (frame->rescue_level > 0)
    {
        g_string_append_printf(strbuf,
                              ",   \"rescue_level\": %"PRIu32"\n",
                              frame->rescue_level);
    }


    strbuf->str[0] = '{';
    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
}

struct sr_ruby_frame *
sr_ruby_frame_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "frame", error_message))
        return NULL;

    struct sr_ruby_frame *result = sr_ruby_frame_new();
    json_object *val;

    /* Source file name */
    if (json_object_object_get_ex(root, "file_name", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "file_name", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->file_name = g_strdup(string);
    }

    /* Function name / special function. */
    if (json_object_object_get_ex(root, "function_name", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "function_name", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->special_function = false;
        result->function_name = g_strdup(string);
    }
    else if (json_object_object_get_ex(root, "special_function", &val))
    {
        const char *string;

        if (!json_check_type(val, json_type_string, "special_function", error_message))
            goto fail;

        string = json_object_get_string(val);

        result->special_function = true;
        result->function_name = g_strdup(string);
    }

    bool success =
        JSON_READ_UINT32(root, "file_line", &result->file_line) &&
        JSON_READ_UINT32(root, "block_level", &result->block_level) &&
        JSON_READ_UINT32(root, "rescue_level", &result->rescue_level);

    if (!success)
        goto fail;

    return result;

fail:
    sr_ruby_frame_free(result);
    return NULL;
}

void
sr_ruby_frame_append_to_str(struct sr_ruby_frame *frame,
                            GString *dest)
{
    for (uint32_t i = 0; i < frame->rescue_level; i++)
    {
        g_string_append(dest, "rescue in ");
    }

    if (frame->block_level == 1)
    {
        g_string_append(dest, "block in ");
    }
    else if (frame->block_level > 1)
    {
        g_string_append_printf(dest, "block (%u levels) in ", (unsigned)frame->block_level);
    }

    g_string_append_printf(dest, "%s%s%s",
                          (frame->special_function ? "<" : ""),
                          (frame->function_name ? frame->function_name : "??"),
                          (frame->special_function ? ">" : ""));

    if (frame->file_name)
    {
        g_string_append_printf(dest, " in %s", frame->file_name);

        if (frame->file_line)
        {
            g_string_append_printf(dest, ":%"PRIu32, frame->file_line);
        }
    }
}

static void
ruby_append_bthash_text(struct sr_ruby_frame *frame, enum sr_bthash_flags flags,
                        GString *strbuf)
{
    g_string_append_printf(strbuf,
                          "%s, %"PRIu32", %s, %d, %"PRIu32", %"PRIu32"\n",
                          OR_UNKNOWN(frame->file_name),
                          frame->file_line,
                          OR_UNKNOWN(frame->function_name),
                          frame->special_function,
                          frame->block_level,
                          frame->rescue_level);
}

static void
ruby_append_duphash_text(struct sr_ruby_frame *frame, enum sr_duphash_flags flags,
                         GString *strbuf)
{
    /* filename:line */
    g_string_append_printf(strbuf, "%s:%"PRIu32"\n",
                          OR_UNKNOWN(frame->file_name),
                          frame->file_line);
}
