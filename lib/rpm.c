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
#include "json.h"
#include "strbuf.h"
#include "config.h"
#include "internal_utils.h"
#include <errno.h>
#ifdef HAVE_LIBRPM
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmtd.h>
#include <rpm/header.h>
#endif
#include <fcntl.h>
#include <assert.h>
#include <string.h>

struct sr_rpm_package *
sr_rpm_package_new()
{
    struct sr_rpm_package *package =
        sr_malloc(sizeof(struct sr_rpm_package));

    sr_rpm_package_init(package);
    return package;
}

void
sr_rpm_package_init(struct sr_rpm_package *package)
{
    memset(package, 0, sizeof(struct sr_rpm_package));
}

void
sr_rpm_package_free(struct sr_rpm_package *package,
                     bool recursive)
{
    if (!package)
        return;

    free(package->name);
    free(package->version);
    free(package->release);
    free(package->architecture);
    sr_rpm_consistency_free(package->consistency, true);
    if (package->next && recursive)
        sr_rpm_package_free(package->next, true);

    free(package);
}

int
sr_rpm_package_cmp(struct sr_rpm_package *package1,
                   struct sr_rpm_package *package2)
{
    int nevra = sr_rpm_package_cmp_nevra(package1, package2);
    if (nevra != 0)
        return nevra;

    /* Install time. */
    int64_t install_time = package1->install_time - package2->install_time;
    if (install_time != 0)
        return install_time;

    /* Consistency. */
    int consistency = sr_rpm_consistency_cmp_recursive(package1->consistency,
                                                       package2->consistency);

    if (consistency)
        return consistency;

    return 0;
}

int
sr_rpm_package_cmp_nevra(struct sr_rpm_package *package1,
                         struct sr_rpm_package *package2)
{
    int nvr = sr_rpm_package_cmp_nvr(package1, package2);
    if (nvr != 0)
        return nvr;

    /* Epoch. */
    int32_t epoch = package1->epoch - package2->epoch;
    if (epoch != 0)
        return epoch;

    /* Architecture. */
    int architecture = sr_strcmp0(package1->architecture,
                                   package2->architecture);
    if (architecture != 0)
        return architecture;

    return 0;
}

int
sr_rpm_package_cmp_nvr(struct sr_rpm_package *package1,
                       struct sr_rpm_package *package2)
{
    /* Name. */
    int name = sr_strcmp0(package1->name, package2->name);
    if (name != 0)
        return name;

    /* Version. */
    int version = sr_strcmp0(package1->version, package2->version);
    if (version != 0)
        return version;

    /* Release. */
    int release = sr_strcmp0(package1->release, package2->release);
    if (release != 0)
        return release;

    return 0;
}

static int
cmp_nevra_qsort_wrapper(struct sr_rpm_package **package1,
                        struct sr_rpm_package **package2)
{
    return sr_rpm_package_cmp_nevra(*package1, *package2);
}

struct sr_rpm_package *
sr_rpm_package_append(struct sr_rpm_package *dest,
                      struct sr_rpm_package *item)
{
    if (!dest)
        return item;

    struct sr_rpm_package *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}

int
sr_rpm_package_count(struct sr_rpm_package *packages)
{
    int count = 0;
    struct sr_rpm_package *loop = packages;
    while (loop)
    {
        ++count;
        loop = loop->next;
    }

    return count;
}

struct sr_rpm_package *
sr_rpm_package_sort(struct sr_rpm_package *packages)
{
    size_t count = sr_rpm_package_count(packages);
    struct sr_rpm_package **array = sr_malloc_array(count, sizeof(struct sr_rpm_package*));

    /* Copy the linked list to an array. */
    struct sr_rpm_package *list_loop = packages, **array_loop = array;
    while (list_loop)
    {
        *array_loop = list_loop;
        list_loop = list_loop->next;
        ++array_loop;
    }

    /* Sort the array. */
    qsort(array, count, sizeof(struct sr_rpm_package*), (comparison_fn_t)cmp_nevra_qsort_wrapper);

    /* Create a linked list from the sorted array. */
    for (size_t loop = 0; loop < count; ++loop)
    {
        if (loop + 1 < count)
            array[loop]->next = array[loop + 1];
        else
            array[loop]->next = NULL;
    }

    struct sr_rpm_package *result = array[0];
    free(array);
    return result;
}

static struct sr_rpm_package *
package_merge(struct sr_rpm_package *p1, struct sr_rpm_package *p2)
{
    if (0 != sr_rpm_package_cmp_nvr(p1, p2))
        return NULL;

    if (p1->epoch != p2->epoch)
        return NULL;

    if (p1->architecture && p2->architecture &&
        0 != sr_strcmp0(p1->architecture, p2->architecture))
        return NULL;

    struct sr_rpm_package *merged = sr_rpm_package_new();
    merged->name = sr_strdup(p1->name);
    merged->epoch = p1->epoch;
    merged->version = sr_strdup(p1->version);
    merged->release = sr_strdup(p1->release);

    /* architecture is sometimes missing */
    if (p1->architecture)
        merged->architecture = sr_strdup(p1->architecture);
    else if (p2->architecture)
        merged->architecture = sr_strdup(p2->architecture);

    merged->install_time = (p1->install_time ?
                            p1->install_time : p2->install_time);

    merged->role = (p1->role ? p1->role : p2->role);

    return merged;
}

struct sr_rpm_package *
sr_rpm_package_uniq(struct sr_rpm_package *packages)
{
    struct sr_rpm_package *loop = packages, *prev = NULL;
    while (loop && loop->next)
    {
        struct sr_rpm_package *merged = package_merge(loop, loop->next);
        if (merged)
        {
            /* deleted record is not the first in list */
            if (prev)
            {
                prev->next = merged;
            }
            /* deleting first record */
            else
            {
                packages = merged;
            }
            merged->next = loop->next->next;

            sr_rpm_package_free(loop->next, false);
            sr_rpm_package_free(loop, false);

            loop = merged;
        }
        else
        {
            prev = loop;
            loop = loop->next;
        }
    }

    return packages;
}

#ifdef HAVE_LIBRPM
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
    *result = (str ? sr_strdup(str) : NULL);
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

static struct sr_rpm_package *
header_to_rpm_info(Header header,
                   char **error_message)
{
    struct sr_rpm_package *rpm = sr_rpm_package_new();

    if (!header_get_string(header, RPMTAG_NAME, &rpm->name))
    {
        sr_rpm_package_free(rpm, false);
        *error_message = sr_asprintf("Failed to find package name.");
        return NULL;
    }

    bool success = header_get_uint32(header, RPMTAG_EPOCH, &rpm->epoch);
    if (!success)
    {
        *error_message = sr_asprintf("Failed to find package epoch.");
        sr_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_string(header, RPMTAG_VERSION, &rpm->version);
    if (!success)
    {
        *error_message = sr_asprintf("Failed to find package version.");
        sr_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_string(header, RPMTAG_RELEASE, &rpm->release);
    if (!success)
    {
        *error_message = sr_asprintf("Failed to find package release.");
        sr_rpm_package_free(rpm, false);
        return NULL;
    }

    success = header_get_string(header, RPMTAG_ARCH, &rpm->architecture);
    if (!success)
    {
        *error_message = sr_asprintf("Failed to find package architecture.");
        sr_rpm_package_free(rpm, false);
        return NULL;
    }

    uint32_t install_time;
    success = header_get_uint32(header, RPMTAG_INSTALLTIME, &install_time);
    if (!success)
    {
        *error_message = sr_asprintf("Failed to find package installation time.");
        sr_rpm_package_free(rpm, false);
        return NULL;
    }

    rpm->install_time = install_time;

    return rpm;
}
#endif

/**
 * Takes 0.06 second for bash package consisting of 92 files.
 * Takes 0.75 second for emacs-common package consisting of 2585 files.
 */
struct sr_rpm_package *
sr_rpm_package_get_by_name(const char *name, char **error_message)
{
#ifdef HAVE_LIBRPM
    if (rpmReadConfigFiles(NULL, NULL))
    {
        *error_message = sr_asprintf("Failed to read RPM configuration files.");
        return NULL;
    }

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts,
                                                RPMTAG_NAME,
                                                name,
                                                strlen(name));

    struct sr_rpm_package *result = NULL;
    Header header;
    while ((header = rpmdbNextIterator(iter)))
    {
        struct sr_rpm_package *package = header_to_rpm_info(header,
                                                            error_message);
        if (!package)
        {
            sr_rpm_package_free(result, true);
            result = NULL;
            break;
        }

        result = sr_rpm_package_append(result, package);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return result;
#else
    *error_message = sr_asprintf("satyr compiled without rpm");
    return NULL;
#endif
}

struct sr_rpm_package *
sr_rpm_package_get_by_path(const char *path,
                           char **error_message)
{
#ifdef HAVE_LIBRPM
    if (rpmReadConfigFiles(NULL, NULL))
    {
        *error_message = sr_asprintf("Failed to read RPM configuration files.");
        return NULL;
    }

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts,
                                                RPMTAG_BASENAMES,
                                                path,
                                                strlen(path));

    struct sr_rpm_package *result = NULL;
    Header header;
    while ((header = rpmdbNextIterator(iter)))
    {
        struct sr_rpm_package *package = header_to_rpm_info(header,
                                                            error_message);
        if (!package)
        {
            sr_rpm_package_free(result, true);
            result = NULL;
            break;
        }

        result = sr_rpm_package_append(result, package);
    }

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return result;
#else
    *error_message = sr_asprintf("satyr compiled without rpm");
    return NULL;
#endif
}

char *
sr_rpm_package_to_json(struct sr_rpm_package *package,
                       bool recursive)
{
    struct sr_strbuf *strbuf = sr_strbuf_new();

    if (recursive)
    {
        struct sr_rpm_package *p = package;
        while (p)
        {
            if (p == package)
                sr_strbuf_append_str(strbuf, "[ ");
            else
                sr_strbuf_append_str(strbuf, ", ");

            char *package_json = sr_rpm_package_to_json(p, false);
            char *indented_package_json = sr_indent_except_first_line(package_json, 2);
            sr_strbuf_append_str(strbuf, indented_package_json);
            free(indented_package_json);
            free(package_json);
            p = p->next;
            if (p)
                sr_strbuf_append_str(strbuf, "\n");
        }

        sr_strbuf_append_str(strbuf, " ]");
    }
    else
    {
        /* Name. */
        if (package->name)
        {
            sr_strbuf_append_str(strbuf, ",   \"name\": ");
            sr_json_append_escaped(strbuf, package->name);
            sr_strbuf_append_str(strbuf, "\n");
        }

        /* Epoch. */
        sr_strbuf_append_strf(strbuf,
                              ",   \"epoch\": %"PRIu32"\n",
                              package->epoch);

        /* Version. */
        if (package->version)
        {
            sr_strbuf_append_str(strbuf, ",   \"version\": ");
            sr_json_append_escaped(strbuf, package->version);
            sr_strbuf_append_str(strbuf, "\n");
        }

        /* Release. */
        if (package->release)
        {
            sr_strbuf_append_str(strbuf, ",   \"release\": ");
            sr_json_append_escaped(strbuf, package->release);
            sr_strbuf_append_str(strbuf, "\n");
        }

        /* Architecture. */
        if (package->architecture)
        {
            sr_strbuf_append_str(strbuf, ",   \"architecture\": ");
            sr_json_append_escaped(strbuf, package->architecture);
            sr_strbuf_append_str(strbuf, "\n");
        }

        /* Install time. */
        if (package->install_time > 0)
        {
            sr_strbuf_append_strf(strbuf,
                                  ",   \"install_time\": %"PRIu64"\n",
                                  package->install_time);
        }

        /* Package role. */
        if (package->role != 0)
        {
            char *role;
            switch (package->role)
            {
            case SR_ROLE_AFFECTED:
                role = "affected";
                break;
            default:
                assert(0 && "Invalid role");
                break;
            }

            sr_strbuf_append_strf(strbuf, ",   \"package_role\": \"%s\"\n", role);
        }

        /* Consistency. */
        if (package->consistency)
        {
            // TODO
            //sr_strbuf_append_strf(strbuf,
            //                      ",   \"consistency\": \"%s\"\n",
            //                      package->architecture);
        }

        strbuf->buf[0] = '{';
        sr_strbuf_append_str(strbuf, "}");
    }

    return sr_strbuf_free_nobuf(strbuf);
}

static struct sr_rpm_package *
single_rpm_pacakge_from_json(struct sr_json_value *root, char **error_message)
{
    if (!JSON_CHECK_TYPE(root, SR_JSON_OBJECT, "package"))
        return NULL;

    struct sr_rpm_package *package = sr_rpm_package_new();

    bool success =
        JSON_READ_STRING(root, "name", &package->name) &&
        JSON_READ_STRING(root, "version", &package->version) &&
        JSON_READ_STRING(root, "release", &package->release) &&
        JSON_READ_STRING(root, "architecture", &package->architecture) &&
        JSON_READ_UINT32(root, "epoch", &package->epoch) &&
        JSON_READ_UINT64(root, "install_time", &package->install_time);

    if (!success)
        goto fail;

    struct sr_json_value *role_json = json_element(root, "package_role");
    if (role_json)
    {
        if (!JSON_CHECK_TYPE(role_json, SR_JSON_STRING, "package_role"))
            goto fail;

        /* We only know "affected" so far. */
        char *role = role_json->u.string.ptr;
        if (0 != strcmp(role, "affected"))
        {
            if (error_message)
                *error_message = sr_asprintf("Invalid package role %s", role);
            goto fail;
        }

        package->role = SR_ROLE_AFFECTED;
    }

    return package;

fail:
    sr_rpm_package_free(package, true);
    return NULL;
}

struct sr_rpm_package *
sr_rpm_package_from_json(struct sr_json_value *json, bool recursive, char **error_message)
{
    if (!recursive)
    {
        return single_rpm_pacakge_from_json(json, error_message);
    }
    else
    {
        if (!JSON_CHECK_TYPE(json, SR_JSON_ARRAY, "package list"))
            return NULL;

        struct sr_rpm_package *result = NULL;

        struct sr_json_value *pkg_json;
        FOR_JSON_ARRAY(json, pkg_json)
        {
            struct sr_rpm_package *pkg = single_rpm_pacakge_from_json(pkg_json, error_message);
            if (!pkg)
                goto fail;

            result = sr_rpm_package_append(result, pkg);
        }

        if (result == NULL && error_message)
            *error_message = sr_strdup("No RPM packages present");
        return result;

fail:
        sr_rpm_package_free(result, true);
        return NULL;
    }
}

bool
sr_rpm_package_parse_nvr(const char *text,
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
    *release = sr_strdup(last_dash + 1);

    // Version is before the last dash.
    *version = sr_strndup(last_but_one_dash + 1,
                          last_dash - last_but_one_dash - 1);

    // Name is before version.
    *name = sr_strndup(text, last_but_one_dash - text);

    return true;
}

bool
sr_rpm_package_parse_nevra(const char *text,
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
    *architecture = sr_strdup(last_dot + 1);

    // Release is after the last dash.
    *release = sr_strndup(last_dash + 1, last_dot - last_dash - 1);

    // Epoch is optional.
    if (colon)
    {
        // Version is before the last dash.
        *version = sr_strndup(colon + 1,
                              last_dash - colon - 1);

        char *epoch_str = sr_strndup(last_but_one_dash + 1,
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
        *version = sr_strndup(last_but_one_dash + 1,
                              last_dash - last_but_one_dash - 1);

        *epoch = 0;
    }

    // Name is before version.
    *name = sr_strndup(text, last_but_one_dash - text);

    return true;
}

struct sr_rpm_consistency *
sr_rpm_consistency_new()
{
    struct sr_rpm_consistency *consistency =
        sr_malloc(sizeof(struct sr_rpm_consistency));

    sr_rpm_consistency_init(consistency);
    return consistency;
}

void
sr_rpm_consistency_init(struct sr_rpm_consistency *consistency)
{
    memset(consistency, 0, sizeof(struct sr_rpm_consistency));
}

void
sr_rpm_consistency_free(struct sr_rpm_consistency *consistency,
                        bool recursive)
{
    if (!consistency)
        return;

    free(consistency->path);
    if (consistency->next && recursive)
        sr_rpm_consistency_free(consistency->next, true);

    free(consistency);
}

int
sr_rpm_consistency_cmp(struct sr_rpm_consistency *consistency1,
                       struct sr_rpm_consistency *consistency2)
{
    /* Path. */
    int path = sr_strcmp0(consistency1->path, consistency2->path);
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
sr_rpm_consistency_cmp_recursive(struct sr_rpm_consistency *consistency1,
                                 struct sr_rpm_consistency *consistency2)
{
    if (consistency1)
    {
        if (consistency2)
        {
            int cmp = sr_rpm_consistency_cmp(consistency1, consistency2);
            if (cmp != 0)
                return cmp;
            else
            {
                return sr_rpm_consistency_cmp_recursive(
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

struct sr_rpm_consistency *
sr_rpm_consistency_append(struct sr_rpm_consistency *dest,
                          struct sr_rpm_consistency *item)
{
    if (!dest)
        return item;

    struct sr_rpm_consistency *dest_loop = dest;
    while (dest_loop->next)
        dest_loop = dest_loop->next;

    dest_loop->next = item;
    return dest;
}
