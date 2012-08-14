/*
    core_backtrace.c

    Copyright (C) 2012  Red Hat, Inc.

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
#include "core_backtrace.h"
#include "core_frame.h"
#include "core_thread.h"
#include "gdb_backtrace.h"
#include "gdb_frame.h"
#include "gdb_thread.h"
#include "location.h"
#include "normalize.h"
#include "utils.h"
#include "utils_strbuf.h"
#include "utils_unstrip.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct btp_core_backtrace *
btp_core_backtrace_new()
{
    struct btp_core_backtrace *backtrace = btp_malloc(sizeof(struct btp_core_backtrace));
    btp_core_backtrace_init(backtrace);
    return backtrace;
}

void
btp_core_backtrace_init(struct btp_core_backtrace *backtrace)
{
    backtrace->type = BTP_USERSPACE;
    backtrace->threads = NULL;
}

void
btp_core_backtrace_free(struct btp_core_backtrace *backtrace)
{
    if (!backtrace)
        return;

    while (backtrace->threads)
    {
        struct btp_core_thread *thread = backtrace->threads;
        backtrace->threads = thread->next;
        btp_core_thread_free(thread);
    }

    free(backtrace);
}

struct btp_core_backtrace *
btp_core_backtrace_dup(struct btp_core_backtrace *backtrace)
{
    struct btp_core_backtrace *result = btp_core_backtrace_new();
    memcpy(result, backtrace, sizeof(struct btp_core_backtrace));

    if (backtrace->threads)
        result->threads = btp_core_thread_dup(backtrace->threads, true);

    return result;
}

int
btp_core_backtrace_get_thread_count(struct btp_core_backtrace *backtrace)
{
    struct btp_core_thread *thread = backtrace->threads;
    int count = 0;
    while (thread)
    {
        thread = thread->next;
        ++count;
    }
    return count;
}

struct btp_core_backtrace *
btp_core_backtrace_parse(const char **input,
                         struct btp_location *location)
{
    // TODO
    return NULL;
}

char *
btp_core_backtrace_to_text(struct btp_core_backtrace *backtrace)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();

    switch (backtrace->type)
    {
    case BTP_USERSPACE:
        btp_strbuf_append_str(strbuf, "type userspace\n\n");
        break;
    case BTP_PYTHON:
        btp_strbuf_append_str(strbuf, "type python\n\n");
        break;
    case BTP_KERNELOOPS:
        btp_strbuf_append_str(strbuf, "type kerneloops\n\n");
        break;
    default:
        btp_strbuf_append_str(strbuf, "type unknown\n\n");
        break;
    }

    struct btp_core_thread *thread = backtrace->threads;
    while (thread)
    {
        btp_core_thread_append_to_str(thread, strbuf);
        thread = thread->next;
    }

    return btp_strbuf_free_nobuf(strbuf);
}

struct btp_core_backtrace *
btp_core_backtrace_create(const char *gdb_backtrace_text,
                          const char *unstrip_text,
                          const char *executable_path)
{
    // Parse the GDB backtrace.
    struct btp_location location;
    btp_location_init(&location);

    struct btp_gdb_backtrace *gdb_backtrace = btp_gdb_backtrace_parse(&gdb_backtrace_text, &location);
    if (!gdb_backtrace)
    {
        if (btp_debug_parser)
        {
            fprintf(stderr, "Unable to parse backtrace: %d:%d: %s\n",
                    location.line, location.column, location.message);
        }

        return NULL;
    }

    // Parse the unstrip output.
    struct btp_unstrip_entry *unstrip = btp_unstrip_parse(unstrip_text);
    if (!unstrip)
    {
        if (btp_debug_parser)
            fprintf(stderr, "Unable to parse unstrip output.");

        return NULL;
    }

    // Create the core backtrace
    struct btp_core_backtrace *core_backtrace = btp_core_backtrace_new();

    struct btp_gdb_thread *gdb_thread = gdb_backtrace->threads;
    while (gdb_thread)
    {
        struct btp_core_thread *core_thread = btp_core_thread_new();

        struct btp_gdb_frame *gdb_frame = gdb_thread->frames;
        while (gdb_frame)
        {
            gdb_frame = gdb_frame->next;

            struct btp_core_frame *core_frame = btp_core_frame_new();
            core_frame->address = gdb_frame->address;

            struct btp_unstrip_entry *unstrip_entry = btp_unstrip_find_address(unstrip, gdb_frame->address);

            if (unstrip_entry)
            {
                core_frame->build_id = btp_strdup(unstrip_entry->build_id);
                core_frame->build_id_offset = core_frame->address - unstrip_entry->start;
                core_frame->file_name = btp_strdup(unstrip_entry->file_name);
            }

            if (gdb_frame->function_name &&
                0 != strcmp(gdb_frame->function_name, "??"))
            {
                core_frame->function_name = btp_strdup(gdb_frame->function_name);
            }
        }

        core_backtrace->threads = btp_core_thread_append(core_backtrace->threads,
                                                         core_thread);

        gdb_thread = gdb_thread->next;
    }

    return core_backtrace;
}
