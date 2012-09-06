/*
    core_fingerprint.h

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
#ifndef BTPARSER_CORE_FINGERPRINT_H
#define BTPARSER_CORE_FINGERPRINT_H

/**
 * @file
 * @brief Fingerprint algorithm for core stack traces.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct btp_core_stacktrace;
struct btp_core_thread;

bool
btp_core_fingerprint_generate(struct btp_core_stacktrace *stacktrace,
                              char **error_message);

bool
btp_core_fingerprint_generate_for_binary(struct btp_core_thread *thread,
                                         const char *binary_path,
                                         char **error_message);

#ifdef __cplusplus
}
#endif

#endif
