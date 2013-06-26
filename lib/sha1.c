/*
    sha1.c

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

    This is a byte oriented version of SHA1 that operates on arrays of bytes
    stored in memory. It runs at 22 cycles per byte on a Pentium P4 processor
*/
#include "sha1.h"
#include "utils.h"

#if defined(__BIG_ENDIAN__) && __BIG_ENDIAN__
# define SHA1_BIG_ENDIAN 1
# define SHA1_LITTLE_ENDIAN 0
#elif __BYTE_ORDER == __BIG_ENDIAN
# define SHA1_BIG_ENDIAN 1
# define SHA1_LITTLE_ENDIAN 0
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define SHA1_BIG_ENDIAN 0
# define SHA1_LITTLE_ENDIAN 1
#else
# error "Can't determine endianness"
#endif

#define rotl32(x,n) (((x) << (n)) | ((x) >> (32 - (n))))
/* for sha256: */
#define rotr32(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
/* for sha512: */
#define rotr64(x,n) (((x) >> (n)) | ((x) << (64 - (n))))

/* Generic 64-byte helpers for 64-byte block hashes */
static void
common64_hash(struct sr_sha1_state *state, const void *buffer, size_t len);

static void
common64_end(struct sr_sha1_state *state, int swap_needed);

/* sha1 specific code */

static void
sha1_process_block64(struct sr_sha1_state *state)
{
    static const uint32_t rconsts[] = {
        0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
    };
    int i, j;
    int cnt;
    uint32_t W[16+16];
    uint32_t a, b, c, d, e;

    /* On-stack work buffer frees up one register in the main loop
     * which otherwise will be needed to hold state pointer */
    for (i = 0; i < 16; i++)
        if (SHA1_BIG_ENDIAN)
            W[i] = W[i+16] = state->wbuffer.u4[i];
        else
            W[i] = W[i+16] = bswap_32(state->wbuffer.u4[i]);

    a = state->hash[0];
    b = state->hash[1];
    c = state->hash[2];
    d = state->hash[3];
    e = state->hash[4];

    /* 4 rounds of 20 operations each */
    cnt = 0;
    for (i = 0; i < 4; i++)
    {
        j = 19;
        do
        {
            uint32_t work;

            work = c ^ d;
            if (i == 0)
            {
                work = (work & b) ^ d;
                if (j <= 3)
                    goto ge16;
                /* Used to do bswap_32 here, but this
                 * requires state (see comment above) */
                work += W[cnt];
            }
            else
            {
                if (i == 2)
                    work = ((b | c) & d) | (b & c);
                else /* i = 1 or 3 */
                    work ^= b;
 ge16:
                W[cnt] = W[cnt+16] = rotl32(W[cnt+13] ^ W[cnt+8] ^ W[cnt+2] ^ W[cnt], 1);
                work += W[cnt];
            }
            work += e + rotl32(a, 5) + rconsts[i];

            /* Rotate by one for next time */
            e = d;
            d = c;
            c = /* b = */ rotl32(b, 30);
            b = a;
            a = work;
            cnt = (cnt + 1) & 15;
        } while (--j >= 0);
    }

    state->hash[0] += a;
    state->hash[1] += b;
    state->hash[2] += c;
    state->hash[3] += d;
    state->hash[4] += e;
}

void
sr_sha1_begin(struct sr_sha1_state *state)
{
    state->hash[0] = 0x67452301;
    state->hash[1] = 0xefcdab89;
    state->hash[2] = 0x98badcfe;
    state->hash[3] = 0x10325476;
    state->hash[4] = 0xc3d2e1f0;
    state->total64 = 0;
    /* for sha256: state->process_block = sha1_process_block64; */
}

void
sr_sha1_hash(struct sr_sha1_state *state,
              const void *buffer,
              size_t len)
{
    common64_hash(state, buffer, len);
}

/* May be used also for sha256 */
void
sr_sha1_end(struct sr_sha1_state *state, void *resbuf)
{
    unsigned hash_size;

    /* SHA stores total in BE, need to swap on LE arches: */
    common64_end(state, /*swap_needed:*/ SHA1_LITTLE_ENDIAN);

    hash_size = 5; /* (state->process_block == sha1_process_block64) ? 5 : 8; */
    /* This way we do not impose alignment constraints on resbuf: */
    if (SHA1_LITTLE_ENDIAN)
    {
        unsigned i;
        for (i = 0; i < hash_size; ++i)
            state->hash[i] = bswap_32(state->hash[i]);
    }
    memcpy(resbuf, state->hash, sizeof(state->hash[0]) * hash_size);
}

/* Generic 64-byte helpers for 64-byte block hashes */

/*#define PROCESS_BLOCK(state) state->process_block(state)*/
#define PROCESS_BLOCK(state) sha1_process_block64(state)

/* Feed data through a temporary buffer.
 * The internal buffer remembers previous data until it has 64
 * bytes worth to pass on.
 */
static void
common64_hash(struct sr_sha1_state *state,
              const void *buffer,
              size_t len)
{
    unsigned bufpos = state->total64 & 63;

    state->total64 += len;

    while (1)
    {
        unsigned remaining = 64 - bufpos;
        if (remaining > len)
            remaining = len;
        /* Copy data into aligned buffer */
        memcpy(state->wbuffer.u1 + bufpos, buffer, remaining);
        len -= remaining;
        buffer = (const char *)buffer + remaining;
        bufpos += remaining;
        /* clever way to do "if (bufpos != 64) break; ... ; bufpos = 0;" */
        bufpos -= 64;
        if (bufpos != 0)
            break;
        /* Buffer is filled up, process it */
        PROCESS_BLOCK(state);
        /*bufpos = 0; - already is */
    }
}

/* Process the remaining bytes in the buffer */
static void
common64_end(struct sr_sha1_state *state,
             int swap_needed)
{
    unsigned bufpos = state->total64 & 63;
    /* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0... */
    state->wbuffer.u1[bufpos++] = 0x80;

    /* This loop iterates either once or twice, no more, no less */
    while (1)
    {
        unsigned remaining = 64 - bufpos;
        memset(state->wbuffer.u1 + bufpos, 0, remaining);
        /* Do we have enough space for the length count? */
        if (remaining >= 8)
        {
            /* Store the 64-bit counter of bits in the buffer */
            uint64_t t = state->total64 << 3;
            if (swap_needed)
                t = bswap_64(t);
            state->wbuffer.u8[7] = t;
        }
        PROCESS_BLOCK(state);
        if (remaining >= 8)
            break;
        bufpos = 0;
    }
}

char *
sr_sha1_hash_string(const char *str)
{
    struct sr_sha1_state ctx;
    char bin_hash[SR_SHA1_RESULT_BIN_LEN];
    char *hex_hash = sr_malloc(SR_SHA1_RESULT_LEN);

    sr_sha1_begin(&ctx);
    sr_sha1_hash(&ctx, str, strlen(str));
    sr_sha1_end(&ctx, bin_hash);
    sr_bin2hex(hex_hash, bin_hash, sizeof(bin_hash))[0] = '\0';

    return hex_hash;
}
