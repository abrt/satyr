/*
    gdb_frame.c

    Copyright (C) 2010, 2011, 2012  Red Hat, Inc.

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
#include "gdb/frame.h"
#include "utils.h"
#include "strbuf.h"
#include "location.h"
#include "generic_frame.h"
#include "thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

/* Method table */

static void
gdb_append_bthash_text(struct sr_gdb_frame *frame, enum sr_bthash_flags flags,
                       struct sr_strbuf *strbuf);
static void
gdb_append_duphash_text(struct sr_gdb_frame *frame, enum sr_duphash_flags flags,
                        struct sr_strbuf *strbuf);

DEFINE_NEXT_FUNC(gdb_next, struct sr_frame, struct sr_gdb_frame)
DEFINE_SET_NEXT_FUNC(gdb_set_next, struct sr_frame, struct sr_gdb_frame)

static int
frame_cmp_without_number(struct sr_gdb_frame *frame1, struct sr_gdb_frame *frame2)
{
    return sr_gdb_frame_cmp(frame1, frame2, false);
}

struct frame_methods gdb_frame_methods =
{
    .append_to_str = (append_to_str_fn_t) sr_gdb_frame_append_to_str,
    .next = (next_frame_fn_t) gdb_next,
    .set_next = (set_next_frame_fn_t) gdb_set_next,
    .cmp = (frame_cmp_fn_t) frame_cmp_without_number,
    .cmp_distance = (frame_cmp_fn_t) sr_gdb_frame_cmp_distance,
    .frame_append_bthash_text =
        (frame_append_bthash_text_fn_t) gdb_append_bthash_text,
    .frame_append_duphash_text =
        (frame_append_duphash_text_fn_t) gdb_append_duphash_text,
    .frame_free = (frame_free_fn_t) sr_gdb_frame_free,
};

/* Public functions */

struct sr_gdb_frame *
sr_gdb_frame_new()
{
    struct sr_gdb_frame *frame = sr_malloc(sizeof(struct sr_gdb_frame));
    sr_gdb_frame_init(frame);
    return frame;
}

void
sr_gdb_frame_init(struct sr_gdb_frame *frame)
{
    frame->function_name = NULL;
    frame->function_type = NULL;
    frame->number = 0;
    frame->source_file = NULL;
    frame->source_line = -1;
    frame->signal_handler_called = false;
    frame->address = -1;
    frame->library_name = NULL;
    frame->next = NULL;
    frame->type = SR_REPORT_GDB;
}

void
sr_gdb_frame_free(struct sr_gdb_frame *frame)
{
    if (!frame)
        return;

    free(frame->function_name);
    free(frame->function_type);
    free(frame->source_file);
    free(frame->library_name);
    free(frame);
}

struct sr_gdb_frame *
sr_gdb_frame_dup(struct sr_gdb_frame *frame, bool siblings)
{
    if (!frame)
        return NULL;

    struct sr_gdb_frame *result = sr_gdb_frame_new();
    memcpy(result, frame, sizeof(struct sr_gdb_frame));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_gdb_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->function_name)
        result->function_name = sr_strdup(result->function_name);
    if (result->function_type)
        result->function_type = sr_strdup(result->function_type);
    if (result->source_file)
        result->source_file = sr_strdup(result->source_file);
    if (result->library_name)
        result->library_name = sr_strdup(result->library_name);

    return result;
}

bool
sr_gdb_frame_calls_func(struct sr_gdb_frame *frame,
                        const char *function_name,
                        ...)
{
    if (!frame->function_name ||
        0 != strcmp(frame->function_name, function_name))
    {
        return false;
    }

    va_list source_files;
    va_start(source_files, function_name);
    bool success = false;

    while (true)
    {
        char *source_file = va_arg(source_files, char*);
        if (!source_file)
            break;

        if (frame->source_file &&
            NULL != strstr(frame->source_file, source_file))
        {
            success = true;
            break;
        }
    }

   va_end(source_files);
   return success;
}

int
sr_gdb_frame_cmp(struct sr_gdb_frame *frame1,
                 struct sr_gdb_frame *frame2,
                 bool compare_number)
{
    /* Singnal handler. */
    if (frame1->signal_handler_called)
    {
        if (!frame2->signal_handler_called)
            return 1;

        /* Both contain signal handler called. */
        return 0;
    }
    else
    {
        if (frame2->signal_handler_called)
            return -1;
        /* No signal handler called, continue. */
    }

    /* Function. */
    int function_name = sr_strcmp0(frame1->function_name,
                                   frame2->function_name);
    if (function_name != 0)
        return function_name;

    int function_type = sr_strcmp0(frame1->function_type,
                                   frame2->function_type);
    if (function_type != 0)
        return function_type;

    /* Sourcefile. */
    int source_file = sr_strcmp0(frame1->source_file,
                                 frame2->source_file);
    if (source_file != 0)
        return source_file;

    /* Sourceline. */
    int source_line = frame1->source_line - frame2->source_line;
    if (source_line != 0)
        return source_line;

    /* Library name. */
    int library_name = sr_strcmp0(frame1->library_name,
                                  frame2->library_name);
    if (library_name != 0)
        return library_name;

    /* Frame number. */
    if (compare_number)
    {
        int number = frame1->number - frame2->number;
        if (number != 0)
            return number;
    }

    return 0;
}

int
sr_gdb_frame_cmp_distance(struct sr_gdb_frame *frame1,
                          struct sr_gdb_frame *frame2)
{
    if (sr_strcmp0(frame1->function_name, "??") == 0 &&
        sr_strcmp0(frame2->function_name, "??") == 0)
        return -1;

    int function_name = sr_strcmp0(frame1->function_name,
                                   frame2->function_name);
    if (function_name != 0)
        return function_name;

    /* Assume they are the same if one of them is not known. */
    if (frame1->library_name && frame2->library_name)
    {
        return strcmp(frame1->library_name,
                      frame2->library_name);
    }

    return 0;
}

struct sr_gdb_frame *
sr_gdb_frame_append(struct sr_gdb_frame *dest,
                    struct sr_gdb_frame *item)
{
    if (!dest)
        return item;

    struct sr_gdb_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

void
sr_gdb_frame_append_to_str(struct sr_gdb_frame *frame,
                           struct sr_strbuf *str,
                            bool verbose)
{
    if (verbose)
        sr_strbuf_append_strf(str, " #%d", frame->number);
    else
        sr_strbuf_append_str(str, " ");

    if (frame->function_type)
        sr_strbuf_append_strf(str, " %s", frame->function_type);
    if (frame->function_name)
        sr_strbuf_append_strf(str, " %s", frame->function_name);
    if (verbose && frame->source_file)
    {
        if (frame->function_name)
            sr_strbuf_append_str(str, " at");
        sr_strbuf_append_strf(str, " %s", frame->source_file);
        if (frame->source_line != -1)
            sr_strbuf_append_strf(str, ":%d", frame->source_line);
    }

    if (frame->signal_handler_called)
        sr_strbuf_append_str(str, " <signal handler called>");
}

/**
 * Find string a or b in input, whichever comes first.
 * @param location
 * Location structure, which is updated during the search.
 * @returns
 * Return pointer to input to first a or b string.
 * If neither a nor b string is found, return the \0 character at the
 * end of input.
 */
static const char *
findfirstabnul(const char *input,
               const char *a,
               const char *b,
               struct sr_location *location)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    const char *p = input;
    while (*p)
    {
        if (strncmp(p, a, alen) == 0)
            return p;
        if (strncmp(p, b, blen) == 0)
            return p;
        sr_location_eat_char(location, *p);
        ++p;
    }
    return p;
}

struct sr_gdb_frame *
sr_gdb_frame_parse(const char **input,
                   struct sr_location *location)
{
    const char *local_input = *input;
    struct sr_gdb_frame *header = sr_gdb_frame_parse_header(input, location);
    if (!header)
        return NULL;

    /* Skip the variables section for now. */
    local_input = findfirstabnul(local_input, "\n#", "\nThread", location);
    if (*local_input != '\0')
    {
        /* skip the newline */
        sr_location_eat_char(location, *local_input);
        ++local_input;
    }

    if (sr_debug_parser)
    {
        printf("frame #%u %s\n",
               header->number,
               header->function_name ? header->function_name : "signal handler called");
    }

    *input = local_input;
    return header;
}

int
sr_gdb_frame_parse_frame_start(const char **input, uint32_t *number)
{
    const char *local_input = *input;

    /* Read the hash sign. */
    if (!sr_skip_char(&local_input, '#'))
        return 0;
    int count = 1;

    /* Read the frame position. */
    int digits = sr_parse_uint32(&local_input, number);
    count += digits;
    if (0 == digits)
        return 0;

    /* Read all the spaces after the positon. */
    int spaces = sr_skip_char_sequence(&local_input, ' ');
    count += spaces;
    if (0 == spaces)
        return 0;

    *input = local_input;
    return count;
}

int
sr_gdb_frame_parseadd_operator(const char **input,
                               struct sr_strbuf *target)
{
    const char *local_input = *input;
    if (0 == sr_skip_string(&local_input, "operator"))
        return 0;

#define OP(x) \
    if (0 < sr_skip_string(&local_input, x))      \
    {                                             \
        sr_strbuf_append_str(target, "operator"); \
        sr_strbuf_append_str(target, x);          \
        int length = local_input - *input;        \
        *input = local_input;                     \
        return length;                            \
    }

    OP(">>=")OP(">>")OP(">=")OP(">");
    OP("<<=")OP("<<")OP("<=")OP("<");
    OP("->*")OP("->")OP("-");
    OP("==")OP("=");
    OP("&&")OP("&=")OP("&");
    OP("||")OP("|=")OP("|");
    OP("++")OP("+=")OP("+");
    OP("--")OP("-=")OP("-");
    OP("/=")OP("/");
    OP("*=")OP("*");
    OP("%=")OP("%");
    OP("!=")OP("!");
    OP("~");
    OP("()");
    OP("[]");
    OP(",");
    OP("^=")OP("^");
    OP(" new[]")OP(" new");
    OP(" delete[]")OP(" delete");
    /* User defined operators are not parsed.
       Should they be? */
#undef OP
    return 0;
}

#define FUNCTION_NAME_CHARS SR_alnum "@.:=!*+-[]~&/%^|,_"

int
sr_gdb_frame_parse_function_name_chunk(const char **input,
                                       bool space_allowed,
                                       char **target)
{
    const char *local_input = *input;
    struct sr_strbuf *buf = sr_strbuf_new();
    while (*local_input)
    {
        if (0 < sr_gdb_frame_parseadd_operator(&local_input, buf))
        {
            /* Space is allowed after operator even when it
               is not normally allowed. */
            if (sr_skip_char(&local_input, ' '))
            {
                /* ...but if ( follows, it is not allowed. */
                if (sr_skip_char(&local_input, '('))
                {
                    /* Return back both the space and (. */
                    local_input -= 2;
                }
                else
                    sr_strbuf_append_char(buf, ' ');
            }
        }

        if (strchr(FUNCTION_NAME_CHARS, *local_input) == NULL)
        {
            if (!space_allowed || strchr(" ", *local_input) == NULL)
                break;
        }

        sr_strbuf_append_char(buf, *local_input);
        ++local_input;
    }

    if (buf->len == 0)
    {
        sr_strbuf_free(buf);
        return 0;
    }

    *target = sr_strbuf_free_nobuf(buf);
    int total_char_count = local_input - *input;
    *input = local_input;
    return total_char_count;
}

int
sr_gdb_frame_parse_function_name_braces(const char **input, char **target)
{
    const char *local_input = *input;
    if (!sr_skip_char(&local_input, '('))
        return 0;

    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_char(buf, '(');
    while (true)
    {
        char *namechunk = NULL;
        if (0 < sr_gdb_frame_parse_function_name_chunk(&local_input, true, &namechunk) ||
            0 < sr_gdb_frame_parse_function_name_braces(&local_input, &namechunk) ||
            0 < sr_gdb_frame_parse_function_name_template(&local_input, &namechunk))
        {
            sr_strbuf_append_str(buf, namechunk);
            free(namechunk);
        }
        else
            break;
    }

    if (!sr_skip_char(&local_input, ')'))
    {
        sr_strbuf_free(buf);
        return 0;
    }

    sr_strbuf_append_char(buf, ')');
    *target = sr_strbuf_free_nobuf(buf);
    int total_char_count = local_input - *input;
    *input = local_input;
    return total_char_count;
}

int
sr_gdb_frame_parse_function_name_template(const char **input, char **target)
{
    const char *local_input = *input;
    if (!sr_skip_char(&local_input, '<'))
        return 0;

    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_char(buf, '<');
    while (true)
    {
        char *namechunk = NULL;
        if (0 < sr_gdb_frame_parse_function_name_chunk(&local_input, true, &namechunk) ||
            0 < sr_gdb_frame_parse_function_name_braces(&local_input, &namechunk) ||
            0 < sr_gdb_frame_parse_function_name_template(&local_input, &namechunk))
        {
            sr_strbuf_append_str(buf, namechunk);
            free(namechunk);
        }
        else
            break;
    }

    if (!sr_skip_char(&local_input, '>'))
    {
        sr_strbuf_free(buf);
        return 0;
    }

    sr_strbuf_append_char(buf, '>');
    *target = sr_strbuf_free_nobuf(buf);
    int total_char_count = local_input - *input;
    *input = local_input;
    return total_char_count;
}

bool
sr_gdb_frame_parse_function_name(const char **input,
                                 char **function_name,
                                 char **function_type,
                                 struct sr_location *location)
{
    /* Handle unknown function name, represended by double question
       mark. */
    if (sr_parse_string(input, "??", function_name))
    {
        *function_type = NULL;
        location->column += 2;
        return true;
    }

    const char *local_input = *input;
    /* Up to three parts of function name. */
    struct sr_strbuf *buf0 = sr_strbuf_new(), *buf1 = NULL;

    /* First character:
       '~' for destructor
       '*' for ????
       '_a-zA-Z' for mangled/nonmangled function name
       '(' to start "(anonymous namespace)::" or something
     */
    char first;
    char *namechunk;
    if (sr_parse_char_limited(&local_input, "~*_" SR_alpha, &first))
    {
        /* If it's a start of 'o'perator, put the 'o' back! */
        if (first == 'o')
            --local_input;
        else
        {
            sr_strbuf_append_char(buf0, first);
            ++location->column;
        }
    }
    else
    {
        int chars = sr_gdb_frame_parse_function_name_braces(&local_input,
                                                            &namechunk);
        if (0 < chars)
        {
            sr_strbuf_append_str(buf0, namechunk);
            free(namechunk);
            location->column += chars;
        }
        else
        {
            location->message = "Expected function name.";
            sr_strbuf_free(buf0);
            return false;
        }
    }

    /* The rest consists of function name, braces, templates...*/
    while (true)
    {
        char *namechunk = NULL;
        int chars = sr_gdb_frame_parse_function_name_chunk(&local_input,
                                                           false,
                                                           &namechunk);

        if (0 == chars)
        {
            chars = sr_gdb_frame_parse_function_name_braces(&local_input,
                                                            &namechunk);
        }

        if (0 == chars)
        {
            chars = sr_gdb_frame_parse_function_name_template(&local_input,
                                                              &namechunk);
        }

        if (0 == chars)
            break;

        sr_strbuf_append_str(buf0, namechunk);
        free(namechunk);
        location->column += chars;
    }

    /* Function name MUST be ended by empty space. */
    char space;
    if (!sr_parse_char_limited(&local_input, SR_space, &space))
    {
        sr_strbuf_free(buf0);
        location->message = "Space or newline expected after function name.";
        return false;
    }

    /* Some C++ function names and function types might contain suffix
       " const". */
    int chars = sr_skip_string(&local_input, "const");
    if (0 < chars)
    {
        sr_strbuf_append_char(buf0, space);
        sr_location_eat_char(location, space);
        sr_strbuf_append_str(buf0, "const");
        location->column += chars;

        /* Check the empty space after function name again.*/
        if (!sr_parse_char_limited(&local_input, SR_space, &space))
        {
            /* Function name MUST be ended by empty space. */
            sr_strbuf_free(buf0);
            location->message = "Space or newline expected after function name.";
            return false;
        }
    }

    /* Maybe the first series was just a type of the function, and now
       the real function follows. Now, we know it must not start with
       '(', nor with '<'. */
    chars = sr_gdb_frame_parse_function_name_chunk(&local_input,
                                                   false,
                                                   &namechunk);
    if (0 < chars)
    {
        /* Eat the space separator first. */
        sr_location_eat_char(location, space);

        buf1 = sr_strbuf_new();
        sr_strbuf_append_str(buf1, namechunk);
        free(namechunk);
        location->column += chars;

        /* The rest consists of a function name parts, braces, templates...*/
        while (true)
        {
            char *namechunk = NULL;
            chars = sr_gdb_frame_parse_function_name_chunk(&local_input,
                                                           false,
                                                           &namechunk);
            if (0 == chars)
            {
                chars = sr_gdb_frame_parse_function_name_braces(&local_input,
                                                                &namechunk);
            }
            if (0 == chars)
            {
                chars = sr_gdb_frame_parse_function_name_template(&local_input,
                                                                  &namechunk);
            }
            if (0 == chars)
                break;

            sr_strbuf_append_str(buf1, namechunk);
            free(namechunk);
            location->column += chars;
        }

        /* Function name MUST be ended by empty space. */
        if (!sr_parse_char_limited(&local_input, SR_space, &space))
        {
            sr_strbuf_free(buf0);
            sr_strbuf_free(buf1);
            location->message = "Space or newline expected after function name.";
            return false;
        }
    }

    /* Again, some C++ function names might contain suffix " const" */
    chars = sr_skip_string(&local_input, "const");
    if (0 < chars)
    {
        struct sr_strbuf *buf = buf1 ? buf1 : buf0;
        sr_strbuf_append_char(buf, space);
        sr_location_eat_char(location, space);
        sr_strbuf_append_str(buf, "const");
        location->column += chars;

        /* Check the empty space after function name again.*/
        if (!sr_skip_char_limited(&local_input, SR_space))
        {
            /* Function name MUST be ended by empty space. */
            sr_strbuf_free(buf0);
            sr_strbuf_free(buf1);
            location->message = "Space or newline expected after function name.";
            return false;
        }
    }

    /* Return back to the empty space. */
    --local_input;

    if (buf1)
    {
        *function_name = sr_strbuf_free_nobuf(buf1);
        *function_type = sr_strbuf_free_nobuf(buf0);
    }
    else
    {
        *function_name = sr_strbuf_free_nobuf(buf0);
        *function_type = NULL;
    }

    *input = local_input;
    return true;
}

bool
sr_gdb_frame_skip_function_args(const char **input,
                                struct sr_location *location)
{
    const char *local_input = *input;
    if (!sr_skip_char(&local_input, '('))
    {
        location->message = "Expected '(' to start function argument list.";
        return false;
    }
    location->column += 1;

    int depth = 0;
    bool string = false;
    bool escape = false;
    do
    {
        /* Sometimes function args would be too long so printer
         * truncates them. See frame #33 in rhbz#803600.
         */
        if (!escape && 0 == strncmp(local_input,
                                    "(truncated)",
                                    strlen("(truncated)")))
        {
            depth = 0;
            string = false;
        }

        if (string)
        {
            if (escape)
                escape = false;
            else if (*local_input == '\\')
                escape = true;
            else if (*local_input == '"')
                string = false;
        }
        else
        {
            if (*local_input == '"')
                string = true;
            else if (*local_input == '(')
                ++depth;
            else if (*local_input == ')')
            {
                if (depth > 0)
                    --depth;
                else
                    break;
            }
        }
        sr_location_eat_char(location, *local_input);
        ++local_input;
    }
    while (*local_input);

    if (depth != 0 || string || escape)
    {
        location->message = "Unbalanced function parameter list.";
        return false;
    }

    if (!sr_skip_char(&local_input, ')'))
    {
        location->message = "Expected ')' to close the function parameter list.";
        return false;
    }
    location->column += 1;

    *input = local_input;
    return true;
}

bool
sr_gdb_frame_parse_function_call(const char **input,
                                 char **function_name,
                                 char **function_type,
                                 struct sr_location *location)
{
    const char *local_input = *input;
    char *name = NULL, *type = NULL;
    if (!sr_gdb_frame_parse_function_name(&local_input,
                                          &name,
                                          &type,
                                          location))
    {
        /* The location message is set by the function returning
         * false, no need to update it here. */
        return false;
    }

    int line, column;
    if (0 == sr_skip_char_span_location(&local_input,
                                        " \n",
                                        &line,
                                        &column))
    {
        free(name);
        free(type);
        location->message = "Expected a space or newline after the function name.";
        return false;
    }

    sr_location_add(location, line, column);

    if (!sr_gdb_frame_skip_function_args(&local_input, location))
    {
        free(name);
        free(type);
        /* The location message is set by the function returning
         * false, no need to update it here. */
        return false;
    }

    *function_name = name;
    *function_type = type;
    *input = local_input;
    return true;
}

bool
sr_gdb_frame_parse_address_in_function(const char **input,
                                       uint64_t *address,
                                       char **function_name,
                                       char **function_type,
                                       struct sr_location *location)
{
    const char *local_input = *input;

    /* Read memory address in hexadecimal format. */
    int digits = sr_parse_hexadecimal_0xuint64(&local_input, address);
    location->column += digits;
    /* Memory address is optional. It is not present for inlined frames. */
    if (digits == 0)
        *address = -1;
    else
    {
        /* Skip spaces. */
        int chars = sr_skip_char_sequence(&local_input, ' ');
        location->column += chars;
        if (0 == chars)
        {
            location->message = "Space expected after memory address.";
            return false;
        }

        /* Skip keyword "in". */
        chars = sr_skip_string(&local_input, "in");
        location->column += chars;
        if (0 == chars)
        {
            location->message = "Keyword \"in\" expected after memory address.";
            return false;
        }

        /* Skip spaces. */
        chars = sr_skip_char_sequence(&local_input, ' ');
        location->column += chars;
        if (0 == chars)
        {
            location->message = "Space expected after 'in'.";
            return false;
        }

        /* C++ specific case for "0xfafa in vtable for function ()" */
        chars = sr_skip_string(&local_input, "vtable");
        location->column += chars;
        if (0 <  chars)
        {
            chars = sr_skip_char_sequence(&local_input, ' ');
            location->column += chars;
            if (0 == chars)
            {
                location->message = "Space expected after 'vtable'.";
                return false;
            }

            chars = sr_skip_string(&local_input, "for");
            location->column += chars;
            if (0 == chars)
            {
                location->message = "Keyword \"for\" expected.";
                return false;
            }

            chars = sr_skip_char_sequence(&local_input, ' ');
            location->column += chars;
            if (0 == chars)
            {
                location->message = "Space expected after 'for'.";
                return false;
            }
        }
    }

    if (!sr_gdb_frame_parse_function_call(&local_input,
                                          function_name,
                                          function_type,
                                          location))
    {
        /* Do not update location here, it has been modified by the
           called function. */
        return false;
    }

    *input = local_input;
    return true;
}

bool
sr_gdb_frame_parse_file_location(const char **input,
                                 char **file,
                                 uint32_t *file_line,
                                 struct sr_location *location)
{
    const char *local_input = *input;
    int line, column;
    if (0 == sr_skip_char_span_location(&local_input, " \n", &line, &column))
    {
        location->message = "Expected a space or a newline.";
        return false;
    }
    sr_location_add(location, line, column);

    int chars = sr_skip_string(&local_input, "at");
    if (0 == chars)
    {
        chars = sr_skip_string(&local_input, "from");
        if (0 == chars)
        {
            location->message = "Expected 'at' or 'from'.";
            return false;
        }
    }
    location->column += chars;

    int spaces = sr_skip_char_sequence(&local_input, ' ');
    location->column += spaces;
    if (0 == spaces)
    {
        location->message = "Expected a space before file location.";
        return false;
    }

    char *file_name;
    chars = sr_parse_char_span(&local_input, SR_alnum "_/\\+.-", &file_name);
    location->column += chars;
    if (0 == chars)
    {
        location->message = "Expected a file name.";
        return false;
    }

    if (sr_skip_char(&local_input, ':'))
    {
        location->column += 1;
        int digits = sr_parse_uint32(&local_input, file_line);
        location->column += digits;
        if (0 == digits)
        {
            free(file_name);
            location->message = "Expected a line number.";
            return false;
        }
    }
    else
        *file_line = -1;

    *file = file_name;
    *input = local_input;
    return true;
}

struct sr_gdb_frame *
sr_gdb_frame_parse_header(const char **input,
                          struct sr_location *location)
{
    const char *local_input = *input;
    /* im - intermediate */
    struct sr_gdb_frame *imframe = sr_gdb_frame_new();
    int chars = sr_gdb_frame_parse_frame_start(&local_input,
                                               &imframe->number);

    location->column += chars;
    if (0 == chars)
    {
        location->message = "Frame start sequence expected.";
        sr_gdb_frame_free(imframe);
        return NULL;
    }

    struct sr_location internal_location;
    sr_location_init(&internal_location);
    if (sr_gdb_frame_parse_address_in_function(&local_input,
                                               &imframe->address,
                                               &imframe->function_name,
                                               &imframe->function_type,
                                               &internal_location))
    {
        sr_location_add(location,
                        internal_location.line,
                        internal_location.column);

        /* Optional section " from file.c:65" */
        /* Optional section " at file.c:65" */
        sr_location_init(&internal_location);
        if (sr_gdb_frame_parse_file_location(&local_input,
                                             &imframe->source_file,
                                             &imframe->source_line,
                                             &internal_location))
        {
            sr_location_add(location,
                            internal_location.line,
                            internal_location.column);
        }
    }
    else
    {
        sr_location_init(&internal_location);
        if (sr_gdb_frame_parse_function_call(&local_input,
                                             &imframe->function_name,
                                             &imframe->function_type,
                                             &internal_location))
        {
            sr_location_add(location,
                            internal_location.line,
                            internal_location.column);

            /* Mandatory section " at file.c:65" */
            sr_location_init(&internal_location);
            if (!sr_gdb_frame_parse_file_location(&local_input,
                                                  &imframe->source_file,
                                                  &imframe->source_line,
                                                  &internal_location))
            {
                location->message = "Function call in the frame header "
                    "misses mandatory \"at file.c:xy\" section";
                sr_gdb_frame_free(imframe);
                return NULL;
            }

            sr_location_add(location,
                            internal_location.line,
                            internal_location.column);
        }
        else
        {
            int chars = sr_skip_string(&local_input, "<signal handler called>");
            if (0 < chars)
            {
                location->column += chars;
                imframe->signal_handler_called = true;
            }
            else
            {
                location->message = "Frame header variant not recognized.";
                sr_gdb_frame_free(imframe);
                return NULL;
            }
        }
    }

    *input = local_input;
    return imframe;
}

static void
gdb_append_bthash_text(struct sr_gdb_frame *frame, enum sr_bthash_flags flags,
                       struct sr_strbuf *strbuf)
{
    sr_strbuf_append_strf(strbuf,
                          "%s, %s, %" PRIu32 ", %s, %" PRIu32 ", %d, 0x%" PRIx64 ", %s\n",
                          OR_UNKNOWN(frame->function_name),
                          OR_UNKNOWN(frame->function_type),
                          frame->number,
                          OR_UNKNOWN(frame->source_file),
                          frame->source_line,
                          frame->signal_handler_called,
                          frame->address,
                          OR_UNKNOWN(frame->library_name));
}

static void
gdb_append_duphash_text(struct sr_gdb_frame *frame, enum sr_duphash_flags flags,
                        struct sr_strbuf *strbuf)
{
    /* Taken from btparser. */
    sr_strbuf_append_str(strbuf, " ");

    if (frame->function_type)
        sr_strbuf_append_strf(strbuf, " %s", frame->function_type);

    if (frame->function_name)
        sr_strbuf_append_strf(strbuf, " %s", frame->function_name);

    if (frame->signal_handler_called)
        sr_strbuf_append_str(strbuf, " <signal handler called>");

    sr_strbuf_append_str(strbuf, "\n");
}
