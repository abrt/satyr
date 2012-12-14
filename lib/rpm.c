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
#include "strbuf.h"
#include <errno.h>
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

int
btp_rpm_package_cmp(struct btp_rpm_package *package1,
                    struct btp_rpm_package *package2)
{
    /* Name. */
    int name = btp_strcmp0(package1->name, package2->name);
    if (name != 0)
        return name;

    /* Epoch. */
    int32_t epoch = package1->epoch - package2->epoch;
    if (epoch != 0)
        return epoch;

    /* Version. */
    int version = btp_strcmp0(package1->version, package2->version);
    if (version != 0)
        return version;

    /* Release. */
    int release = btp_strcmp0(package1->release, package2->release);
    if (release != 0)
        return release;

    /* Architecture. */
    int architecture = btp_strcmp0(package1->architecture,
                                   package2->architecture);

    if (architecture != 0)
        return architecture;

    /* Install time. */
    int64_t install_time = package1->install_time - package2->install_time;
    if (install_time != 0)
        return install_time;

    /* Consistency. */
    int consistency = btp_rpm_consistency_cmp_recursive(package1->consistency,
                                                        package2->consistency);

    if (consistency)
        return consistency;

    return 0;
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

    uint32_t install_time;
    success = header_get_uint32(header, RPMTAG_INSTALLTIME, &install_time);
    if (!success)
    {
        *error_message = btp_asprintf("Failed to find package installation time.");
        btp_rpm_package_free(rpm, false);
        return NULL;
    }

    rpm->install_time = install_time;

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

char *
btp_rpm_package_to_json(struct btp_rpm_package *package,
                        bool recursive)
{
    struct btp_strbuf *strbuf = btp_strbuf_new();

    if (recursive)
    {
        struct btp_rpm_package *p = package;
        while (p)
        {
            if (p == package)
                btp_strbuf_append_str(strbuf, "[ ");
            else
                btp_strbuf_append_str(strbuf, ", ");

            char *package_json = btp_rpm_package_to_json(package, false);
            char *indented_package_json = btp_indent_except_first_line(package_json, 2);
            btp_strbuf_append_str(strbuf, indented_package_json);
            free(indented_package_json);
            free(package_json);
            p = p->next;
            if (p)
                btp_strbuf_append_str(strbuf, "\n");
        }

        btp_strbuf_append_str(strbuf, " ]");
    }
    else
    {
        /* Name. */
        if (package->name)
        {
            btp_strbuf_append_strf(strbuf,
                                   ",   \"name\": \"%s\"\n",
                                   package->name);
        }

        /* Epoch. */
        btp_strbuf_append_strf(strbuf,
                               ",   \"epoch\": %"PRIu32"\n",
                               package->epoch);

        /* Version. */
        if (package->version)
        {
            btp_strbuf_append_strf(strbuf,
                                   ",   \"version\": \"%s\"\n",
                                   package->version);
        }

        /* Release. */
        if (package->release)
        {
            btp_strbuf_append_strf(strbuf,
                                   ",   \"release\": \"%s\"\n",
                                   package->release);
        }

        /* Architecture. */
        if (package->architecture)
        {
            btp_strbuf_append_strf(strbuf,
                                   ",   \"architecture\": \"%s\"\n",
                                   package->architecture);
        }

        /* Install time. */
        if (package->install_time > 0)
        {
            btp_strbuf_append_strf(strbuf,
                                   ",   \"install_time\": %"PRIu64"\n",
                                   package->install_time);
        }

        /* Consistency. */
        if (package->consistency)
        {
            // TODO
            //btp_strbuf_append_strf(strbuf,
            //                       ",   \"consistency\": \"%s\"\n",
            //                       package->architecture);
        }

        strbuf->buf[0] = '{';
        btp_strbuf_append_str(strbuf, "}");
    }

    return btp_strbuf_free_nobuf(strbuf);
}

struct btp_rpm_package *
btp_rpm_packages_from_abrt_dir(const char *directory,
                               char **error_message)
{
    char *package_filename = btp_build_path(directory, "package", NULL);
    char *package_contents = btp_file_to_string(package_filename, error_message);
    free(package_filename);
    if (!package_contents)
        return NULL;

    struct btp_rpm_package *packages = btp_rpm_package_new();
    bool success = btp_rpm_package_parse_nvr(package_contents,
                                             &packages->name,
                                             &packages->version,
                                             &packages->release);

    if (!success)
    {
        btp_rpm_package_free(packages, true);
        *error_message = btp_asprintf("Failed to parse \"%s\".",
                                      package_contents);

        free(package_contents);
        return NULL;
    }

    free(package_contents);

    char *dso_list_filename = btp_build_path(directory, "dso_list", NULL);
    char *dso_list_contents = btp_file_to_string(dso_list_filename, error_message);
    free(dso_list_filename);
    if (!dso_list_contents)
    {
        btp_rpm_package_free(packages, true);
        return NULL;
    }

    struct btp_rpm_package *dso_packages =
        btp_rpm_package_parse_dso_list(dso_list_contents);

    if (dso_packages)
        packages = btp_rpm_package_append(packages, dso_packages);

    free(dso_list_contents);
    return packages;
}

struct btp_rpm_package *
btp_rpm_package_parse_dso_list(const char *text)
{
    struct btp_rpm_package *packages = NULL;
    const char *pos = text;
    while (pos && *pos)
    {
        // Skip dynamic library name.
        pos = strchr(pos, ' ');

        if (!pos)
            continue;

        // Skip the space.
        ++pos;

        // Find the package NEVRA.
        char *end = strchr(pos, ' ');
        if (!end || end - pos <= 1)
        {
            pos = strchr(pos, '\n');
            continue;
        }

        // Parse the package NEVRA.
        char *nevra = btp_strndup(pos, end - pos);
        struct btp_rpm_package *dso_package = btp_rpm_package_new();
        bool success = btp_rpm_package_parse_nevra(nevra,
                                                   &dso_package->name,
                                                   &dso_package->epoch,
                                                   &dso_package->version,
                                                   &dso_package->release,
                                                   &dso_package->architecture);

        free(nevra);

        // If parsing failed, move to the next line.
        if (!success)
        {
            btp_rpm_package_free(dso_package, true);
            pos = strchr(pos, '\n');
            continue;
        }

        // Find the package install time.
        char *eol = strchr(pos, '\n');
        if (!eol)
        {
            btp_rpm_package_free(dso_package, true);
            break;
        }

        pos = strrchr(pos, ' ');
        if (!pos)
        {
            pos = eol;
            btp_rpm_package_free(dso_package, true);
            continue;
        }

        ++pos;

        // Parse the package install time.
        int len = btp_parse_uint64((const char**)&pos,
                                   &dso_package->install_time);

        if (len <= 0)
        {
            pos = eol;
            btp_rpm_package_free(dso_package, true);
            continue;
        }

        // Append the package to the list.
        packages = btp_rpm_package_append(packages, dso_package);
        pos = eol;
    }

    return packages;
}

bool
btp_rpm_package_parse_nvr(const char *text,
                          char **name,
                          char **version,
                          char **release)
{
    char *last_dash = strrchr(text, '-');
    if (!last_dash)
        return false;

    if (0 == strlen(last_dash))
        return false;

    char *last_but_one_dash = last_dash;
    while (last_but_one_dash > text)
    {
        --last_but_one_dash;
        if (*last_but_one_dash == '-')
            break;
    }

    if (last_but_one_dash == text ||
        last_dash - last_but_one_dash == 1)
    {
        return false;
    }

    // Release is after the last dash.
    *release = btp_strdup(last_dash + 1);

    // Version is before the last dash.
    *version = btp_strndup(last_but_one_dash + 1,
                           last_dash - last_but_one_dash - 1);

    // Name is before version.
    *name = btp_strndup(text, last_but_one_dash - text);

    return true;
}

bool
btp_rpm_package_parse_nevra(const char *text,
                            char **name,
                            uint32_t *epoch,
                            char **version,
                            char **release,
                            char **architecture)
{
    char *last_dot = strrchr(text, '.');
    if (!last_dot || 0 == strlen(last_dot))
        return false;

    char *last_dash = strrchr(text, '-');
    if (!last_dash || last_dot - last_dash <= 1)
        return false;

    char *last_but_one_dash = last_dash;
    while (last_but_one_dash > text)
    {
        --last_but_one_dash;
        if (*last_but_one_dash == '-')
            break;
    }

    if (last_but_one_dash == text ||
        last_dash - last_but_one_dash == 1)
    {
        return false;
    }

    char *colon = strchr(last_but_one_dash, ':');
    if (colon && colon > last_dash)
        colon = NULL;

    if (colon && (last_dash - colon == 1 ||
                  colon - last_but_one_dash == 1))
    {
        return false;
    }

    // Architecture is after the last dot.
    *architecture = btp_strdup(last_dot + 1);

    // Release is after the last dash.
    *release = btp_strndup(last_dash + 1, last_dot - last_dash - 1);

    // Epoch is optional.
    if (colon)
    {
        // Version is before the last dash.
        *version = btp_strndup(colon + 1,
                               last_dash - colon - 1);

        char *epoch_str = btp_strndup(last_but_one_dash + 1,
                                      colon - last_but_one_dash - 1);

        char *endptr;
        errno = 0;
        unsigned long r = strtoul(epoch_str, &endptr, 10);
        bool failure = (errno || epoch_str == endptr || *endptr != '\0'
                        || r > UINT32_MAX);

        if (failure)
        {
            free(epoch_str);
            free(*release);
            free(*architecture);
            return false;
        }

        *epoch = r;
        free(epoch_str);
    }
    else
    {
        // Version is before the last dash.
        *version = btp_strndup(last_but_one_dash + 1,
                               last_dash - last_but_one_dash - 1);

        *epoch = 0;
    }

    // Name is before version.
    *name = btp_strndup(text, last_but_one_dash - text);

    return true;
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

    free(consistency->path);
    if (consistency->next && recursive)
        btp_rpm_consistency_free(consistency->next, true);

    free(consistency);
}

int
btp_rpm_consistency_cmp(struct btp_rpm_consistency *consistency1,
                        struct btp_rpm_consistency *consistency2)
{
    /* Path. */
    int path = btp_strcmp0(consistency1->path, consistency2->path);
    if (path != 0)
        return path;

    /* Owner changed. */
    int owner_changed = consistency1->owner_changed - consistency2->owner_changed;
    if (owner_changed != 0)
        return owner_changed;

    /* Group changed. */
    int group_changed = consistency1->group_changed - consistency2->group_changed;
    if (group_changed != 0)
        return group_changed;

    /* Mode changed. */
    int mode_changed = consistency1->mode_changed - consistency2->mode_changed;
    if (mode_changed != 0)
        return mode_changed;

    /* MD5 mismatch. */
    int md5_mismatch = consistency1->md5_mismatch - consistency2->md5_mismatch;
    if (md5_mismatch != 0)
        return md5_mismatch;

    /* Size changed. */
    int size_changed = consistency1->size_changed - consistency2->size_changed;
    if (size_changed != 0)
        return size_changed;

    /* Major number changed. */
    int major_number_changed = consistency1->major_number_changed - consistency2->major_number_changed;
    if (major_number_changed != 0)
        return major_number_changed;

    /* Minor number changed. */
    int minor_number_changed = consistency1->minor_number_changed - consistency2->minor_number_changed;
    if (minor_number_changed != 0)
        return minor_number_changed;

    /* Symlink changed. */
    int symlink_changed = consistency1->symlink_changed - consistency2->symlink_changed;
    if (symlink_changed != 0)
        return symlink_changed;

    /* Modification time changed. */
    int modification_time_changed = consistency1->modification_time_changed - consistency2->modification_time_changed;
    if (modification_time_changed != 0)
        return modification_time_changed;

    return 0;
}

int
btp_rpm_consistency_cmp_recursive(struct btp_rpm_consistency *consistency1,
                                  struct btp_rpm_consistency *consistency2)
{
    if (consistency1)
    {
        if (consistency2)
        {
            int cmp = btp_rpm_consistency_cmp(consistency1, consistency2);
            if (cmp != 0)
                return cmp;
            else
            {
                return btp_rpm_consistency_cmp_recursive(
                    consistency1->next,
                    consistency2->next);
            }
        }
        else
            return 1;
    }
    else
        return consistency2 ? -1 : 0;
}
