/*
    strbuf.c - string buffer

    Copyright (C) 2010  Red Hat, Inc.

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
#include "strbuf.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

struct sr_strbuf *
sr_strbuf_new()
{
    struct sr_strbuf *strbuf = sr_malloc(sizeof(struct sr_strbuf));
    sr_strbuf_init(strbuf);
    return strbuf;
}

void
sr_strbuf_init(struct sr_strbuf *strbuf)
{
    strbuf->alloc = 8;
    strbuf->len = 0;
    strbuf->buf = sr_malloc(strbuf->alloc);
    strbuf->buf[strbuf->len] = '\0';
}

void
sr_strbuf_free(struct sr_strbuf *strbuf)
{
    if (!strbuf)
        return;

    free(strbuf->buf);
    free(strbuf);
}

char *
sr_strbuf_free_nobuf(struct sr_strbuf *strbuf)
{
    char *buf = strbuf->buf;
    free(strbuf);
    return buf;
}


void
sr_strbuf_clear(struct sr_strbuf *strbuf)
{
    assert(strbuf->alloc > 0);
    strbuf->len = 0;
    strbuf->buf[0] = '\0';
}

/* Ensures that the buffer can be extended by num characters
 * without touching malloc/realloc.
 */
void
sr_strbuf_grow(struct sr_strbuf *strbuf, int num)
{
    if (strbuf->len + num + 1 > strbuf->alloc)
    {
        while (strbuf->len + num + 1 > strbuf->alloc)
            strbuf->alloc *= 2; /* huge grow = infinite loop */

        strbuf->buf = sr_realloc(strbuf->buf, strbuf->alloc);
    }
}

struct sr_strbuf *
sr_strbuf_append_char(struct sr_strbuf *strbuf,
                      char c)
{
    sr_strbuf_grow(strbuf, 1);
    strbuf->buf[strbuf->len++] = c;
    strbuf->buf[strbuf->len] = '\0';
    return strbuf;
}

struct sr_strbuf *
sr_strbuf_append_str(struct sr_strbuf *strbuf,
                     const char *str)
{
    int len = strlen(str);
    sr_strbuf_grow(strbuf, len);
    assert(strbuf->len + len < strbuf->alloc);
    strcpy(strbuf->buf + strbuf->len, str);
    strbuf->len += len;
    return strbuf;
}

struct sr_strbuf *
sr_strbuf_prepend_str(struct sr_strbuf *strbuf,
                      const char *str)
{
    int len = strlen(str);
    sr_strbuf_grow(strbuf, len);
    assert(strbuf->len + len < strbuf->alloc);
    memmove(strbuf->buf + len, strbuf->buf, strbuf->len + 1);
    memcpy(strbuf->buf, str, len);
    strbuf->len += len;
    return strbuf;
}

struct sr_strbuf *
sr_strbuf_append_strfv(struct sr_strbuf *strbuf,
                       const char *format, va_list p)
{
    char *string_ptr = sr_vasprintf(format, p);
    sr_strbuf_append_str(strbuf, string_ptr);
    free(string_ptr);
    return strbuf;
}

struct sr_strbuf *
sr_strbuf_append_strf(struct sr_strbuf *strbuf,
                      const char *format, ...)
{
    va_list p;

    va_start(p, format);
    sr_strbuf_append_strfv(strbuf, format, p);
    va_end(p);

    return strbuf;
}

struct sr_strbuf *
sr_strbuf_prepend_strfv(struct sr_strbuf *strbuf,
                        const char *format, va_list p)
{
    char *string_ptr = sr_vasprintf(format, p);
    sr_strbuf_prepend_str(strbuf, string_ptr);
    free(string_ptr);
    return strbuf;
}

struct sr_strbuf *
sr_strbuf_prepend_strf(struct sr_strbuf *strbuf,
                       const char *format, ...)
{
    va_list p;

    va_start(p, format);
    sr_strbuf_prepend_strfv(strbuf, format, p);
    va_end(p);

    return strbuf;
}
