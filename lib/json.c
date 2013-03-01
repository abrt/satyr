/*
    json.c

    Copyright (C) 2012  James McLaughlin et al.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef unsigned short json_uchar;

const struct sr_json_value
sr_json_value_none = { 0 };

static unsigned char
hex_value(char c)
{
    if (c >= 'A' && c <= 'F')
        return (c - 'A') + 10;

    if (c >= 'a' && c <= 'f')
        return (c - 'a') + 10;

    if (c >= '0' && c <= '9')
        return c - '0';

    return 0xFF;
}

struct json_state
{
   struct sr_json_settings settings;
   int first_pass;

   unsigned long used_memory;
   unsigned int uint_max;
   unsigned long ulong_max;
};

static void *
json_alloc(struct json_state *state, unsigned long size, int zero)
{
    void * mem;

    if ((state->ulong_max - state->used_memory) < size)
        return 0;

    if (state->settings.max_memory
        && (state->used_memory += size) > state->settings.max_memory)
    {
        return 0;
    }

    if (!(mem = zero ? calloc (size, 1) : malloc (size)))
        return 0;

    return mem;
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
            value->u.array.values = (struct sr_json_value **)json_alloc(
                state, value->u.array.length * sizeof(struct sr_json_value *), 0);

            if (!value->u.array.values)
                return 0;

            break;
        case SR_JSON_OBJECT:
            values_size = sizeof(*value->u.object.values) * value->u.object.length;
            (*(void**)&value->u.object.values) =
                json_alloc(state, values_size + ((unsigned long)value->u.object.values), 0);

            if (!value->u.object.values)
                return 0;

            value->_reserved.object_mem = (*(char**)&value->u.object.values) + values_size;
            break;
        case SR_JSON_STRING:
            value->u.string.ptr = (char*)
                json_alloc(state, (value->u.string.length + 1), 0);

            if (!value->u.string.ptr)
                return 0;

            break;

        default:
            break;
        };

        value->u.array.length = 0;

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
   ((int) (i - cur_line_begin))

#define whitespace \
   case '\n': ++location->line;  cur_line_begin = i; \
   case ' ': case '\t': case '\r'

#define string_add(b)  \
   do { if (!state.first_pass) string [string_length] = b;  ++ string_length; } while (0);

const static int
   flag_next = 1, flag_reproc = 2, flag_need_comma = 4, flag_seek_value = 8, flag_exponent = 16,
   flag_got_exponent_sign = 32, flag_escaped = 64, flag_string = 128, flag_need_colon = 256,
   flag_done = 512;

struct sr_json_value *
sr_json_parse_ex(struct sr_json_settings *settings,
                  const char *json,
                  struct sr_location *location)
{
    const char *cur_line_begin, *i;
    struct sr_json_value *top, *root, *alloc = 0;
    struct json_state state;
    int flags;

    memset(&state, 0, sizeof(struct json_state));
    memcpy(&state.settings, settings, sizeof(struct sr_json_settings));
    memset(&state.uint_max, 0xFF, sizeof(state.uint_max));
    memset(&state.ulong_max, 0xFF, sizeof(state.ulong_max));
    state.uint_max -= 8; /* limit of how much can be added before next check */
    state.ulong_max -= 8;

    for (state.first_pass = 1; state.first_pass >= 0; --state.first_pass)
    {
        json_uchar uchar;
        unsigned char uc_b1, uc_b2, uc_b3, uc_b4;
        char *string;
        unsigned string_length;

        top = root = 0;
        flags = flag_seek_value;

        location->line = 1;
        cur_line_begin = json;

        for (i = json ;; ++ i)
        {
            char b = *i;

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
                };
            }

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
                    case 'b':  string_add ('\b');  break;
                    case 'f':  string_add ('\f');  break;
                    case 'n':  string_add ('\n');  break;
                    case 'r':  string_add ('\r');  break;
                    case 't':  string_add ('\t');  break;
                    case 'u':

                        if ((uc_b1 = hex_value(*++i)) == 0xFF || (uc_b2 = hex_value(*++i)) == 0xFF
                            || (uc_b3 = hex_value(*++i)) == 0xFF || (uc_b4 = hex_value(*++i)) == 0xFF)
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Invalid character value `%c`", b);
                            goto e_failed;
                        }

                        uc_b1 = uc_b1 * 16 + uc_b2;
                        uc_b2 = uc_b3 * 16 + uc_b4;

                        uchar = ((char) uc_b1) * 256 + uc_b2;

                        if (uc_b1 == 0 && uc_b2 <= 0x7F)
                        {
                            string_add((char) uchar);
                            break;
                        }

                        if (uchar <= 0x7FF)
                        {
                            if (state.first_pass)
                                string_length += 2;
                            else
                            {
                                string[string_length ++] = 0xC0 | ((uc_b2 & 0xC0) >> 6) | ((uc_b1 & 0x3) << 3);
                                string[string_length ++] = 0x80 | (uc_b2 & 0x3F);
                            }

                            break;
                        }

                        if (state.first_pass)
                            string_length += 3;
                        else
                        {
                            string[string_length ++] = 0xE0 | ((uc_b1 & 0xF0) >> 4);
                            string[string_length ++] = 0x80 | ((uc_b1 & 0xF) << 2) | ((uc_b2 & 0xC0) >> 6);
                            string[string_length ++] = 0x80 | (uc_b2 & 0x3F);
                        }

                        break;

                    default:
                        string_add (b);
                    };

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
                            (*(char**) &top->u.object.values) += string_length + 1;
                        else
                        {
                            top->u.object.values [top->u.object.length].name =
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

            if (flags & flag_seek_value)
            {
                switch (b)
                {
                whitespace:
                    continue;

                case ']':
                    if (top->type == SR_JSON_ARRAY)
                        flags = (flags & ~ (flag_need_comma | flag_seek_value)) | flag_next;
                    else if (!state.settings.settings & SR_JSON_RELAXED_COMMAS)
                    {
                        location->column = e_off;
                        location->message = sr_strdup("Unexpected ]");
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
                            location->message = sr_asprintf("Expected , before %c", b);
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
                            location->message = sr_asprintf("Expected : before %c", b);
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
                        if (*(++ i) != 'r' || *(++ i) != 'u' || *(++ i) != 'e')
                            goto e_unknown_value;

                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_BOOLEAN))
                            goto e_alloc_failure;

                        top->u.boolean = 1;
                        flags |= flag_next;
                        break;
                    case 'f':
                        if (*(++ i) != 'a' || *(++ i) != 'l' || *(++ i) != 's' || *(++ i) != 'e')
                            goto e_unknown_value;

                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_BOOLEAN))
                            goto e_alloc_failure;

                        flags |= flag_next;
                        break;
                    case 'n':
                        if (*(++ i) != 'u' || *(++ i) != 'l' || *(++ i) != 'l')
                            goto e_unknown_value;

                        if (!new_value(&state, &top, &root, &alloc, SR_JSON_NULL))
                            goto e_alloc_failure;

                        flags |= flag_next;
                        break;
                    default:
                        if (isdigit (b) || b == '-')
                        {
                            if (!new_value(&state, &top, &root, &alloc, SR_JSON_INTEGER))
                                goto e_alloc_failure;

                            flags &= ~ (flag_exponent | flag_got_exponent_sign);
                            if (state.first_pass)
                                continue;

                            if (top->type == SR_JSON_DOUBLE)
                                top->u.dbl = strtod(i, (char**)&i);
                            else
                                top->u.integer = strtol(i, (char**)&i, 10);

                            flags |= flag_next | flag_reproc;
                        }
                        else
                        {
                            location->column = e_off;
                            location->message = sr_asprintf("Unexpected %c when seeking value", b);
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
                        if (flags & flag_need_comma && (!state.settings.settings & SR_JSON_RELAXED_COMMAS))
                        {
                            location->column = e_off;
                            location->message = sr_strdup("Expected , before \"");
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
                        continue;

                    if (b == 'e' || b == 'E')
                    {
                        if (!(flags & flag_exponent))
                        {
                            flags |= flag_exponent;
                            top->type = SR_JSON_DOUBLE;

                            continue;
                        }
                    }
                    else if (b == '+' || b == '-')
                    {
                        if (flags & flag_exponent && !(flags & flag_got_exponent_sign))
                        {
                            flags |= flag_got_exponent_sign;
                            continue;
                        }
                    }
                    else if (b == '.' && top->type == SR_JSON_INTEGER)
                    {
                        top->type = SR_JSON_DOUBLE;
                        continue;
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
                -- i;
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

                if ( (++ top->parent->u.array.length) > state.uint_max)
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
sr_json_parse(const char *json)
{
    struct sr_json_settings settings;
    memset(&settings, 0, sizeof(struct sr_json_settings));
    struct sr_location location;
    sr_location_init(&location);
    return sr_json_parse_ex(&settings, json, &location);
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
