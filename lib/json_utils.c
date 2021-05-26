/*  Copyright (C) 2019  Red Hat, Inc.
 *
 *  satyr is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  satyr is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "json_utils.h"

#include "utils.h"

#define DEFINE_JSON_READ(name, c_type, json_type, getter_suffix, converter)             \
    bool                                                                                \
    name(json_object *object, const char *key_name, c_type *dest, char **error_message) \
    {                                                                                   \
        json_object *child;                                                             \
                                                                                        \
        if (!json_object_object_get_ex(object, key_name, &child))                       \
            return true;                                                                \
                                                                                        \
        if (!json_check_type(child, json_type, key_name, error_message))                \
            return false;                                                               \
                                                                                        \
        if (NULL != dest)                                                               \
            *dest = converter(json_object_get_ ## getter_suffix(child));                \
                                                                                        \
        return true;                                                                    \
    }

#define NOOP

DEFINE_JSON_READ(json_read_uint64, uint64_t, json_type_int, int64, NOOP)
DEFINE_JSON_READ(json_read_uint32, uint32_t, json_type_int, int, NOOP)
DEFINE_JSON_READ(json_read_uint16, uint16_t, json_type_int, int, NOOP)
DEFINE_JSON_READ(json_read_string, char *, json_type_string, string, g_strdup)
DEFINE_JSON_READ(json_read_bool, bool, json_type_boolean, boolean, NOOP)

bool
json_check_type(json_object *object, json_type type,
                const char *name, char **error_message)
{
    if (json_object_is_type (object, type))
    {
        return true;
    }

    if (error_message)
    {
        const char *type_name;

        type_name = json_type_to_name(type);

        *error_message = g_strdup_printf("Invalid type of `%s`; `%s` expected",
                name, type_name);
    }

    return false;
}

static char *
sr_json_escape(const char *text)
{
    GString *strbuf = g_string_new(NULL);
    const char *c = text;
    while (*c != '\0')
    {
        switch (*c)
        {
        case '"':
            g_string_append(strbuf, "\\\"");
            break;
        case '\n':
            g_string_append(strbuf, "\\n");
            break;
        case '\\':
            g_string_append(strbuf, "\\\\");
            break;
        case '\r':
            g_string_append(strbuf, "\\r");
            break;
        case '\f':
            g_string_append(strbuf, "\\f");
            break;
        case '\b':
            g_string_append(strbuf, "\\b");
            break;
        case '\t':
            g_string_append(strbuf, "\\t");
            break;
        default:
            g_string_append_c(strbuf, *c);
        }

        ++c;
    }

    return g_string_free(strbuf, FALSE);
}

GString *
sr_json_append_escaped(GString *strbuf, const char *str)
{
    char *escaped_str = sr_json_escape(str);
    g_string_append_c(strbuf, '\"');
    g_string_append(strbuf, escaped_str);
    g_string_append_c(strbuf, '\"');
    g_free(escaped_str);

    return strbuf;
}
