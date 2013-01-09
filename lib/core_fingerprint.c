/*
    core_fingerprint.c

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
#include "core_fingerprint.h"
#include "core_stacktrace.h"
#include "core_frame.h"
#include "core_thread.h"
#include "utils.h"
#include "strbuf.h"
#include "elves.h"
#include "disasm.h"
#include "callgraph.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

static void
fingerprint_add_bool(struct btp_strbuf *buffer,
                     const char *name,
                     bool value)
{
    btp_strbuf_append_strf(buffer, "%s:%d", name, value ? 1 : 0);
}

static void
fingerprint_add_list(struct btp_strbuf *buffer,
                     const char *name,
                     char **list,
                     size_t count)
{
    btp_strbuf_append_strf(buffer, "%s:", name);

    bool first = true;
    for (size_t loop = 0; loop < count; ++loop)
    {
        btp_strbuf_append_strf(buffer,
                               "%s%s",
                               (first ? "" : ","),
                               list[loop]);

        first = false;
    }

    if (first)
    {
        btp_strbuf_append_strf(buffer, "-");
    }
}

static void
fp_jump_equality(struct btp_strbuf *fingerprint,
                 char **instructions)
{
    static const char *mnemonics[] = {"je", "jne", "jz", "jnz", NULL};
    bool present = btp_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "j_eql", present);
}

static void
fp_jump_signed(struct btp_strbuf *fingerprint,
               char **instructions)
{
    static const char *mnemonics[] =
        {"jg", "jl", "jnle", "jnge", "jng", "jnl", "jle", "jge", NULL};

    bool present = btp_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "j_sgn", present);
}

static void
fp_jump_unsigned(struct btp_strbuf *fingerprint,
                 char **instructions)
{
    static const char *mnemonics[] =
        {"ja", "jb", "jnae", "jnbe", "jna", "jnb", "jbe", "jae", NULL};

    bool present = btp_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "j_usn", present);
}

static void
fp_and_or(struct btp_strbuf *fingerprint,
          char **instructions)
{
    static const char *mnemonics[] = {"and", "or", NULL};
    bool present = btp_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "and_or", present);
}

static void
fp_shift(struct btp_strbuf *fingerprint,
         char **instructions)
{
    static const char *mnemonics[] = {"shl", "shr", NULL};
    bool present = btp_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "shift", present);
}

static void
fp_has_cycle(struct btp_strbuf *fingerprint,
	     char **instructions,
             uint64_t function_start_address,
             uint64_t function_end_address)
{
    static const char *jmp_mnemonics[] =
      {"jmp", "jmpb", "jmpw", "jmpl", "jmpq", NULL};

    bool found = false;
    while (*instructions)
    {
        if (!btp_disasm_instruction_is_one_of(*instructions,
                                              jmp_mnemonics))
        {
            ++instructions;
            continue;
        }

        uint64_t target_address;
        if (!btp_disasm_instruction_parse_single_address_operand(
                *instructions, &target_address))
        {
            ++instructions;
            continue;
        }

        if (function_start_address <= target_address &&
            target_address < function_end_address)
        {
            found = true;
            break;
        }

        ++instructions;
    }

    fingerprint_add_bool(fingerprint, "has_cycle", found);
}

static bool
get_libcalls(char ***symbol_list,
             size_t *symbol_list_size,
             uint64_t function_start_address,
             int depth,
             struct btp_elf_plt_entry *plt,
             struct btp_elf_fde *eh_frame,
             struct btp_disasm_state *disassembler,
             struct btp_callgraph **callgraph,
             char **error_message)
{
    *callgraph = btp_callgraph_extend(*callgraph,
                                      function_start_address,
                                      disassembler,
                                      eh_frame,
                                      error_message);

    if (!*callgraph)
    {
        *error_message = btp_asprintf(
            "Unable to extend callgraph for address 0x%"PRIx64,
            function_start_address);

        return false;
    }

    struct btp_callgraph *current =
        btp_callgraph_find(*callgraph, function_start_address);

    if (!current)
    {
        *error_message = btp_asprintf(
            "Unable to find callgraph for address 0x%"PRIx64,
            function_start_address);

        return false;
    }

    /* Obtain the symbol names for called functions. */
    uint64_t *callees = current->callees;
    char **sub_symbol_list = NULL;
    size_t sub_symbol_list_size = 0;
    while (*callees)
    {
        struct btp_elf_plt_entry *plt_entry =
            btp_elf_plt_find_for_address(plt, *callees);

        if (!plt_entry && depth > 0)
        {
            char **tmp_symbol_list;
            size_t tmp_symbol_list_size;
            bool success = get_libcalls(&tmp_symbol_list,
                                        &tmp_symbol_list_size,
                                        *callees,
                                        depth - 1,
                                        plt,
                                        eh_frame,
                                        disassembler,
                                        callgraph,
                                        error_message);

            if (!success)
            {
                free(sub_symbol_list);
                return false;
            }

            sub_symbol_list = btp_realloc(
                sub_symbol_list,
                (sub_symbol_list_size + tmp_symbol_list_size) * sizeof(char*));

            memcpy(sub_symbol_list + sub_symbol_list_size,
                   tmp_symbol_list,
                   tmp_symbol_list_size * sizeof(char*));

            sub_symbol_list_size += tmp_symbol_list_size;
            free(tmp_symbol_list);
        }

        ++callees;
    }

    /* Calculate the number of symbol names for the current
     * function. */
    callees = current->callees;
    *symbol_list_size = 0;
    while (*callees)
    {
        struct btp_elf_plt_entry *plt_entry =
            btp_elf_plt_find_for_address(plt, *callees);

        if (plt_entry)
            ++(*symbol_list_size);

        ++callees;
    }

    /* Obtain the symbol names for the current function. */
    *symbol_list = btp_malloc(
        (*symbol_list_size + sub_symbol_list_size) * sizeof(char*));

    char **item = *symbol_list;
    callees = current->callees;
    while (*callees)
    {
        struct btp_elf_plt_entry *plt_entry =
            btp_elf_plt_find_for_address(plt, *callees);

        if (plt_entry)
        {
            *item = plt_entry->symbol_name;
            ++item;
        }
    }

    memcpy(symbol_list + *symbol_list_size,
           sub_symbol_list,
           sub_symbol_list_size * sizeof(char*));

    symbol_list_size += sub_symbol_list_size;
    return true;
}

static bool
fp_libcalls(struct btp_strbuf *fingerprint,
            uint64_t function_start_address,
            struct btp_elf_plt_entry *plt,
            struct btp_elf_fde *eh_frame,
            struct btp_disasm_state *disassembler,
            struct btp_callgraph **callgraph,
            char **error_message)
{
    char **symbol_list;
    size_t symbol_list_size;
    bool success = get_libcalls(&symbol_list,
                                &symbol_list_size,
                                function_start_address,
                                1,
                                plt,
                                eh_frame,
                                disassembler,
                                callgraph,
                                error_message);
    if (!success)
        return false;

    qsort(symbol_list, symbol_list_size,
          sizeof(char*), (comparison_fn_t)strcmp);

    /* Make it unique. */
    btp_struniq(symbol_list, &symbol_list_size);

    /* Format the result */
    fingerprint_add_list(fingerprint,
                         "libcalls",
                         symbol_list,
                         symbol_list_size);

    free(symbol_list);
    return true;
}

static bool
fp_calltree_leaves(struct btp_strbuf *fingerprint,
                   uint64_t function_start_address,
                   struct btp_elf_plt_entry *plt,
                   struct btp_elf_fde *eh_frame,
                   struct btp_disasm_state *disassembler,
                   struct btp_callgraph **callgraph,
                   char **error_message)
{
    char **symbol_list;
    size_t symbol_list_size;
    bool success = get_libcalls(&symbol_list,
                                &symbol_list_size,
                                function_start_address,
                                6,
                                plt,
                                eh_frame,
                                disassembler,
                                callgraph,
                                error_message);
    if (!success)
        return false;

    qsort(symbol_list, symbol_list_size,
          sizeof(char*), (comparison_fn_t)strcmp);

    /* Make it unique. */
    btp_struniq(symbol_list, &symbol_list_size);

    /* Format the result */
    fingerprint_add_list(fingerprint,
                         "calltree_leaves",
                         symbol_list,
                         symbol_list_size);

    free(symbol_list);
    return true;
}

bool
btp_core_fingerprint_generate(struct btp_core_stacktrace *stacktrace,
                              char **error_message)
{
    struct btp_core_thread *thread = stacktrace->threads;
    while (thread)
    {
        struct btp_core_frame *frame = thread->frames;
        while (frame)
        {
            if (!frame->fingerprint)
            {
                bool success = btp_core_fingerprint_generate_for_binary(
                    thread, frame->file_name, error_message);

                if (!success)
                    return false;
            }

            frame = frame->next;
        }

        thread = thread->next;
    }

    return true;
}

static bool
compute_fingerprint(struct btp_core_frame *frame,
                    struct btp_elf_plt_entry *plt,
                    struct btp_elf_fde *eh_frame,
                    struct btp_disasm_state *disassembler,
                    struct btp_callgraph **callgraph,
                    char **error_message)
{
    struct btp_elf_fde *fde =
        btp_elf_find_fde_for_address(eh_frame, frame->build_id_offset);

    if (!fde)
    {
        *error_message = btp_asprintf("No frame description entry found"
                                      " for an offset %"PRIu64".",
                                      frame->build_id_offset);
        return false;
    }

    char **instructions =
        btp_disasm_get_function_instructions(disassembler,
                                             fde->start_address,
                                             fde->length,
                                             error_message);

    if (!instructions)
        return false;

    //puts("BEGIN");
    //printf("Function\n%s\n%s\n\n\n\n",
    //       btp_disasm_instructions_to_text(instructions),
    //       btp_disasm_binary_to_text(disassembler,
    //                                 fde->start_address,
    //                                 fde->length,
    //                                 error_message));

    struct btp_strbuf *fingerprint = btp_strbuf_new();

    fp_jump_equality(fingerprint, instructions);
    fp_jump_signed(fingerprint, instructions);
    fp_jump_unsigned(fingerprint, instructions);
    fp_and_or(fingerprint, instructions);
    fp_shift(fingerprint, instructions);
    fp_has_cycle(fingerprint,
                 instructions,
                 fde->start_address,
                 fde->start_address + fde->length);
/*
    if (!fp_libcalls(fingerprint,
                     fde->start_address,
                     plt,
                     eh_frame,
                     disassembler,
                     callgraph,
                     error_message))
    {
        return false;
    }

    if (!fp_calltree_leaves(fingerprint,
                            fde->start_address,
                            plt,
                            eh_frame,
                            disassembler,
                            callgraph,
                            error_message))
    {
        return false;
    }
*/
    frame->fingerprint = btp_strbuf_free_nobuf(fingerprint);
    btp_disasm_instructions_free(instructions);
    return true;
}

bool
btp_core_fingerprint_generate_for_binary(struct btp_core_thread *thread,
                                         const char *binary_path,
                                         char **error_message)
{
    /* Procedure Linkage Table */
    struct btp_elf_plt_entry *plt =
        btp_elf_get_procedure_linkage_table(binary_path,
                                            error_message);

    if (!plt)
        return false;

    struct btp_elf_fde *eh_frame =
        btp_elf_get_eh_frame(binary_path, error_message);

    if (!eh_frame)
    {
        btp_elf_procedure_linkage_table_free(plt);
        return false;
    }

    struct btp_disasm_state *disassembler =
        btp_disasm_init(binary_path, error_message);

    if (!disassembler)
    {
        btp_elf_procedure_linkage_table_free(plt);
        btp_elf_eh_frame_free(eh_frame);
        return false;
    }

    struct btp_callgraph *callgraph = NULL;

    while (thread)
    {
        struct btp_core_frame *frame = thread->frames;
        while (frame)
        {
            if (!frame->fingerprint &&
                0 == strcmp(frame->file_name, binary_path))
            {
                bool success = compute_fingerprint(frame,
                                                   plt,
                                                   eh_frame,
                                                   disassembler,
                                                   &callgraph,
                                                   error_message);
                if (!success)
                {
                    btp_disasm_free(disassembler);
                    btp_elf_procedure_linkage_table_free(plt);
                    btp_elf_eh_frame_free(eh_frame);
                    return false;
                }
            }

            frame = frame->next;
        }

        thread = thread->next;
    }

    btp_callgraph_free(callgraph);
    btp_disasm_free(disassembler);
    btp_elf_procedure_linkage_table_free(plt);
    btp_elf_eh_frame_free(eh_frame);
    return true;
}

