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
#include "callgraph.h"
#include "elves.h"
#include "disasm.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>

struct btp_callgraph *
btp_callgraph_compute(struct btp_disasm_state *disassembler,
                      struct btp_elf_frame_description_entry *eh_frame,
                      char **error_message)
{
    struct btp_elf_frame_description_entry *fde_entry = eh_frame;
    struct btp_callgraph *result = NULL, *last = NULL;
    while (fde_entry)
    {
        struct btp_callgraph *entry = malloc(sizeof(struct btp_callgraph));
        entry->address = fde_entry->start_address;
        entry->callees = NULL;
        entry->next = NULL;

        char **instructions = btp_disasm_get_function_instructions(
            disassembler,
            fde_entry->start_address,
            fde_entry->length,
            error_message);

        if (!instructions)
        {
            btp_callgraph_free(result);
            free(entry);
            return NULL;
        }

        entry->callees = btp_disasm_get_callee_addresses(instructions);

        if (result)
        {
            last->next = entry;
            last = entry;
        }
        else
            result = last = entry;

        fde_entry = fde_entry->next;
    }

    return result;
}

struct btp_callgraph *
btp_callgraph_extend(struct btp_callgraph *callgraph,
                     uint64_t start_address,
                     struct btp_disasm_state *disassembler,
                     struct btp_elf_frame_description_entry *eh_frame,
                     char **error_message)
{
    if (btp_callgraph_find(callgraph, start_address))
        return callgraph;

    struct btp_elf_frame_description_entry *fde =
        btp_elf_find_fde_for_address(eh_frame,
                                     start_address);

    if (!fde)
    {
        *error_message = btp_asprintf(
            "Unable to find FDE for address 0x%"PRIx64,
            start_address);

        return NULL;
    }

    struct btp_callgraph *result = callgraph,
        *last = btp_callgraph_last(callgraph);

    struct btp_callgraph *entry = malloc(sizeof(struct btp_callgraph));
    entry->address = fde->start_address;
    entry->callees = NULL;
    entry->next = NULL;

    char **instructions = btp_disasm_get_function_instructions(
        disassembler,
        fde->start_address,
        fde->length,
        error_message);

    if (!instructions)
    {
        free(entry);
        return NULL;
    }

    entry->callees = btp_disasm_get_callee_addresses(instructions);

    if (result)
    {
        last->next = entry;
        last = entry;
    }
    else
        result = last = entry;

    uint64_t *callees = entry->callees;
    while (*callees != 0)
    {
        result = btp_callgraph_extend(result,
                                      *callees,
                                      disassembler,
                                      eh_frame,
                                      error_message);

        if (!result)
            return NULL;
    }

    return result;
}


void
btp_callgraph_free(struct btp_callgraph *callgraph)
{
    while (callgraph)
    {
        struct btp_callgraph *entry = callgraph;
        callgraph = entry->next;
        free(entry->callees);
        free(entry);
    }
}

struct btp_callgraph *
btp_callgraph_find(struct btp_callgraph *callgraph,
                   uint64_t address)
{
    while (callgraph)
    {
        if (callgraph->address == address)
            return callgraph;

        callgraph = callgraph->next;
    }

    return NULL;
}

struct btp_callgraph *
btp_callgraph_last(struct btp_callgraph *callgraph)
{
    struct btp_callgraph *last = callgraph;
    if (last)
    {
        while (last->next)
            last = last->next;
    }

    return last;
}
