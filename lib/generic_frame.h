/*
    generic_frame.h

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

#include "frame.h"

typedef void (*append_to_str_fn_t)(struct sr_frame *, struct sr_strbuf *);
typedef struct sr_frame* (*next_frame_fn_t)(struct sr_frame *);
typedef int (*frame_cmp_fn_t)(struct sr_frame *, struct sr_frame *);

struct frame_methods
{
    append_to_str_fn_t append_to_str;
    next_frame_fn_t next;
    frame_cmp_fn_t cmp;
    frame_cmp_fn_t cmp_distance;
};

extern struct frame_methods core_frame_methods, python_frame_methods,
       koops_frame_methods, gdb_frame_methods, java_frame_methods;
