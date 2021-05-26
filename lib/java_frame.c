/*
    java_frame.c

    Copyright (C) 2012  ABRT Team
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
*/
#include "java/frame.h"
#include "location.h"
#include "utils.h"
#include "json.h"
#include "generic_frame.h"
#include "thread.h"
#include "stacktrace.h"
#include "internal_utils.h"
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#define SR_JF_MARK_NATIVE_METHOD "Native Method"
#define SR_JF_MARK_UNKNOWN_SOURCE "Unknown Source"

/* Method table */

static void
java_append_bthash_text(struct sr_java_frame *frame, enum sr_bthash_flags flags,
                        GString *strbuf);
static void
java_append_duphash_text(struct sr_java_frame *frame, enum sr_duphash_flags flags,
                         GString *strbuf);

DEFINE_NEXT_FUNC(java_next, struct sr_frame, struct sr_java_frame)
DEFINE_SET_NEXT_FUNC(java_set_next, struct sr_frame, struct sr_java_frame)

struct frame_methods java_frame_methods =
{
    .append_to_str = (append_to_str_fn_t) sr_java_frame_append_to_str,
    .next = (next_frame_fn_t) java_next,
    .set_next = (set_next_frame_fn_t) java_set_next,
    .cmp = (frame_cmp_fn_t) sr_java_frame_cmp,
    .cmp_distance = (frame_cmp_fn_t) sr_java_frame_cmp_distance,
    .frame_append_bthash_text =
        (frame_append_bthash_text_fn_t) java_append_bthash_text,
    .frame_append_duphash_text =
        (frame_append_duphash_text_fn_t) java_append_duphash_text,
    .frame_free = (frame_free_fn_t) sr_java_frame_free,
};

/* Public functions */

struct sr_java_frame *
sr_java_frame_new()
{
    struct sr_java_frame *frame =
        g_malloc(sizeof(*frame));

    sr_java_frame_init(frame);
    return frame;
}

struct sr_java_frame *
sr_java_frame_new_exception()
{
    struct sr_java_frame *frame =
        g_malloc(sizeof(*frame));

    sr_java_frame_init(frame);
    frame->is_exception = true;
    return frame;
}

void
sr_java_frame_init(struct sr_java_frame *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->type = SR_REPORT_JAVA;
}

void
sr_java_frame_free(struct sr_java_frame *frame)
{
    if (!frame)
        return;

    g_free(frame->file_name);
    g_free(frame->name);
    g_free(frame->class_path);
    g_free(frame->message);
    g_free(frame);
}

void
sr_java_frame_free_full(struct sr_java_frame *frame)
{
    while(frame)
    {
        struct sr_java_frame *tmp = frame;
        frame = frame->next;
        sr_java_frame_free(tmp);
    }
}

struct sr_java_frame *
sr_java_frame_get_last(struct sr_java_frame *frame)
{
    while(frame && frame->next)
        frame = frame->next;

    return frame;
}

struct sr_java_frame *
sr_java_frame_dup(struct sr_java_frame *frame, bool siblings)
{
    struct sr_java_frame *result = sr_java_frame_new();
    memcpy(result, frame, sizeof(*result));

    /* Handle siblings. */
    if (siblings)
    {
        if (result->next)
            result->next = sr_java_frame_dup(result->next, true);
    }
    else
        result->next = NULL; /* Do not copy that. */

    /* Duplicate all strings. */
    if (result->file_name)
        result->file_name = g_strdup(result->file_name);

    if (result->name)
        result->name = g_strdup(result->name);

    if (result->class_path)
        result->class_path = g_strdup(result->class_path);

    if (result->message)
        result->message = g_strdup(result->message);

    return result;
}

int
sr_java_frame_cmp(struct sr_java_frame *frame1,
                  struct sr_java_frame *frame2)
{
    if (frame1->is_exception != frame2->is_exception)
        return frame1->is_exception ? 1 : -1;

    int res = g_strcmp0(frame1->name, frame2->name);
    if (res != 0)
        return res;

    /* Don't compare exception messages because of localization! */
    if (frame1->is_exception)
        return 0;

    /* Method call comparsion */
    res = g_strcmp0(frame1->class_path, frame2->class_path);
    if (res != 0)
        return res;

    res = g_strcmp0(frame1->file_name, frame2->file_name);
    if (res != 0)
        return res;

    if (frame1->is_native != frame2->is_native)
        return frame1->is_native ? 1 : -1;

    return frame1->file_line - frame2->file_line;
}

int
sr_java_frame_cmp_distance(struct sr_java_frame *frame1,
                           struct sr_java_frame *frame2)
{
    int res = g_strcmp0(frame1->name, frame2->name);
    if (res != 0)
        return res;

    return 0;
}

struct sr_java_frame *
sr_java_frame_append(struct sr_java_frame *dest,
                     struct sr_java_frame *item)
{
    if (!dest)
        return item;

    struct sr_java_frame *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

void
sr_java_frame_append_to_str(struct sr_java_frame *frame,
                            GString *dest)
{
    if (frame->is_exception)
    {
        if (frame->name)
            g_string_append(dest, frame->name);

        if (frame->message)
            g_string_append_printf(dest, ": %s", frame->message);
    }
    else
    {
        g_string_append_printf(dest, "\tat %s(",
                              frame->name ? frame->name : "");

        if (frame->is_native)
            g_string_append(dest, SR_JF_MARK_NATIVE_METHOD);
        else if (!frame->file_name)
            g_string_append(dest, SR_JF_MARK_UNKNOWN_SOURCE);
        else
            g_string_append(dest, frame->file_name);

        /* YES even if the frame is native method or source is unknown */
        /* WHY? Because it was parsed in this form */
        /* Ooops! Maybe the source file was empty string. Don't care! */
        if (frame->file_line)
            g_string_append_printf(dest, ":%"PRIu32, frame->file_line);

        g_string_append(dest, ")");

        if (!frame->class_path)
            g_string_append(dest, " [unknown]");
        else
        {
            g_string_append(dest, " [");

            if (strchrnul(frame->class_path,':') > strchrnul(frame->class_path, '/'))
                g_string_append(dest, "file:");

            g_string_append_printf(dest, "%s]", frame->class_path);
        }
    }
}

/*
 * We can expect different formats hence these two following helper functions
 */
static bool
sr_java_frame_parse_is_native_method(const char *input_mark)
{
    return 0 == strncmp(input_mark, SR_JF_MARK_NATIVE_METHOD,
                                    strlen(SR_JF_MARK_NATIVE_METHOD));
}

static bool
sr_java_frame_parse_is_unknown_source(const char *input_mark)
{
    return 0 == strncmp(input_mark, SR_JF_MARK_UNKNOWN_SOURCE,
                                    strlen(SR_JF_MARK_UNKNOWN_SOURCE));
}

struct sr_java_frame *
sr_java_frame_parse_exception(const char **input,
                              struct sr_location *location)
{
    /* java.lang.NullPointerException: foo */
    const char *cursor = sr_skip_whitespace(*input);
    sr_location_add(location, 0, cursor - *input);
    const char *mark = cursor;

    sr_location_add(location, 0, sr_skip_char_cspan(&cursor, ": \t\n"));

    if (mark == cursor)
    {
        location->message = "Expected exception name";
        return NULL;
    }

    struct sr_java_frame *exception = sr_java_frame_new_exception();
    exception->name = g_strndup(mark, cursor - mark);

    /* : foo */
    if (*cursor == ':')
    {
        ++cursor;
        sr_location_add(location, 0, 1);
        mark = cursor;

        /* foo */
        cursor = sr_skip_whitespace(mark);
        sr_location_add(location, 0, cursor - mark);
        mark = cursor;

        sr_location_add(location, 0, sr_skip_char_cspan(&cursor, "\n"));

        if (mark != cursor)
            exception->message = g_strndup(mark, cursor - mark);
    }
    else
    {
        /* just to be sure, that we skip white space behind exception name */
        sr_location_add(location, 0, sr_skip_char_cspan(&cursor, "\n"));
    }

    if (*cursor == '\n')
    {
        ++cursor;
        /* this adds one line */
        sr_location_add(location, 2, 0);
    }
    /* else *cursor == '\0' */

    mark = cursor;

    struct sr_java_frame *frame = NULL;
    /* iterate line by line
       best effort - continue on error */
    while (*cursor != '\0')
    {
        cursor = sr_skip_whitespace(mark);
        sr_location_add(location, 0, cursor - mark);

        /* Each inner exception has '...' at its end */
        if (strncmp("... ", cursor, strlen("... ")) == 0)
            goto current_exception_done;

        /* Suppressed exceptions follow after the end of current exception */
        if (strncmp("Suppressed: ", cursor, strlen("Suppressed: ")) == 0)
            goto current_exception_done;

        /* The top most exception does not have '...' at its end */
        if (strncmp("Caused by: ", cursor, strlen("Caused by: ")) == 0)
            goto parse_inner_exception;

        struct sr_java_frame *parsed = sr_java_frame_parse(&cursor, location);

        if (parsed == NULL)
        {
            sr_java_frame_free(exception);
            return NULL;
        }

        mark = cursor;

        if (exception->next == NULL)
            exception->next = parsed;
        else
        {
            assert(frame);
            frame->next = parsed;
        }

        frame = parsed;
    }
    /* We are done with the top most exception without inner exceptions */
    /* because of no 'Caused by:' and no '...' */
    goto exception_parsing_successful;

current_exception_done:
    sr_skip_to_next_line_location(&cursor, &location->line, &location->column);

    mark = cursor;
    cursor = sr_skip_whitespace(mark);
    sr_location_add(location, 0, cursor - mark);

    if (strncmp("Suppressed: ", cursor, strlen("Suppressed: ")) == 0)
    {
        /* Skip all lines related to the suppressed exception. We can do
         * this by skipping all lines that begin with a whitespace - the
         * main exception chain always begins without preceding whitespace.
         */
        sr_skip_to_next_line_location(&cursor, &location->line, &location->column);

        while (cursor && isspace(*cursor))
            sr_skip_to_next_line_location(&cursor, &location->line, &location->column);
    }

    if (strncmp("Caused by: ", cursor, strlen("Caused by: ")) == 0)
    {
parse_inner_exception:
        cursor += strlen("Caused by: ");
        sr_location_add(location, 0, strlen("Caused by: "));

        struct sr_java_frame *inner = sr_java_frame_parse_exception(&cursor, location);
        if (inner == NULL)
        {
            sr_java_frame_free(exception);
            return NULL;
        }

        struct sr_java_frame *last_inner = sr_java_frame_get_last(inner);
        last_inner->next = exception;
        exception = inner;
    }

exception_parsing_successful:
    *input = cursor;

    return exception;
}


/* [file:/usr/lib/java/Foo.class] */
/* [http://usr/lib/java/Foo.class] */
/* [jar:file:/usr/lib/java/foo.jar!/Foo.class] */
/* [jar:http://locahost/usr/lib/java/foo.jar!/Foo.class] */
static
const char *sr_java_frame_parse_frame_url(struct sr_java_frame *frame, const char *mark,
                                            struct sr_location *location)
{
    const char *cursor = mark;

    sr_location_add(location, 0, sr_skip_char_cspan(&cursor, "[\n"));

    if (*cursor != '[')
        return cursor;

    ++cursor;
    sr_location_add(location, 0, 1);
    mark = cursor;

    sr_location_add(location, 0, sr_skip_char_cspan(&cursor, ":\n"));

    if (*cursor == ':')
    {
        const char *path_stop = "]\n";
        if (strncmp("jar:", mark, strlen("jar:")) == 0)
        {   /* From jar:file:/usr/lib/java/foo.jar!/Foo.class] */
            /*                                               ^ */
            ++cursor;
            sr_location_add(location, 0, 1);
            mark = cursor;
            sr_location_add(location, 0, sr_skip_char_cspan(&cursor, ":\n"));
            path_stop = "!\n";
            /* To   file:/usr/lib/java/foo.jar!/Foo.class] */
            /*                                ^            */

            if (*cursor != ':')
                return cursor;
        }

        if (strncmp("file:", mark, strlen("file:")) != 0)
        {   /* move cursor back in case of http: ... */
            sr_location_add(location, 0, -(cursor - mark));
            cursor = mark;
        }
        else
        {
            ++cursor;
            sr_location_add(location, 0, 1);
            mark = cursor;
        }

        sr_location_add(location, 0, sr_skip_char_cspan(&cursor, path_stop));

        if (mark != cursor)
        {
            frame->class_path = g_strndup(mark, cursor - mark);
            frame->class_path = anonymize_path(frame->class_path);
        }
    }

    if (*cursor != ']' && *cursor != '\n')
        sr_location_add(location, 0, sr_skip_char_cspan(&cursor, "]\n"));

    return cursor;
}

struct sr_java_frame *
sr_java_frame_parse(const char **input,
                     struct sr_location *location)
{
    const char *mark = *input;
    int lines, columns;
    /*      at SimpleTest.throwNullPointerException(SimpleTest.java:36) [file:/usr/lib/java/foo.class] */
    const char *cursor = sr_strstr_location(mark, "at", &lines, &columns);

    if (!cursor)
    {
        location->message = "Frame expected";
        return NULL;
    }

    /*  SimpleTest.throwNullPointerException(SimpleTest.java:36) [file:/usr/lib/java/foo.class] */
    cursor = mark = cursor + 2;
    sr_location_add(location, lines, columns + 2);

    /* SimpleTest.throwNullPointerException(SimpleTest.java:36) [file:/usr/lib/java/foo.class] */
    cursor = sr_skip_whitespace(cursor);
    sr_location_add(location, 0, cursor - mark);
    mark = cursor;

    sr_location_add(location, 0, sr_skip_char_cspan(&cursor, "(\n"));

    struct sr_java_frame *frame = sr_java_frame_new();

    if (cursor != mark)
        frame->name = g_strndup(mark, cursor - mark);

    /* (SimpleTest.java:36) [file:/usr/lib/java/foo.class] */
    if (*cursor == '(')
    {
        ++cursor;
        sr_location_add(location, 0, 1);
        mark = cursor;

        sr_location_add(location, 0, sr_skip_char_cspan(&cursor, ":)\n"));

        if (mark != cursor)
        {
            if (sr_java_frame_parse_is_native_method(mark))
                frame->is_native = true;
            else if (!sr_java_frame_parse_is_unknown_source(mark))
            {
                /* DO NOT set file_name if input says that source isn't known */
                frame->file_name = g_strndup(mark, cursor - mark);
                frame->file_name = anonymize_path(frame->file_name);
            }
        }

        if (*cursor == ':')
        {
            ++cursor;
            sr_location_add(location, 0, 1);
            mark = cursor;

            sr_parse_uint32(&cursor, &(frame->file_line));

            sr_location_add(location, 0, cursor - mark);
        }
    }

    /* [file:/usr/lib/java/foo.class] */
    mark = sr_java_frame_parse_frame_url(frame, cursor, location);
    cursor = strchrnul(mark, '\n');

    if (*cursor == '\n')
    {
        *input = cursor + 1;
        sr_location_add(location, 2, 0);
    }
    else
    {
        *input = cursor;
        /* don't take \0 Byte into account */
        sr_location_add(location, 0, (cursor - mark) - 1);
    }

    return frame;
}

char *
sr_java_frame_to_json(struct sr_java_frame *frame)
{
    GString *strbuf = g_string_new(NULL);

    /* Name. */
    if (frame->name)
    {
        g_string_append(strbuf, ",   \"name\": ");
        sr_json_append_escaped(strbuf, frame->name);
        g_string_append(strbuf, "\n");
    }

    /* File name. */
    if (frame->file_name)
    {
        g_string_append(strbuf, ",   \"file_name\": ");
        sr_json_append_escaped(strbuf, frame->file_name);
        g_string_append(strbuf, "\n");

        /* File line. */
        g_string_append_printf(strbuf,
                              ",   \"file_line\": %"PRIu32"\n",
                              frame->file_line);
    }

    /* Class path. */
    if (frame->class_path)
    {
        g_string_append(strbuf, ",   \"class_path\": ");
        sr_json_append_escaped(strbuf, frame->class_path);
        g_string_append(strbuf, "\n");
    }

    /* Is native? */
    g_string_append_printf(strbuf,
                          ",   \"is_native\": %s\n",
                          frame->is_native ? "true" : "false");

    /* Is exception? */
    g_string_append_printf(strbuf,
                          ",   \"is_exception\": %s\n",
                          frame->is_exception ? "true" : "false");

    /* Message. */
    if (frame->message)
    {
        g_string_append(strbuf, ",   \"message\": ");
        sr_json_append_escaped(strbuf, frame->message);
        g_string_append(strbuf, "\n");
    }

    strbuf->str[0] = '{';
    g_string_append(strbuf, "}");
    return g_string_free(strbuf, FALSE);
}

struct sr_java_frame *
sr_java_frame_from_json(json_object *root, char **error_message)
{
    if (!json_check_type(root, json_type_object, "frame", error_message))
        return NULL;

    struct sr_java_frame *result = sr_java_frame_new();

    bool success =
        JSON_READ_STRING(root, "name", &result->name) &&
        JSON_READ_STRING(root, "file_name", &result->file_name) &&
        JSON_READ_UINT32(root, "file_line", &result->file_line) &&
        JSON_READ_STRING(root, "class_path", &result->class_path) &&
        JSON_READ_BOOL(root, "is_native", &result->is_native) &&
        JSON_READ_BOOL(root, "is_exception", &result->is_exception) &&
        JSON_READ_STRING(root, "message", &result->message);

    if (!success)
    {
        sr_java_frame_free(result);
        return NULL;
    }

    return result;
}

static void
java_append_bthash_text(struct sr_java_frame *frame, enum sr_bthash_flags flags,
                        GString *strbuf)
{
    g_string_append_printf(strbuf,
                          "%s, %s, %"PRIu32", %s, %d, %d, %s\n",
                          OR_UNKNOWN(frame->name),
                          OR_UNKNOWN(frame->file_name),
                          frame->file_line,
                          OR_UNKNOWN(frame->class_path),
                          frame->is_native,
                          frame->is_exception,
                          OR_UNKNOWN(frame->message));
}

static void
java_append_duphash_text(struct sr_java_frame *frame, enum sr_duphash_flags flags,
                         GString *strbuf)
{
    if (frame->name)
        g_string_append_printf(strbuf, "%s\n", frame->name);
    else
        g_string_append_printf(strbuf, "%s/%s:%"PRIu32"\n",
                              OR_UNKNOWN(frame->class_path),
                              OR_UNKNOWN(frame->file_name),
                              frame->file_line);
}
