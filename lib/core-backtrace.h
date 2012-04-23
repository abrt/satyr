/*
    backtrace.c

    Copyright (C) 2010  Red Hat, Inc.

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
#ifndef BTPARSER_CORE_BACKTRACE_H
#define BTPARSER_CORE_BACKTRACE_H

#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

struct frame_aux
{
    char *build_id;
    char *modname;
    char *fingerprint;
};

struct backtrace_entry {
    uintptr_t  address;
    char      *build_id;
    uintptr_t  build_id_offset;
    char      *symbol;
    char      *modname;
    char      *filename;
    char      *fingerprint;
    uintptr_t  function_initial_loc;
    uintptr_t  function_length;
};

char *
btp_core_backtrace_fmt(GList *backtrace);

void
btp_assign_build_ids(GList *backtrace, const char *dump_dir_name);

struct btp_thread *
btp_load_core_backtrace(const char *text);

int
btp_core_backtrace_frame_cmp(struct btp_frame *frame1, struct btp_frame *frame2);

void
btp_free_core_backtrace(struct btp_thread *thread);

void
btp_backtrace_add_build_id(GList *backtrace, uintmax_t start, uintmax_t length,
                           const char *build_id, unsigned build_id_len,
                           const char *modname, unsigned modname_len,
                           const char *filename, unsigned filename_len);

void
btp_core_assign_build_ids(GList *backtrace, const char *unstrip_output,
                          const char *executable);

GList *
btp_backtrace_extract_addresses(const char *bt);

#ifdef __cplusplus
}
#endif

#endif
