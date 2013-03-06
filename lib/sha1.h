/*
    sha1.h

    Copyright (C) 2002  Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
    Copyright (C) 2003  Glenn L. McGrath
    Copyright (C) 2003  Erik Andersen
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

    Based on shasum from http://www.netsw.org/crypto/hash/
    Majorly hacked up to use Dr Brian Gladman's sha1 code

    This is a byte oriented version of SHA1 that operates on arrays of
    bytes stored in memory. It runs at 22 cycles per byte on a Pentium
    P4 processor.
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

#define SR_SHA1_RESULT_BIN_LEN (5 * 4)
#define SR_SHA1_RESULT_LEN (5 * 4 * 2 + 1)

/**
 * @brief Internal state of a SHA-1 hash algorithm.
 */
struct sr_sha1_state
{
    /* usage of a union avoids aliasing compiler warnings */
    union {
        uint8_t u1[64];
        uint32_t u4[16];
        uint64_t u8[8];
    } wbuffer;
    /* for sha256: void (*process_block)(struct md5_state_t*); */
    /* must be directly before hash[] */
    uint64_t total64;
    /* 4 elements for md5, 5 for sha1, 8 for sha256 */
    uint32_t hash[8];
};

void
sr_sha1_begin(struct sr_sha1_state *state);

void
sr_sha1_hash(struct sr_sha1_state *state,
             const void *buffer,
             size_t len);

void
sr_sha1_end(struct sr_sha1_state *state, void *resbuf);

#ifdef __cplusplus
}
#endif

#endif
