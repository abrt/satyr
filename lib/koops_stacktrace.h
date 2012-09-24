/*
    koops_stacktrace.h

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
#ifndef BTPARSER_KOOPS_STACKTRACE_H
#define BTPARSER_KOOPS_STACKTRACE_H

/**
 * @file
 * @brief Kernel oops stack trace structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <inttypes.h>

struct btp_location;

struct btp_koops_stacktrace
{
    /**
     * @brief Version of the kernel.
     */
    char *version;

    /**
     * http://www.mjmwired.net/kernel/Documentation/oops-tracing.txt
     */

    bool taint_module_proprietary;
    bool taint_module_gpl;
    bool taint_module_out_of_tree;
    bool taint_forced_module;
    bool taint_forced_removal;
    bool taint_smp_unsafe;
    /** A machine check exception has been raised. */
    bool taint_mce;
    /** A process has been found in a bad page state.*/
    bool taint_page_release;
    bool taint_userspace;
    bool taint_died_recently;
    bool taint_acpi_overridden;
    bool taint_warning;
    bool taint_staging_driver;
    bool taint_firmware_workaround;

    char **modules;

    /**
     * @brief Call trace
     */
    struct btp_koops_frame *frames;
};

/**
 * Creates and initializes a new stack trace structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_koops_stacktrace_free().
 */
struct btp_koops_stacktrace *
btp_koops_stacktrace_new();

/**
 * Initializes all members of the stacktrace structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a stacktrace structure placed on the
 * stack.
 */
void
btp_koops_stacktrace_init(struct btp_koops_stacktrace *stacktrace);

/**
 * Releases the memory held by the stacktrace.
 * @param stacktrace
 * If the stacktrace is NULL, no operation is performed.
 */
void
btp_koops_stacktrace_free(struct btp_koops_stacktrace *stacktrace);

/**
 * Creates a duplicate of a stacktrace.
 * @param stacktrace
 * The stacktrace to be copied. It's not modified by this function.
 * @returns
 * This function never returns NULL.  The returned duplicate must be
 * released by calling the function btp_koops_stacktrace_free().
 */
struct btp_koops_stacktrace *
btp_koops_stacktrace_dup(struct btp_koops_stacktrace *stacktrace);

/**
 * Parses a textual kernel oops and puts it into a structure.  If
 * parsing fails, the input parameter is not changed and NULL is
 * returned.
 * @param input
 * Pointer to the string with the kernel oops. If this function
 * returns a non-NULL value, the input pointer is modified to point
 * after the stacktrace that was just parsed.
 */
struct btp_koops_stacktrace *
btp_koops_stacktrace_parse(const char **input,
                           struct btp_location *location);

/**
 * Timestamp may be present in the oops lines.
 * @example
 * [123456.654321]
 * [   65.470000]
 */
bool
btp_koops_skip_timestamp(const char **input);

bool
btp_koops_parse_address(const char **input, uint64_t *address);

bool
btp_koops_parse_module_name(const char **input,
                            char **module_name);

bool
btp_koops_parse_function_name(const char **input,
                              char **function_name,
                              uint64_t *function_offset,
                              uint64_t *function_length);

/**
 * @returns
 * True if line was successfully parsed, false if line is in unknown
 * format.
 */
bool
btp_koops_parse_frame_line(const char **input);

#ifdef __cplusplus
}
#endif

#endif
