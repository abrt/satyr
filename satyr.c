/*
    satyr.c

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
#include "gdb/stacktrace.h"
#include "gdb/thread.h"
#include "gdb/frame.h"
#include "utils.h"
#include "location.h"
#include "strbuf.h"
#include "cluster.h"
#include "normalize.h"
#include "report.h"
#include "abrt.h"
#include "thread.h"
#include "stacktrace.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sysexits.h>
#include <assert.h>
#include <libgen.h>

static char *g_program_name;

static void
help()
{
    printf("Usage: %s COMMAND [OPTION...]\n", g_program_name);
    printf("%s -- Automatic problem management with anonymous reports\n\n", g_program_name);
    puts("The following commands are available:");
    puts("   abrt-print-report-from-dir   Create report from an ABRT directory");
    puts("   abrt-report-dir              Create report from an ABRT directory and");
    puts("                                send it to a server");
    puts("   abrt-create-core-stacktrace  Create core stacktrace from an ABRT directory");
    puts("   debug                        Commands for debugging and development support");
}

static void
short_usage_and_exit()
{
    printf("Usage: %s COMMAND [OPTION...]\n", g_program_name);
    printf("Try `%s --help' or `%s --usage' for more information.\n", g_program_name, g_program_name);
    exit(1);
}

static void
usage()
{
    printf("Usage: %s --version\n", g_program_name);
    printf("Usage: %s abrt-print-report-from-dir DIR [OPTION...]\n", g_program_name);
    printf("Usage: %s abrt-report-dir DIR URL [OPTION...]\n", g_program_name);
    printf("Usage: %s abrt-create-core-stacktrace DIR [OPTION...]\n", g_program_name);
    printf("Usage: %s debug COMMAND [OPTION...]\n", g_program_name);
}

static void
version()
{
    printf("Satyr version %s\n", PACKAGE_VERSION);
}

static void
abrt_print_report_from_dir(int argc, char **argv)
{
    /* Require ABRT problem directory path. */
    if (argc == 0)
    {
        fprintf(stderr, "Missing ABRT problem directory path.\n");
        short_usage_and_exit();
    }

    char *error_message;
    bool success = sr_abrt_print_report_from_dir(argv[0],
                                                 &error_message);

    if (!success)
    {
        fprintf(stderr, "%s\n", error_message);
        free(error_message);
        exit(1);
    }
}

static void
abrt_report_dir(int argc, char **argv)
{
    fprintf(stderr, "Not implemented.\n");
}

static void
abrt_create_core_stacktrace(int argc, char **argv)
{
    /* Require ABRT problem directory path. */
    if (argc == 0)
    {
        fprintf(stderr, "Missing ABRT problem directory path.\n");
        short_usage_and_exit();
    }

    bool hash_fingerprints = true;
    if (argc > 1 &&
        0 == strcmp(argv[0], "-r"))
    {
        hash_fingerprints = false;
        argc--;
        argv++;
    }


    char *error_message;
    bool success = sr_abrt_create_core_stacktrace(argv[0],
                                                  hash_fingerprints,
                                                  &error_message);

    if (!success)
    {
        fprintf(stderr, "%s\n", error_message);
        free(error_message);
        exit(1);
    }
}

static void
debug_normalize(int argc, char **argv)
{
    /* Debug normalize requires a filename. */
    if (argc == 0)
    {
        fprintf(stderr, "Missing file name.\n");
        short_usage_and_exit();
    }

    char *error_message;
    char *text = sr_file_to_string(argv[0], &error_message);
    if (!text)
    {
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }

    struct sr_gdb_thread *thread = sr_gdb_thread_new();

    char *cur = text;

    /* Parse the text. */
    while (*cur)
    {
        struct sr_gdb_frame *frame = sr_gdb_frame_new();

        /* SYMBOL */
        /* may contain spaces! RHBZ #857475 */
        /* if ( character is found, we do not stop on white space, but on ) */
        /* parentheses may be nested, we need to consider the depth */
        sr_skip_whitespace(cur);
        char *end;
        for (end = cur; *end && *end != ' '; ++end)
        {
        }

        frame->function_name = sr_strndup(cur, end - cur);
        cur = end;
        sr_skip_whitespace(cur);

        for (end = cur; *end && *end != '\n'; ++end)
        {
        }

        frame->library_name = sr_strndup(cur, end - cur);
        cur = end;
        sr_skip_whitespace(cur);

        for (end = cur; *end && *end != '\n'; ++end)
        {
        }

        frame->source_file = sr_strndup(cur, end - cur);

        /* Skip the rest of the line. */
        while (*cur && *cur++ != '\n')
        {
        }

        thread->frames = sr_gdb_frame_append(thread->frames,
                                              frame);
    }

    sr_normalize_gdb_thread(thread);

    struct sr_strbuf *strbuf = sr_strbuf_new();
    sr_gdb_thread_append_to_str(thread, strbuf, false);
    puts(strbuf->buf);
}

static void
debug_duphash(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s debug duphash TYPE FILE [COMPONENT]\n",
                g_program_name);
        short_usage_and_exit();
    }

    enum sr_report_type type = sr_report_type_from_string(argv[0]);

    if (type == SR_REPORT_INVALID)
    {
        fprintf(stderr, "Invalid report type %s\n", argv[0]);
        exit(1);
    }

    char *error_message;
    char *text = sr_file_to_string(argv[1], &error_message);
    if (!text)
    {
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }

    struct sr_stacktrace *stacktrace = sr_stacktrace_parse(type, text,
                                                           &error_message);
    if (!stacktrace)
    {
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }

    struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
    if (!thread)
    {
        fprintf(stderr, "Cannot find crash thread\n");
        exit(1);
    }

    char *component = argc >= 3 ? argv[2] : NULL;

    char *duphash = sr_thread_get_duphash(thread, 3, component,
                                          SR_DUPHASH_NOHASH);
    if (duphash)
        printf("Cleartext:\n%s\n", duphash);
    else
    {
        fprintf(stderr, "Computing duphash failed\n");
        exit(1);
    }

    free(duphash);
    duphash = sr_thread_get_duphash(thread, 3, component,
                                    SR_DUPHASH_NORMAL);
    if (duphash)
        printf("Hash: %s\n", duphash);
    else
    {
        fprintf(stderr, "Computing duphash failed\n");
        exit(1);
    }
}

static void
debug(int argc, char **argv)
{
    /* Debug command requires a subcommand. */
    if (argc == 0)
    {
        fprintf(stderr, "Missing debug command.\n");
        short_usage_and_exit();
    }

    if (0 == strcmp(argv[0], "normalize"))
        debug_normalize(argc - 1, argv + 1);
    else if (0 == strcmp(argv[0], "duphash"))
        debug_duphash(argc - 1, argv + 1);
    else
    {
        fprintf(stderr, "Unknown debug subcommand.\n");
        short_usage_and_exit();
        exit(1);
    }
}

int
main(int argc, char **argv)
{
    g_program_name = basename(argv[0]);

    if (argc == 1)
        short_usage_and_exit();

    if (0 == strcmp(argv[1], "--help"))
        help();
    else if (0 == strcmp(argv[1], "--usage"))
        usage();
    else if (0 == strcmp(argv[1], "--version"))
        version();
    else if (0 == strcmp(argv[1], "abrt-print-report-from-dir"))
        abrt_print_report_from_dir(argc - 2, argv + 2);
    else if (0 == strcmp(argv[1], "abrt-report-dir"))
        abrt_report_dir(argc - 2, argv + 2);
    else if (0 == strcmp(argv[1], "abrt-create-core-stacktrace"))
        abrt_create_core_stacktrace(argc - 2, argv + 2);
    else if (0 == strcmp(argv[1], "debug"))
        debug(argc - 2, argv + 2);
    else
    {
        fprintf(stderr, "Unknown command.\n");
        short_usage_and_exit();
        return 1;
    }

    return 0;
}
