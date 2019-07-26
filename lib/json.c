/*
    json.c

    Copyright (C) 2012, 2013, 2014  James McLaughlin et al.
    https://github.com/udp/json-parser

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
    OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
    USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/
#include "json.h"
#include "strbuf.h"
#include "location.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

typedef unsigned short json_uchar;

const struct sr_json_value
sr_json_value_none = { 0 };

static unsigned char
hex_value(char c)
{
    if (isdigit(c))
        return c - '0';

    switch (c) {
    case 'a': case 'A': return 0x0A;
    case 'b': case 'B': return 0x0B;
    case 'c': case 'C': return 0x0C;
    case 'd': case 'D': return 0x0D;
    case 'e': case 'E': return 0x0E;
    case 'f': case 'F': return 0x0F;
    default: return 0xFF;
    }
}

static int
would_overflow(long long value, char b)
{
    return ((LLONG_MAX - (b - '0')) / 10) < value;
}

struct json_state
{
   struct sr_json_settings settings;
   int first_pass;

   unsigned long used_memory;
   unsigned int uint_max;
   unsigned long ulong_max;

   const char *ptr;
};

static void *
json_alloc(struct json_state *state, unsigned long size, int zero)
{
    if ((state->ulong_max - state->used_memory) < size)
        return NULL;

    if (state->settings.max_memory
        && (state->used_memory += size) > state->settings.max_memory)
    {
        return NULL;
    }

    return zero ? calloc(size, 1) : malloc(size);
}

static int
new_value(struct json_state *state,
          struct sr_json_value **top,
          struct sr_json_value **root,
          struct sr_json_value **alloc,
          enum sr_json_type type)
{
    struct sr_json_value *value;
    int values_size;

    if (!state->first_pass)
    {
        value = *top = *alloc;
        *alloc = (*alloc)->_reserved.next_alloc;

        if (!*root)
            *root = value;

        switch (value->type)
        {
        case SR_JSON_ARRAY:
            if (value->u.array.length == 0)
               break;

            value->u.array.values = (struct sr_json_value **)json_alloc(
                state, value->u.array.length * sizeof(struct sr_json_value *), 0);

            if (!value->u.array.values)
                return 0;

            value->u.array.length = 0;
            break;
        case SR_JSON_OBJECT:
            if (value->u.array.length == 0)
               break;

            values_size = sizeof(*value->u.object.values) * value->u.object.length;
            (*(void**)&value->u.object.values) =
                json_alloc(state, values_size + ((unsigned long)value->u.object.values), 0);

            if (!value->u.object.values)
                return 0;

            value->_reserved.object_mem = (*(char**)&value->u.object.values) + values_size;
            value->u.object.length = 0;
            break;
        case SR_JSON_STRING:
            value->u.string.ptr = (char*)
                json_alloc(state, (value->u.string.length + 1), 0);

            if (!value->u.string.ptr)
                return 0;

            value->u.string.length = 0;
            break;

        default:
            break;
        }

        return 1;
    }

    value = (struct sr_json_value *)json_alloc(state, sizeof(struct sr_json_value), 1);

    if (!value)
        return 0;

    if (!*root)
        *root = value;

    value->type = type;
    value->parent = *top;

    if (*alloc)
        (*alloc)->_reserved.next_alloc = value;

    *alloc = *top = value;
    return 1;
}

#define e_off \
   ((int) (state.ptr - cur_line_begin))

#define whitespace \
   case '\n': ++location->line;  cur_line_begin = state.ptr; \
   case ' ': case '\t': case '\r'

#define string_add(b)  \
   do { if (!state.first_pass) string[string_length] = b; ++string_length; } while (0);

static const long
    flag_next             = 1 << 0,
    flag_reproc           = 1 << 1,
    flag_need_comma       = 1 << 2,
    flag_seek_value       = 1 << 3,
    flag_escaped          = 1 << 4,
    flag_string           = 1 << 5,
    flag_need_colon       = 1 << 6,
    flag_done             = 1 << 7,
    flag_num_negative     = 1 << 8,
    flag_num_zero         = 1 << 9,
    flag_num_e            = 1 << 10,
    flag_num_e_got_sign   = 1 << 11,
    flag_num_e_negative   = 1 << 12,
    flag_num_got_decimal  = 1 << 13;

struct sr_json_value *
sr_json_parse_ex(struct sr_json_settings *settings,
                  const char *json,
                  struct sr_location *location)
{
    const char *cur_line_begin;
    struct sr_json_value *top, *root, *alloc = NULL;
    struct json_state state = { 0 };
    int flags = 0;
    double num_digits = 0, num_e = 0, num_fraction = 0;

    size_t length = strlen(json);
    const char *end = json + length;

    /* Skip UTF-8 BOM */
    if (length >= 3 && ((unsigned char)json[0]) == 0xEF
                    && ((unsigned char)json[1]) == 0xBB
                    && ((unsigned char)json[2]) == 0xBF)
    {
        json += 3;
        length -= 3;
    }

    memcpy(&state.settings, settings, sizeof(struct sr_json_settings));
    /* limit of how much can be added before next check */
    state.uint_max = UINT_MAX - 8;
    state.ulong_max = ULONG_MAX - 8;

    for (state.first_pass = 1; state.first_pass >= 0; --state.first_pass)
    {
        json_uchar uchar;
        unsigned char uc_b1, uc_b2, uc_b3, uc_b4;
        char *string = NULL;
        unsigned string_length = 0;

        top = root = NULL;
        flags = flag_seek_value;

        location->line = 1;
        cur_line_begin = json;

        for (state.ptr = json ;; ++state.ptr)
        {
            char b = *state.ptr;

            if (flags & flag_string)
            {
                if (!b)
                {
                    location->column = e_off;
                    location->message = sr_strdup("Unexpected EOF in string");
                    goto e_failed;
                }

                if (string_length > state.uint_max)
                    goto e_overflow;

                if (flags & flag_escaped)
                {
                    flags &= ~ flag_escaped;

                    switch (b)
                    {
                    case 'b': string_add('\b'); break;
                    case 'f': string_add('\f'); break;
                    case 'n': string_add('\n'); break;
                    case 'r': string_add('\r'); break;
                    case 't': string_add('\t'); break;
                    case 'u':

                        if (end - state.ptr <= 4 ||
                                (uc_b1 = hex_value(*++state.ptr)) == 0xFF ||
                                (uc_b2 = hex_value(*++state.ptr)) == 0xFF ||
                                (uc_b3 = hex_value(*++state.ptr)) == 0xFF ||
                                (uc_b4 = hex_value(*++state.ptr)) == 0xFF)
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Invalid character value `%c`", b);
                            goto e_failed;
                        }

                        uc_b1 = (uc_b1 << 4) | uc_b2;
                        uc_b2 = (uc_b3 << 4) | uc_b4;
                        uchar = (uc_b1 << 8) | uc_b2;

                        if ((uchar & 0xF800) == 0xD800)
                        {
                            json_uchar uchar2;

                            if (end - state.ptr <= 6 ||
                                    (*++state.ptr) != '\\' ||
                                    (*++state.ptr) != 'u' ||
                                    (uc_b1 = hex_value(*++state.ptr)) == 0xFF ||
                                    (uc_b2 = hex_value(*++state.ptr)) == 0xFF ||
                                    (uc_b3 = hex_value(*++state.ptr)) == 0xFF ||
                                    (uc_b4 = hex_value(*++state.ptr)) == 0xFF)
                            {
                                location->column = e_off;
                                location->message = sr_asprintf("Invalid character value `%c`", b);
                                goto e_failed;
                            }

                            uc_b1 = (uc_b1 << 4) | uc_b2;
                            uc_b2 = (uc_b3 << 4) | uc_b4;
                            uchar2 = (uc_b1 << 8) | uc_b2;

                            uchar = 0x010000 | ((uchar & 0x3FF) << 10) | (uchar2 & 0x3FF);
                        }

                        if (uchar <= 0x7F)
                        {
                            string_add((char)uchar);
                            break;
                        }

                        if (uchar <= 0x7FF)
                        {
                            if (state.first_pass)
                                string_length += 2;
                            else
                            {
                                string[string_length++] = 0xC0 | (uchar >> 6);
                                string[string_length++] = 0x80 | (uchar & 0x3F);
                            }

                            break;
                        }

                        if (uchar <= 0xFFFF)
                        {
                            if (state.first_pass)
                                string_length += 3;
                            else
                            {
                                string[string_length++] = 0xE0 | (uchar >> 12);
                                string[string_length++] = 0x80 | ((uchar >> 6) & 0x3F);
                                string[string_length++] = 0x80 | (uchar & 0x3F);
                            }

                            break;
                        }

                        if (state.first_pass)
                            string_length += 4;
                        else
                        {
                            string[string_length++] = 0xF0 | (uchar >> 18);
                            string[string_length++] = 0x80 | ((uchar >> 12) & 0x3F);
                            string[string_length++] = 0x80 | ((uchar >> 6) & 0x3F);
                            string[string_length++] = 0x80 | (uchar & 0x3F);
                        }

                        break;

                    default:
                        string_add (b);
                    }

                    continue;
                }

                if (b == '\\')
                {
                    flags |= flag_escaped;
                    continue;
                }

                if (b == '"')
                {
                    if (!state.first_pass)
                        string[string_length] = 0;

                    flags &= ~ flag_string;
                    string = 0;

                    switch (top->type)
                    {
                    case SR_JSON_STRING:
                        top->u.string.length = string_length;
                        flags |= flag_next;
                        break;
                    case SR_JSON_OBJECT:
                        if (state.first_pass)
                            (*(char**)&top->u.object.values) += string_length + 1;
                        else
                        {
                            top->u.object.values[top->u.object.length].name =
                                (char*)top->_reserved.object_mem;

                            (*(char**)&top->_reserved.object_mem) += string_length + 1;
                        }

                        flags |= flag_seek_value | flag_need_colon;
                        continue;
                    default:
                        break;
                    }
                }
                else
                {
                    string_add(b);
                    continue;
                }
            }

            if (flags & flag_done)
            {
                if (!b)
                    break;

                switch (b)
                {
                whitespace:
                    continue;

                default:
                    location->column = e_off;
                    location->message = sr_asprintf("Trailing garbage: `%c`", b);
                    goto e_failed;
                }
            }

            if (flags & flag_seek_value)
            {
                switch (b)
                {
                whitespace:
                    continue;

                case ']':
                    if (top && top->type == SR_JSON_ARRAY)
                        flags = (flags & ~ (flag_need_comma | flag_seek_value)) | flag_next;
                    else
                    {
                        location->column = e_off;
                        location->message = sr_strdup("Unexpected `]`");
                        goto e_failed;
                    }

                    break;

                default:
                    if (flags & flag_need_comma)
                    {
                        if (b == ',')
                        {
                            flags &= ~ flag_need_comma;
                            continue;
                        }
                        else
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Expected `,` before `%c`", b);
                            goto e_failed;
                        }
                    }

                    if (flags & flag_need_colon)
                    {
                        if (b == ':')
                        {  flags &= ~ flag_need_colon;
                            continue;
                        }
                        else
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Expected `:` before `%c`", b);
                            goto e_failed;
                        }
                    }

                    flags &= ~ flag_seek_value;

                    switch (b)
                    {
                    case '{':
                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_OBJECT))
                            goto e_alloc_failure;

                        continue;
                    case '[':
                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_ARRAY))
                            goto e_alloc_failure;

                        flags |= flag_seek_value;
                        continue;
                    case '"':
                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_STRING))
                            goto e_alloc_failure;

                        flags |= flag_string;
                        string = top->u.string.ptr;
                        string_length = 0;
                        continue;
                    case 't':
                        if ((end - state.ptr) < 3 ||
                                *(++state.ptr) != 'r' ||
                                *(++state.ptr) != 'u' ||
                                *(++state.ptr) != 'e')
                        {
                            goto e_unknown_value;
                        }

                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_BOOLEAN))
                            goto e_alloc_failure;

                        top->u.boolean = 1;
                        flags |= flag_next;
                        break;
                    case 'f':
                        if ((end - state.ptr) < 4 ||
                                *(++state.ptr) != 'a' ||
                                *(++state.ptr) != 'l' ||
                                *(++state.ptr) != 's' ||
                                *(++state.ptr) != 'e')
                        {
                            goto e_unknown_value;
                        }

                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_BOOLEAN))
                            goto e_alloc_failure;

                        flags |= flag_next;
                        break;
                    case 'n':
                        if ((end - state.ptr) < 3 ||
                                *(++state.ptr) != 'u' ||
                                *(++state.ptr) != 'l' ||
                                *(++state.ptr) != 'l')
                        {
                            goto e_unknown_value;
                        }

                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_NULL))
                            goto e_alloc_failure;

                        flags |= flag_next;
                        break;
                    default:
                        if (isdigit(b) || b == '-')
                        {
                            if (!new_value(&state, &top, &root, &alloc, SR_JSON_INTEGER))
                                goto e_alloc_failure;

                            if (!state.first_pass)
                            {
                                while (isdigit(b) || b == '+' || b == '-' ||
                                        b == 'e' || b == 'E' || b == '.')
                                {
                                    if ((++state.ptr) == end)
                                    {
                                        b = 0;
                                        break;
                                    }

                                    b = *state.ptr;
                                }

                                flags |= flag_next | flag_reproc;
                                break;
                            }

                            flags &= ~ (flag_num_negative | flag_num_e |
                                    flag_num_e_got_sign | flag_num_e_negative |
                                    flag_num_zero);

                            num_digits = 0;
                            num_fraction = 0;
                            num_e = 0;

                            if (b != '-')
                            {
                                flags |= flag_reproc;
                                break;
                            }

                            flags |= flag_num_negative;
                            continue;
                        }
                        else if (!b)
                        {
                            /* End of file reached, but not expected */
                            location->column = e_off;
                            location->message = sr_strdup(top
                                    ? "Unexpected EOF"
                                    : "Empty input");
                            goto e_failed;
                        }
                        else
                        {
                            /* Unexpected character in input */
                            location->column = e_off;
                            location->message = sr_asprintf("Unexpected `%c` when "
                                    "seeking value", b);
                            goto e_failed;
                        }
                    }
                }
            }
            else
            {
                switch (top->type)
                {
                case SR_JSON_OBJECT:
                    switch (b)
                    {
                    whitespace:
                        continue;

                    case '"':
                        if (flags & flag_need_comma)
                        {
                            location->column = e_off;
                            location->message = sr_strdup("Expected `,` before `\"`");
                            goto e_failed;
                        }

                        flags |= flag_string;
                        string = (char*)top->_reserved.object_mem;
                        string_length = 0;
                        break;
                    case '}':
                        flags = (flags & ~ flag_need_comma) | flag_next;
                        break;
                    case ',':
                        if (flags & flag_need_comma)
                        {
                            flags &= ~ flag_need_comma;
                            break;
                        }
                    default:
                        location->column = e_off;
                        location->message = sr_asprintf("Unexpected `%c` in object", b);
                        goto e_failed;
                    }

                    break;
                case SR_JSON_INTEGER:
                case SR_JSON_DOUBLE:
                    if (isdigit (b))
                    {
                        ++num_digits;

                        if (top->type == SR_JSON_INTEGER || flags & flag_num_e)
                        {
                            if (!(flags & flag_num_e))
                            {
                                if (flags & flag_num_zero)
                                {
                                    location->column = e_off;
                                    location->message = sr_asprintf("Unexpected `0` before `%c`", b);
                                    goto e_failed;
                                }

                                if (num_digits == 1 && b == '0')
                                    flags |= flag_num_zero;
                            }
                            else
                            {
                                flags |= flag_num_e_got_sign;
                                num_e = (num_e * 10) + (b - '0');
                                continue;
                            }

                            if (would_overflow(top->u.integer, b))
                            {
                                --num_digits;
                                --state.ptr;
                                top->type = SR_JSON_DOUBLE;
                                top->u.dbl = (double)top->u.integer;
                                continue;
                            }

                            top->u.integer = (top->u.integer * 10) + (b - '0');
                            continue;
                        }

                        if (flags & flag_num_got_decimal)
                            num_fraction = (num_fraction * 10) + (b - '0');
                        else
                            top->u.dbl = (top->u.dbl * 10) + (b - '0');

                        continue;
                    }

                    if (b == '+' || b == '-')
                    {
                        if ((flags & flag_num_e) && !(flags & flag_num_e_got_sign))
                        {
                            flags |= flag_num_e_got_sign;

                            if (b == '-')
                                flags |= flag_num_e_negative;

                            continue;
                        }
                    }
                    else if (b == '.' && top->type == SR_JSON_INTEGER)
                    {
                        if (!num_digits)
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Expected digit before `.`");
                            goto e_failed;
                        }

                        top->type = SR_JSON_DOUBLE;
                        top->u.dbl = (double)top->u.integer;

                        flags |= flag_num_got_decimal;
                        num_digits = 0;
                        continue;
                    }

                    if (!(flags & flag_num_e))
                    {
                        if (top->type == SR_JSON_DOUBLE)
                        {
                            if (!num_digits)
                            {
                                location->column = e_off;
                                location->message = sr_asprintf("Expected digit after `.`");
                                goto e_failed;
                            }

                            top->u.dbl += num_fraction / pow(10.0, num_digits);
                        }

                        if (b == 'e' || b == 'E')
                        {
                            flags |= flag_num_e;

                            if (top->type == SR_JSON_INTEGER)
                            {
                                top->type = SR_JSON_DOUBLE;
                                top->u.dbl = (double)top->u.integer;
                            }

                            num_digits = 0;
                            flags &= ~ flag_num_zero;

                            continue;
                        }
                    }
                    else
                    {
                        if (!num_digits)
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Expected digit after `e`");
                            goto e_failed;
                        }

                        top->u.dbl *= pow(10.0, (flags & flag_num_e_negative) ? -num_e : num_e);
                    }

                    if (flags & flag_num_negative)
                    {
                        if (top->type == SR_JSON_INTEGER)
                            top->u.integer = -top->u.integer;
                        else
                            top->u.dbl = -top->u.dbl;
                    }

                    flags |= flag_next | flag_reproc;
                    break;
                default:
                    break;
                }
            }

            if (flags & flag_reproc)
            {
                flags &= ~ flag_reproc;
                --state.ptr;
            }

            if (flags & flag_next)
            {
                flags = (flags & ~ flag_next) | flag_need_comma;

                if (!top->parent)
                {
                    /* root value done */

                    flags |= flag_done;
                    continue;
                }

                if (top->parent->type == SR_JSON_ARRAY)
                    flags |= flag_seek_value;

                if (!state.first_pass)
                {
                    struct sr_json_value *parent = top->parent;

                    switch (parent->type)
                    {
                    case SR_JSON_OBJECT:
                        parent->u.object.values
                            [parent->u.object.length].value = top;

                        break;
                    case SR_JSON_ARRAY:
                        parent->u.array.values
                            [parent->u.array.length] = top;

                        break;
                    default:
                        break;
                    };
                }

                if ((++top->parent->u.array.length) > state.uint_max)
                    goto e_overflow;

                top = top->parent;
                continue;
            }
        }

        alloc = root;
    }

    return root;

 e_unknown_value:
    location->column = e_off;
    location->message = sr_strdup("Unknown value");
    goto e_failed;

 e_alloc_failure:
    location->column = e_off;
    location->message = sr_strdup("Memory allocation failure");
    goto e_failed;

 e_overflow:
    location->column = e_off;
    location->message = sr_strdup("Too long (caught overflow)");
    goto e_failed;

 e_failed:
    if (state.first_pass)
        alloc = root;

    while (alloc)
    {
        top = alloc->_reserved.next_alloc;
        free(alloc);
        alloc = top;
    }

    if (!state.first_pass)
        sr_json_value_free(root);

    return NULL;
}

struct sr_json_value *
sr_json_parse(const char *json, char **error_message)
{
    struct sr_json_settings settings;
    memset(&settings, 0, sizeof(struct sr_json_settings));
    struct sr_location location;
    sr_location_init(&location);
    struct sr_json_value *json_root = sr_json_parse_ex(&settings, json,
                                                       &location);

    if (!json_root)
        *error_message = sr_location_to_string(&location);

    return json_root;
}

void
sr_json_value_free(struct sr_json_value *value)
{
    struct sr_json_value *cur_value;

    if (!value)
        return;

    value->parent = 0;

    while (value)
    {
        switch (value->type)
        {
        case SR_JSON_ARRAY:
            if (!value->u.array.length)
            {
                free(value->u.array.values);
                break;
            }

            value = value->u.array.values[--value->u.array.length];
            continue;
        case SR_JSON_OBJECT:
            if (!value->u.object.length)
            {
                free(value->u.object.values);
                break;
            }

            value = value->u.object.values[--value->u.object.length].value;
            continue;
        case SR_JSON_STRING:
            free(value->u.string.ptr);
            break;
        default:
            break;
        };

        cur_value = value;
        value = value->parent;
        free(cur_value);
    }
}

char *
sr_json_escape(const char *text)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();
    const char *c = text;
    while (*c != '\0')
    {
        switch (*c)
        {
        case '"':
            sr_strbuf_append_str(strbuf, "\\\"");
            break;
        case '\n':
            sr_strbuf_append_str(strbuf, "\\n");
            break;
        case '\\':
            sr_strbuf_append_str(strbuf, "\\\\");
            break;
        case '\r':
            sr_strbuf_append_str(strbuf, "\\r");
            break;
        case '\f':
            sr_strbuf_append_str(strbuf, "\\f");
            break;
        case '\b':
            sr_strbuf_append_str(strbuf, "\\b");
            break;
        case '\t':
            sr_strbuf_append_str(strbuf, "\\t");
            break;
        default:
            sr_strbuf_append_char(strbuf, *c);
        }

        ++c;
    }

    return sr_strbuf_free_nobuf(strbuf);
}

struct sr_strbuf *
sr_json_append_escaped(struct sr_strbuf *strbuf, const char *str)
{
    char *escaped_str = sr_json_escape(str);
    sr_strbuf_append_char(strbuf, '\"');
    sr_strbuf_append_str(strbuf, escaped_str);
    sr_strbuf_append_char(strbuf, '\"');
    free(escaped_str);

    return strbuf;
}

static char *type_names[] = {
   [SR_JSON_NONE]    = "none",
   [SR_JSON_OBJECT]  = "object",
   [SR_JSON_ARRAY]   = "array",
   [SR_JSON_INTEGER] = "integer",
   [SR_JSON_DOUBLE]  = "double",
   [SR_JSON_STRING]  = "string",
   [SR_JSON_BOOLEAN] = "boolean",
   [SR_JSON_NULL]    = "null",
};

bool
json_check_type(struct sr_json_value *value, enum sr_json_type type,
                const char *name, char **error_message)
{
    if (value->type == type)
        return true;

    assert(type >= SR_JSON_NONE && type <= SR_JSON_NULL);
    char *type_str = type_names[type];

    if (error_message)
        *error_message = sr_asprintf("Invalid type of `%s`; `%s` expected",
                name, type_str);
    return false;
}

struct sr_json_value *
json_element(struct sr_json_value *object, const char *key_name)
{
    assert(object->type == SR_JSON_OBJECT);

    for (unsigned i = 0; i < object->u.object.length; ++i)
    {
        if (0 == strcmp(key_name, object->u.object.values[i].name))
            return object->u.object.values[i].value;
    }

    return NULL;
}

unsigned
json_object_children_count(struct sr_json_value *object)
{
    assert(object->type == SR_JSON_OBJECT);

    return object->u.object.length;
}

struct sr_json_value *
json_object_get_child(struct sr_json_value *object, unsigned child_no, const char **child_name)
{
    assert(object->type == SR_JSON_OBJECT);
    assert(child_no < object->u.object.length);

    *child_name = object->u.object.values[child_no].name;
    return object->u.object.values[child_no].value;
}

const char *
json_string_get_value(struct sr_json_value *object)
{
    assert(object->type == SR_JSON_STRING);

    return object->u.string.ptr;
}

#define DEFINE_JSON_READ(name, c_type, json_type, json_member, conversion)                       \
    bool                                                                                         \
    name(struct sr_json_value *object, const char *key_name, c_type *dest, char **error_message) \
    {                                                                                            \
        struct sr_json_value *val = json_element(object, key_name);                              \
                                                                                                 \
        if (!val)                                                                                \
            return true;                                                                         \
                                                                                                 \
        if (!json_check_type(val, json_type, key_name, error_message))                           \
            return false;                                                                        \
                                                                                                 \
        *dest = conversion(val->json_member);                                                    \
                                                                                                 \
        return true;                                                                             \
    }

#define NOOP

DEFINE_JSON_READ(json_read_uint64, uint64_t, SR_JSON_INTEGER, u.integer, NOOP)
DEFINE_JSON_READ(json_read_uint32, uint32_t, SR_JSON_INTEGER, u.integer, NOOP)
DEFINE_JSON_READ(json_read_uint16, uint16_t, SR_JSON_INTEGER, u.integer, NOOP)
DEFINE_JSON_READ(json_read_string, char *, SR_JSON_STRING, u.string.ptr, sr_strdup)
DEFINE_JSON_READ(json_read_bool, bool, SR_JSON_BOOLEAN, u.boolean, NOOP)
