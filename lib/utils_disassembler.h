/*
    utils_disassembler.h

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
#ifndef BTPARSER_UTILS_DISASSEMBLER_H
#define BTPARSER_UTILS_DISASSEMBLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bfd.h>
#include <dis-asm.h>
#include <inttypes.h>
#include <stdbool.h>

struct btp_disassembler_state
{
    bfd *bfd_file;
    disassembler_ftype disassembler;
    struct disassemble_info info;
    char *error_message;
};

struct btp_disassembler_state *
btp_disassembler_init(const char *file_name,
                      char **error_message);

void
btp_disassembler_free(struct btp_disassembler_state *state);

/**
 * Disassemble the function starting at 'start_offset' and taking
 * 'size' bytes, returning a list of (char*) instructions.
 */
char **
btp_disassembler_get_function_instructions(struct btp_disassembler_state *state,
                                           uint64_t start_offset,
                                           uint64_t size,
                                           char **error_message);

void
btp_disassembler_function_instructions_free(char **instructions);

bool
btp_disassembler_function_instruction_is_one_of(char *instruction,
                                                char **mnemonics);

bool
btp_disassembler_function_instruction_present(char **instructions,
                                              char **mnemonics);

bool
btp_disassembler_instruction_parse_single_address_operand(char *instruction,
                                                          uint64_t *dest);

#ifdef __cplusplus
}
#endif

#endif
