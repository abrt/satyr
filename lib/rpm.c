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
#include "rpm.h"
#include "utils.h"
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmtd.h>
#include <rpm/header.h>
#include <fcntl.h>
#include <assert.h>

struct btp_rpm_package *
btp_rpm_package_new()
{
    struct btp_rpm_package *package =
        btp_malloc(sizeof(struct btp_rpm_package));

    btp_rpm_package_init(package);
    return package;
}

void
btp_rpm_package_init(struct btp_rpm_package *package)
{
    memset(package, 0, sizeof(struct btp_rpm_package));
}

void
btp_rpm_package_free(struct btp_rpm_package *package,
                     bool recursive)
{
    if (!package)
        return;

    free(package->name);
    free(package->version);
    free(package->release);
    free(package->architecture);
    btp_rpm_consistency_free(package->consistency, true);
    if (package->next && recursive)
        btp_rpm_package_free(package->next, true);

    free(package);
}

struct btp_rpm_package *
btp_rpm_package_append(struct btp_rpm_package *dest,
                       struct btp_rpm_package *item)
{
    if (!dest)
        return item;

    struct btp_rpm_package *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

static bool
header_get_string(Header header,
                  rpmTag tag,
                  char **result)
{
    rpmtd tag_data = rpmtdNew();
    int success = headerGet(header,
                            tag,
                            tag_data,
                            HEADERGET_DEFAULT);

    if (success != 1)
        return false;

    const char *str = rpmtdGetString(tag_data);
    *result = (str ? btp_strdup(str) : NULL);
    rpmtdFree(tag_data);
    return str;
}

static bool
header_get_uint32(Header header,
                  rpmTag tag,
                  uint32_t *result)
{
    rpmtd tag_data = rpmtdNew();
    int success = headerGet(header,
                            tag,
                            tag_data,
                            HEADERGET_DEFAULT);

    if (success != 1)
        return false;

    uint32_t *num = rpmtdGetUint32(tag_data);
    if (num)
        *result = *num;

    rpmtdFree(tag_data);
    return num;
}

static struct btp_rpm_package *
header_to_rpm_info(Header header,
                   char **error_message)
{
    struct btp_rpm_package *rpm = btp_rpm_package_new();

    if (!header_get_string(header, RPMTAG_NAME, &rpm->name))
    {
        btp_rpm_package_free(rpm, false);
        *error_message = btp_asprintf("Failed to find package name.");
        return NULL;
    }

    bool success = header_get_uint32(header, RPMTAG_EPOCH, &rpm->epoch);
    if (!success)
    {
        *error_message = btp_asprintf("Failed to find package epoch.");
        btp_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_string(header, RPMTAG_VERSION, &rpm->version);
    if (!success)
    {
        *error_message = btp_asprintf("Failed to find package version.");
        btp_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_string(header, RPMTAG_RELEASE, &rpm->release);
    if (!success)
    {
        *error_message = btp_asprintf("Failed to find package release.");
        btp_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_string(header, RPMTAG_ARCH, &rpm->architecture);
    if (!success)
    {
        *error_message = btp_asprintf("Failed to find package architecture.");
        btp_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_uint32(header, RPMTAG_INSTALLTIME, &rpm->install_time);
    if (!success)
    {
        *error_message = btp_asprintf("Failed to find package installation time.");
        btp_rpm_package_free(rpm, false);
        return NULL;
    }

    return rpm;
}

/**
 * Takes 0.06 second for bash package consisting of 92 files.
 * Takes 0.75 second for emacs-common package consisting of 2585 files.
 */
struct btp_rpm_package *
btp_rpm_package_get_by_name(const char *name, char **error_message)
{
    if (rpmReadConfigFiles(NULL, NULL))
    {
        *error_message = btp_asprintf("Failed to read RPM configuration files.");
        return NULL;
    }

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts,
                                                RPMTAG_NAME,
                                                name,
                                                strlen(name));

    struct btp_rpm_package *result = NULL;
    Header header;
    while ((header = rpmdbNextIterator(iter)))
    {
        struct btp_rpm_package *package = header_to_rpm_info(header,
                                                             error_message);
        if (!package)
        {
            btp_rpm_package_free(result, true);
            result = NULL;
            break;
        }

        result = btp_rpm_package_append(result, package);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return result;
}

struct btp_rpm_package *
btp_rpm_package_get_by_path(const char *path,
                            char **error_message)
{
    if (rpmReadConfigFiles(NULL, NULL))
    {
        *error_message = btp_asprintf("Failed to read RPM configuration files.");
        return NULL;
    }

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts,
                                                RPMTAG_BASENAMES,
                                                path,
                                                strlen(path));

    struct btp_rpm_package *result = NULL;
    Header header;
    while ((header = rpmdbNextIterator(iter)))
    {
        struct btp_rpm_package *package = header_to_rpm_info(header,
                                                             error_message);
        if (!package)
        {
            btp_rpm_package_free(result, true);
            result = NULL;
            break;
        }

        result = btp_rpm_package_append(result, package);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return result;
}

struct btp_rpm_consistency *
btp_rpm_consistency_new()
{
    struct btp_rpm_consistency *consistency =
        btp_malloc(sizeof(struct btp_rpm_consistency));

    btp_rpm_consistency_init(consistency);
    return consistency;
}

void
btp_rpm_consistency_init(struct btp_rpm_consistency *consistency)
{
    memset(consistency, 0, sizeof(struct btp_rpm_consistency));
}

void
btp_rpm_consistency_free(struct btp_rpm_consistency *consistency,
                         bool recursive)
{
    if (!consistency)
        return;

    free(consistency->file_name);
    if (consistency->next && recursive)
        btp_rpm_consistency_free(consistency->next, true);

    free(consistency);
}
