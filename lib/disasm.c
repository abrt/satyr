/*
    disasm.c

    Copyright (C) 2011, 2012  Red Hat, Inc.

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
#include "config.h"

#include "disasm.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <glib.h>

/**
 * @brief Internal state of a disassembler.
 */
struct sr_disasm_state
{
};

struct sr_disasm_state *
sr_disasm_init(const char *file_name,
               char **error_message)
{
    *error_message = g_strdup_printf("satyr compiled without libopcodes");
    return NULL;
}

void
sr_disasm_free(struct sr_disasm_state *state)
{
    if (!state)
        return;

   free(state);
}

char **
sr_disasm_get_function_instructions(struct sr_disasm_state *state,
                                    uint64_t start_offset,
                                    uint64_t size,
                                    char **error_message)
{
    *error_message = g_strdup_printf("satyr compiled without libopcodes");
    return NULL;
}

void
sr_disasm_instructions_free(char **instructions)
{
    size_t offset = 0;
    while (instructions[offset])
    {
        free(instructions[offset]);
        ++offset;
    }

    free(instructions);
}

bool
sr_disasm_instruction_is_one_of(char *instruction,
                                const char **mnemonics)
{
    while (*mnemonics)
    {
        size_t len = strlen(*mnemonics);
        if (0 == strncmp(instruction, *mnemonics, len) &&
            (' ' == *(*mnemonics + len) || '\0' == *(*mnemonics + len)))
        {
            return true;
        }

        ++mnemonics;
    }

    return false;
}

bool
sr_disasm_instruction_present(char **instructions,
                              const char **mnemonics)
{
    while (*instructions)
    {
        if (sr_disasm_instruction_is_one_of(*instructions,
                                             mnemonics))
        {
            return true;
        }

        ++instructions;
    }

    return false;
}

bool
sr_disasm_instruction_parse_single_address_operand(char *instruction,
                                                   uint64_t *dest)
{
    /* Parse the mnemonic. */
    const char *p = instruction;
    p = sr_skip_non_whitespace(p);
    p = sr_skip_whitespace(p);

    /* Parse the address. */
    int chars_read;
    uint64_t addr;
    int ret = sscanf(p, "%"SCNx64" %n", &addr, &chars_read);
    if (ret < 1)
        return false;

    /* check that there is nothing else after the address */
    p += chars_read;
    if(*p != '\0')
        return false;

    if (dest)
        *dest = addr;

    return true;
}

uint64_t *
sr_disasm_get_callee_addresses(char **instructions)
{
    static const char *call_mnems[] = {"call", "callb", "callw",
                                       "calll", "callq", NULL};

    /* Determine the upper bound on the number of calls. */
    size_t result_size = 0, instruction_offset = 0;
    while (instructions[instruction_offset])
    {
        char *instruction = instructions[instruction_offset];
        if (sr_disasm_instruction_is_one_of(instruction, call_mnems))
        {
            uint64_t address;
            if (sr_disasm_instruction_parse_single_address_operand(
                    instruction, &address))
                ++result_size;
        }

        ++instruction_offset;
    }

    /* Create the output array and fill it */
    uint64_t *result = malloc(result_size * sizeof(uint64_t) + 1);
    size_t result_offset = 0;
    instruction_offset = 0;
    while (instructions[instruction_offset])
    {
        char *instruction = instructions[instruction_offset];
        if (sr_disasm_instruction_is_one_of(instruction, call_mnems))
        {
            uint64_t address;
            if (sr_disasm_instruction_parse_single_address_operand(instruction, &address))
            {
                /* Check if address is already stored in the list. */
                size_t result_loop = 0;
                for (; result_loop < result_offset; ++result_loop)
                {
                    if (result[result_loop] == address)
                        break;
                }

                /* If the address is not present in the list, store it
                 * there.
                 */
                if (result_loop == result_offset)
                    result[result_offset++] = address;
            }
        }

        ++instruction_offset;
    }

    result[result_offset] = 0;
    return result;
}

char *
sr_disasm_instructions_to_text(char **instructions)
{
    GString *strbuf = g_string_new(NULL);
    while (*instructions)
    {
        g_string_append(strbuf, *instructions);
        g_string_append_c(strbuf, '\n');
        ++instructions;
    }

    return g_string_free(strbuf, FALSE);
}

char *
sr_disasm_binary_to_text(struct sr_disasm_state *state,
                         uint64_t start_offset,
                         uint64_t size,
                         char **error_message)
{
    *error_message = g_strdup_printf("satyr compiled without libopcodes");
    return NULL;
}
