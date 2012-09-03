/*
    utils_sha1.h

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
#ifndef BTPARSER_SHA1_H
#define BTPARSER_SHA1_H

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

#define BTP_SHA1_RESULT_BIN_LEN (5 * 4)
#define BTP_SHA1_RESULT_LEN (5 * 4 * 2 + 1)

/**
 * @brief Internal state of a SHA-1 hash algorithm.
 */
struct btp_sha1_state
{
    /* always correctly aligned for uint64_t */
    uint8_t wbuffer[64];
    /* for sha256: void (*process_block)(struct md5_state_t*); */
    /* must be directly before hash[] */
    uint64_t total64;
    /* 4 elements for md5, 5 for sha1, 8 for sha256 */
    uint32_t hash[8];
};

void
btp_sha1_begin(struct btp_sha1_state *state);

void
btp_sha1_hash(struct btp_sha1_state *state,
              const void *buffer,
              size_t len);

void
btp_sha1_end(struct btp_sha1_state *state, void *resbuf);

char *
btp_bin2hex(char *dst, const char *str, int count);

#ifdef __cplusplus
}
#endif

#endif
