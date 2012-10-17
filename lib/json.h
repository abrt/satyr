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
#ifndef BTPARSER_JSON_H
#define BTPARSER_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

struct btp_json_settings
{
   unsigned long max_memory;
   int settings;
};

#define btp_json_relaxed_commas 1

enum btp_json_type
{
   BTP_JSON_NONE,
   BTP_JSON_OBJECT,
   BTP_JSON_ARRAY,
   BTP_JSON_INTEGER,
   BTP_JSON_DOUBLE,
   BTP_JSON_STRING,
   BTP_JSON_BOOLEAN,
   BTP_JSON_NULL
};

extern const struct btp_json_value
btp_json_value_none;

struct btp_json_value
{
   struct btp_json_value *parent;
   enum btp_json_type type;

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
            struct btp_json_value *value;
         } *values;

      } object;

      struct
      {
         unsigned length;
         struct btp_json_value **values;
      } array;
   } u;

   union
   {
      struct btp_json_value *next_alloc;
      void *object_mem;
   } _reserved;
};

struct btp_json_value *
btp_json_parse(const char *json);

struct btp_json_value *
btp_json_parse_ex(struct btp_json_settings *settings,
                  const char *json,
                  char *error);

void
btp_json_value_free(struct btp_json_value *value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
