/*
    strbuf.h - a string buffer

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
#ifndef SATYR_STRBUF_H
#define SATYR_STRBUF_H

/**
 * @file
 * @brief A string buffer structure and related algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/**
 * @brief A resizable string buffer.
 */
struct sr_strbuf
{
    /**
     * Size of the allocated buffer. Always > 0.
     */
    int alloc;
    /**
     * Length of the string, without the ending \0.
     */
    int len;
    char *buf;
};

/**
 * Creates and initializes a new string buffer.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_strbuf_free().
 */
struct sr_strbuf *
sr_strbuf_new();

/**
 * Initializes all members of the strbuf structure to their default
 * values. No memory is released, members are simply overritten. This
 * is useful for initializing a strbuf structure placed on the stack.
 */
void
sr_strbuf_init(struct sr_strbuf *strbuf);

/**
 * Releases the memory held by the string buffer.
 * @param strbuf
 * If the strbuf is NULL, no operation is performed.
 */
void
sr_strbuf_free(struct sr_strbuf *strbuf);

/**
 * Releases the strbuf, but not the internal buffer.  The internal
 * string buffer is returned.  Caller is responsible to release the
 * returned memory using free().
 */
char *
sr_strbuf_free_nobuf(struct sr_strbuf *strbuf);

/**
 * The string content is set to an empty string, erasing any previous
 * content and leaving its length at 0 characters.
 */
void
sr_strbuf_clear(struct sr_strbuf *strbuf);

/**
 * Ensures that the buffer can be extended by num characters
 * without dealing with malloc/realloc.
 */
void
sr_strbuf_grow(struct sr_strbuf *strbuf, int num);

/**
 * The current content of the string buffer is extended by adding a
 * character c at its end.
 */
struct sr_strbuf *
sr_strbuf_append_char(struct sr_strbuf *strbuf,
                      char c);

/**
 * The current content of the string buffer is extended by adding a
 * string str at its end.
 */
struct sr_strbuf *
sr_strbuf_append_str(struct sr_strbuf *strbuf,
                     const char *str);

/**
 * The current content of the string buffer is extended by inserting a
 * string str at its beginning.
 */
struct sr_strbuf *
sr_strbuf_prepend_str(struct sr_strbuf *strbuf,
                      const char *str);

/**
 * The current content of the string buffer is extended by adding a
 * sequence of data formatted as the format argument specifies.
 */
struct sr_strbuf *
sr_strbuf_append_strf(struct sr_strbuf *strbuf,
                      const char *format, ...);

/**
 * Same as sr_strbuf_append_strf except that va_list is used instead of
 * variable number of arguments.
 */
struct sr_strbuf *
sr_strbuf_append_strfv(struct sr_strbuf *strbuf,
                       const char *format, va_list p);

/**
 * The current content of the string buffer is extended by inserting a
 * sequence of data formatted as the format argument specifies at the
 * buffer beginning.
 */
struct sr_strbuf *
sr_strbuf_prepend_strf(struct sr_strbuf *strbuf,
                       const char *format, ...);

/**
 * Same as sr_strbuf_prepend_strf except that va_list is used instead of
 * variable number of arguments.
 */
struct sr_strbuf *
sr_strbuf_prepend_strfv(struct sr_strbuf *strbuf,
                        const char *format, va_list p);

#ifdef __cplusplus
}
#endif

#endif
