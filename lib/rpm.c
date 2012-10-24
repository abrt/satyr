/*
    rpm.c

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
#inlcude "rpm.h"
#include <rpm/rpmlib.h>

struct btp_rpm_package *
btp_rpm_package_get_by_name(const char *name)
{
}

struct btp_rpm_package *
btp_rpm_package_get_by_path(const char *path)
{
}

void
btp_rpm_package_free(struct btp_rpm_package *package, bool recursive)
{
}


/**
 * Takes 0.06 second for bash package consisting of 92 files.
 * Takes 0.75 second for emacs-common package consisting of 2585 files.
 */
struct btp_rpm_verify *rpm_verify_package_by_name(const char *name);

struct btp_rpm_info *
rpm_get_package_info(const char *name)
{
    rpmReadConfigFiles(NULL, NULL, NULL, 0);

    rpmdb db;
    if (rpmdbOpen("", &db, O_RDONLY, 0644) != 0)
        return NULL;

    dbiIndexSet matches;
    int stat = rpmdbFindPackage(db, name, &matches);
    if (0 != stat)
    {
        rpmdbClose(db);
        return NULL;
    }

    if (1 != matches.count)
    {
        dbiFreeIndexRecord(matches);
        rpmdbClose(db);
        return NULL;
    }

    Header header = rpmdbGetRecord(db, matches.recs[0].recOffset);
    if (!header)
    {
        dbiFreeIndexRecord(matches);
        rpmdbClose(db);
        return NULL;
    }

    int_32 type, count;
    char *name;
    headerGetEntry(header,
                   RPMTAG_NAME,
                   &type,
                   (void**)&name,
                   &count);

    void *install_time;
    headerGetEntry(header,
                   RPMTAG_INSTALLTIME,
                   &type,
                   (void**)&install_time,
                   &count);

    void *version;
    headerGetEntry(header,
                   RPMTAG_VERSION,
                   &type,
                   (void**)&install_time,
                   &count);

    void *release;
    headerGetEntry(header,
                   RPMTAG_RELEASE,
                   &type,
                   (void**)&install_time,
                   &count);

    void *epoch;
    headerGetEntry(header,
                   RPMTAG_EPOCH,
                   &type,
                   (void**)&install_time,
                   &count);

    void *vendor;
    headerGetEntry(header,
                   RPMTAG_VENDOR,
                   &type,
                   (void**)&install_time,
                   &count);

    headerFree(header);
    dbiFreeIndexRecord(matches);
    rpmdbClose(db);
    return NULL;
}
