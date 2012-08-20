/* vi: set sw=4 ts=4: */
/*
 * Based on shasum from http://www.netsw.org/crypto/hash/
 * Majorly hacked up to use Dr Brian Gladman's sha1 code
 *
 * Copyright (C) 2002 Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * Copyright (C) 2003 Glenn L. McGrath
 * Copyright (C) 2003 Erik Andersen
 *
 * Copyright (C) 2012  ABRT team
 * Copyright (C) 2012  RedHat Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * ---------------------------------------------------------------------------
 * Issue Date: 10/11/2002
 *
 * This is a byte oriented version of SHA1 that operates on arrays of bytes
 * stored in memory. It runs at 22 cycles per byte on a Pentium P4 processor
 *
 * ---------------------------------------------------------------------------
 */

#ifndef BTP_HASH_SHA1_H
#define BTP_HASH_SHA1_H

#include <byteswap.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BTP_SHA1_RESULT_BIN_LEN (5 * 4)
#define BTP_SHA1_RESULT_LEN (5 * 4 * 2 + 1)

typedef struct btp_sha1_ctx_t {
    uint8_t wbuffer[64]; /* always correctly aligned for uint64_t */
    /* for sha256: void (*process_block)(struct md5_ctx_t*); */
    uint64_t total64;    /* must be directly before hash[] */
    uint32_t hash[8];    /* 4 elements for md5, 5 for sha1, 8 for sha256 */
} btp_sha1_ctx_t;

void btp_sha1_begin(btp_sha1_ctx_t *ctx);
void btp_sha1_hash(btp_sha1_ctx_t *ctx, const void *buffer, size_t len);
void btp_sha1_end(btp_sha1_ctx_t *ctx, void *resbuf);
char *btp_bin2hex(char *dst, const char *str, int count);

#endif
