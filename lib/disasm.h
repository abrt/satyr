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
#ifndef BTPARSER_DISASSEMBLER_H
#define BTPARSER_DISASSEMBLER_H

/**
 * @file
 * @brief BFD-based function disassembler.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <bfd.h>
#include <dis-asm.h>
#include <inttypes.h>
#include <stdbool.h>

/**
 * @brief Internal state of a disassembler.
 */
struct btp_disasm_state
{
    bfd *bfd_file;
    disassembler_ftype disassembler;
    struct disassemble_info info;
    char *error_message;
};

struct btp_disasm_state *
btp_disasm_init(const char *file_name,
                      char **error_message);

void
btp_disasm_free(struct btp_disasm_state *state);

/**
 * Disassemble the function starting at 'start_offset' and taking
 * 'size' bytes, returning a list of (char*) instructions.
 */
char **
btp_disasm_get_function_instructions(struct btp_disasm_state *state,
                                     uint64_t start_offset,
                                     uint64_t size,
                                     char **error_message);

void
btp_disasm_instructions_free(char **instructions);

bool
btp_disasm_instruction_is_one_of(char *instruction,
                                 const char **mnemonics);

bool
btp_disasm_instruction_present(char **instructions,
                               const char **mnemonics);

bool
btp_disasm_instruction_parse_single_address_operand(char *instruction,
                                                    uint64_t *dest);

/* Given list of instructions, returns lists of addresses that are
 * (directly) called by them.  The list is terminated by address 0.
 */
uint64_t *
btp_disasm_get_callee_addresses(char **instructions);

#ifdef __cplusplus
}
#endif

#endif
