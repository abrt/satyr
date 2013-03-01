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
#ifndef SATYR_CALLGRAPH_H
#define SATYR_CALLGRAPH_H

/**
 * @file
 * @brief Calling relationships between subroutines.
 *
 * Call graph represents calling relationships between subroutines.
 * In our case, we create the call graph from ELF binaries.  Only
 * static relationships obtained from CALL-like instructions with
 * numeric offsets are handled.
 *
 * Call graph is used by fingerprinting algorithms.
 */

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

struct sr_disasm_state;
struct sr_elf_fde;

/**
 * @brief A call graph representing calling relationships between
 * subroutines.
 *
 * It's a context-insensitive static call graph specialized to
 * low-level programs.  Functions are identified by their numeric
 * address (an offset to a binary file).
 */
struct sr_callgraph
{
    /**
     * @brief Memory address of the start of a function executable code.
     */
    uint64_t address;

    /**
     * @brief A list of addresses of called functions.
     *
     * It is terminated by a zero address.
     */
    uint64_t *callees;

    /**
     * @brief Next node of the call graph or NULL.
     */
    struct sr_callgraph *next;
};

struct sr_callgraph *
sr_callgraph_compute(struct sr_disasm_state *disassembler,
                     struct sr_elf_fde *eh_frame,
                     char **error_message);

/// Assumption: when a fde is included in the callgraph, we assume
/// that all callees are included as well.
struct sr_callgraph *
sr_callgraph_extend(struct sr_callgraph *callgraph,
                    uint64_t start_address,
                    struct sr_disasm_state *disassembler,
                    struct sr_elf_fde *eh_frame,
                    char **error_message);

void
sr_callgraph_free(struct sr_callgraph *callgraph);

struct sr_callgraph *
sr_callgraph_find(struct sr_callgraph *callgraph,
                  uint64_t address);

struct sr_callgraph *
sr_callgraph_last(struct sr_callgraph *callgraph);

#ifdef __cplusplus
}
#endif

#endif // SATYR_CALLGRAPH_H
