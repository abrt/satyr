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

#include <assert.h>

#define DISPATCH(table, type, method) \
    (assert((type > SR_REPORT_INVALID) && (type) < SR_REPORT_NUM && table[type]->method), \
    table[type]->method)

#define DEFINE_NEXT_FUNC(name, abstract_t, concrete_t)   \
    static abstract_t *                                  \
    name(abstract_t *node)                               \
    {                                                    \
        return (abstract_t *)((concrete_t *)node)->next; \
    }

