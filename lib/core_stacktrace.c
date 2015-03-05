/*
    core_stacktrace.c

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
#include "core/stacktrace.h"
#include "core/frame.h"
#include "core/thread.h"
#include "gdb/stacktrace.h"
#include "gdb/frame.h"
#include "gdb/thread.h"
#include "location.h"
#include "normalize.h"
#include "utils.h"
#include "strbuf.h"
#include "unstrip.h"
#include "json.h"
#include "generic_stacktrace.h"
#include "internal_utils.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Method table */

static void
core_append_bthash_text(struct sr_core_stacktrace *stacktrace, enum sr_bthash_flags flags,
                        struct sr_strbuf *strbuf);

DEFINE_THREADS_FUNC(core_threads, struct sr_core_stacktrace)
DEFINE_SET_THREADS_FUNC(core_set_threads, struct sr_core_stacktrace)

struct stacktrace_methods core_stacktrace_methods =
{
    /* core parser returns error_message directly */
    .parse = (parse_fn_t) sr_core_stacktrace_from_json_text,
    .parse_location = (parse_location_fn_t) NULL,
    .to_short_text = (to_short_text_fn_t) stacktrace_to_short_text,
    .to_json = (to_json_fn_t) sr_core_stacktrace_to_json,
    .from_json = (from_json_fn_t) sr_core_stacktrace_from_json,
    .get_reason = (get_reason_fn_t) sr_core_stacktrace_get_reason,
    .find_crash_thread =
        (find_crash_thread_fn_t) sr_core_stacktrace_find_crash_thread,
    .threads = (threads_fn_t) core_threads,
    .set_threads = (set_threads_fn_t) core_set_threads,
    .stacktrace_free = (stacktrace_free_fn_t) sr_core_stacktrace_free,
    .stacktrace_append_bthash_text =
        (stacktrace_append_bthash_text_fn_t) core_append_bthash_text,
};

/* Public functions */

struct sr_core_stacktrace *
sr_core_stacktrace_new()
{
    struct sr_core_stacktrace *stacktrace =
        sr_malloc(sizeof(struct sr_core_stacktrace));

    sr_core_stacktrace_init(stacktrace);
    return stacktrace;
}

void
sr_core_stacktrace_init(struct sr_core_stacktrace *stacktrace)
{
    stacktrace->signal = 0;
    stacktrace->executable = NULL;
    stacktrace->crash_thread = NULL;
    stacktrace->threads = NULL;
    stacktrace->type = SR_REPORT_CORE;
}

void
sr_core_stacktrace_free(struct sr_core_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->threads)
    {
        struct sr_core_thread *thread = stacktrace->threads;
        stacktrace->threads = thread->next;
        sr_core_thread_free(thread);
    }

    free(stacktrace);
}

struct sr_core_stacktrace *
sr_core_stacktrace_dup(struct sr_core_stacktrace *stacktrace)
{
    struct sr_core_stacktrace *result = sr_core_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_core_stacktrace));

    if (stacktrace->threads)
        result->threads = sr_core_thread_dup(stacktrace->threads, true);

    return result;
}

int
sr_core_stacktrace_get_thread_count(struct sr_core_stacktrace *stacktrace)
{
    struct sr_core_thread *thread = stacktrace->threads;
    int count = 0;
    while (thread)
    {
        thread = thread->next;
        ++count;
    }

    return count;
}

struct sr_core_thread *
sr_core_stacktrace_find_crash_thread(struct sr_core_stacktrace *stacktrace)
{
    /* If the crash thread is already known return it immediately */
    if (stacktrace->crash_thread)
        return stacktrace->crash_thread;

    /* If there is no thread, be silent and report NULL. */
    if (!stacktrace->threads)
        return NULL;

    /* If there is just one thread, it is simple. */
    if (!stacktrace->threads->next)
        return stacktrace->threads;

    struct sr_core_thread *thread = stacktrace->threads;
    while (thread)
    {
        if (sr_core_thread_find_exit_frame(thread))
            return thread;

        thread = thread->next;
    }

    return thread;
}

struct sr_core_stacktrace *
sr_core_stacktrace_from_json(struct sr_json_value *root,
                             char **error_message)
{
    if (!JSON_CHECK_TYPE(root, SR_JSON_OBJECT, "stacktrace"))
        return NULL;

    struct sr_core_stacktrace *result = sr_core_stacktrace_new();

    bool success =
        JSON_READ_UINT16(root, "signal", &result->signal) &&
        JSON_READ_STRING(root, "executable", &result->executable);

    if (!success)
        goto fail;

    /* Read threads. */
    struct sr_json_value *stacktrace = json_element(root, "stacktrace");
    if (stacktrace)
    {
        if (!JSON_CHECK_TYPE(stacktrace, SR_JSON_ARRAY, "stacktrace"))
            goto fail;

        struct sr_json_value *json_thread;
        FOR_JSON_ARRAY(stacktrace, json_thread)
        {
            struct sr_core_thread *thread = sr_core_thread_from_json(json_thread,
                    error_message);

            if (!thread)
                goto fail;

            struct sr_json_value *crash_thread = json_element(json_thread, "crash_thread");
            if (crash_thread)
            {
                if (!JSON_CHECK_TYPE(crash_thread, SR_JSON_BOOLEAN, "crash_thread"))
                {
                    sr_core_thread_free(thread);
                    goto fail;
                }

                if (crash_thread->u.boolean)
                    result->crash_thread = thread;
            }

            result->threads = sr_core_thread_append(result->threads, thread);
        }
    }

    return result;

fail:
    sr_core_stacktrace_free(result);
    return NULL;
}

struct sr_core_stacktrace *
sr_core_stacktrace_from_json_text(const char *text,
                                  char **error_message)
{
    struct sr_json_value *json_root = sr_json_parse(text, error_message);
    if (!json_root)
        return NULL;

    struct sr_core_stacktrace *stacktrace =
        sr_core_stacktrace_from_json(json_root, error_message);

    sr_json_value_free(json_root);
    return stacktrace;
}

char *
sr_core_stacktrace_to_json(struct sr_core_stacktrace *stacktrace)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();
    sr_strbuf_append_strf(strbuf,
                          "{   \"signal\": %"PRIu8"\n",
                          stacktrace->signal);

    if (stacktrace->executable)
    {
        sr_strbuf_append_str(strbuf, ",   \"executable\": ");
        sr_json_append_escaped(strbuf, stacktrace->executable);
        sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_str(strbuf, ",   \"stacktrace\":\n");

    struct sr_core_thread *thread = stacktrace->threads;
    while (thread)
    {
        if (thread == stacktrace->threads)
            sr_strbuf_append_str(strbuf, "      [ ");
        else
            sr_strbuf_append_str(strbuf, "      , ");

        bool crash_thread = (thread == stacktrace->crash_thread);
        /* If we don't know the crash thread, just take the first one. */
        crash_thread |= (stacktrace->crash_thread == NULL
                         && thread == stacktrace->threads);

        char *thread_json = sr_core_thread_to_json(thread, crash_thread);
        char *indented_thread_json = sr_indent_except_first_line(thread_json, 8);

        sr_strbuf_append_str(strbuf, indented_thread_json);
        free(indented_thread_json);
        free(thread_json);
        thread = thread->next;
        if (thread)
            sr_strbuf_append_str(strbuf, "\n");
    }

    sr_strbuf_append_str(strbuf, " ]\n");
    sr_strbuf_append_char(strbuf, '}');
    return sr_strbuf_free_nobuf(strbuf);
}

struct sr_core_stacktrace *
sr_core_stacktrace_create(const char *gdb_stacktrace_text,
                          const char *unstrip_text,
                          const char *executable_path)
{
    // Parse the GDB stacktrace.
    struct sr_location location;
    sr_location_init(&location);

    struct sr_gdb_stacktrace *gdb_stacktrace =
        sr_gdb_stacktrace_parse(&gdb_stacktrace_text, &location);

    if (!gdb_stacktrace)
    {
        warn("Unable to parse stacktrace: %d:%d: %s\n",
             location.line, location.column, location.message);

        return NULL;
    }

    // Parse the unstrip output.
    struct sr_unstrip_entry *unstrip = sr_unstrip_parse(unstrip_text);
    if (!unstrip)
    {
        warn("Unable to parse unstrip output.");

        sr_gdb_stacktrace_free(gdb_stacktrace);
        return NULL;
    }

    // Create the core stacktrace
    struct sr_core_stacktrace *core_stacktrace =
        sr_core_stacktrace_new();

    struct sr_gdb_thread *gdb_thread = gdb_stacktrace->threads;
    while (gdb_thread)
    {
        struct sr_core_thread *core_thread = sr_core_thread_new();

        struct sr_gdb_frame *gdb_frame = gdb_thread->frames;
        while (gdb_frame)
        {
            gdb_frame = gdb_frame->next;

            struct sr_core_frame *core_frame = sr_core_frame_new();
            core_frame->address = gdb_frame->address;

            struct sr_unstrip_entry *unstrip_entry =
                sr_unstrip_find_address(unstrip, gdb_frame->address);

            if (unstrip_entry)
            {
                core_frame->build_id = sr_strdup(unstrip_entry->build_id);
                core_frame->build_id_offset = core_frame->address - unstrip_entry->start;
                core_frame->file_name = sr_strdup(unstrip_entry->file_name);
            }

            if (gdb_frame->function_name &&
                0 != strcmp(gdb_frame->function_name, "??"))
            {
                core_frame->function_name =
                    sr_strdup(gdb_frame->function_name);
            }

            core_thread->frames = sr_core_frame_append(core_thread->frames, core_frame);
        }

        core_stacktrace->threads =
            sr_core_thread_append(core_stacktrace->threads,
                                  core_thread);

        gdb_thread = gdb_thread->next;
    }

    sr_unstrip_free(unstrip);
    sr_gdb_stacktrace_free(gdb_stacktrace);
    return core_stacktrace;
}

char *
sr_core_stacktrace_get_reason(struct sr_core_stacktrace *stacktrace)
{
    char *prog = stacktrace->executable ? stacktrace->executable : "<unknown>";

    return sr_asprintf("Program %s was terminated by signal %"PRIu16, prog, stacktrace->signal);
}

static void
core_append_bthash_text(struct sr_core_stacktrace *stacktrace, enum sr_bthash_flags flags,
                        struct sr_strbuf *strbuf)
{
    sr_strbuf_append_strf(strbuf, "Executable: %s\n", OR_UNKNOWN(stacktrace->executable));
    sr_strbuf_append_strf(strbuf, "Signal: %"PRIu16"\n", stacktrace->signal);
    sr_strbuf_append_char(strbuf, '\n');
}
