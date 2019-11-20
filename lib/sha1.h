/*
    sha1.h

    Copyright (C) 2019  Red Hat, Inc.

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
#ifndef SATYR_SHA1_H
#define SATYR_SHA1_H

/**
 * @file
 * @brief An implementation of SHA-1 cryptographic hash function.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <byteswap.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <nettle/sha1.h>

/**
 * High level function that hashes C string and returns hexadecimal encoding of
 * the hash.
 */
char *
sr_sha1_hash_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif
