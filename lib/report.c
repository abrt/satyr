/*
    report.c

    Copyright (C) 2012  Red Hat, Inc.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301 USA.
*/
#include "report.h"
#include "config.h"
#include "utils.h"
#include "koops_stacktrace.h"
#include "core_stacktrace.h"
#include "python_stacktrace.h"
#include "rpm.h"
#include "deb.h"

struct btp_report *
btp_report_new()
{
    struct btp_report *report = btp_malloc(sizeof(struct btp_report));
    btp_report_init(report);
    return report;
}

void
btp_report_init(struct btp_report *report)
{
    report->report_version = 1;
    report->report_type = BTP_REPORT_INVALID;
    report->reporter_name = "btparser";
    report->reporter_version = PACKAGE_VERSION;
    report->user_type = BTP_USER_INVALID;
    report->operating_system.name = NULL;
    report->operating_system.version = NULL;
    report->operating_system.architecture = NULL;
    report->operating_system.uptime = 0;

    report->component_name = NULL;
    report->rpm_packages = NULL;
    report->deb_packages = NULL;

    report->python_stacktrace = NULL;
    report->koops_stacktrace = NULL;
    report->core_stacktrace = NULL;
}

void
btp_report_free(struct btp_report *report)
{
    free(report->component_name);
    btp_rpm_package_free(report->rpm_packages, true);
    btp_deb_package_free(report->deb_packages, true);
    btp_python_stacktrace_free(report->python_stacktrace);
    btp_koops_stacktrace_free(report->koops_stacktrace);
    btp_core_stacktrace_free(report->core_stacktrace);
    free(report);
}

char *
btp_report_to_json(struct btp_report *report)
{
    return NULL;
}
