/*
    location.c

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
#include "location.h"
#include "utils.h"
#include <stdlib.h>
#include <glib.h>

void
sr_location_init(struct sr_location *location)
{
    location->line = 1;
    location->column = 0;
    location->message = NULL;
}

int
sr_location_cmp(struct sr_location *location1,
                struct sr_location *location2,
                bool compare_messages)
{
    int linediff = location1->line - location2->line;
    if (0 != linediff)
        return linediff;

    int columndiff = location1->column - location2->column;
    if (0 != columndiff)
        return columndiff;

    if (compare_messages)
        return sr_strcmp0(location1->message, location2->message);

    return 0;
}

char *
sr_location_to_string(struct sr_location *location)
{
    GString strbuf;
    strbuf.allocated_len = 8;
    strbuf.len = 0;
    strbuf.str = sr_malloc(strbuf.allocated_len);
    strbuf.str[0] = '\0';
    g_string_append_printf(&strbuf,
                          "Line %d, column %d",
                          location->line,
                          location->column);
    if (location->message)
        g_string_append_printf(&strbuf, ": %s", location->message);

    return strbuf.str;
}


void
sr_location_add(struct sr_location *location,
                int add_line,
                int add_column)
{
    sr_location_add_ext(&location->line,
                        &location->column,
                        add_line,
                        add_column);
}

void
sr_location_add_ext(int *line,
                    int *column,
                    int add_line,
                    int add_column)
{
    if (add_line > 1)
    {
        *line += add_line - 1;
        *column = add_column;
    }
    else
        *column += add_column;
}

void
sr_location_eat_char(struct sr_location *location,
                     char c)
{
    sr_location_eat_char_ext(&location->line,
                             &location->column,
                             c);
}

void
sr_location_eat_char_ext(int *line,
                         int *column,
                         char c)
{
    if (c == '\n')
    {
        *line += 1;
        *column = 0;
    }
    else
        *column += 1;
}
