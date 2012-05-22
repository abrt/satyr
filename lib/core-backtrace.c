/*
    core-backtrace.c

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
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "strbuf.h"
#include "core-backtrace.h"
#include "utils.h"
#include "thread.h"
#include "normalize.h"
#include "location.h"
#include "backtrace.h"
#include "frame.h"

#define BACKTRACE_TRUNCATE_LENGTH 7
#define OR_UNKNOWN(s) ((s) ? (s) : "-")

void
btp_backtrace_entry_free(struct backtrace_entry *entry)
{
    if (!entry)
        return;

    free(entry->build_id);
    free(entry->symbol);
    free(entry->modname);
    free(entry->filename);
    free(entry->fingerprint);
    free(entry);
}

/* GFunc for g_list_foreach */
static void
btp_core_backtrace_elem_free(void *data, void *user_data)
{
    btp_backtrace_entry_free(data);
}

void
btp_core_backtrace_free(GList *backtrace)
{
    g_list_foreach(backtrace, btp_core_backtrace_elem_free, NULL);
    g_list_free(backtrace);
}

char *
btp_core_backtrace_fmt(GList *backtrace)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();
    struct backtrace_entry *entry;

    while (backtrace != NULL)
    {
        entry = backtrace->data;

        /* BUILD_ID OFFSET SYMBOL MODNAME FINGERPRINT */
        btp_strbuf_append_strf(strbuf, "%s 0x%x %s %s %s\n",
                               OR_UNKNOWN(entry->build_id),
                               entry->build_id_offset,
                               OR_UNKNOWN(entry->symbol),
                               OR_UNKNOWN(entry->filename),
                               OR_UNKNOWN(entry->fingerprint));
        backtrace = g_list_next(backtrace);
    }

    return btp_strbuf_free_nobuf(strbuf);
}

void
btp_backtrace_add_build_id(GList *backtrace, uintmax_t start, uintmax_t length,
                           const char *build_id, unsigned build_id_len,
                           const char *modname, unsigned modname_len,
                           const char *filename, unsigned filename_len)
{
    struct backtrace_entry *entry;

    while (backtrace != NULL)
    {
        entry = backtrace->data;
        if (start <= entry->address
            && entry->address <= start + length)
        {
            /* NOTE: we could get by with just one copy of the string, but that
             * would mean more bookkeeping for us ... */
            entry->build_id = btp_strndup(build_id, build_id_len);
            entry->build_id_offset = entry->address - start;
            entry->modname = btp_strndup(modname, modname_len);
            entry->filename = btp_strndup(filename, filename_len);
        }

        backtrace = g_list_next(backtrace);
    }
}

static void
free_frame_aux(void *user_data)
{
    struct frame_aux *aux = user_data;

    if (!aux)
        return;

    free(aux->build_id);
    free(aux->filename);
    free(aux->fingerprint);
    free(aux);
}

static char *
read_string(const char **inptr)
{
    const char *cur = *inptr;
    const char *str;
    int len;

    cur = btp_skip_whitespace(cur);
    str = cur;
    cur = btp_skip_non_whitespace(cur);

    len = cur-str;
    *inptr = cur;

    if (len == 1 && *str == '-')
        return NULL;

    return btp_strndup(str, len);
}

struct btp_thread *
btp_load_core_backtrace(const char *text)
{
    const char *cur = text;
    int ret;
    int chars_read;
    uintmax_t off;

    struct btp_thread *thread = btp_mallocz(sizeof(*thread));
    struct btp_frame **prev_link = &(thread->frames);

    /* Parse the text. */
    while (*cur)
    {
        struct btp_frame *frame = btp_mallocz(sizeof(*frame));
        btp_frame_init(frame);
        struct frame_aux *aux = btp_mallocz(sizeof(*aux));
        frame->user_data = aux;
        frame->user_data_destructor = free_frame_aux;
        *prev_link = frame;
        prev_link = &(frame->next);

        /* BUILD ID */
        aux->build_id = read_string(&cur);

        /* OFFSET */
        cur = btp_skip_whitespace(cur);
        ret = sscanf(cur, "0x%jx %n", &off, &chars_read);
        if (ret < 1)
        {
            btp_thread_free(thread);
            return NULL;
        }
        cur += chars_read;
        frame->address = (uint64_t)off;

        /* SYMBOL */
        char *symbol = read_string(&cur);
        /* btparser uses "??" to denote unknown function name */
        frame->function_name = (symbol ? symbol : btp_strdup("??"));

        /* FILENAME */
        aux->filename = read_string(&cur);

        /* FINGERPRINT */
        aux->fingerprint = read_string(&cur);

        /* Skip the rest of the line. */
        while (*cur && *cur++ != '\n')
            continue;
    }

    btp_normalize_thread(thread);
    btp_thread_remove_frames_below_n(thread, BACKTRACE_TRUNCATE_LENGTH);

    return thread;
}

void
btp_free_core_backtrace(struct btp_thread *thread)
{
    if (thread)
        btp_thread_free(thread);
}

int
btp_core_backtrace_frame_cmp(struct btp_frame *frame1, struct btp_frame *frame2)
{
    /* If both function names are known, compare them directly. */
    if (frame1->function_name
        && frame2->function_name
        && strcmp(frame1->function_name, "??")
        && strcmp(frame2->function_name, "??"))
        return strcmp(frame1->function_name, frame2->function_name);

    struct frame_aux *aux1 = frame1->user_data;
    struct frame_aux *aux2 = frame2->user_data;

    /* If build ids are equal, we can compare the offsets.
     * Note that this may miss the case where the same function is called from
     * other function in multiple places, which would pass if we were comparing
     * the function names. */
    if (aux1->build_id
        && aux2->build_id
        && !strcmp(aux1->build_id, aux2->build_id))
        return (frame1->address != frame2->address);

    /* Compare the fingerprints if present. */
    if (aux1->fingerprint && aux2->fingerprint)
        return strcmp(aux1->fingerprint, aux2->fingerprint);

    /* No match, assume the functions are different. */
    return -1;
}

void
btp_core_assign_build_ids(GList *backtrace, const char *unstrip_output,
                          const char *executable)
{
    const char *cur = unstrip_output;

    uintmax_t start;
    uintmax_t length;
    const char *build_id;
    unsigned build_id_len;
    const char *modname;
    unsigned modname_len;
    const char *filename;
    unsigned filename_len;

    int ret;
    int chars_read;

    while (*cur)
    {
        /* beginning of the line */

        /* START+SIZE */
        ret = sscanf(cur, "0x%jx+0x%jx %n", &start, &length, &chars_read);
        if (ret < 2)
            goto eat_line;

        cur += chars_read;

        /* BUILDID */
        build_id = cur;
        while (isxdigit(*cur))
            cur++;

        build_id_len = cur-build_id;

        /* there may be @ADDR after the ID */
        cur = btp_skip_non_whitespace(cur);
        cur = btp_skip_whitespace(cur);

        /* FILE */
        filename = cur;
        cur = btp_skip_non_whitespace(cur);
        filename_len = cur-filename;
        cur = btp_skip_whitespace(cur);

        /* DEBUGFILE */
        cur = btp_skip_non_whitespace(cur);
        cur = btp_skip_whitespace(cur);

        /* MODULENAME */
        modname = cur;
        cur = btp_skip_non_whitespace(cur);
        modname_len = cur-modname;

        /* Use real executable file name instead of "-". */
        if (modname_len == 5 && strncmp(modname, "[exe]", 5) == 0)
        {
            filename = executable;
            filename_len = strlen(executable);
        }

        btp_backtrace_add_build_id(backtrace, start, length,
                                   build_id, build_id_len, modname, modname_len,
                                   filename, filename_len);

eat_line:
        while (*cur && *cur++ != '\n')
            continue;
    }
}

GList *
btp_backtrace_extract_addresses(const char *bt)
{
    struct btp_location location;
    btp_location_init(&location);

    struct btp_backtrace *backtrace = btp_backtrace_parse(&bt, &location);
    if (!backtrace)
    {
        if (btp_debug_parser)
            fprintf(stderr, "Unable to parse backtrace: %d:%d: %s\n",
                    location.line, location.column, location.message);
        return NULL;
    }

    struct btp_thread *thread = btp_backtrace_find_crash_thread(backtrace);
    if (!thread)
    {
        if (btp_debug_parser)
            fprintf(stderr, "Unable to find crash thread\n");
        return NULL;
    }

    struct btp_frame *f;
    GList *core_backtrace = NULL;
    struct backtrace_entry *entry;

    for (f = thread->frames; f != NULL; f = f->next)
    {
        entry = btp_mallocz(sizeof(*entry));

        if (f->address != (uint64_t)(-1))
            entry->address = (uintptr_t)f->address;

        if (f->function_name && strcmp(f->function_name, "??") != 0)
            entry->symbol = btp_strdup(f->function_name);

        core_backtrace = g_list_append(core_backtrace, entry);
    }

    btp_backtrace_free(backtrace);
    return core_backtrace;
}
