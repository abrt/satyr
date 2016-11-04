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
#ifndef SATYR_GENERIC_THREAD_H
#define SATYR_GENERIC_THREAD_H

#include "thread.h"
#include "internal_utils.h"

enum sr_bthash_flags;

typedef struct sr_frame* (*frames_fn_t)(struct sr_thread*);
typedef void (*set_frames_fn_t)(struct sr_thread*, struct sr_frame*);
typedef int (*thread_cmp_fn_t)(struct sr_thread*, struct sr_thread*);
typedef int (*frame_count_fn_t)(struct sr_thread*);
typedef struct sr_thread* (*next_thread_fn_t)(struct sr_thread*);
typedef void (*set_next_thread_fn_t)(struct sr_thread*, struct sr_thread*);
typedef void (*thread_append_bthash_text_fn_t)(struct sr_thread*, enum sr_bthash_flags,
                                               struct sr_strbuf*);
typedef void (*thread_free_fn_t)(struct sr_thread*);
typedef void (*normalize_fn_t)(struct sr_thread*);
typedef bool (*remove_frame_fn_t)(struct sr_thread*, struct sr_frame*);
typedef bool (*remove_frames_above_fn_t)(struct sr_thread*, struct sr_frame*);
typedef struct sr_thread* (*thread_dup_fn_t)(struct sr_thread*);

struct thread_methods
{
    frames_fn_t frames;
    set_frames_fn_t set_frames;
    thread_cmp_fn_t cmp;
    frame_count_fn_t frame_count;
    next_thread_fn_t next;
    set_next_thread_fn_t set_next;
    thread_append_bthash_text_fn_t thread_append_bthash_text;
    thread_free_fn_t thread_free;
    normalize_fn_t normalize;
    remove_frame_fn_t remove_frame;
    remove_frames_above_fn_t remove_frames_above;
    thread_dup_fn_t thread_dup;
};

extern struct thread_methods core_thread_methods, python_thread_methods,
       koops_thread_methods, gdb_thread_methods, java_thread_methods,
       ruby_thread_methods, js_thread_methods;

/* Macros to generate accessors for the "frames" member. */
#define DEFINE_FRAMES_FUNC(name, concrete_t) \
    DEFINE_GETTER(name, frames, struct sr_thread, concrete_t, struct sr_frame)

#define DEFINE_SET_FRAMES_FUNC(name, concrete_t) \
    DEFINE_SETTER(name, frames, struct sr_thread, concrete_t, struct sr_frame)

/* _dup wrapper for thread fucntions that accept 'sibling' argument */
#define DEFINE_DUP_WRAPPER_FUNC(name, concrete_t, wrappee) \
    static concrete_t *                                    \
    name(concrete_t *thread)                               \
    {                                                      \
      return wrappee(thread, false);                       \
    }

int
thread_frame_count(struct sr_thread *thread);

bool
thread_remove_frame(struct sr_thread *thread, struct sr_frame *frame);

bool
thread_remove_frames_above(struct sr_thread *thread, struct sr_frame *frame);

struct sr_thread *
thread_no_next_thread(struct sr_thread *thread);

void
thread_no_bthash_text(struct sr_thread *thread, enum sr_bthash_flags flags,
                      struct sr_strbuf *strbuf);

void
thread_no_normalization(struct sr_thread *thread);

/* Uses dispatch table but not intended for public use. */
void
thread_append_bthash_text(struct sr_thread *thread, enum sr_bthash_flags flags,
                          struct sr_strbuf *strbuf);

#endif
