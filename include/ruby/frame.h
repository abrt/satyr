/*
    ruby_frame.h

    Copyright (C) 2015  ABRT Team
    Copyright (C) 2015  Red Hat, Inc.

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
#ifndef SATYR_RUBY_FRAME_H
#define SATYR_RUBY_FRAME_H

/**
 * @file
 * @brief Ruby frame structure and related routines.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <stdbool.h>
#include <stdint.h>

struct sr_location;
struct sr_strbuf;
struct sr_json_value;

struct sr_ruby_frame
{
    enum sr_report_type type;

    char *file_name;

    uint32_t file_line;

    bool special_function;

    char *function_name;

    uint32_t block_level;

    uint32_t rescue_level;

    struct sr_ruby_frame *next;
};

struct sr_ruby_frame *
sr_ruby_frame_new();

void
sr_ruby_frame_init(struct sr_ruby_frame *frame);

void
sr_ruby_frame_free(struct sr_ruby_frame *frame);

struct sr_ruby_frame *
sr_ruby_frame_dup(struct sr_ruby_frame *frame, bool siblings);

int
sr_ruby_frame_cmp(struct sr_ruby_frame *frame1, struct sr_ruby_frame *frame2);

int
sr_ruby_frame_cmp_distance(struct sr_ruby_frame *frame1,
                           struct sr_ruby_frame *frame2);

struct sr_ruby_frame *
sr_ruby_frame_append(struct sr_ruby_frame *dest,
                     struct sr_ruby_frame *item);

struct sr_ruby_frame *
sr_ruby_frame_parse(const char **input, struct sr_location *location);

char *
sr_ruby_frame_to_json(struct sr_ruby_frame *frame);

struct sr_ruby_frame *
sr_ruby_frame_from_json(struct sr_json_value *root, char **error_message);

void
sr_ruby_frame_append_to_str(struct sr_ruby_frame *frame, struct sr_strbuf *dest);

#ifdef __cplusplus
}
#endif

#endif
