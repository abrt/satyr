/*
    normalize_glibc.c

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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct btp_gdb_frame *
btp_glibc_thread_find_exit_frame(struct btp_gdb_thread *thread)
{
    struct btp_gdb_frame *frame = thread->frames;
    struct btp_gdb_frame *result = NULL;
    while (frame)
    {
        bool is_exit_frame =
            btp_frame_calls_func_in_file(frame, "__run_exit_handlers", "exit.c") ||
            btp_frame_calls_func_in_file4(frame, "raise", "pt-raise.c", "libc.so", "libc-", "libpthread.so") ||
            btp_frame_calls_func_in_file(frame, "__GI_raise", "raise.c") ||
            btp_frame_calls_func_in_file(frame, "exit", "exit.c") ||
            btp_frame_calls_func_in_file3(frame, "abort", "abort.c", "libc.so", "libc-") ||
            btp_frame_calls_func_in_file(frame, "__GI_abort", "abort.c") ||
            /* Terminates a function in case of buffer overflow. */
            btp_frame_calls_func_in_file2(frame, "__chk_fail", "chk_fail.c", "libc.so") ||
            btp_frame_calls_func_in_file2(frame, "__stack_chk_fail", "stack_chk_fail.c", "libc.so") ||
            btp_frame_calls_func_in_file(frame, "kill", "syscall-template.S");

        if (is_exit_frame)
            result = frame;

        frame = frame->next;
    }

    return result;
}


void
btp_normalize_glibc_thread(struct btp_gdb_thread *thread)
{
    /* Find the exit frame and remove everything above it. */
    struct btp_gdb_frame *exit_frame = btp_glibc_thread_find_exit_frame(thread);
    if (exit_frame)
    {
        bool success = btp_thread_remove_frames_above(thread, exit_frame);
        assert(success); /* if this fails, some code become broken */
        success = btp_thread_remove_frame(thread, exit_frame);
        assert(success); /* if this fails, some code become broken */
    }

    /* Standard function filtering loop. */
    struct btp_gdb_frame *frame = thread->frames;
    while (frame)
    {
        struct btp_gdb_frame *next_frame = frame->next;

        /* Normalize frame names. */
#define NORMALIZE_ARCH_SPECIFIC(func)                                   \
        if (btp_frame_calls_func_in_file3(frame, "__" func "_sse2", func, "/sysdeps/", "libc.so") || \
            btp_frame_calls_func_in_file3(frame, "__" func "_sse2_bsf", func, "/sysdeps/", "libc.so") || \
            btp_frame_calls_func_in_file3(frame, "__" func "_ssse3", func, "/sysdeps/", "libc.so") /* ssse3, not sse3! */ || \
            btp_frame_calls_func_in_file3(frame, "__" func "_ssse3_rep", func, "/sysdeps/", "libc.so") || \
            btp_frame_calls_func_in_file3(frame, "__" func "_sse42", func, "/sysdeps/", "libc.so") || \
            btp_frame_calls_func_in_file3(frame, "__" func "_ia32", func, "/sysdeps", "libc.so")) \
        {                                                               \
            /* We know for sure that 'func' is shorter than		\
               'function_name', no need to use strncpy here. */		\
            strcpy(frame->function_name, func);                         \
        }

        NORMALIZE_ARCH_SPECIFIC("memchr");
        NORMALIZE_ARCH_SPECIFIC("memcmp");
        NORMALIZE_ARCH_SPECIFIC("memcpy");
        NORMALIZE_ARCH_SPECIFIC("memmove");
        NORMALIZE_ARCH_SPECIFIC("memset");
        NORMALIZE_ARCH_SPECIFIC("rawmemchr");
        NORMALIZE_ARCH_SPECIFIC("strcasecmp");
        NORMALIZE_ARCH_SPECIFIC("strcasecmp_l");
        NORMALIZE_ARCH_SPECIFIC("strcat");
        NORMALIZE_ARCH_SPECIFIC("strchr");
        NORMALIZE_ARCH_SPECIFIC("strchrnul");
        NORMALIZE_ARCH_SPECIFIC("strcmp");
        NORMALIZE_ARCH_SPECIFIC("strcpy");
        NORMALIZE_ARCH_SPECIFIC("strcspn");
        NORMALIZE_ARCH_SPECIFIC("strlen");
        NORMALIZE_ARCH_SPECIFIC("strncmp");
        NORMALIZE_ARCH_SPECIFIC("strncpy");
        NORMALIZE_ARCH_SPECIFIC("strpbrk");
        NORMALIZE_ARCH_SPECIFIC("strrchr");
        NORMALIZE_ARCH_SPECIFIC("strspn");
        NORMALIZE_ARCH_SPECIFIC("strstr");
        NORMALIZE_ARCH_SPECIFIC("strtok");

        /* Remove __GI_ (glibc internal) prefix. */
        btp_frame_remove_func_prefix(frame, "__GI_", strlen("__GI_"));

        /* Remove frames which are not a cause of the crash. */
        bool removable_with_above =
            btp_frame_calls_func(frame, "__assert_fail") ||
            btp_frame_calls_func(frame, "__chk_fail") ||
            btp_frame_calls_func(frame, "__longjmp_chk") ||
            btp_frame_calls_func(frame, "__malloc_assert") ||
            btp_frame_calls_func(frame, "__strcat_chk") ||
            btp_frame_calls_func(frame, "__strcpy_chk") ||
            btp_frame_calls_func(frame, "__strncpy_chk") ||
            btp_frame_calls_func(frame, "__vsnprintf_chk") ||
            btp_frame_calls_func(frame, "___vsnprintf_chk") ||
            btp_frame_calls_func(frame, "__snprintf_chk") ||
            btp_frame_calls_func(frame, "___snprintf_chk") ||
            btp_frame_calls_func(frame, "__vasprintf_chk") ||
            btp_frame_calls_func_in_file2(frame, "malloc_consolidate", "malloc.c", "libc") ||
            btp_frame_calls_func_in_file2(frame, "malloc_printerr", "malloc.c", "libc") ||
            btp_frame_calls_func_in_file2(frame, "_int_malloc", "malloc.c", "libc") ||
            btp_frame_calls_func_in_file2(frame, "_int_free", "malloc.c", "libc") ||
            btp_frame_calls_func_in_file2(frame, "_int_realloc", "malloc.c", "libc") ||
            btp_frame_calls_func_in_file(frame, "_int_memalign", "malloc.c") ||
            btp_frame_calls_func_in_file(frame, "__libc_free", "malloc.c") ||
            btp_frame_calls_func_in_file(frame, "__libc_malloc", "malloc.c") ||
            btp_frame_calls_func_in_file(frame, "__libc_memalign", "malloc.c") ||
            btp_frame_calls_func_in_file(frame, "__libc_realloc", "malloc.c") ||
            btp_frame_calls_func_in_file(frame, "__posix_memalign", "malloc.c") ||
            btp_frame_calls_func_in_file(frame, "__libc_calloc", "malloc.c");
        bool removable =
            btp_frame_calls_func(frame, "_start") ||
            btp_frame_calls_func_in_file(frame, "__libc_start_main", "libc") ||
            btp_frame_calls_func_in_file2(frame, "clone", "clone.S", "libc") ||
            btp_frame_calls_func_in_file2(frame, "start_thread", "pthread_create.c", "libpthread");

        if (removable_with_above)
        {
            bool success = btp_thread_remove_frames_above(thread, frame);
            assert(success);
        }
        if (removable_with_above || removable)
        {
            bool success = btp_thread_remove_frame(thread, frame);
            assert(success);
        }

        frame = next_frame;
    }
}
