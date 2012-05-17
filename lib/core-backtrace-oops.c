/*
    core-backtrace-oops.c

    Copyright (C) 2012  ABRT Team
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

#include <stdio.h>
#include "utils.h"
#include "core-backtrace-oops.h"

/*
Example input:

----- BEGIN -----
Call Trace:
[<f88e11c7>] ? radeon_cp_resume+0x7d/0xbc [radeon]
[<f88745f8>] ? drm_ioctl+0x1b0/0x225 [drm]
[<f88e114a>] ? radeon_cp_resume+0x0/0xbc [radeon]
[<c049b1c0>] ? vfs_ioctl+0x50/0x69
[<c049b414>] ? do_vfs_ioctl+0x23b/0x247
[<c0460a56>] ? audit_syscall_entry+0xf9/0x123
[<c049b460>] ? sys_ioctl+0x40/0x5c
[<c0403c76>] ? syscall_call+0x7/0xb
----- END -----

regexp would (probably) be
"^ *\[<([0-9a-f]+)>\]( \?)? ([a-zA-Z0-9_]+)\+(0x[0-9a-f]+)/(0x[0-9a-f]+)( \[([a-zA-Z0-9_]+)\])?$"
\1 = address; \2 = ? flag; \3 = function name; \4 = offset; \5 = function length; \7 = module

A different one:

----- BEGIN -----
WARNING: at drivers/video/omap2/dss/dispc.c:570 dss_driver_probe+0x44/0xd8()
Modules linked in:
[<c0014210>] (unwind_backtrace+0x0/0xf8) from [<c004c7bc>] (warn_slowpath_common+0x4c/0x64)
[<c004c7bc>] (warn_slowpath_common+0x4c/0x64) from [<c004c7f0>] (warn_slowpath_null+0x1c/0x24)
[<c004c7f0>] (warn_slowpath_null+0x1c/0x24) from [<c024c208>] (dss_driver_probe+0x44/0xd8)
[<c024c208>] (dss_driver_probe+0x44/0xd8) from [<c0294520>] (driver_probe_device+0x90/0x19c)
[<c0294520>] (driver_probe_device+0x90/0x19c) from [<c02946b8>] (__driver_attach+0x8c/0x90)
[<c02946b8>] (__driver_attach+0x8c/0x90) from [<c0293788>] (bus_for_each_dev+0x5c/0x88)
[<c0293788>] (bus_for_each_dev+0x5c/0x88) from [<c0293ef4>] (bus_add_driver+0x180/0x248)
[<c0293ef4>] (bus_add_driver+0x180/0x248) from [<c0294b64>] (driver_register+0x78/0x13c)
[<c0294b64>] (driver_register+0x78/0x13c) from [<c0008858>] (do_one_initcall+0x34/0x174)
[<c0008858>] (do_one_initcall+0x34/0x174) from [<c06c48bc>] (kernel_init+0xb8/0x15c)
[<c06c48bc>] (kernel_init+0xb8/0x15c) from [<c000e6cc>] (kernel_thread_exit+0x0/0x8)
----- END -----
*/

GList *
btp_parse_kerneloops(char *text, const char *kernelver)
{
    GList *result = NULL;

    char *nextline = NULL, *line = text;
    while (line)
    {
        /* \n was replaced by \0 in the previous round */
        if (nextline)
            *(nextline - 1) = '\n';

        nextline = strchr(line, '\n');
        if (nextline)
        {
            *nextline = '\0';
            ++nextline;
        }

        /* timestamp [123456.654321] may be present in the oops, skip it */
        line = strchr(line, '[');
        if (!line)
        {
            line = nextline;
            continue;
        }

        if (isdigit(line[1]))
        {
            line = strchr(line + 1, '[');
            if (!line || line[1] != '<')
            {
                line = nextline;
                continue;
            }
        }

        struct backtrace_entry *frame = btp_mallocz(sizeof(struct backtrace_entry));

        /* address */
        if (sscanf(line, "[<%llx>]", &frame->address) != 1)
        {
            btp_backtrace_entry_free(frame);
            /* not printing the error - not match means just skip line */
            /* fprintf(stderr, "Unable to read address from pattern [<addr>]\n"); */
            line = nextline;
            continue;
        }

        /* function name */
        char *funcname = strchr(line, ' ');
        if (!funcname)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find functon name: '%s'\n", line);
            line = nextline;
            continue;
        }
        ++funcname;

        while (*funcname && !isalpha(*funcname))
        {
            /* threre is no correct place for the '?' flag in
             * struct backtrace_entry. took function_initial_loc
             * because it was unused */
            if (*funcname == '?')
                frame->function_initial_loc = -1;
            ++funcname;
        }

        if (!*funcname)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find functon name: '%s'\n", line);
            line = nextline;
            continue;
        }

        char *splitter = strchr(funcname, '+');
        if (!splitter)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find offset & function length: '%s'\n", line);
            line = nextline;
            continue;
        }
        *splitter = '\0';
        frame->symbol = btp_strdup(funcname);
        *splitter = '+';
        ++splitter;

        /* offset, function legth */
        if (sscanf(splitter, "0x%x/0x%x", &frame->build_id_offset, &frame->function_length) != 2)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to read offset & function length: '%s'\n", line);
            line = nextline;
            continue;
        }

        /* module */
        /* in the 2nd example mentioned above, [] also matches
         * the address part of line's 2nd half (callback). */
        char *callback = strstr(splitter, "from");
        char *module = strchr(splitter, '[');
        if (module && (!callback || callback > module))
        {
            ++module;
            splitter = strchr(module, ']');
            if (splitter)
            {
                *splitter = '\0';
                frame->filename = btp_strdup(module);
                *splitter = ']';
            }
        }

        if (!frame->filename)
            frame->filename = btp_strdup("vmlinux");

        /* build-id */
        /* there is no actual "build-id" for kernel */
        if (!frame->build_id && kernelver)
            frame->build_id = btp_strdup(kernelver);

        result = g_list_append(result, frame);
        line = nextline;
    }

    /* paranoia - should never happen */
    if (nextline)
        *(nextline - 1) = '\n';

    return result;
}

GList *
btp_kerneloops_drop_unreliable(GList *kerneloops)
{
    GList *start = kerneloops, *next = kerneloops;
    struct backtrace_entry *frame;
    while (next)
    {
        frame = (struct backtrace_entry *)next->data;
        /* function_initial_loc != 0 means '?' flag was present in the oops */
        if (frame->function_initial_loc)
        {
            start = g_list_remove(start, frame);
            next = start;
            btp_backtrace_entry_free(frame);
            continue;
        }

        next = g_list_next(next);
    }

    return start;
}
