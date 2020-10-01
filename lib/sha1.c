/*
    sha1.c

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
#include "sha1.h"
#include "utils.h"

#define SHA1_HEX_DIGEST_SIZE (5 * 4 * 2)

char *
sr_sha1_hash_string(const char *str)
{
    struct sha1_ctx ctx;
    char bin_hash[SHA1_DIGEST_SIZE];
    char *hex_hash = g_malloc(SHA1_HEX_DIGEST_SIZE + 1);

    sha1_init(&ctx);
    sha1_update(&ctx, strlen(str), (const unsigned char *)str);
    sha1_digest(&ctx, SHA1_DIGEST_SIZE, (unsigned char *)bin_hash);
    sr_bin2hex(hex_hash, bin_hash, sizeof(bin_hash));
    hex_hash[SHA1_HEX_DIGEST_SIZE] = '\0';

    return hex_hash;
}
