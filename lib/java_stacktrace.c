/*
    java_stacktrace.c

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
#include "java_stacktrace.h"
#include "java_thread.h"
#include "java_frame.h"
#include "utils.h"
#include "sha1.h"
#include <stdio.h>
#include <string.h>

struct btp_java_stacktrace *
btp_java_stacktrace_new()
{
    struct btp_java_stacktrace *stacktrace =
        btp_malloc(sizeof(*stacktrace));

    btp_java_stacktrace_init(stacktrace);
    return stacktrace;
}

void
btp_java_stacktrace_init(struct btp_java_stacktrace *stacktrace)
{
    memset(stacktrace, 0, sizeof(*stacktrace));
}

void
btp_java_stacktrace_free(struct btp_java_stacktrace *stacktrace)
{
    if (!stacktrace)
        return;

    while (stacktrace->threads)
    {
        struct btp_java_thread *thread = stacktrace->threads;
        stacktrace->threads = thread->next;
        btp_java_thread_free(thread);
    }

    free(stacktrace);
}

struct btp_java_stacktrace *
btp_java_stacktrace_dup(struct btp_java_stacktrace *stacktrace)
{
    struct btp_java_stacktrace *result = btp_java_stacktrace_new();
    memcpy(result, stacktrace, sizeof(*result));

    if (result->threads)
        result->threads = btp_java_thread_dup(result->threads, true);

    return result;
}


/*
Example input:

----- BEGIN -----
Exception in thread "main" java.lang.NullPointerException
        at SimpleTest.throwNullPointerException(SimpleTest.java:36)
        at SimpleTest.throwAndDontCatchException(SimpleTest.java:70)
        at SimpleTest.main(SimpleTest.java:82)
----- END -----

relevant lines:
*/

struct btp_java_stacktrace *
btp_java_parse_stacktrace(const char **input, struct btp_location *location)
{
    struct btp_java_thread *thread = btp_java_thread_parse(input, location);
    if (thread == NULL)
        return NULL;

    struct btp_java_stacktrace *stacktrace = btp_java_stacktrace_new();
    stacktrace->threads = thread;

    return stacktrace;
}
