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
#include <glib.h>

/* Method table */

static void
core_append_bthash_text(struct sr_core_stacktrace *stacktrace, enum sr_bthash_flags flags,
                        GString *strbuf);

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
    struct sr_core_stacktrace *stacktrace = g_malloc(sizeof(*stacktrace));

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
    stacktrace->only_crash_thread = false;
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

    if (stacktrace->executable)
        free(stacktrace->executable);

    free(stacktrace);
}

struct sr_core_stacktrace *
sr_core_stacktrace_dup(struct sr_core_stacktrace *stacktrace)
{
    struct sr_core_stacktrace *result = sr_core_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct sr_core_stacktrace));

    if (stacktrace->threads)
        result->threads = sr_core_thread_dup(stacktrace->threads, true);
    if (stacktrace->executable)
        result->executable = sr_strdup(stacktrace->executable);

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
sr_core_stacktrace_from_json(json_object *root,
                             char **error_message)
{
    if (!json_check_type(root, json_type_object, "stacktrace", error_message))
        return NULL;

    struct sr_core_stacktrace *result = sr_core_stacktrace_new();

    bool success =
        JSON_READ_UINT16(root, "signal", &result->signal) &&
        JSON_READ_STRING(root, "executable", &result->executable) &&
        JSON_READ_BOOL(root, "only_crash_thread", &result->only_crash_thread);

    if (!success)
        goto fail;

    /* Read threads. */
    json_object *stacktrace;
    if (json_object_object_get_ex(root, "stacktrace", &stacktrace))
    {
        size_t array_length;

        if (!json_check_type(stacktrace, json_type_array, "stacktrace", error_message))
            goto fail;

        array_length = json_object_array_length(stacktrace);

        for (size_t i = 0; i < array_length; i++)
        {
            json_object *json_thread;
            struct sr_core_thread *thread;
            json_object *crash_thread;

            json_thread = json_object_array_get_idx(stacktrace, i);
            thread = sr_core_thread_from_json(json_thread, error_message);
            if (!thread)
                goto fail;

            if (json_object_object_get_ex(json_thread, "crash_thread", &crash_thread))
            {
                if (!json_check_type(crash_thread, json_type_boolean, "crash_thread", error_message))
                {
                    sr_core_thread_free(thread);
                    goto fail;
                }

                if (json_object_get_boolean(crash_thread))
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
    enum json_tokener_error error;
    json_object *json_root = json_tokener_parse_verbose(text, &error);
    if (!json_root)
    {
        if (NULL != error_message)
        {
            const char *description;

            description = json_tokener_error_desc(error);

            *error_message = sr_strdup(description);
        }

        return NULL;
    }

    struct sr_core_stacktrace *stacktrace =
        sr_core_stacktrace_from_json(json_root, error_message);

    json_object_put(json_root);
    return stacktrace;
}

char *
sr_core_stacktrace_to_json(struct sr_core_stacktrace *stacktrace)
{
    GString *strbuf = g_string_new(NULL);
    g_string_append_printf(strbuf,
                          "{   \"signal\": %"PRIu16"\n",
                          stacktrace->signal);

    if (stacktrace->executable)
    {
        g_string_append(strbuf, ",   \"executable\": ");
        sr_json_append_escaped(strbuf, stacktrace->executable);
        g_string_append(strbuf, "\n");
    }

    if (stacktrace->only_crash_thread)
        g_string_append(strbuf, ",   \"only_crash_thread\": true\n");

    g_string_append(strbuf, ",   \"stacktrace\":\n");

    struct sr_core_thread *thread = stacktrace->threads;
    while (thread)
    {
        if (thread == stacktrace->threads)
            g_string_append(strbuf, "      [ ");
        else
            g_string_append(strbuf, "      , ");

        bool crash_thread = (thread == stacktrace->crash_thread);
        /* If we don't know the crash thread, just take the first one. */
        crash_thread |= (stacktrace->crash_thread == NULL
                         && thread == stacktrace->threads);

        char *thread_json = sr_core_thread_to_json(thread, crash_thread);
        char *indented_thread_json = sr_indent_except_first_line(thread_json, 8);

        g_string_append(strbuf, indented_thread_json);
        free(indented_thread_json);
        free(thread_json);
        thread = thread->next;
        if (thread)
            g_string_append(strbuf, "\n");
    }

    g_string_append(strbuf, " ]\n");
    g_string_append_c(strbuf, '}');
    return g_string_free(strbuf, FALSE);
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
                        GString *strbuf)
{
    g_string_append_printf(strbuf, "Executable: %s\n", OR_UNKNOWN(stacktrace->executable));
    g_string_append_printf(strbuf, "Signal: %"PRIu16"\n", stacktrace->signal);
    g_string_append_c(strbuf, '\n');
}
