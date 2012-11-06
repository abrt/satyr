/*
    core-backtrace-python.c

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
#include "core-backtrace-python.h"

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

GList *
btp_parse_python_backtrace(char *text)
{
    GList *result = NULL;

    /* there may be other lines that match the regexp,
       the actual backtrace always has "Traceback" header */
    char *line = strstr(text, "\nTraceback");
    if (!line)
        return NULL;

    /* jump to the first line of the actual backtrace */
    line = strchr(++line, '\n');
    if (!line)
        return NULL;
    ++line;

    /* iterate line by line
       best effort - continue on error */
    char *nextline = NULL, *splitter;
    while (strncmp(line, "  ", strlen("  ")) == 0)
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

        struct backtrace_entry *frame = btp_mallocz(sizeof(struct backtrace_entry));

        /* file name */
        const char *filename = strchr(line, '"');
        if (!filename)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find the '\"' character "
                            "identifying the beginning of file name\n");
            line = nextline;
            continue;
        }
        splitter = strchr(++filename, '"');
        if (!splitter)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find the '\"' character "
                            "identifying the end of file name\n");
            line = nextline;
            continue;
        }
        char old = *splitter;
        *splitter = '\0';
        frame->filename = btp_strdup(filename);
        *splitter = old;

        /* line number */
        char *offsetstr = strstr(++splitter, "line ");
        if (!offsetstr)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find 'line ' pattern "
                            "identifying line number\n");
            line = nextline;
            continue;
        }
        offsetstr += strlen("line ");
        if (sscanf(offsetstr, "%u", &frame->build_id_offset) != 1)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to parse line number\n");
            line = nextline;
            continue;
        }

        /* function name */
        const char *funcname = strstr(splitter, ", in ");
        if (!funcname)
        {
            btp_backtrace_entry_free(frame);
            fprintf(stderr, "Unable to find ', in ' pattern "
                            "identifying function name\n");
            line = nextline;
            continue;
        }
        funcname += strlen(", in ");
        frame->symbol = btp_strdup(funcname);

        /* if C callback is called, the function is named */
        /* 'calling callback function'. as quotes are not */
        /* allowed, change it to <calling_callback_function> */
        int last = strlen(frame->symbol) - 1;
        if (frame->symbol[0] == '\'' && frame->symbol[last] == '\'')
        {
            frame->symbol[0] = '<';
            frame->symbol[last] = '>';
            int i;
            for (i = 1; i < last; ++i)
                if (frame->symbol[i] == ' ')
                    frame->symbol[i] = '_';
        }

        /* build-id */
        frame->build_id = btp_mallocz(BTP_SHA1_RESULT_LEN);
        if (!btp_hash_file(frame->build_id, frame->filename))
        {
            fprintf(stderr, "hash_text_file failed for '%s'\n", frame->filename);
            free(frame->build_id);
            frame->build_id = NULL;
        }

        /* need prepend to flip the backtrace upside-down */
        /* the last frame should be on top */
        result = g_list_prepend(result, frame);
        line = nextline;
    }

    if (nextline)
        *(nextline - 1) = '\n';

    return result;
}
