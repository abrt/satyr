/*
    json.h

    Copyright (C) 2012  James McLaughlin et al.
    https://github.com/udp/json-parser

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
    OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
    USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef SATYR_JSON_H
#define SATYR_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

struct sr_location;

struct sr_json_settings
{
   unsigned long max_memory;
   int settings;
};

#define SR_JSON_RELAXED_COMMAS 1

enum sr_json_type
{
   SR_JSON_NONE,
   SR_JSON_OBJECT,
   SR_JSON_ARRAY,
   SR_JSON_INTEGER,
   SR_JSON_DOUBLE,
   SR_JSON_STRING,
   SR_JSON_BOOLEAN,
   SR_JSON_NULL
};

extern const struct sr_json_value
sr_json_value_none;

struct sr_json_value
{
   struct sr_json_value *parent;
   enum sr_json_type type;

   union
   {
      int boolean;
      long integer;
      double dbl;

      struct
      {
         unsigned length;

          /* null terminated */
         char *ptr;
      } string;

      struct
      {
         unsigned length;

         struct
         {
            char *name;
            struct sr_json_value *value;
         } *values;

      } object;

      struct
      {
         unsigned length;
         struct sr_json_value **values;
      } array;
   } u;

   union
   {
      struct sr_json_value *next_alloc;
      void *object_mem;
   } _reserved;
};

struct sr_json_value *
sr_json_parse(const char *json);

struct sr_json_value *
sr_json_parse_ex(struct sr_json_settings *settings,
                 const char *json,
                 struct sr_location *location);

void
sr_json_value_free(struct sr_json_value *value);

char *
sr_json_escape(const char *text);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
