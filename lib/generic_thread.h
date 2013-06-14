/*
    generic_stacktrace.h

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

#include "thread.h"

typedef struct sr_frame* (*frames_fn_t)(struct sr_thread*);
typedef int (*thread_cmp_fn_t)(struct sr_thread*, struct sr_thread*);
typedef int (*frame_count_fn_t)(struct sr_thread*);
typedef struct sr_thread* (*next_thread_fn_t)(struct sr_thread*);

struct thread_methods
{
    frames_fn_t frames;
    thread_cmp_fn_t cmp;
    frame_count_fn_t frame_count;
    next_thread_fn_t next;
};

extern struct thread_methods core_thread_methods, python_thread_methods,
       koops_thread_methods, gdb_thread_methods, java_thread_methods;

/* Macro to generate accessors for the "frames" member. */
#define DEFINE_THREAD_FUNC(name, concrete_t)                    \
    static struct sr_frame *                                    \
    name(struct sr_frame *node)                                 \
    {                                                           \
        return (struct sr_frame *)((concrete_t *)node)->frames; \
    }                                                           \

int
thread_frame_count(struct sr_thread *thread);

struct sr_thread *
thread_no_next_thread(struct sr_thread *thread);
