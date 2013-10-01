/*
    utils.c

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
#include "utils.h"
#include "location.h"
#include "strbuf.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

bool
sr_debug_parser = false;

static const char
hexdigits_locase[] = "0123456789abcdef";

void
warn(const char *fmt, ...)
{
    va_list ap;

    if (!sr_debug_parser)
        return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

}

void *
sr_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        fprintf(stderr, "sr_malloc: out of memory");
        exit(1);
    }
    return ptr;
}

void *
sr_mallocz(size_t size)
{
    void *ptr = sr_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void *
sr_realloc(void *ptr, size_t size)
{
    void *result = realloc(ptr, size);
    /* When size is 0, realloc may return NULL on success. */
    if (!result && size > 0)
    {
        fprintf(stderr, "sr_realloc: out of memory");
        exit(1);
    }
    return result;
}

char *
sr_vasprintf(const char *format, va_list p)
{
    int r;
    char *string_ptr;

#if 1
    // GNU extension
    r = vasprintf(&string_ptr, format, p);
#else
    // Bloat for systems that haven't got the GNU extension.
    va_list p2;
    va_copy(p2, p);
    r = vsnprintf(NULL, 0, format, p);
    string_ptr = sr_malloc(r+1);
    r = vsnprintf(string_ptr, r+1, format, p2);
    va_end(p2);
#endif

    if (r < 0)
    {
        fprintf(stderr, "satyr: out of memory");
        exit(1);
    }

    return string_ptr;
}

char *
sr_asprintf(const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = sr_vasprintf(format, p);
    va_end(p);

    return string_ptr;
}


char *
sr_strdup(const char *s)
{
    return sr_strndup(s, strlen(s));
}

char *
sr_strndup(const char *s, size_t n)
{
    char *result = strndup(s, n);
    if (result == NULL)
    {
        fprintf(stderr, "satyr: out of memory");
        exit(1);
    }
    return result;
}

void
sr_struniq(char **strings, size_t *size)
{
    if (*size < 1)
        return;

    for (size_t loop = *size - 1; loop > 0; --loop)
    {
        if (0 == strcmp(strings[loop], strings[loop - 1]))
        {
            for (size_t loop2 = loop; loop2 < *size - 1; ++loop2)
                strings[loop2] = strings[loop2 + 1];

            --(*size);
        }
    }
}

int
sr_strcmp0(const char *s1, const char *s2)
{
    if (s1)
        return s2 ? strcmp(s1, s2) : 1;
    else
        return s2 ? -1 : 0;
}

int
sr_ptrstrcmp(const void *s1, const void *s2)
{
    return sr_strcmp0(*(const char**)s1, *(const char**)s2);
}

char *
sr_strchr_location(const char *s, int c, int *line, int *column)
{
    *line = 1;
    *column = 0;

    /* Scan s for the character.  When this loop is finished,
       s will either point to the end of the string or the
       character we were looking for.  */
    while (*s != '\0' && *s != (char)c)
    {
        sr_location_eat_char_ext(line, column, *s);
        ++s;
    }
    return ((*s == c) ? (char*)s : NULL);
}

char *
sr_strstr_location(const char *haystack,
                   const char *needle,
                   int *line,
                   int *column)
{
    *line = 1;
    *column = 0;
    size_t needlelen;

    /* Check for the null needle case.  */
    if (*needle == '\0')
        return (char*)haystack;

    needlelen = strlen(needle);
    int chrline, chrcolumn;
    for (;(haystack = sr_strchr_location(haystack, *needle, &chrline, &chrcolumn)) != NULL; ++haystack)
    {
        sr_location_add_ext(line, column, chrline, chrcolumn);

        if (strncmp(haystack, needle, needlelen) == 0)
            return (char*)haystack;

        sr_location_eat_char_ext(line, column, *haystack);
    }
    return NULL;
}

size_t
sr_strspn_location(const char *s,
                   const char *accept,
                   int *line,
                   int *column)
{
    *line = 1;
    *column = 0;
    const char *sc;
    for (sc = s; *sc != '\0'; ++sc)
    {
        if (strchr(accept, *sc) == NULL)
            return (sc - s);

        sr_location_eat_char_ext(line, column, *sc);
    }
    return sc - s; /* terminating nulls don't match */
}

char *
sr_file_to_string(const char *filename,
                  char **error_message)
{
    /* Open input file, and parse it. */
    int fd = open(filename, O_RDONLY | O_LARGEFILE);
    if (fd < 0)
    {
        *error_message = sr_asprintf("Unable to open '%s': %s.",
                                     filename,
                                     strerror(errno));

        return NULL;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == (off_t)-1) /* EOVERFLOW? */
    {
        *error_message = sr_asprintf("Unable to seek in '%s': %s.",
                                     filename,
                                     strerror(errno));

        close(fd);
        return NULL;
    }

    lseek(fd, 0, SEEK_SET); /* No reason to fail. */

    static const size_t FILE_SIZE_LIMIT = 20000000; /* ~ 20 MB */
    if (size > FILE_SIZE_LIMIT)
    {
        *error_message = sr_asprintf("Input file too big (%lld). Maximum size is %zd.",
                                     (long long)size,
                                     FILE_SIZE_LIMIT);

        close(fd);
        return NULL;
    }

    char *contents = sr_malloc(size + 1);
    if (size != read(fd, contents, size))
    {
        *error_message = sr_asprintf("Unable to read from '%s'.", filename);
        close(fd);
        free(contents);
        return NULL;
    }

    /* Just reading, so no need to check the returned value. */
    close(fd);

    contents[size] = '\0';
    return contents;
}

bool
sr_string_to_file(const char *filename,
                  char *contents,
                  char **error_message)
{
    /* Open the file. */
    int fd = open(filename, O_WRONLY | O_LARGEFILE | O_TRUNC | O_CREAT, 0640);
    if (fd < 0)
    {
        *error_message = sr_asprintf("Unable to open '%s': %s.",
                                     filename,
                                     strerror(errno));

        return false;
    }

    size_t count = strlen(contents);
    ssize_t write_result = write(fd, contents, count);
    if (count != write_result)
    {
        char *error = "Failed to write the contents to a file at once.";
        if (-1 == write_result)
            error = strerror(errno);

        *error_message = sr_asprintf("Unable to write to '%s': %s.",
                                     filename,
                                     error);

        return false;
    }

    int close_result = close(fd);
    if (-1 == close_result)
    {
        *error_message = sr_asprintf("Unable to close '%s': %s.",
                                     filename,
                                     strerror(errno));

        return false;
    }

    return true;
}

bool
sr_skip_char(const char **input, char c)
{
    if (**input != c)
        return false;
    ++*input;
    return true;
}

bool
sr_skip_char_limited(const char **input, const char *allowed)
{
    if (strchr(allowed, **input) == NULL)
        return false;
    ++*input;
    return true;
}

bool
sr_parse_char_limited(const char **input, const char *allowed, char *result)
{
    if (**input == '\0')
        return false;
    if (strchr(allowed, **input) == NULL)
        return false;
    *result = **input;
    ++*input;
    return true;
}

int
sr_skip_char_sequence(const char **input, char c)
{
    int count = 0;

    /* Skip all the occurences of c. */
    while (sr_skip_char(input, c))
        ++count;

    return count;
}

int
sr_skip_char_span(const char **input, const char *chars)
{
    size_t count = strspn(*input, chars);
    if (0 == count)
        return count;
    *input += count;
    return count;
}

int
sr_skip_char_span_location(const char **input,
                           const char *chars,
                           int *line,
                           int *column)
{
    size_t count = sr_strspn_location(*input, chars, line, column);
    if (0 == count)
        return count;
    *input += count;
    return count;
}

int
sr_parse_char_span(const char **input, const char *accept, char **result)
{
    size_t count = strspn(*input, accept);
    if (count == 0)
        return 0;
    *result = sr_strndup(*input, count);
    *input += count;
    return count;
}

int
sr_skip_char_cspan(const char **input, const char *reject)
{
    size_t count = strcspn(*input, reject);
    if (0 == count)
        return count;
    *input += count;
    return count;
}

bool
sr_parse_char_cspan(const char **input, const char *reject, char **result)
{
    size_t count = strcspn(*input, reject);
    if (count == 0)
        return false;
    *result = sr_strndup(*input, count);
    *input += count;
    return true;
}

int
sr_skip_string(const char **input, const char *string)
{
    const char *local_input = *input;
    const char *local_string = string;
    while (*local_string && *local_input && *local_input == *local_string)
    {
        ++local_input;
        ++local_string;
    }
    if (*local_string != '\0')
        return 0;
    int count = local_input - *input;
    *input = local_input;
    return count;
}

bool
sr_parse_string(const char **input, const char *string, char **result)
{
    const char *local_input = *input;
    const char *local_string = string;
    while (*local_string && *local_input && *local_input == *local_string)
    {
        ++local_input;
        ++local_string;
    }
    if (*local_string != '\0')
        return false;
    *result = sr_strndup(string, local_input - *input);
    *input = local_input;
    return true;
}

char
sr_parse_digit(const char **input)
{
    char digit = **input;
    if (digit < '0' || digit > '9')
        return '\0';
    ++*input;
    return digit;
}

int
sr_skip_uint(const char **input)
{
    return sr_skip_char_span(input, "0123456789");
}

int
sr_parse_uint32(const char **input, uint32_t *result)
{
    const char *local_input = *input;
    char *numstr;
    int length = sr_parse_char_span(&local_input,
                                    "0123456789",
                                    &numstr);

    if (0 == length)
        return 0;

    char *endptr;
    errno = 0;
    unsigned long r = strtoul(numstr, &endptr, 10);
    bool failure = (errno || numstr == endptr || *endptr != '\0'
                    || r > UINT32_MAX);

    free(numstr);
    if (failure) /* number too big or some other error */
        return 0;

    *result = r;
    *input = local_input;
    return length;
}

int
sr_parse_uint64(const char **input, uint64_t *result)
{
    const char *local_input = *input;
    char *numstr;
    int length = sr_parse_char_span(&local_input,
                                    "0123456789",
                                    &numstr);
    if (0 == length)
        return 0;

    char *endptr;
    errno = 0;
    unsigned long long result_tmp = strtoull(numstr, &endptr, 10);
    bool failure = (errno || numstr == endptr || *endptr != '\0'
                    || result_tmp == UINT64_MAX);
    free(numstr);
    if (failure) /* number too big or some other error */
        return 0;
    *result = result_tmp;
    *input = local_input;
    return length;
}

int
sr_skip_hexadecimal_uint(const char **input)
{
    return sr_skip_char_span(input, "abcdefABCDEF0123456789");
}

int
sr_skip_hexadecimal_0xuint(const char **input)
{
    const char *local_input = *input;
    if (!sr_skip_char(&local_input, '0'))
        return 0;

    if (!sr_skip_char(&local_input, 'x'))
        return 0;

    int count = 2;
    count += sr_skip_hexadecimal_uint(&local_input);
    if (2 == count) /* sr_skip_char_span returned 0 */
        return 0;

    *input = local_input;
    return count;
}

int
sr_parse_hexadecimal_uint64(const char **input, uint64_t *result)
{
    const char *local_input = *input;
    char *numstr;
    int count = sr_parse_char_span(&local_input,
                                   "abcdefABCDEF0123456789",
                                   &numstr);

    if (0 == count) /* sr_parse_char_span returned 0 */
        return 0;

    char *endptr;
    errno = 0;
    unsigned long long r = strtoull(numstr, &endptr, 16);
    bool failure = (errno || numstr == endptr || *endptr != '\0');
    free(numstr);
    if (failure) /* number too big or some other error */
        return 0;

    *result = r;
    *input = local_input;
    return count;
}

int
sr_parse_hexadecimal_0xuint64(const char **input, uint64_t *result)
{
    const char *local_input = *input;
    if (!sr_skip_char(&local_input, '0'))
        return 0;

    if (!sr_skip_char(&local_input, 'x'))
        return 0;

    int count = 2;
    count += sr_parse_hexadecimal_uint64(&local_input, result);
    if (2 == count) /* sr_parse_hexadecimal_uint64 returned 0 */
        return 0;

    *input = local_input;
    return count;
}

char *
sr_skip_whitespace(const char *s)
{
    /* NB: isspace('\0') returns 0 */
    while (isspace(*s))
            ++s;

    return (char *) s;
}

char *
sr_skip_non_whitespace(const char *s)
{
    while (*s && !isspace(*s))
            ++s;

    return (char *) s;
}

char *
sr_bin2hex(char *dst, const char *str, int count)
{
    while (count)
    {
        unsigned char c = *str++;
        /* put lowercase hex digits */
        *dst++ = hexdigits_locase[c >> 4];
        *dst++ = hexdigits_locase[c & 0xf];
        count--;
    }

    return dst;
}

char *
sr_indent(const char *input, int spaces)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();
    if (*input)
    {
        for (int i = 0; i < spaces; ++i)
            sr_strbuf_append_char(strbuf, ' ');
    }

    char *indented = sr_indent_except_first_line(input, spaces);
    sr_strbuf_append_str(strbuf, indented);
    free(indented);

    return sr_strbuf_free_nobuf(strbuf);
}

char *
sr_indent_except_first_line(const char *input, int spaces)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    const char *c = input;
    while (*c)
    {
        if (*c == '\n')
        {
            sr_strbuf_append_char(strbuf, '\n');
            if (*++c)
            {
                for (int i = 0; i < spaces; ++i)
                    sr_strbuf_append_char(strbuf, ' ');
            }

            continue;
        }
        else
            sr_strbuf_append_char(strbuf, *c);

        ++c;
    }

    return sr_strbuf_free_nobuf(strbuf);
}

char *
sr_build_path(const char *first_element, ...)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();
    sr_strbuf_append_str(strbuf, first_element);

    va_list elements;
    va_start(elements, first_element);

    const char *element;
    while ((element = va_arg(elements, const char *)))
    {
        sr_strbuf_append_char(strbuf, '/');
        sr_strbuf_append_str(strbuf, element);
    }

    va_end(elements);
    return sr_strbuf_free_nobuf(strbuf);
}

static void
unescape_osinfo_value(const char *source, char* dest)
{
    while (source[0] != '\0')
    {
        if (source[0] == '\\')
        {   /* some characters may be escaped -> remove '\' */
            ++source;
            if (source[0] == '\0')
                break;
            *dest++ = *source++;
        }
        else if (source[0] == '\'' || source[0] == '"')
        {   /* skip non escaped quotes and don't care where they are */
            ++source;
        }
        else
        {
            *dest++ = *source++;
        }
    }
    dest[0] = '\0';
}

void
sr_parse_os_release(const char *input, void (*callback)(char*, char*, void*),
                    void *data)
{
    const char *cursor = input;
    unsigned line = 0;
    while (cursor[0] != '\0')
    {
        ++line;
        if (cursor[0] == '#')
            goto skip_line;

        const char *key_end = strchrnul(cursor, '=');
        if (key_end[0] == '\0')
        {
            warn("os-release:%u: non empty last line", line);
            break;
        }

        if (key_end - cursor == 0)
        {
            warn("os-release:%u: 0 length key", line);
            goto skip_line;
        }

        const char *value_end = strchrnul(cursor, '\n');
        if (key_end > value_end)
        {
            warn("os-release:%u: missing '='", line);
            goto skip_line;
        }

        char *key = sr_strndup(cursor, key_end - cursor);
        char *value = sr_strndup(key_end + 1, value_end - key_end - 1);
        unescape_osinfo_value(value, value);

        warn("os-release:%u: parsed line: '%s'='%s'", line, key, value);

        callback(key, value, data);

        cursor = value_end;
        if (value_end[0] == '\0')
        {
            warn("os-release:%u: the last value is not terminated by newline", line);
        }
        else
            ++cursor;

        continue;
  skip_line:
        cursor = strchrnul(cursor, '\n');
        cursor += (cursor[0] != '\0');
    }
}
