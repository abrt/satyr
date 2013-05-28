/*
    report_type.h

    Copyright (C) 2013  Red Hat, Inc.

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
#ifndef SATYR_REPORT_TYPE_H
#define SATYR_REPORT_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

enum sr_report_type
{
    SR_REPORT_INVALID = 0,
    SR_REPORT_CORE,
    SR_REPORT_PYTHON,
    SR_REPORT_KERNELOOPS,
    SR_REPORT_JAVA,
    SR_REPORT_GDB,

    /* Keep this the last entry. */
    SR_REPORT_NUM
};

#ifdef __cplusplus
}
#endif

#endif
