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

struct sr_callgraph *
sr_callgraph_compute(struct sr_disasm_state *disassembler,
                     struct sr_elf_fde *eh_frame,
                     char **error_message)
{
    struct sr_elf_fde *fde_entry = eh_frame;
    struct sr_callgraph *result = NULL, *last = NULL;
    while (fde_entry)
    {
        struct sr_callgraph *entry = malloc(sizeof(struct sr_callgraph));
        entry->address = fde_entry->start_address;
        entry->callees = NULL;
        entry->next = NULL;

        char **instructions = sr_disasm_get_function_instructions(
            disassembler,
            fde_entry->start_address,
            fde_entry->length,
            error_message);

        if (!instructions)
        {
            sr_callgraph_free(result);
            g_free(entry);
            return NULL;
        }

        entry->callees = sr_disasm_get_callee_addresses(instructions);

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

struct sr_callgraph *
sr_callgraph_extend(struct sr_callgraph *callgraph,
                    uint64_t start_address,
                    struct sr_disasm_state *disassembler,
                    struct sr_elf_fde *eh_frame,
                    char **error_message)
{
    if (sr_callgraph_find(callgraph, start_address))
        return callgraph;

    struct sr_elf_fde *fde =
        sr_elf_find_fde_for_start_address(eh_frame,
                                           start_address);

    if (!fde)
    {
        *error_message = g_strdup_printf(
            "Unable to find FDE for address 0x%"PRIx64,
            start_address);

        return NULL;
    }

    struct sr_callgraph *last = sr_callgraph_last(callgraph);
    struct sr_callgraph *entry = malloc(sizeof(struct sr_callgraph));
    entry->address = fde->exec_base + fde->start_address;
    entry->callees = NULL;
    entry->next = NULL;

    char **instructions = sr_disasm_get_function_instructions(
        disassembler,
        fde->exec_base + fde->start_address,
        fde->length,
        error_message);

    if (!instructions)
    {
        g_free(entry);
        return NULL;
    }

    entry->callees = sr_disasm_get_callee_addresses(instructions);

    if (callgraph)
    {
        last->next = entry;
        last = entry;
    }
    else
        callgraph = last = entry;

    uint64_t *callees = entry->callees;
    while (*callees != 0)
    {
        struct sr_callgraph *result = sr_callgraph_extend(callgraph,
                                                            *callees,
                                                            disassembler,
                                                            eh_frame,
                                                            error_message);

        /* Failure here may mean that the address points to PLT. */
        if (result)
            callgraph = result;
        else if (*error_message)
        {
            g_free(*error_message);
            *error_message = NULL;
        }

        ++callees;
    }

    return callgraph;
}


void
sr_callgraph_free(struct sr_callgraph *callgraph)
{
    while (callgraph)
    {
        struct sr_callgraph *entry = callgraph;
        callgraph = entry->next;
        g_free(entry->callees);
        g_free(entry);
    }
}

struct sr_callgraph *
sr_callgraph_find(struct sr_callgraph *callgraph,
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

struct sr_callgraph *
sr_callgraph_last(struct sr_callgraph *callgraph)
{
    struct sr_callgraph *last = callgraph;
    if (last)
    {
        while (last->next)
            last = last->next;
    }

    return last;
}
