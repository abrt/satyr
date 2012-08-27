#include "callgraph.h"
#include "elves.h"
#include "disassembler.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>

struct btp_callgraph *
btp_callgraph_compute(struct btp_disasm_state *disassembler,
                      struct btp_elf_frame_description_entry *fde_entries,
                      char **error_message)
{
    struct btp_elf_frame_description_entry *fde_entry = fde_entries;
    struct btp_callgraph *result = NULL, *last = NULL;
    while (fde_entry)
    {
        struct btp_callgraph *entry = malloc(sizeof(struct btp_callgraph));
        entry->address = fde_entry->start_address;
        entry->callees = NULL;
        entry->next = NULL;

        const char **instructions = (const char**)btp_disasm_get_function_instructions(
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
                     struct btp_elf_frame_description_entry *fde_entries,
                     char **error_message)
{
    if (btp_callgraph_find(callgraph, start_address))
        return callgraph;

    struct btp_elf_frame_description_entry *fde =
        btp_elf_find_fde_for_address(fde_entries,
                                     start_address);

    if (!fde)
    {
        *error_message = btp_asprintf("Unable to find FDE for address 0x%"PRIx64,
                                      start_address);

        return NULL;
    }

    struct btp_callgraph *result = callgraph,
        *last = btp_callgraph_last(callgraph);

    struct btp_callgraph *entry = malloc(sizeof(struct btp_callgraph));
    entry->address = fde->start_address;
    entry->callees = NULL;
    entry->next = NULL;

    const char **instructions = (const char**)btp_disasm_get_function_instructions(
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
                                      fde_entries,
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
