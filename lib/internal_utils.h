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

#include <stddef.h>
#include <assert.h>

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

#endif
