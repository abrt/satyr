/*
    disasm.h

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
#ifndef SATYR_DISASSEMBLER_H
#define SATYR_DISASSEMBLER_H

/**
 * @file
 * @brief BFD-based function disassembler.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

struct sr_disasm_state;

struct sr_disasm_state *
sr_disasm_init(const char *file_name,
               char **error_message);

void
sr_disasm_free(struct sr_disasm_state *state);

/**
 * Disassemble the function starting at 'start_offset' and taking
 * 'size' bytes, returning a list of (char*) instructions.
 */
char **
sr_disasm_get_function_instructions(struct sr_disasm_state *state,
                                    uint64_t start_offset,
                                    uint64_t size,
                                    char **error_message);

void
sr_disasm_instructions_free(char **instructions);

bool
sr_disasm_instruction_is_one_of(char *instruction,
                                const char **mnemonics);

bool
sr_disasm_instruction_present(char **instructions,
                              const char **mnemonics);

bool
sr_disasm_instruction_parse_single_address_operand(char *instruction,
                                                   uint64_t *dest);

/* Given list of instructions, returns lists of addresses that are
 * (directly) called by them.  The list is terminated by address 0.
 */
uint64_t *
sr_disasm_get_callee_addresses(char **instructions);

char *
sr_disasm_instructions_to_text(char **instructions);

char *
sr_disasm_binary_to_text(struct sr_disasm_state *state,
                         uint64_t start_offset,
                         uint64_t size,
                         char **error_message);

#ifdef __cplusplus
}
#endif

#endif // SATYR_DISASSEMBLER_H
