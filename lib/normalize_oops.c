/*
    normalize_oops.c

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
#include "utils.h"
#include <stdlib.h>
#include <assert.h>

static int ptrstrcmp(const void *s1, const void *s2)
{
    return btp_strcmp0(*(const char**)s1, *(const char**)s2);
}

void
btp_normalize_oops_thread(struct btp_thread *thread)
{
    /* !!! MUST BE SORTED !!! */
    const char *blacklist[] = {
        "do_softirq",
        "do_vfs_ioctl",
        "flush_kthread_worker",
        "gs_change",
        "irq_exit",
        "kernel_thread_helper",
        "kthread",
        "process_one_work",
        "system_call_fastpath",
        "warn_slowpath_common",
        "warn_slowpath_fmt",
        "warn_slowpath_fmt_taint",
        "warn_slowpath_null",
        "worker_thread"
    };

    struct btp_frame *frame = thread->frames;
    while (frame)
    {
        struct btp_frame *next_frame = frame->next;

        /* do not drop frames belonging to a module */
        bool in_module = (btp_strcmp0(frame->library_name, "vmlinux") != 0);
        bool in_blacklist = bsearch(&(frame->function_name), blacklist,
                                    sizeof(blacklist)/sizeof(blacklist[0]),
                                    sizeof(blacklist[0]), ptrstrcmp);

        if (!in_module && in_blacklist)
        {
            bool success = btp_thread_remove_frame(thread, frame);
            assert(success || !"failed to remove frame");
        }

        frame = next_frame;
    }
}
