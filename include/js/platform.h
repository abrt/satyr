/*
    js_platform.h

    Copyright (C) 2016  ABRT Team
    Copyright (C) 2016  Red Hat, Inc.

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
#ifndef SATYR_JS_PLATFORM_H
#define SATYR_JS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

struct sr_location;
struct sr_json_value;

enum sr_js_engine
{
    /* Skipped 0 to enable use of ! in conditions */

    SR_JS_ENGINE_V8 = 0x1,

    /* SR_JS_ENGINE_SPIDERMONKEY = 0x2, */
    /* SR_JS_ENGINE_JAVASCRIPTCORE = 0x3, */

    /* Keep this the last entry.
     * Must be lower than 0xF.
     */
    _SR_JS_ENGINE_UPPER_BOUND,

    /* Uninitialized value.
     */
    _SR_JS_ENGINE_UNINIT = 0xF,
};

#define SR_JS_ENGINE_VALIDITY_CHECK(engine) (engine > 0 && engine < _SR_JS_ENGINE_UPPER_BOUND)

enum sr_js_runtime
{
    /* Skipped 0 to enable use of ! in conditions */

    SR_JS_RUNTIME_NODEJS = 0x1,

    /* SR_JS_RUNTIME_MONGODB = 0x2, */
    /* SR_JS_RUNTIME_PHANTOMEJS = 0x3, */
    /* SR_JS_RUNTIME_CHROME = 0x4, */
    /* SR_JS_RUNTIME_FIREFOX = 0x5, */
    /* SR_JS_RUNTIME_GJS = 0x6, */


    /* Keep this the last entry.
     * Must be lower than 0xFFF.
     */
    _SR_JS_RUNTIME_UPPER_BOUND,

    /* Uninitialized.
     */
    _SR_JS_RUNTIME_UNINIT=0xFFF,
};

#define SR_JS_RUNTIME_VALIDITY_CHECK(runtime) (runtime > 0 && runtime < _SR_JS_RUNTIME_UPPER_BOUND)

/* bits 31-9 = rutime
 * bits  8-0 = engine
 */
typedef uint32_t sr_js_platform_t;

#define SR_JS_PLATFORM_NULL 0

#define _sr_js_platform_assemble(runtime, engine) ((uint32_t)(runtime << 4) | engine)

/* Initialize to 0xFFFF to mimic a pointer logic with !. */
#define sr_js_platform_new() (_sr_js_platform_assemble(_SR_JS_RUNTIME_UNINIT, _SR_JS_ENGINE_UNINIT))

#define sr_js_platform_init(platform, runtime, engine) \
    do { platform = _sr_js_platform_assemble(runtime, engine); } while (0)

#define sr_js_platform_free(platform) ((void)platform)

#define sr_js_platform_dup(platform) (platform)

#define sr_js_platform_engine(platform) (platform & 0xF)

#define sr_js_platform_runtime(platform) (platform >> 4)

const char *
sr_js_engine_to_string(enum sr_js_engine engine);

enum sr_js_engine
sr_js_engine_from_string(const char * engine);

const char *
sr_js_runtime_to_string(enum sr_js_runtime runtime);

enum sr_js_runtime
sr_js_runtime_from_string(const char * runtime);

/* Creates new JavaScript platform entity from passed information.
 *
 * The runtime_name argument is mandatory.
 *
 * The runtime_version argument can be NULL which means "the most common
 * version". The argument is used to determine JavaScript engine because the
 * engine is sometimes subject to change in JavaScript platforms (i.e. MongoDB
 * : V8 -> * SpiderMonkey) .
 */
sr_js_platform_t
sr_js_platform_from_string(const char *runtime_name,
                           const char *runtime_version,
                           char **error_message);

char *
sr_js_platform_to_json(sr_js_platform_t platform);

sr_js_platform_t
sr_js_platform_from_json(struct sr_json_value *root, char **error_message);

struct sr_js_stacktrace *
sr_js_platform_parse_stacktrace(sr_js_platform_t platform, const char **input,
                                struct sr_location *location);

struct sr_js_frame *
sr_js_platform_parse_frame(sr_js_platform_t platform, const char **input,
                           struct sr_location *location);

#ifdef __cplusplus
}
#endif

#endif /* SATYR_JS_PLATFORM_H */
