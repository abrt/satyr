/*
    js_platfrom.c

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
#include "js/platform.h"
#include "js/stacktrace.h"
#include "js/frame.h"
#include "location.h"

#include "utils.h"
#include "internal_utils.h"
#include "json.h"

#include <string.h>


#define RETURN_ON_INVALID_ENGINE(engine, retval) \
    do { if (!SR_JS_ENGINE_VALIDITY_CHECK(engine)) { \
            warn("Invalid JavaScript engine code %0x", engine); \
            return NULL; \
    } } while (0)

#define RETURN_ON_INVALID_RUNTIME(runtime, retval) \
    do { if (!SR_JS_RUNTIME_VALIDITY_CHECK(runtime)) { \
            warn("Invalid JavaScript runtime code %0x", runtime); \
            return NULL; \
    } } while (0)


typedef struct sr_js_frame *(js_runtime_frame_parser_t)(enum sr_js_engine,
                                                        const char **input,
                                                        struct sr_location *location);

typedef struct sr_js_frame *(js_engine_frame_parser_t)(const char **input,
                                                       struct sr_location *location);

typedef struct sr_js_stacktrace *(js_runtime_stacktrace_parser_t)(enum sr_js_engine,
                                                                  const char **input,
                                                                  struct sr_location *location);

typedef struct sr_js_stacktrace *(js_engine_stacktrace_parser_t)(const char **input,
                                                                 struct sr_location *location);


struct js_engine_meta
{
    const char *name;
    js_engine_frame_parser_t *parse_frame;
    js_engine_stacktrace_parser_t *parse_stacktrace;
}
js_engines[_SR_JS_ENGINE_UPPER_BOUND] =
{
    [SR_JS_ENGINE_V8] =
    {
        .name = "V8",
        .parse_frame = &sr_js_frame_parse_v8,
        .parse_stacktrace = &sr_js_stacktrace_parse_v8,
    },
};

struct js_runtime_meta
{
    const char *name;
    js_runtime_frame_parser_t *parse_frame;
    js_runtime_stacktrace_parser_t *parse_stacktrace;
}
js_runtimes[_SR_JS_RUNTIME_UPPER_BOUND] =
{
    [SR_JS_RUNTIME_NODEJS] =
    {
        .name = "Node.js",
        .parse_frame = NULL,
        .parse_stacktrace = NULL,
    },
};


sr_js_platform_t
sr_js_platform_from_string(const char *runtime_name,
                           const char *runtime_version,
                           char **error_message)
{
    enum sr_js_runtime runtime = sr_js_runtime_from_string(runtime_name);
    if (!runtime)
    {
        *error_message = sr_asprintf("No known JavaScript platform with runtime "
                                    "'%s'", runtime_name);

        return SR_JS_PLATFORM_NULL;
    }

    enum sr_js_engine engine = 0;

    switch (runtime)
    {
        case SR_JS_RUNTIME_NODEJS:
            engine = SR_JS_ENGINE_V8;
            break;

        default:
            /* pass - calm down the compiler */
            break;
    }

    if (!engine)
    {
        *error_message = sr_asprintf("No known JavaScript engine for runtime"
                                    "by '%s%s%s'",
                                    runtime_name,
                                    runtime_version ? " " : "",
                                    runtime_version ? runtime_version : "");

        return SR_JS_PLATFORM_NULL;
    }

    return _sr_js_platform_assemble(runtime, engine);
}

const char *
sr_js_engine_to_string(enum sr_js_engine engine)
{
    RETURN_ON_INVALID_ENGINE(engine, NULL);

    return js_engines[engine].name;
}

enum sr_js_engine
sr_js_engine_from_string(const char *engine_str)
{
    unsigned engine = 1;
    for (; engine < _SR_JS_ENGINE_UPPER_BOUND; ++engine)
        if (strcmp(engine_str, js_engines[engine].name) == 0)
            return engine;

    return 0;
}

const char *
sr_js_runtime_to_string(enum sr_js_runtime runtime)
{
    RETURN_ON_INVALID_RUNTIME(runtime, NULL);

    return js_runtimes[runtime].name;
}

enum sr_js_runtime
sr_js_runtime_from_string(const char *runtime_str)
{
    unsigned runtime = 1;
    for (; runtime < _SR_JS_RUNTIME_UPPER_BOUND; ++runtime)
        if (strcmp(runtime_str, js_runtimes[runtime].name) == 0)
            return runtime;

    return 0;
}

char *
sr_js_platform_to_json(sr_js_platform_t platform)
{
    const char *runtime_str = sr_js_runtime_to_string(sr_js_platform_runtime(platform));
    const char *engine_str = sr_js_engine_to_string(sr_js_platform_engine(platform));

    if (!runtime_str)
        runtime_str = "<unknown>";

    if (!engine_str)
        engine_str = "<unknown>";

   return sr_asprintf("{    \"engine\": \"%s\"\n"
                      ",    \"runtime\": \"%s\"\n"
                      "}\n",
                      engine_str,
                      runtime_str);
}

sr_js_platform_t
sr_js_platform_from_json(struct sr_json_value *root, char **error_message)
{
    sr_js_platform_t platform = SR_JS_PLATFORM_NULL;

    char *engine_str = NULL;
    char *runtime_str = NULL;

    if (!JSON_READ_STRING(root, "engine", &engine_str))
        goto fail;

    if (engine_str == NULL)
    {
        *error_message = sr_strdup("No 'engine' member");
        goto fail;
    }

    enum sr_js_engine engine = 0;
    if (     strcmp(engine_str, "<unknown>") != 0
        && !(engine = sr_js_engine_from_string(engine_str)))
    {
        *error_message = sr_asprintf("Unknown JavaScript engine '%s'", engine_str);
        goto fail;
    }

    if (!JSON_READ_STRING(root, "runtime", &runtime_str))
        goto fail;

    if (runtime_str == NULL)
    {
        *error_message = sr_strdup("No 'runtime' member");
        goto fail;
    }

    enum sr_js_runtime runtime = 0;
    if (     strcmp(runtime_str, "<unknown>") != 0
        && !(runtime = sr_js_runtime_from_string(runtime_str)))
    {
        *error_message = sr_asprintf("Unknown JavaScript runtime '%s'", runtime_str);
        goto fail;
    }

    platform = sr_js_platform_new();
    sr_js_platform_init(platform, engine, runtime);

fail:
    free(engine_str);
    free(runtime_str);
    return platform;
}

struct sr_js_frame *
sr_js_platform_parse_frame(sr_js_platform_t platform, const char **input,
                           struct sr_location *location)
{
    enum sr_js_runtime runtime = sr_js_platform_runtime(platform);
    RETURN_ON_INVALID_RUNTIME(runtime, NULL);

    enum sr_js_engine engine = sr_js_platform_engine(platform);
    RETURN_ON_INVALID_ENGINE(engine, NULL);

    struct sr_js_frame *frame = NULL;

    if (js_runtimes[runtime].parse_frame)
        frame = js_runtimes[runtime].parse_frame(engine, input, location);
    else
        frame = js_engines[engine].parse_frame(input, location);

    return frame;
}

struct sr_js_stacktrace *
sr_js_platform_parse_stacktrace(sr_js_platform_t platform, const char **input,
                                struct sr_location *location)
{
    enum sr_js_runtime runtime = sr_js_platform_runtime(platform);
    RETURN_ON_INVALID_RUNTIME(runtime, NULL);

    enum sr_js_engine engine = sr_js_platform_engine(platform);
    RETURN_ON_INVALID_ENGINE(engine, NULL);

    struct sr_js_stacktrace *stacktrace = NULL;

    if (js_runtimes[runtime].parse_stacktrace != NULL)
        stacktrace = js_runtimes[runtime].parse_stacktrace(engine, input, location);
    else
        stacktrace = js_engines[engine].parse_stacktrace(input, location);

    if (stacktrace != NULL)
    {
        stacktrace->platform = sr_js_platform_new();
        stacktrace->platform = sr_js_platform_dup(platform);
    }

    return stacktrace;
}
