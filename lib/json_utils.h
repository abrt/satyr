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

#pragma once

#include <json.h>
#include <stdbool.h>

/* Checks whether json value is of the right type and sets the error_message if
 * not. The "name" argument is used in the error message.
 */
bool
json_check_type(json_object *object, json_type type,
                const char *name, char **error_message);

/* These functions check whether JSON object "object" has key "key_name". If it
 * does not, they don't do anything and return true.
 *
 * If it does, they check whether the JSON type of the element corresponds to
 * the C type of "dest" -- if it does not, they set the error message and
 * return false.
 *
 * Finally, if the type is correct, they write the value to "dest" and return true.
 */
bool
json_read_uint64(json_object *object, const char *key_name, uint64_t *dest,
                 char **error_message);
bool
json_read_uint32(json_object *object, const char *key_name, uint32_t *dest,
                 char **error_message);
bool
json_read_uint16(json_object *object, const char *key_name, uint16_t *dest,
                 char **error_message);
bool
json_read_string(json_object *object, const char *key_name, char **dest,
                 char **error_message);
bool
json_read_bool(json_object *object, const char *key_name, bool *dest,
               char **error_message);

/* same as above with implicit error_message argument */
#define JSON_READ_UINT64(object, key_name, dest) \
    json_read_uint64(object, key_name, dest, error_message)
#define JSON_READ_UINT32(object, key_name, dest) \
    json_read_uint32(object, key_name, dest, error_message)
#define JSON_READ_UINT16(object, key_name, dest) \
    json_read_uint16(object, key_name, dest, error_message)
#define JSON_READ_STRING(object, key_name, dest) \
    json_read_string(object, key_name, dest, error_message)
#define JSON_READ_BOOL(object, key_name, dest) \
    json_read_bool(object, key_name, dest, error_message)

struct sr_strbuf *
sr_json_append_escaped(struct sr_strbuf *strbuf, const char *str);
