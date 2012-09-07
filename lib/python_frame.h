/*
    python_frame.h

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
#ifndef BTPARSER_PYTHON_FRAME_H
#define BTPARSER_PYTHON_FRAME_H

/**
 * @file
 * @brief Python frame structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct btp_python_frame
{
    char *file_name;

    uint32_t file_line;

    bool is_module;

    char *function_name;

    char *line;

    struct btp_python_frame *next;
};

#ifdef __cplusplus
}
#endif

#endif
