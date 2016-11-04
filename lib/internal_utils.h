/*
    internal_utils.h

    Copyright (C) 2013  Red Hat, Inc.

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
#ifndef SATYR_INTERNAL_UTILS_H
#define SATYR_INTERNAL_UTILS_H

#include "utils.h"
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct sr_json_value;
struct sr_location;
enum sr_json_type;

void
warn(const char *fmt, ...) __sr_printf(1, 2);

#define DISPATCH(table, type, method) \
    (assert((type > SR_REPORT_INVALID) && (type) < SR_REPORT_NUM && table[type]->method), \
    table[type]->method)

#define DEFINE_GETTER(name, member, struct_abstract_t, struct_concrete_t, member_abstract_t) \
    static member_abstract_t *                                                               \
    name(struct_abstract_t *node)                                                            \
    {                                                                                        \
        return (member_abstract_t *)((struct_concrete_t *)node)->member;                     \
    }

#define DEFINE_SETTER(name, member, struct_abstract_t, struct_concrete_t, member_abstract_t) \
    static void                                                                              \
    name(struct_abstract_t *node, member_abstract_t *val)                                    \
    {                                                                                        \
        ((struct_concrete_t *)node)->member = (void*) val;                                   \
    }

#define DEFINE_NEXT_FUNC(name, abstract_t, concrete_t) DEFINE_GETTER(name, next, abstract_t, concrete_t, abstract_t)
#define DEFINE_SET_NEXT_FUNC(name, abstract_t, concrete_t) DEFINE_SETTER(name, next, abstract_t, concrete_t, abstract_t)

/* beware the side effects */
#define OR_UNKNOWN(s) ((s) ? (s) : "<unknown>")

/* kerneloops taint flag structure and global table declaration */
struct sr_taint_flag
{
    char letter;
    size_t member_offset;
    char *name;
};
extern struct sr_taint_flag sr_flags[];

/* Checks whether json value is of the right type and sets the error_message if
 * not. The "name" argument is used in the error message. */
bool
json_check_type(struct sr_json_value *value, enum sr_json_type type,
                const char *name, char **error_message);

/* same as above with implicit error_message argument */
#define JSON_CHECK_TYPE(value, type, name) json_check_type(value, type, name, error_message)

struct sr_json_value *
json_element(struct sr_json_value *object, const char *name);

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
json_read_uint64(struct sr_json_value *object, const char *key_name, uint64_t *dest,
                 char **error_message);
bool
json_read_uint32(struct sr_json_value *object, const char *key_name, uint32_t *dest,
                 char **error_message);
bool
json_read_uint16(struct sr_json_value *object, const char *key_name, uint16_t *dest,
                 char **error_message);
bool
json_read_string(struct sr_json_value *object, const char *key_name, char **dest,
                 char **error_message);
bool
json_read_bool(struct sr_json_value *object, const char *key_name, bool *dest,
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

/* iterates over json array "json", assigning the current element to "elem_var" */
#define FOR_JSON_ARRAY(json, elem_var)                                      \
    assert(json->type == SR_JSON_ARRAY);                                    \
    for(unsigned _j = 0;                                                    \
        _j < json->u.array.length && (elem_var = json->u.array.values[_j]); \
        ++_j)

/* assert that is never compiled out */
#define SR_ASSERT(cond)                                                               \
    if (!(cond))                                                                      \
    {                                                                                 \
        fprintf(stderr, "Assertion failed (%s:%d): %s\n", __FILE__, __LINE__, #cond); \
        abort();                                                                      \
    }

/* returns the number of object's children */
unsigned
json_object_children_count(struct sr_json_value *object);

/* returns the child_noth child and passes its name in child_name arg */
struct sr_json_value *
json_object_get_child(struct sr_json_value *object, unsigned child_no, const char **child_name);

/* returns string's value */
const char *
json_string_get_value(struct sr_json_value *object);

#endif
