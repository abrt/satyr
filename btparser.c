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

static void
help(char *prog)
{
    printf("Usage: %s COMMAND [OPTION...]\n", prog);
    printf("%s -- Automatic problem management with anonymous reports\n\n", prog);
    puts("Currently, a single command is available:");
    puts("   abrt-dir-to-report  Create report from an ABRT directory");
    puts("   report-abrt-dir     Create report from an ABRT directory and send it to a server");
}

static void
usage(char *prog)
{
    printf("Usage: %s --version\n", prog);
    printf("Usage: %s abrt-dir-to-report DIR [FILE] [OPTION...]\n", prog);
    printf("Usage: %s report-abrt-dir DIR URL [OPTION...]\n", prog);
}

static void
version(char *prog)
{
    printf("btparser version %s\n", PACKAGE_VERSION);
}

static void
abrt_dir_to_report(int argc, char **argv)
{
    char *error_message;
    struct btp_report *report = btp_report_from_abrt_dir(argv[0], &error_message);
    if (!report)
    {
        fputs(error_message, stderr);
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
    struct btp_report *report = btp_report_from_abrt_dir(argv[0], &error_message);
    if (!report)
    {
        fputs(error_message, stderr);
        exit(1);
    }

    char *json = btp_report_to_json(report);
    free(report);


    free(json);
}

int
main(int argc, char **argv)
{
    char *prog = basename(argv[0]);

    if (argc == 1)
    {
        printf("Usage: %s COMMAND [OPTION...]\n", prog);
        printf("Try `%s --help' or `%s --usage' for more information.\n", prog, prog);
        exit(1);
    }

    if (0 == strcmp(argv[1], "--help"))
        help(prog);

    if (0 == strcmp(argv[1], "--usage"))
        usage(prog);

    if (0 == strcmp(argv[1], "--version"))
        version(prog);

    if (0 == strcmp(argv[1], "abrt-dir-to-report"))
        abrt_dir_to_report(argc - 2, argv + 2);

    if (0 == strcmp(argv[1], "report-abrt-dir"))
        report_abrt_dir(argc - 2, argv + 2);

    return 0;
}
