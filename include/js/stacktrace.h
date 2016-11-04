/*
    js_stacktrace.h

    Copyright (C) 2016  ABRT Team
    Copyright (C) 2016  Red Hat, Inc.

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
#ifndef SATYR_JS_STACKTRACE_H
#define SATYR_JS_STACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"
#include "../report_type.h"
#include <stdint.h>

struct sr_js_frame;
struct sr_location;
struct sr_json_value;

struct sr_js_stacktrace
{
    enum sr_report_type type;

    char *exception_name;

    struct sr_js_frame *frames;

    sr_js_platform_t platform;
};

struct sr_js_stacktrace *
sr_js_stacktrace_new(void);

void
sr_js_stacktrace_init(struct sr_js_stacktrace *stacktrace);

void
sr_js_stacktrace_free(struct sr_js_stacktrace *stacktrace);

struct sr_js_stacktrace *
sr_js_stacktrace_dup(struct sr_js_stacktrace *stacktrace);

struct sr_js_stacktrace *
sr_js_stacktrace_parse(const char **input,
                       struct sr_location *location);

struct sr_js_stacktrace *
sr_js_stacktrace_parse_v8(const char **input,
                          struct sr_location *location);

char *
sr_js_stacktrace_get_reason(struct sr_js_stacktrace *stacktrace);

char *
sr_js_stacktrace_to_json(struct sr_js_stacktrace *stacktrace);

struct sr_js_stacktrace *
sr_js_stacktrace_from_json(struct sr_json_value *root, char **error_message);

#ifdef __cplusplus
}
#endif

#endif
