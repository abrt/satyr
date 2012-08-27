/*
    callgraph.h

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
#ifndef BTPARSER_CALLGRAPH_H
#define BTPARSER_CALLGRAPH_H

/**
 * @file
 * @brief Call graph for ELF binaries.
 *
 * Call graph represents calling relationships between subroutines in
 * ELF binaries.  Only static relationships obtained from CALL-like
 * instructions with numeric offsets are handled.
 *
 * Call graph is used by fingerprinting algorithms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

struct btp_disasm_state;
struct btp_elf_frame_description_entry;

struct btp_callgraph
{
    uint64_t address;
    uint64_t *callees;

    struct btp_callgraph *next;
};

struct btp_callgraph *
btp_callgraph_compute(struct btp_disasm_state *disassembler,
                      struct btp_elf_frame_description_entry *fde_entries,
                      char **error_message);

/// Assumption: when a fde is included in the callgraph, we assume
/// that all callees are included as well.
struct btp_callgraph *
btp_callgraph_extend(struct btp_callgraph *callgraph,
                     uint64_t start_address,
                     struct btp_disasm_state *disassembler,
                     struct btp_elf_frame_description_entry *fde_entries,
                     char **error_message);

void
btp_callgraph_free(struct btp_callgraph *callgraph);

struct btp_callgraph *
btp_callgraph_find(struct btp_callgraph *callgraph,
                   uint64_t address);

struct btp_callgraph *
btp_callgraph_last(struct btp_callgraph *callgraph);

#ifdef __cplusplus
}
#endif

#endif
