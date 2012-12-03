/*
    btparser.c - stacktrace parsing tool

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
#include "lib/gdb_stacktrace.h"
#include "lib/gdb_thread.h"
#include "lib/gdb_frame.h"
#include "lib/utils.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/metrics.h"
#include "lib/cluster.h"
#include "lib/normalize.h"
#include "lib/report.h"
#include "lib/config.h"
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
    puts("   abrt-dir-to-report  Create report from an ABRT directory");
    puts("   report-abrt-dir     Create report from an ABRT directory and send it to a server");
    puts("   debug               Commands for debugging and development support.");
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
    printf("Usage: %s abrt-dir-to-report DIR [FILE] [OPTION...]\n", g_program_name);
    printf("Usage: %s report-abrt-dir DIR URL [OPTION...]\n", g_program_name);
    printf("Usage: %s debug COMMAND [OPTION...]\n", g_program_name);
}

static void
version()
{
    printf("Satyr version %s\n", PACKAGE_VERSION);
}

static void
abrt_dir_to_report(int argc, char **argv)
{
    char *error_message;
    struct btp_report *report = btp_report_from_abrt_dir(argv[0],
                                                         &error_message);

    if (!report)
    {
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }

    char *json = btp_report_to_json(report);
    free(report);
    puts(json);
    free(json);
}

static void
report_abrt_dir(int argc, char **argv)
{
    char *error_message;
    struct btp_report *report = btp_report_from_abrt_dir(argv[0],
                                                         &error_message);

    if (!report)
    {
        fputs(error_message, stderr);
        exit(1);
    }

    char *json = btp_report_to_json(report);
    free(report);
    free(json);
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
    char *text = btp_file_to_string(argv[0], &error_message);
    if (!text)
    {
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }

    struct btp_gdb_thread *thread = btp_gdb_thread_new();

    char *cur = text;

    /* Parse the text. */
    while (*cur)
    {
        struct btp_gdb_frame *frame = btp_gdb_frame_new();

        /* SYMBOL */
        /* may contain spaces! RHBZ #857475 */
        /* if ( character is found, we do not stop on white space, but on ) */
        /* parentheses may be nested, we need to consider the depth */
        btp_skip_whitespace(cur);
        char *end;
        for (end = cur; *end && *end != ' '; ++end)
        {
        }

        frame->function_name = btp_strndup(cur, end - cur);
        cur = end;
        btp_skip_whitespace(cur);

        for (end = cur; *end && *end != '\n'; ++end)
        {
        }

        frame->library_name = btp_strndup(cur, end - cur);
        cur = end;
        btp_skip_whitespace(cur);

        for (end = cur; *end && *end != '\n'; ++end)
        {
        }

        frame->source_file = btp_strndup(cur, end - cur);

        /* Skip the rest of the line. */
        while (*cur && *cur++ != '\n')
        {
        }

        thread->frames = btp_gdb_frame_append(thread->frames,
                                              frame);
    }

    btp_normalize_gdb_thread(thread);

    struct btp_strbuf *strbuf = btp_strbuf_new();
    btp_gdb_thread_append_to_str(thread, strbuf, false);
    puts(strbuf->buf);
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
    else if (0 == strcmp(argv[1], "abrt-dir-to-report"))
        abrt_dir_to_report(argc - 2, argv + 2);
    else if (0 == strcmp(argv[1], "report-abrt-dir"))
        report_abrt_dir(argc - 2, argv + 2);
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
