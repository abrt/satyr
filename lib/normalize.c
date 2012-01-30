/*
    normalize.c

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
#include "normalize.h"
#include "frame.h"
#include "thread.h"
#include "backtrace.h"
#include "utils.h"
#include <string.h>

void
btp_normalize_thread(struct btp_thread *thread)
{
    btp_normalize_dbus_thread(thread);
    btp_normalize_gdk_thread(thread);
    btp_normalize_glib_thread(thread);
    btp_normalize_glibc_thread(thread);
    btp_normalize_libstdcpp_thread(thread);
    btp_normalize_linux_thread(thread);
    btp_normalize_xorg_thread(thread);

    /* If the first frame has address 0x0000 and its name is '??', it
     * is a dereferenced null, and we remove it. This frame is not
     * really invalid, and it affects backtrace quality rating. See
     * Red Hat Bugzilla bug #639038.
     * @code
     * #0  0x0000000000000000 in ?? ()
     * No symbol table info available.
     * #1  0x0000000000422648 in main (argc=1, argv=0x7fffa57cf0d8) at totem.c:242
     *       error = 0x0
     *       totem = 0xdee070 [TotemObject]
     * @endcode
     */
    if (thread->frames &&
        thread->frames->address == 0x0000 &&
        thread->frames->function_name &&
        0 == strcmp(thread->frames->function_name, "??"))
    {
        btp_thread_remove_frame(thread, thread->frames);
    }

    /* If the last frame has address 0x0000 and its name is '??',
     * remove it. This frame is not really invalid, and it affects
     * backtrace quality rating. See Red Hat Bugzilla bug #592523.
     * @code
     * #2  0x00007f4dcebbd62d in clone ()
     * at ../sysdeps/unix/sysv/linux/x86_64/clone.S:112
     * No locals.
     * #3  0x0000000000000000 in ?? ()
     * @endcode
     */
    struct btp_frame *last = thread->frames;
    while (last && last->next)
        last = last->next;
    if (last &&
        last->address == 0x0000 &&
        last->function_name &&
        0 == strcmp(last->function_name, "??"))
    {
        btp_thread_remove_frame(thread, last);
    }

    /* Merge recursively called functions into single frame */
    struct btp_frame *curr_frame = thread->frames;
    struct btp_frame *prev_frame = NULL;
    while(curr_frame)
    {
        if(prev_frame && 0 != btp_strcmp0(prev_frame->function_name, "??") &&
           0 == btp_strcmp0(prev_frame->function_name, curr_frame->function_name))
        {
            prev_frame->next = curr_frame->next;
            btp_frame_free(curr_frame);
            curr_frame = prev_frame->next;
            continue;
        }
        prev_frame = curr_frame;
        curr_frame = curr_frame->next;
    }
}

void
btp_normalize_backtrace(struct btp_backtrace *backtrace)
{
    struct btp_thread *thread = backtrace->threads;
    while (thread)
    {
        btp_normalize_thread(thread);
        thread = thread->next;
    }
}

static bool
next_functions_similar(struct btp_frame *frame1, struct btp_frame *frame2)
{
    if ((!frame1->next && frame2->next) ||
        (frame1->next && !frame2->next) ||
        (frame1->next && frame2->next &&
         (0 != btp_strcmp0(frame1->next->function_name,
                         frame2->next->function_name) ||
          0 == btp_strcmp0(frame1->next->function_name, "??") ||
         (frame1->next->library_name && frame2->next->library_name &&
          0 != strcmp(frame1->next->library_name, frame2->next->library_name)))))
        return false;
    return true;
}

void
btp_normalize_paired_unknown_function_names(struct btp_thread *thread1, struct btp_thread *thread2)

{
    if (!thread1->frames || !thread2->frames)
    {
        return;
    }

    int i = 0;
    struct btp_frame *curr_frame1 = thread1->frames;
    struct btp_frame *curr_frame2 = thread2->frames;

    if (0 == btp_strcmp0(curr_frame1->function_name, "??") &&
        0 == btp_strcmp0(curr_frame2->function_name, "??") &&
        !(curr_frame1->library_name && curr_frame2->library_name &&
          strcmp(curr_frame1->library_name, curr_frame2->library_name)) &&
        next_functions_similar(curr_frame1, curr_frame2))
    {
        free(curr_frame1->function_name);
        curr_frame1->function_name = btp_asprintf("__unknown_function_%d", i);
        free(curr_frame2->function_name);
        curr_frame2->function_name = btp_asprintf("__unknown_function_%d", i);
        i++;
    }

    struct btp_frame *prev_frame1 = curr_frame1;
    struct btp_frame *prev_frame2 = curr_frame2;
    curr_frame1 = curr_frame1->next;
    curr_frame2 = curr_frame2->next;

    while (curr_frame1)
    {
        if (0 == btp_strcmp0(curr_frame1->function_name, "??"))
        {
            while (curr_frame2)
            {
                if (0 == btp_strcmp0(curr_frame2->function_name, "??") &&
                    !(curr_frame1->library_name && curr_frame2->library_name &&
                      strcmp(curr_frame1->library_name, curr_frame2->library_name)) &&
                    0 != btp_strcmp0(prev_frame2->function_name, "??") &&
                    next_functions_similar(curr_frame1, curr_frame2) &&
                    0 == btp_strcmp0(prev_frame1->function_name,
                                     prev_frame2->function_name) &&
                    !(prev_frame1->library_name && prev_frame2->library_name &&
                      strcmp(prev_frame1->library_name, prev_frame2->library_name)))
                {
                    free(curr_frame1->function_name);
                    curr_frame1->function_name = btp_asprintf("__unknown_function_%d", i);
                    free(curr_frame2->function_name);
                    curr_frame2->function_name = btp_asprintf("__unknown_function_%d", i);
                    i++;
                    break;
                }
                prev_frame2 = curr_frame2;
                curr_frame2 = curr_frame2->next;
            }
        }
        prev_frame1 = curr_frame1;
        curr_frame1 = curr_frame1->next;
        prev_frame2 = thread2->frames;
        curr_frame2 = prev_frame2->next;
    }
}
