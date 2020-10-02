/*
    utils.h

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
#ifndef SATYR_UTILS_H
#define SATYR_UTILS_H

/**
 * @file
 * @brief Various utility functions, macros and variables that do not
 * fit elsewhere.
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <glib.h>

#define SR_lower "abcdefghijklmnopqrstuvwxyz"
#define SR_upper "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define SR_alpha SR_lower SR_upper
#define SR_space " \t\r\n\v\f"
#define SR_digit "0123456789"
#define SR_alnum SR_alpha SR_digit

#define __sr_printf(x, y) __attribute__((format(printf, (x), (y))))

/**
 * Debugging output to stdout while parsing.
 * Default value is false.
 */
extern bool
sr_debug_parser;

/**
 * Never returns NULL.
 */
char *
sr_vasprintf(const char *format, va_list p);

/**
 * Never returns NULL.
 */
char *
sr_strdup(const char *s);

/**
 * Never returns NULL.
 */
char *
sr_strndup(const char *s, size_t n);

void
sr_struniq(char **strings, size_t *size);

/**
 * A strcmp() variant that works also with NULL parameters.  NULL is
 * considered to be less than a string.
 */
int
sr_strcmp0(const char *s1, const char *s2);

/**
 * A wrapper around sr_strcmp0 that takes pointers to strings. Can be used as a
 * paramter to qsort or bsearch.
 */
int
sr_ptrstrcmp(const void *s1, const void *s2);

/**
 * A strchr() variant providing line and column in the string s
 * indicating where the char c was found.
 * @param line
 * Starts from 1.  Its value is valid only when this function does not
 * return NULL.
 * @param column
 * Starts from 0.  Its value is valid only when this function does not
 * return NULL.
 */
char *
sr_strchr_location(const char *s, int c, int *line, int *column);

/**
 * A strstr() variant providing line and column of the haystick
 * indicating where the needle was found.
 * @param line
 * Starts from 1.  Its value is valid only when this function does not
 * return NULL.
 * @param column
 * Starts from 0.  Its value is valid only when this function does not
 * return NULL.
 */
char *
sr_strstr_location(const char *haystack,
                   const char *needle,
                   int *line,
                   int *column);

/**
 * A strspn() variant providing line and column of the string s which
 * corresponds to the returned length.
 * @param line
 * Starts from 1.
 * @param column
 * Starts from 0.
 */
size_t
sr_strspn_location(const char *s,
                   const char *accept,
                   int *line,
                   int *column);

/**
 * Loads file contents to a string.
 * @returns
 * File contents. If file opening/reading fails, NULL is returned.
 */
char *
sr_file_to_string(const char *filename,
                  char **error_message);

bool
sr_string_to_file(const char *filename,
                  char *contents,
                  char **error_message);

/**
 * If the input contains character c in the current positon, move the
 * input pointer after the character, and return true. Otherwise do
 * not modify the input and return false.
 */
bool
sr_skip_char(const char **input, char c);

/**
 * If the input contains one of allowed characters, move
 * the input pointer after that character, and return true.
 * Otherwise do not modify the input and return false.
 */
bool
sr_skip_char_limited(const char **input, const char *allowed);

/**
 * If the input contains one of allowed characters, store
 * the character to the result, move the input pointer after
 * that character, and return true. Otherwise do not modify
 * the input and return false.
 */
bool
sr_parse_char_limited(const char **input,
                      const char *allowed,
                      char *result);

/**
 * If the input contains the character c one or more times, update it
 * so that the characters are skipped. Returns the number of characters
 * skipped, thus zero if **input does not contain c.
 */
int
sr_skip_char_sequence(const char **input, char c);

/**
 * If the input contains one or more characters from string chars,
 * move the input pointer after the sequence. Otherwise do not modify
 * the input.
 * @returns
 * The number of characters skipped.
 */
int
sr_skip_char_span(const char **input, const char *chars);

/**
 * If the input contains one or more characters from string chars,
 * move the input pointer after the sequence. Otherwise do not modify
 * the input.
 * @param line
 * Starts from 1. Corresponds to the returned number.
 * @param column
 * Starts from 0. Corresponds to the returned number.
 * @returns
 * The number of characters skipped.
 */
int
sr_skip_char_span_location(const char **input,
                           const char *chars,
                           int *line,
                           int *column);

/**
 * If the input contains one or more characters from string accept,
 * create a string from this sequence and store it to the result, move
 * the input pointer after the sequence, and return the lenght of the
 * sequence.  Otherwise do not modify the input and return 0.
 *
 * If this function returns nonzero value, the caller is responsible
 * to free the result.
 */
int
sr_parse_char_span(const char **input,
                   const char *accept,
                   char **result);

/**
 * If the input contains one or more characters which are not
 * presentin string reject, move the input pointer after the
 * sequence. Otherwise do not modify the input.
 * @returns
 * The number of characters skipped.
 */
int
sr_skip_char_cspan(const char **input, const char *reject);

/**
 * If the input contains characters which are not in string reject,
 * create a string from this sequence and store it to the result,
 * move the input pointer after the sequence, and return true.
 * Otherwise do not modify the input and return false.
 *
 * If this function returns true, the caller is responsible to
 * free the result.
 */
bool
sr_parse_char_cspan(const char **input,
                    const char *reject,
                    char **result);

/**
 * If the input contains the string, move the input pointer after
 * the sequence. Otherwise do not modify the input.
 * @returns
 * Number of characters skipped. 0 if the input does not contain the
 * string.
 */
int
sr_skip_string(const char **input, const char *string);

/**
 * If the input contains the string, copy the string to result,
 * move the input pointer after the string, and return true.
 * Otherwise do not modify the input and return false.
 *
 * If this function returns true, the caller is responsible to free
 * the result.
 */
bool
sr_parse_string(const char **input, const char *string, char **result);

/**
 * If the input contains digit 0-9, return it as a character
 * and move the input pointer after it. Otherwise return
 * '\0' and do not modify the input.
 */
char
sr_parse_digit(const char **input);

/**
 * If the input contains [0-9]+, move the input pointer
 * after the number.
 * @returns
 * The number of skipped characters. 0 if input does not start with a
 * digit.
 */
int
sr_skip_uint(const char **input);

/**
 * If the input contains [0-9]+, parse it, move the input pointer
 * after the number.
 * @returns
 * Number of parsed characters. 0 if input does not contain a number.
 */
int
sr_parse_uint32(const char **input, uint32_t *result);

int
sr_parse_uint64(const char **input, uint64_t *result);

/**
 * If the input contains [0-9a-f]+, move the input pointer
 * after that.
 * @returns
 * The number of characters processed from input. 0 if the input does
 * not contain a hexadecimal number.
 */
int
sr_skip_hexadecimal_uint(const char **input);

/**
 * If the input contains 0x[0-9a-f]+, move the input pointer
 * after that.
 * @returns
 * The number of characters processed from input. 0 if the input does
 * not contain a hexadecimal number.
 */
int
sr_skip_hexadecimal_0xuint(const char **input);

/**
 * If the input contains [0-9a-f]+, parse the number, and move the
 * input pointer after it.  Otherwise do not modify the input.
 * @returns
 * The number of characters read from input. 0 if the input does not
 * contain a hexadecimal number.
 */
int
sr_parse_hexadecimal_uint64(const char **input, uint64_t *result);

/**
 * If the input contains 0x[0-9a-f]+, parse the number, and move the
 * input pointer after it.  Otherwise do not modify the input.
 * @returns
 * The number of characters read from input. 0 if the input does not
 * contain a hexadecimal number.
 */
int
sr_parse_hexadecimal_0xuint64(const char **input, uint64_t *result);

char *
sr_skip_whitespace(const char *s);

char *
sr_skip_non_whitespace(const char *s);

bool
sr_skip_to_next_line_location(const char **s, int *line, int *column);

/**
 * Emit a string of hex representation of bytes.
 */
char *
sr_bin2hex(char *dst, const char *str, int count);

char *
sr_indent(const char *input, int spaces);

char *
sr_indent_except_first_line(const char *input, int spaces);

char *
sr_build_path(const char *first_element, ...);

/**
 * Parses /etc/os-release file, see
 * http://www.freedesktop.org/software/systemd/man/os-release.html for more
 * information. Calls callback for each key-value pair it reads from the file.
 */
void
sr_parse_os_release(const char *input,
                    void (*callback)(char*, char*, void*),
                    void *data);

char*
anonymize_path(char *file_name);

/**
 * Demangles C++ symbol.
 * @returns
 * The demangled symbol (allocated by malloc), or NULL on failure.
 */
char *
sr_demangle_symbol(const char *sym);

#ifdef __cplusplus
}
#endif

#endif
