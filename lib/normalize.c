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
#include "gdb/frame.h"
#include "gdb/thread.h"
#include "gdb/stacktrace.h"
#include "core/frame.h"
#include "core/thread.h"
#include "thread.h"
#include "utils.h"
#include <string.h>
#include <assert.h>

static bool
call_match(const char *function_name,
           const char *source_file,
           const char *expected_function_name,
           ...)
{
    if (!function_name)
        return false;

    if (0 != strcmp(function_name, expected_function_name))
        return false;

    va_list source_files;
    va_start(source_files, expected_function_name);
    bool success = false;

    while (true)
    {
        char *expected_source_file = va_arg(source_files, char*);
        if (!expected_source_file)
            break;

        if (source_file &&
            NULL != strstr(source_file, expected_source_file))
        {
            success = true;
            break;
        }
    }

    va_end(source_files);
    return success;
}

static bool
is_removable_vim(const char *function_name,
                 const char *source_file)
{
    return
        call_match(function_name, source_file, "may_core_dump", "os_unix.c", NULL) ||
        call_match(function_name, source_file, "mch_exit", "os_unix.c", NULL);
}

static bool
is_removable_jvm(const char *function_name,
                 const char *source_file)
{
    return
        call_match(function_name, source_file, "os::abort", "os_linux.cpp", NULL) ||
        call_match(function_name, source_file, "VMError::report_and_die", "vmError.cpp", NULL) ||
        call_match(function_name, source_file, "JVM_handle_linux_signal", "os_linux_x86.cpp", NULL);
}

static bool
is_removable_dbus(const char *function_name,
                  const char *source_file)
{
    return
        call_match(function_name, source_file, "gerror_to_dbus_error_message", "dbus-gobject.c", NULL) ||
        call_match(function_name, source_file, "dbus_g_method_return_error", "dbus-gobject.c", NULL) ||
        call_match(function_name, source_file, "message_queue_dispatch", "dbus-gmain.c", NULL) ||
        call_match(function_name, source_file, "_dbus_abort", "dbus-sysdeps.c", "libdbus", NULL) ||
        call_match(function_name, source_file, "dbus_connection_dispatch", "dbus-connection.c", "libdbus", NULL);
}

static bool
is_removable_gdk(const char *function_name,
                 const char *source_file)
{
    return
        call_match(function_name, source_file, "gdk_x_error", "gdkmain-x11.c", NULL) ||
        call_match(function_name, source_file, "gdk_threads_dispatch", "gdk.c", NULL) ||
        call_match(function_name, source_file, "gdk_event_dispatch", "gdkevents-x11.c", "gdkevents.c", NULL) ||
        call_match(function_name, source_file, "gdk_event_source_dispatch", "gdkeventsource.c", NULL) ||
        call_match(function_name, source_file, "_gdk_x11_display_error_event", "gdkdisplay-x11.c", "libgdk", NULL);
}

static bool
is_removable_glib(const char *function_name,
                  const char *source_file)
{
    return
        call_match(function_name, source_file, "g_log", "gmessages.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_logv", "gmessages.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_assertion_message", "gtestutils.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_assertion_message_expr", "gtestutils.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_closure_invoke", "gclosure.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_free", "gmem.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_type_class_meta_marshal", "gclosure.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_signal_emit_valist", "gsignal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "signal_emit_unlocked_R", "gsignal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_signal_emit", "gsignal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_idle_dispatch", "gmain.c", "gutf8.c", NULL) ||
        call_match(function_name, source_file, "g_object_dispatch_properties_changed", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_object_notify_dispatcher", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_object_unref", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_object_run_dispose", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_object_new", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_object_newv", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_main_context_dispatch", "gmain.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_main_context_iterate", "gmain.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_main_dispatch", "gmain.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_main_loop_run", "gmain.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_timeout_dispatch", "gmain.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_thread_pool_thread_proxy", "gthreadpool.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_thread_create_proxy", "gthread.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_cclosure_marshal_VOID__BOXED", "gmarshal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_cclosure_marshal_VOID__VOID", "gclosure.c", "gmarshal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_object_notify", "gobject.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "Glib::exception_handlers_invoke()", "libglibmm", NULL) ||
        call_match(function_name, source_file, "g_signal_handlers_destroy", "gsignal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "g_vasprintf", "gprintf.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_strdup_vprintf", "libglib", NULL) ||
        call_match(function_name, source_file, "g_strdup_printf", "libglib", NULL) ||
        call_match(function_name, source_file, "g_print", "libglib", NULL) ||
        call_match(function_name, source_file, "invalid_closure_notify", "gsignal.c", "libgobject", NULL) ||
        call_match(function_name, source_file, "smc_tree_abort", "gslice.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_thread_abort", "libglib", NULL) ||
        call_match(function_name, source_file, "_g_log_abort", "gmessages.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_log_default_handler", "gmessages.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_log_writer_default", "gmessages.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_log_structured_array", "gmessages.c", "libglib", NULL) ||
        call_match(function_name, source_file, "g_log_structured", "gmessages.c", "libglib", NULL);
}

static bool
is_removable_libstdcpp(const char *function_name,
                       const char *source_file)
{
    return
        call_match(function_name, source_file, "__gnu_cxx::__verbose_terminate_handler", "vterminate.cc", NULL) ||
        call_match(function_name, source_file, "__cxxabiv1::__terminate", "eh_terminate.cc", NULL) ||
        call_match(function_name, source_file, "std::terminate", "eh_terminate.cc", NULL) ||
        call_match(function_name, source_file, "__cxxabiv1::__cxa_throw", "eh_throw.cc", NULL) ||
        call_match(function_name, source_file, "__cxxabiv1::__cxa_rethrow", "eh_throw.cc", NULL) ||
        call_match(function_name, source_file, "__verbose_terminate_handler", "vterminate.cc", NULL) ||
        call_match(function_name, source_file, "__cxxabiv1::__cxa_pure_virtual", "pure.cc", NULL);
}

static bool
is_removable_linux(const char *function_name,
                   const char *source_file)
{
    return call_match(function_name, source_file, "__kernel_vsyscall", "", NULL);
}

static bool
is_removable_xorg(const char *function_name,
                  const char *source_file)
{
    return
        call_match(function_name, source_file, "_XReply", "xcb_io.c", NULL) ||
        call_match(function_name, source_file, "_XError", "XlibInt.c", NULL) ||
        call_match(function_name, source_file, "XSync", "Sync.c", NULL) ||
        call_match(function_name, source_file, "process_responses", "xcb_io.c", NULL) ||
        call_match(function_name, source_file, "OsSigHandler", "osinit.c", NULL) ||
        call_match(function_name, source_file, "FatalError", "log.c", NULL) ||
        call_match(function_name, source_file, "AbortServer", "log.c", NULL) ||
        call_match(function_name, source_file, "AbortDDX", "xf86Init.c", NULL) ||
        call_match(function_name, source_file, "ddxGiveUp", "xf86Init.c", NULL) ||
        call_match(function_name, source_file, "OsAbort", "utils.c", NULL) ||
        call_match(function_name, source_file, "handle_error", "xcb_io.c", "libX11", NULL) ||
        call_match(function_name, source_file, "_XIOError", "XlibInt.c", "libX11", NULL) ||
        call_match(function_name, source_file, "_XEventsQueued", "xcb_io.c", "libX11", NULL) ||
        call_match(function_name, source_file, "handle_response", "xcb_io.c", "libX11", NULL);
}

static bool
is_removable_glibc(const char *function_name,
                   const char *source_file)
{
    return
        call_match(function_name, source_file, "_start", "", NULL) ||
        call_match(function_name, source_file, "__libc_start_main", "libc", NULL) ||
        call_match(function_name, source_file, "clone", "clone.S", "libc", NULL) ||
        call_match(function_name, source_file, "poll", "libc", NULL) ||
        call_match(function_name, source_file, "_IO_new_fclose", "iofclose.c", "libc", NULL) ||
        call_match(function_name, source_file, "_IO_vfprintf_internal", "vfprintf.c", "libc", NULL) ||
        call_match(function_name, source_file, "_IO_default_xsputn", "genops.c", "libc", NULL) ||
        call_match(function_name, source_file, "_IO_wdefault_xsputn", "wgenops.c", "libc", NULL) ||
        call_match(function_name, source_file, "__libc_message", "libc_fatal.c", "libc", NULL) ||
        call_match(function_name, source_file, "start_thread", "pthread_create.c", "libpthread", NULL);
}

static bool
is_removable_other(const char *function_name,
                   const char *source_file)
{
    return
        call_match(function_name, source_file, "assert_cursor", "intel_display.c", NULL) ||
        call_match(function_name, source_file, "assert_device_not_suspended", "intel_uncore.c", NULL) ||
        call_match(function_name, source_file, "assert_pipe", "intel_display.c", NULL) ||
        call_match(function_name, source_file, "assert_plane", "intel_display.c", NULL) ||
        call_match(function_name, source_file, "assert_transcoder_disabled", "intel_display.c", NULL) ||
        call_match(function_name, source_file, "btrfs_assert_delayed_root_empty", "delayed-inode.c", "btrfs", NULL) ||
        call_match(function_name, source_file, "_cogl_set_error", "cogl-error.c", "libcogl", NULL) ||
        call_match(function_name, source_file, "defaultCrashHandler", "kcrash.cpp", "libKF5Crash", NULL) ||
        call_match(function_name, source_file, "_dl_signal_error", "dl-error.c", "ld-linux", NULL) ||
        call_match(function_name, source_file, "error_dialog_response_cb", "", NULL) ||
        call_match(function_name, source_file, "do_warn", "_warnings.c", "libpython",NULL) ||
        call_match(function_name, source_file, "QMessageLogger::fatal(char const*, ...) const", "", NULL) ||
        call_match(function_name, source_file, "nsProfileLock::FatalSignalHandler(int, siginfo_t*, void*)", "", NULL) ||
        call_match(function_name, source_file, "qt_message_output", "qglobal.cpp", "libQtCore", NULL) ||
        call_match(function_name, source_file, "qt_message_output(QtMsgType, char const*)", "", NULL) ||
        call_match(function_name, source_file, "signalHandler(int, siginfo_t*, void*)", "", NULL) ||
        call_match(function_name, source_file, "FatalSignalHandler", "nsProfileLock.cpp", "libxul", NULL) ||
        call_match(function_name, source_file, "Foam::error::abort()", "", NULL) ||
        call_match(function_name, source_file, "JS_AbortIfWrongThread", "libmozjs", NULL) ||
        call_match(function_name, source_file, "Crash::defaultCrashHandler(int)", "libkdeui", "libKF5Crash", NULL) ||
        call_match(function_name, source_file, "Py_FatalError", "pythonrun.c", "libpython", NULL) ||
        call_match(function_name, source_file, "__btrfs_abort_transaction", "btrfs", NULL) ||
        call_match(function_name, source_file, "assert_pch_hdmi_disabled", "", NULL) ||
        call_match(function_name, source_file, "assert_pll", "", NULL) ||
        call_match(function_name, source_file, "core::system::abort()", "", NULL) ||
        call_match(function_name, source_file, "ddd_assert_fail", "assert.C", NULL) ||
        call_match(function_name, source_file, "debug_dma_assert_idle", "", NULL) ||
        call_match(function_name, source_file, "error_handler", "", NULL) ||
        call_match(function_name, source_file, "fatal_error_signal", "", NULL) ||
        call_match(function_name, source_file, "fatal_handler", "signal.c", "libfreerdp", NULL) ||
        call_match(function_name, source_file, "gpf_notice", "", NULL) ||
        call_match(function_name, source_file, "log", "", NULL) ||
        call_match(function_name, source_file, "_log", "", NULL) ||
        call_match(function_name, source_file, "log_assert_failed", "", NULL) ||
        call_match(function_name, source_file, "mozalloc_abort", "mozalloc_abort.cpp", "libmozalloc", NULL) ||
        call_match(function_name, source_file, "mozalloc_abort(char const*)", "libmozalloc", "content-container", "plugin-container", NULL) ||
        call_match(function_name, source_file, "note_interrupt", "spurious.c", "vmlinux", NULL) ||
        call_match(function_name, source_file, "print_bad_pte", "memory.c", "vmlinux", NULL) ||
        call_match(function_name, source_file, "print_oops_end_marker", "panic.c", "vmlinux", NULL) ||
        call_match(function_name, source_file, "printk", "printk.c", "vmlinux", NULL) ||
        call_match(function_name, source_file, "qupzilla_signal_handler", "main.cpp", "qupzilla", NULL) ||
        call_match(function_name, source_file, "rb_bug", "error.c", "libruby", NULL) ||
        call_match(function_name, source_file, "sighandler", "", NULL) ||
        call_match(function_name, source_file, "signalHandler(int)", "", NULL) ||
        call_match(function_name, source_file, "signal_abort", "signal.c", NULL) ||
        call_match(function_name, source_file, "signal_handler", "", NULL) ||
        call_match(function_name, source_file, "sys_abort", "error.c", "libgfortran", NULL) ||
        call_match(function_name, source_file, "terminate_due_to_signal", "emacs.c", "emacs", NULL) ||
        call_match(function_name, source_file, "wl_log", "wayland-util.c", NULL) ||
        call_match(function_name, source_file, "display_protocol_error", "wayland-client.c", NULL) ||
        call_match(function_name, source_file, "display_handle_error", "wayland-client.c", NULL) ||
        call_match(function_name, source_file, "x_io_error", "libmutter", NULL) ||
        call_match(function_name, source_file, "__ioremap_calle ", "ioremap.c", NULL) ||
        call_match(function_name, source_file, "ioremap_nocache", "ioremap.c", NULL) ||
        call_match(function_name, source_file, "wpa_msg", "wpa_debug.c", NULL) ||
        call_match(function_name, source_file, "js::gc::FinalizeArenas(js::FreeOp*, js::gc::ArenaHeader**, js::gc::ArenaList&, js::gc::AllocKind, js::SliceBudget&)\"js::Shape::finalize(js::FreeOp*)", "", NULL) ||
        call_match(function_name, source_file, "WTF::StringImpl::endsWith(char const*, unsigned int, bool) const", "", NULL) ||
        call_match(function_name, source_file, "mozilla::plugins::child::_invokedefault(_NPP*, NPObject*, _NPVariant const*, unsigned int, _NPVariant*)", "", NULL) ||
        call_match(function_name, source_file, "xitk_signal_handler", "xitk.c", "xine", NULL);
}

static bool
is_removable_glibc_with_above(const char *function_name,
                              const char *source_file)
{
    return
        call_match(function_name, source_file, "__assert_fail", "", NULL) ||
        call_match(function_name, source_file, "__assert_fail_base", "", NULL) ||
        call_match(function_name, source_file, "__chk_fail", "", NULL) ||
        call_match(function_name, source_file, "__longjmp_chk", "", NULL) ||
        call_match(function_name, source_file, "__malloc_assert", "", NULL) ||
        call_match(function_name, source_file, "__strcat_chk", "", NULL) ||
        call_match(function_name, source_file, "__strcpy_chk", "", NULL) ||
        call_match(function_name, source_file, "__strncpy_chk", "", NULL) ||
        call_match(function_name, source_file, "__vsnprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "___vsnprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "__snprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "___snprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "__vasprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "__vsprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "___sprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "__fwprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "__asprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "___printf_chk", "", NULL) ||
        call_match(function_name, source_file, "___fprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "__vswprintf_chk", "", NULL) ||
        call_match(function_name, source_file, "malloc_consolidate", "malloc.c", "libc", NULL) ||
        call_match(function_name, source_file, "malloc_printerr", "malloc.c", "libc", NULL) ||
        call_match(function_name, source_file, "_int_malloc", "malloc.c", "libc", NULL) ||
        call_match(function_name, source_file, "_int_free", "malloc.c", "libc", NULL) ||
        call_match(function_name, source_file, "_int_realloc", "malloc.c", "libc", NULL) ||
        call_match(function_name, source_file, "_int_memalign", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__libc_free", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__libc_malloc", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__libc_memalign", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__libc_realloc", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__posix_memalign", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__libc_calloc", "malloc.c", NULL) ||
        call_match(function_name, source_file, "__libc_fatal", "libc", NULL);
}

static char *
find_new_function_name_glibc(const char *function_name,
                             const char *source_file)
{
    /* Normalize frame names. */
#define NORMALIZE_ARCH_SPECIFIC(func)                                   \
    if (call_match(function_name, source_file, "__" func "_sse2", func, "/sysdeps/", "libc.so", NULL) || \
        call_match(function_name, source_file, "__" func "_sse2_bsf", func, "/sysdeps/", "libc.so", NULL) || \
        call_match(function_name, source_file, "__" func "_ssse3", func, "/sysdeps/", "libc.so", NULL) /* ssse3, not sse3! */ || \
        call_match(function_name, source_file, "__" func "_ssse3_rep", func, "/sysdeps/", "libc.so", NULL) || \
        call_match(function_name, source_file, "__" func "_ssse3_back", func, "/sysdeps/", "libc.so", NULL) || \
        call_match(function_name, source_file, "__" func "_sse42", func, "/sysdeps/", "libc.so", NULL) || \
        call_match(function_name, source_file, "__" func "_ia32", func, "/sysdeps", "libc.so", NULL)) \
        {                                                               \
            return sr_strdup(func);                                     \
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
        return NULL;
}

static void
remove_func_prefix(char *function_name, const char *prefix, int num)
{
    int prefix_len, func_len;

    if (!function_name)
        return;

    prefix_len = strlen(prefix);

    if (strncmp(function_name, prefix, prefix_len))
        return;

    func_len = strlen(function_name);
    if (num > func_len)
        num = func_len;

    memmove(function_name, function_name + num, func_len - num + 1);
}

static bool
sr_gdb_is_exit_frame(struct sr_gdb_frame *frame)
{
    return
        sr_gdb_frame_calls_func(frame, "__run_exit_handlers", "exit.c", NULL) ||
        sr_gdb_frame_calls_func(frame, "raise", "pt-raise.c", "libc.so", "libc-", "libpthread.so", NULL) ||
        sr_gdb_frame_calls_func(frame, "__GI_raise", "raise.c", NULL) ||
        sr_gdb_frame_calls_func(frame, "exit", "exit.c", NULL) ||
        sr_gdb_frame_calls_func(frame, "abort", "abort.c", "libc.so", "libc-", NULL) ||
        sr_gdb_frame_calls_func(frame, "__GI_abort", "abort.c", NULL) ||
        /* Terminates a function in case of buffer overflow. */
        sr_gdb_frame_calls_func(frame, "__chk_fail", "chk_fail.c", "libc.so", NULL) ||
        sr_gdb_frame_calls_func(frame, "__stack_chk_fail", "stack_chk_fail.c", "libc.so", NULL) ||
        sr_gdb_frame_calls_func(frame, "do_exit", "exit.c", NULL) ||
        sr_gdb_frame_calls_func(frame, "kill", "syscall-template.S", NULL);
}

void
sr_normalize_gdb_thread(struct sr_gdb_thread *thread)
{
    /* Find the exit frame and remove everything above it. */
    struct sr_gdb_frame *exit_frame = sr_glibc_thread_find_exit_frame(thread);
    if (exit_frame)
    {
        bool success = sr_gdb_thread_remove_frames_above(thread, exit_frame);
        assert(success); /* if this fails, some code become broken */
        success = sr_gdb_thread_remove_frame(thread, exit_frame);
        assert(success); /* if this fails, some code become broken */
    }

    /* Normalize function names by removing various prefixes that
     * occur only in some cases.
     */
    struct sr_gdb_frame *frame = thread->frames;
    while (frame)
    {
        if (frame->source_file)
        {
            /* Remove IA__ prefix used in GLib, GTK and GDK. */
            remove_func_prefix(frame->function_name, "IA__gdk", strlen("IA__"));
            remove_func_prefix(frame->function_name, "IA__g_", strlen("IA__"));
            remove_func_prefix(frame->function_name, "IA__gtk", strlen("IA__"));

            /* Remove __GI_ (glibc internal) prefix. */
            remove_func_prefix(frame->function_name, "__GI_", strlen("__GI_"));
        }

        frame = frame->next;
    }

    /* Unify some functions by renaming them.
     */
    frame = thread->frames;
    while (frame)
    {
        char *new_function_name =
            find_new_function_name_glibc(frame->function_name, frame->source_file);

        if (new_function_name)
        {
            free(frame->function_name);
            frame->function_name = new_function_name;
        }

        frame = frame->next;
    }

    /* Remove redundant frames from the thread.
     */
    frame = thread->frames;
    while (frame)
    {
        struct sr_gdb_frame *next_frame = frame->next;

        /* Remove frames which are not a cause of the crash. */
        bool removable =
            is_removable_dbus(frame->function_name, frame->source_file) ||
            is_removable_gdk(frame->function_name, frame->source_file) ||
            is_removable_glib(frame->function_name, frame->source_file) ||
            is_removable_glibc(frame->function_name, frame->source_file) ||
            is_removable_libstdcpp(frame->function_name, frame->source_file) ||
            is_removable_linux(frame->function_name, frame->source_file) ||
            is_removable_xorg(frame->function_name, frame->source_file) ||
            is_removable_jvm(frame->function_name, frame->source_file) ||
            is_removable_vim(frame->function_name, frame->source_file) ||
            is_removable_other(frame->function_name, frame->source_file);

        bool removable_with_above =
            is_removable_glibc_with_above(frame->function_name, frame->source_file) ||
            sr_gdb_is_exit_frame(frame);

        if (removable_with_above)
        {
            bool success = sr_gdb_thread_remove_frames_above(thread, frame);
            assert(success);
        }

        if (removable || removable_with_above)
            sr_gdb_thread_remove_frame(thread, frame);

        frame = next_frame;
    }

    /* If the first frame has address 0x0000 and its name is '??', it
     * is a dereferenced null, and we remove it. This frame is not
     * really invalid, but it affects stacktrace quality rating. See
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
        sr_gdb_thread_remove_frame(thread, thread->frames);
    }

    /* If the last frame has address 0x0000 and its name is '??',
     * remove it. This frame is not really invalid, but it affects
     * stacktrace quality rating. See Red Hat Bugzilla bug #592523.
     * @code
     * #2  0x00007f4dcebbd62d in clone ()
     * at ../sysdeps/unix/sysv/linux/x86_64/clone.S:112
     * No locals.
     * #3  0x0000000000000000 in ?? ()
     * @endcode
     */
    struct sr_gdb_frame *last = thread->frames;
    while (last && last->next)
        last = last->next;
    if (last &&
        last->address == 0x0000 &&
        last->function_name &&
        0 == strcmp(last->function_name, "??"))
    {
        sr_gdb_thread_remove_frame(thread, last);
    }

    /* Merge recursively called functions into single frame */
    struct sr_gdb_frame *curr_frame = thread->frames;
    struct sr_gdb_frame *prev_frame = NULL;
    while (curr_frame)
    {
        if (prev_frame &&
            0 != sr_strcmp0(prev_frame->function_name, "??") &&
            0 == sr_strcmp0(prev_frame->function_name, curr_frame->function_name))
        {
            prev_frame->next = curr_frame->next;
            sr_gdb_frame_free(curr_frame);
            curr_frame = prev_frame->next;
            continue;
        }

        prev_frame = curr_frame;
        curr_frame = curr_frame->next;
    }
}

void
sr_normalize_core_thread(struct sr_core_thread *thread)
{
    /* Find the exit frame and remove everything above it. */
    struct sr_core_frame *exit_frame = sr_core_thread_find_exit_frame(thread);
    if (exit_frame)
    {
        bool success = sr_thread_remove_frames_above((struct sr_thread *)thread,
                                                     (struct sr_frame *)exit_frame);
        assert(success); /* if this fails, some code become broken */
        success = sr_thread_remove_frame((struct sr_thread *)thread,
                                         (struct sr_frame *)exit_frame);
        assert(success); /* if this fails, some code become broken */
    }

    /* Normalize function names by removing various prefixes that
     * occur only in some cases.
     */
    struct sr_core_frame *frame = thread->frames;
    while (frame)
    {
        /* Remove IA__ prefix used in GLib, GTK and GDK. */
        remove_func_prefix(frame->function_name, "IA__gdk", strlen("IA__"));
        remove_func_prefix(frame->function_name, "IA__g_", strlen("IA__"));
        remove_func_prefix(frame->function_name, "IA__gtk", strlen("IA__"));

        /* Remove __GI_ (glibc internal) prefix. */
        remove_func_prefix(frame->function_name, "__GI_", strlen("__GI_"));

        frame = frame->next;
    }

    /* Unify some functions by renaming them.
     */
    frame = thread->frames;
    while (frame)
    {
        char *new_function_name =
            find_new_function_name_glibc(frame->function_name, frame->file_name);

        if (new_function_name)
        {
            free(frame->function_name);
            frame->function_name = new_function_name;
        }

        frame = frame->next;
    }

    /* Remove redundant frames from the thread.
     */
    frame = thread->frames;
    while (frame)
    {
        struct sr_core_frame *next_frame = frame->next;

        /* Remove frames which are not a cause of the crash. */
        bool removable =
            is_removable_dbus(frame->function_name, frame->file_name) ||
            is_removable_gdk(frame->function_name, frame->file_name) ||
            is_removable_glib(frame->function_name, frame->file_name) ||
            is_removable_glibc(frame->function_name, frame->file_name) ||
            is_removable_libstdcpp(frame->function_name, frame->file_name) ||
            is_removable_linux(frame->function_name, frame->file_name) ||
            is_removable_xorg(frame->function_name, frame->file_name) ||
            is_removable_jvm(frame->function_name, frame->file_name) ||
            is_removable_vim(frame->function_name, frame->file_name) ||
            is_removable_other(frame->function_name, frame->file_name);

        bool removable_with_above =
            is_removable_glibc_with_above(frame->function_name, frame->file_name)  ||
            sr_core_thread_is_exit_frame(frame);

        if (removable_with_above)
        {
            bool success = sr_thread_remove_frames_above((struct sr_thread *)thread,
                                                         (struct sr_frame *)frame);
            assert(success);
        }

        if (removable || removable_with_above)
            sr_thread_remove_frame((struct sr_thread *)thread, (struct sr_frame *)frame);

        frame = next_frame;
    }

    /* Anonymize file_name if contains /home/<user>/...
     */
    frame = thread->frames;
    while (frame)
    {
        frame->file_name = anonymize_path(frame->file_name);
        frame = frame->next;
    }

    /* If the first frame has address 0x0000 and its name is '??', it
     * is a dereferenced null, and we remove it. This frame is not
     * really invalid, but it affects stacktrace quality rating. See
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
        !thread->frames->function_name)
    {
        sr_thread_remove_frame((struct sr_thread *)thread,
                               (struct sr_frame *)thread->frames);
    }

    /* If the last frame has address 0x0000 and its name is '??',
     * remove it. This frame is not really invalid, but it affects
     * stacktrace quality rating. See Red Hat Bugzilla bug #592523.
     * @code
     * #2  0x00007f4dcebbd62d in clone ()
     * at ../sysdeps/unix/sysv/linux/x86_64/clone.S:112
     * No locals.
     * #3  0x0000000000000000 in ?? ()
     * @endcode
     */
    struct sr_core_frame *last = thread->frames;
    while (last && last->next)
        last = last->next;
    if (last &&
        last->address == 0x0000 &&
        !last->function_name)
    {
        sr_thread_remove_frame((struct sr_thread *)thread, (struct sr_frame *)last);
    }

    /* Merge recursively called functions into single frame */
    struct sr_core_frame *curr_frame = thread->frames;
    struct sr_core_frame *prev_frame = NULL;
    while (curr_frame)
    {
        if (prev_frame &&
            prev_frame->function_name &&
            0 == sr_strcmp0(prev_frame->function_name, curr_frame->function_name))
        {
            prev_frame->next = curr_frame->next;
            sr_core_frame_free(curr_frame);
            curr_frame = prev_frame->next;
            continue;
        }

        prev_frame = curr_frame;
        curr_frame = curr_frame->next;
    }

}

void
sr_normalize_gdb_stacktrace(struct sr_gdb_stacktrace *stacktrace)
{
    struct sr_gdb_thread *thread = stacktrace->threads;
    while (thread)
    {
        sr_normalize_gdb_thread(thread);
        thread = thread->next;
    }
}

static bool
next_functions_similar(struct sr_gdb_frame *frame1,
                       struct sr_gdb_frame *frame2)
{
    if ((!frame1->next && frame2->next) ||
        (frame1->next && !frame2->next) ||
        (frame1->next && frame2->next &&
         (0 != sr_strcmp0(frame1->next->function_name,
                          frame2->next->function_name) ||
          0 == sr_strcmp0(frame1->next->function_name, "??") ||
         (frame1->next->library_name && frame2->next->library_name &&
          0 != strcmp(frame1->next->library_name, frame2->next->library_name)))))
        return false;
    return true;
}

void
sr_normalize_gdb_paired_unknown_function_names(struct sr_gdb_thread *thread1,
                                               struct sr_gdb_thread *thread2)

{
    if (!thread1->frames || !thread2->frames)
    {
        return;
    }

    int i = 0;
    struct sr_gdb_frame *curr_frame1 = thread1->frames;
    struct sr_gdb_frame *curr_frame2 = thread2->frames;

    if (0 == sr_strcmp0(curr_frame1->function_name, "??") &&
        0 == sr_strcmp0(curr_frame2->function_name, "??") &&
        !(curr_frame1->library_name && curr_frame2->library_name &&
          strcmp(curr_frame1->library_name, curr_frame2->library_name)) &&
        next_functions_similar(curr_frame1, curr_frame2))
    {
        free(curr_frame1->function_name);
        curr_frame1->function_name = sr_asprintf("__unknown_function_%d", i);
        free(curr_frame2->function_name);
        curr_frame2->function_name = sr_asprintf("__unknown_function_%d", i);
        i++;
    }

    struct sr_gdb_frame *prev_frame1 = curr_frame1;
    struct sr_gdb_frame *prev_frame2 = curr_frame2;
    curr_frame1 = curr_frame1->next;
    curr_frame2 = curr_frame2->next;

    while (curr_frame1)
    {
        if (0 == sr_strcmp0(curr_frame1->function_name, "??"))
        {
            while (curr_frame2)
            {
                if (0 == sr_strcmp0(curr_frame2->function_name, "??") &&
                    !(curr_frame1->library_name && curr_frame2->library_name &&
                      strcmp(curr_frame1->library_name, curr_frame2->library_name)) &&
                    0 != sr_strcmp0(prev_frame2->function_name, "??") &&
                    next_functions_similar(curr_frame1, curr_frame2) &&
                    0 == sr_strcmp0(prev_frame1->function_name,
                                     prev_frame2->function_name) &&
                    !(prev_frame1->library_name && prev_frame2->library_name &&
                      strcmp(prev_frame1->library_name, prev_frame2->library_name)))
                {
                    free(curr_frame1->function_name);
                    curr_frame1->function_name = sr_asprintf("__unknown_function_%d", i);
                    free(curr_frame2->function_name);
                    curr_frame2->function_name = sr_asprintf("__unknown_function_%d", i);
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

void
sr_gdb_normalize_optimize_thread(struct sr_gdb_thread *thread)
{
    struct sr_gdb_frame *frame = thread->frames;
    while (frame)
    {
        struct sr_gdb_frame *next_frame = frame->next;

        /* Remove main(). */
        if (frame->function_name &&
                strcmp(frame->function_name, "main") == 0)
            sr_gdb_thread_remove_frame(thread, frame);

        frame = next_frame;
    }
}

struct sr_gdb_frame *
sr_glibc_thread_find_exit_frame(struct sr_gdb_thread *thread)
{
    struct sr_gdb_frame *frame = thread->frames;
    struct sr_gdb_frame *result = NULL;
    while (frame)
    {
        if (sr_gdb_is_exit_frame(frame))
            result = frame;

        frame = frame->next;
    }

    return result;
}
