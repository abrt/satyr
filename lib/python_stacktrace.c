/*
    python_stacktrace.c

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
#include "python_stacktrace.h"
#include "python_frame.h"
#include "core_stacktrace.h"
#include "core_thread.h"
#include "core_frame.h"
#include "utils.h"
#include "sha1.h"
#include <stdio.h>
#include <string.h>

struct btp_python_stacktrace *
btp_python_stacktrace_new()
{
    struct btp_python_stacktrace *stacktrace =
        btp_malloc(sizeof(struct btp_python_stacktrace));

    btp_python_stacktrace_init(stacktrace);
    return stacktrace;
}

void
btp_python_stacktrace_init(struct btp_python_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(struct btp_python_stacktrace));
}

void
btp_python_stacktrace_free(struct btp_python_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    free(stacktrace);
}

struct btp_python_stacktrace *
btp_python_stacktrace_dup(struct btp_python_stacktrace *stacktrace)
{
    struct btp_python_stacktrace *result = btp_python_stacktrace_new();
    memcpy(result, stacktrace, sizeof(struct btp_python_stacktrace));

    if (result->file_name)
        result->file_name = btp_strdup(result->file_name);

    if (result->exception_name)
        result->exception_name = btp_strdup(result->exception_name);

    if (result->frames)
        result->frames = btp_python_frame_dup(result->frames, true);

    return result;
}


/*
Example input:

----- BEGIN -----
__init__.py:49:execute:OperationalError: no such table: fedora_koji_rpm_provides

Traceback (most recent call last):
  File "/usr/bin/faf-refreshrepo", line 24, in <module>
    repo.load(pool)
  File "/usr/lib/python2.7/site-packages/pyfaf/libsolv.py", line 58, in load
    return self.load_if_changed()
  File "/usr/lib/python2.7/site-packages/pyfaf/libsolv.py", line 490, in load_if_changed
    self.db.execute("SELECT COUNT(*) FROM {0}_provides".format(self.name))
  File "/usr/lib/python2.7/site-packages/pyfaf/cache/__init__.py", line 181, in execute
    self._cursor.execute(sql, parameters)
  File "/usr/lib/python2.7/site-packages/pyfaf/cache/__init__.py", line 49, in execute
    self.cursor.execute(sql, parameters)
OperationalError: no such table: fedora_koji_rpm_provides

Local variables in innermost frame:
self: <pyfaf.cache.Cursor instance at 0x26f41b8>
parameters: []
sql: 'SELECT COUNT(*) FROM fedora_koji_rpm_provides'
----- END -----

relevant lines:
  File "/usr/bin/faf-refreshrepo", line 24, in <module>
  File "/usr/lib/python2.7/site-packages/pyfaf/libsolv.py", line 58, in load
  File "/usr/lib/python2.7/site-packages/pyfaf/libsolv.py", line 490, in load_if_changed
  File "/usr/lib/python2.7/site-packages/pyfaf/cache/__init__.py", line 181, in execute
  File "/usr/lib/python2.7/site-packages/pyfaf/cache/__init__.py", line 49, in execute

regexp would be "^  File \"([^\"]+)\", line ([0-9]+), in (.*)$"
\1 = file name; \2 = offset; \3 = function name
*/

struct btp_core_stacktrace *
btp_core_python_parse_stacktrace(const char *text)
{
    /* there may be other lines that match the regexp,
       the actual stacktrace always has "Traceback" header */
    char *line = strstr(text, "\nTraceback");
    if (!line)
        return NULL;

    /* jump to the first line of the actual stacktrace */
    line = strchr(++line, '\n');
    if (!line)
        return NULL;

    ++line;

    struct btp_core_stacktrace *stacktrace = btp_core_stacktrace_new();
    stacktrace->threads = btp_core_thread_new();

    /* iterate line by line
       best effort - continue on error */
    char *nextline = NULL, *splitter;
    while (0 == strncmp(line, "  ", strlen("  ")))
    {
        /* '\n' was replaced by '\0' in the previous round */
        if (nextline)
            *(nextline - 1) = '\n';

        /* split lines */
        nextline = strchr(line, '\n');
        if (!nextline)
            break;
        *nextline = '\0';
        ++nextline;

        if (strncmp(line, "  File \"", strlen("  File \"")) != 0)
        {
            line = nextline;
            continue;
        }

        struct btp_core_frame *frame = btp_core_frame_new();

        /* file name */
        const char *filename = strchr(line, '"');
        if (!filename)
        {
            btp_core_frame_free(frame);
            fprintf(stderr, "Unable to find the '\"' character "
                            "identifying the beginning of file name\n");
            line = nextline;
            continue;
        }

        splitter = strchr(++filename, '"');
        if (!splitter)
        {
            btp_core_frame_free(frame);
            fprintf(stderr, "Unable to find the '\"' character "
                            "identifying the end of file name\n");
            line = nextline;
            continue;
        }

        char old = *splitter;
        *splitter = '\0';
        frame->file_name = btp_strdup(filename);
        *splitter = old;

        /* line number */
        char *offsetstr = strstr(++splitter, "line ");
        if (!offsetstr)
        {
            btp_core_frame_free(frame);
            fprintf(stderr, "Unable to find 'line ' pattern "
                            "identifying line number\n");
            line = nextline;
            continue;
        }
        offsetstr += strlen("line ");
        if (sscanf(offsetstr, "%"PRIu64, &frame->build_id_offset) != 1)
        {
            btp_core_frame_free(frame);
            fprintf(stderr, "Unable to parse line number\n");
            line = nextline;
            continue;
        }

        /* function name */
        const char *funcname = strstr(splitter, ", in ");
        if (!funcname)
        {
            btp_core_frame_free(frame);
            fprintf(stderr, "Unable to find ', in ' pattern "
                            "identifying function name\n");
            line = nextline;
            continue;
        }
        funcname += strlen(", in ");
        frame->function_name = btp_strdup(funcname);

        stacktrace->threads->frames = btp_core_frame_append(stacktrace->threads->frames,
                                                           frame);
        line = nextline;
    }

    if (nextline)
        *(nextline - 1) = '\n';

    return stacktrace;
}
