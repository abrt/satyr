/*
    normalize_glib.c

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
#include "gdb_frame.h"
#include "gdb_thread.h"
#include "utils.h"
#include <stdbool.h>
#include <string.h>

void
btp_normalize_glib_thread(struct btp_gdb_thread *thread)
{
    struct btp_gdb_frame *frame = thread->frames;
    while (frame)
    {
        struct btp_gdb_frame *next_frame = frame->next;

        /* Normalize frame names. */
        btp_frame_remove_func_prefix(frame, "IA__g_", strlen("IA__"));

        /* Remove frames which are not a cause of the crash. */
        bool removable =
            btp_frame_calls_func_in_file2(frame, "g_log", "gmessages.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_logv", "gmessages.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_assertion_message", "gtestutils.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_assertion_message_expr", "gtestutils.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_closure_invoke", "gclosure.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_free", "gmem.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_type_class_meta_marshal", "gclosure.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_signal_emit_valist", "gsignal.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "signal_emit_unlocked_R", "gsignal.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_signal_emit", "gsignal.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_idle_dispatch", "gmain.c", "gutf8.c") ||
            btp_frame_calls_func_in_file2(frame, "g_object_dispatch_properties_changed", "gobject.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_object_notify_dispatcher", "gobject.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_object_unref", "gobject.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_object_run_dispose", "gobject.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_object_new", "gobject.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_object_newv", "gobject.c", "libgobject") ||
            btp_frame_calls_func_in_file2(frame, "g_main_context_dispatch", "gmain.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_main_context_iterate", "gmain.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_main_dispatch", "gmain.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_main_loop_run", "gmain.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_timeout_dispatch", "gmain.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_thread_pool_thread_proxy", "gthreadpool.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_thread_create_proxy", "gthread.c", "libglib") ||
            btp_frame_calls_func_in_file2(frame, "g_cclosure_marshal_VOID__BOXED", "gmarshal.c", "libgobject") ||
            btp_frame_calls_func_in_file3(frame, "g_cclosure_marshal_VOID__VOID", "gclosure.c", "gmarshal.c", "libgobject");
        if (removable)
        {
            btp_thread_remove_frame(thread, frame);
        }

        frame = next_frame;
    }
}
