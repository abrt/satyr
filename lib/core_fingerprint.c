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
#include "core/fingerprint.h"
#include "core/stacktrace.h"
#include "core/frame.h"
#include "core/thread.h"
#include "utils.h"
#include "strbuf.h"
#include "elves.h"
#include "disasm.h"
#include "callgraph.h"
#include "sha1.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>

static void
fingerprint_add_bool(struct sr_strbuf *buffer,
                     const char *name,
                     bool value)
{
    sr_strbuf_append_strf(buffer, ";%s:%d", name, value ? 1 : 0);
}

static void
fingerprint_add_list(struct sr_strbuf *buffer,
                     const char *name,
                     char **list,
                     size_t count)
{
    sr_strbuf_append_strf(buffer, ";%s:", name);

    bool first = true;
    for (size_t loop = 0; loop < count; ++loop)
    {
        sr_strbuf_append_strf(buffer,
                               "%s%s",
                               (first ? "" : ","),
                               list[loop]);

        first = false;
    }

    if (first)
    {
        sr_strbuf_append_strf(buffer, "-");
    }
}

static void
fp_jump_equality(struct sr_strbuf *fingerprint,
                 char **instructions)
{
    static const char *mnemonics[] = {"je", "jne", "jz", "jnz", NULL};
    bool present = sr_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "j_eql", present);
}

static void
fp_jump_signed(struct sr_strbuf *fingerprint,
               char **instructions)
{
    static const char *mnemonics[] =
        {"jg", "jl", "jnle", "jnge", "jng", "jnl", "jle", "jge", NULL};

    bool present = sr_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "j_sgn", present);
}

static void
fp_jump_unsigned(struct sr_strbuf *fingerprint,
                 char **instructions)
{
    static const char *mnemonics[] =
        {"ja", "jb", "jnae", "jnbe", "jna", "jnb", "jbe", "jae", NULL};

    bool present = sr_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "j_usn", present);
}

static void
fp_and_or(struct sr_strbuf *fingerprint,
          char **instructions)
{
    static const char *mnemonics[] = {"and", "or", NULL};
    bool present = sr_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "and_or", present);
}

static void
fp_shift(struct sr_strbuf *fingerprint,
         char **instructions)
{
    static const char *mnemonics[] = {"shl", "shr", NULL};
    bool present = sr_disasm_instruction_present(instructions, mnemonics);
    fingerprint_add_bool(fingerprint, "shift", present);
}

static void
fp_has_cycle(struct sr_strbuf *fingerprint,
             char **instructions,
             uint64_t function_start_address,
             uint64_t function_end_address)
{
    static const char *jmp_mnemonics[] =
      {"jmp", "jmpb", "jmpw", "jmpl", "jmpq", NULL};

    bool found = false;
    while (*instructions)
    {
        if (!sr_disasm_instruction_is_one_of(*instructions,
                                              jmp_mnemonics))
        {
            ++instructions;
            continue;
        }

        uint64_t target_address;
        if (!sr_disasm_instruction_parse_single_address_operand(
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
             struct sr_elf_plt_entry *plt,
             struct sr_elf_fde *eh_frame,
             struct sr_disasm_state *disassembler,
             struct sr_callgraph **callgraph,
             char **error_message)
{
    *callgraph = sr_callgraph_extend(*callgraph,
                                     function_start_address,
                                     disassembler,
                                     eh_frame,
                                     error_message);

    if (!*callgraph)
    {
        *error_message = sr_asprintf(
            "Unable to extend callgraph for address 0x%"PRIx64,
            function_start_address);

        return false;
    }

    struct sr_callgraph *current =
        sr_callgraph_find(*callgraph, function_start_address);

    if (!current)
    {
        *error_message = sr_asprintf(
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
        struct sr_elf_plt_entry *plt_entry =
            sr_elf_plt_find_for_address(plt, *callees);

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

            sub_symbol_list = sr_realloc_array(
                sub_symbol_list,
                sub_symbol_list_size + tmp_symbol_list_size,
                sizeof(char*));

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
        struct sr_elf_plt_entry *plt_entry =
            sr_elf_plt_find_for_address(plt, *callees);

        if (plt_entry)
            ++(*symbol_list_size);

        ++callees;
    }

    /* Obtain the symbol names for the current function. */
    *symbol_list = sr_malloc_array(
        *symbol_list_size + sub_symbol_list_size, sizeof(char*));

    char **item = *symbol_list;
    callees = current->callees;
    while (*callees)
    {
        struct sr_elf_plt_entry *plt_entry =
            sr_elf_plt_find_for_address(plt, *callees);

        if (plt_entry)
        {
            *item = plt_entry->symbol_name;
            ++item;
        }

        ++callees;
    }

    memcpy(*symbol_list + *symbol_list_size,
           sub_symbol_list,
           sub_symbol_list_size * sizeof(char*));

    *symbol_list_size += sub_symbol_list_size;
    return true;
}

static bool
fp_libcalls(struct sr_strbuf *fingerprint,
            uint64_t function_start_address,
            struct sr_elf_plt_entry *plt,
            struct sr_elf_fde *eh_frame,
            struct sr_disasm_state *disassembler,
            struct sr_callgraph **callgraph,
            char **error_message)
{
    char **symbol_list;
    size_t symbol_list_size;
    bool success = get_libcalls(&symbol_list,
                                &symbol_list_size,
                                function_start_address,
                                0,
                                plt,
                                eh_frame,
                                disassembler,
                                callgraph,
                                error_message);
    if (!success)
        return false;

    qsort(symbol_list, symbol_list_size,
          sizeof(char*), (comparison_fn_t)sr_ptrstrcmp);

    /* Make it unique. */
    sr_struniq(symbol_list, &symbol_list_size);

    /* Format the result */
    fingerprint_add_list(fingerprint,
                         "libcalls",
                         symbol_list,
                         symbol_list_size);

    free(symbol_list);
    return true;
}

static bool
fp_calltree_leaves(struct sr_strbuf *fingerprint,
                   uint64_t function_start_address,
                   struct sr_elf_plt_entry *plt,
                   struct sr_elf_fde *eh_frame,
                   struct sr_disasm_state *disassembler,
                   struct sr_callgraph **callgraph,
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
          sizeof(char*), (comparison_fn_t)sr_ptrstrcmp);

    /* Make it unique. */
    sr_struniq(symbol_list, &symbol_list_size);

    /* Format the result */
    fingerprint_add_list(fingerprint,
                         "calltree_leaves",
                         symbol_list,
                         symbol_list_size);

    free(symbol_list);
    return true;
}

static void
do_nothing(void *something)
{
}

bool
sr_core_fingerprint_generate(struct sr_core_stacktrace *stacktrace,
                             char **error_message)
{
    /* binary tree that works as a set of already processed binaries */
    void *processed_files = NULL;
    bool result = false;

    struct sr_core_thread *thread = stacktrace->threads;
    while (thread)
    {
        struct sr_core_frame *frame = thread->frames;
        while (frame)
        {
            if (frame->file_name &&
                !tfind(frame->file_name, &processed_files, (comparison_fn_t)strcmp))
            {
                bool success = sr_core_fingerprint_generate_for_binary(
                    thread, frame->file_name, error_message);

                result |= success;

                /* add file_name to the set of already processed files */
                tsearch(frame->file_name, &processed_files, (comparison_fn_t)strcmp);
            }

            frame = frame->next;
        }

        thread = thread->next;
    }

    tdestroy(processed_files, do_nothing);

    return result;
}

static bool
compute_fingerprint(struct sr_core_frame *frame,
                    struct sr_elf_plt_entry *plt,
                    struct sr_elf_fde *eh_frame,
                    struct sr_disasm_state *disassembler,
                    struct sr_callgraph **callgraph,
                    char **error_message)
{
    struct sr_elf_fde *fde =
        sr_elf_find_fde_for_offset(eh_frame, frame->build_id_offset);

    if (!fde)
    {
        *error_message = sr_asprintf("No frame description entry found"
                                      " for an offset %"PRIu64".",
                                      frame->build_id_offset);
        return false;
    }

    char **instructions =
        sr_disasm_get_function_instructions(disassembler,
                                            fde->exec_base + fde->start_address,
                                            fde->length,
                                            error_message);

    if (!instructions)
        return false;

/*    puts("BEGIN");
    char *binary = sr_disasm_binary_to_text(disassembler,
                                             fde->exec_base + fde->start_address,
                                             fde->length,
                                             error_message);

    if (!binary)
        puts(*error_message);

    printf("Function\n%s\n%s\n\n\n\n",
           sr_disasm_instructions_to_text(instructions),
           binary);
*/
    struct sr_strbuf *fingerprint = sr_strbuf_new();
    sr_strbuf_append_strf(fingerprint, "v1");

    fp_jump_equality(fingerprint, instructions);
    fp_jump_signed(fingerprint, instructions);
    fp_jump_unsigned(fingerprint, instructions);
    fp_and_or(fingerprint, instructions);
    fp_shift(fingerprint, instructions);
    fp_has_cycle(fingerprint,
                 instructions,
                 fde->exec_base + fde->start_address,
                 fde->exec_base + fde->start_address + fde->length);

    if (!fp_libcalls(fingerprint,
                     fde->exec_base + fde->start_address,
                     plt,
                     eh_frame,
                     disassembler,
                     callgraph,
                     error_message))
    {
        return false;
    }

    if (!fp_calltree_leaves(fingerprint,
                            fde->exec_base + fde->start_address,
                            plt,
                            eh_frame,
                            disassembler,
                            callgraph,
                            error_message))
    {
        return false;
    }

    frame->fingerprint = sr_strbuf_free_nobuf(fingerprint);
    frame->fingerprint_hashed = false;
    sr_disasm_instructions_free(instructions);
    return true;
}

bool
sr_core_fingerprint_generate_for_binary(struct sr_core_thread *thread,
                                         const char *binary_path,
                                         char **error_message)
{
    /* Procedure Linkage Table */
    struct sr_elf_plt_entry *plt =
        sr_elf_get_procedure_linkage_table(binary_path,
                                            error_message);

    if (!plt)
        return false;

    struct sr_elf_fde *eh_frame =
        sr_elf_get_eh_frame(binary_path, error_message);

    if (!eh_frame)
    {
        sr_elf_procedure_linkage_table_free(plt);
        return false;
    }

    struct sr_disasm_state *disassembler =
        sr_disasm_init(binary_path, error_message);

    if (!disassembler)
    {
        sr_elf_procedure_linkage_table_free(plt);
        sr_elf_eh_frame_free(eh_frame);
        return false;
    }

    struct sr_callgraph *callgraph = NULL;

    while (thread)
    {
        struct sr_core_frame *frame = thread->frames;
        while (frame)
        {
            if (!frame->fingerprint &&
                0 == sr_strcmp0(frame->file_name, binary_path))
            {
                bool success = compute_fingerprint(frame,
                                                   plt,
                                                   eh_frame,
                                                   disassembler,
                                                   &callgraph,
                                                   error_message);

                if (!success)
                {
                    free(*error_message);
                    *error_message = NULL;
                }
            }

            frame = frame->next;
        }

        thread = thread->next;
    }

    sr_callgraph_free(callgraph);
    sr_disasm_free(disassembler);
    sr_elf_procedure_linkage_table_free(plt);
    sr_elf_eh_frame_free(eh_frame);
    return true;
}

static void
hash_frame (struct sr_core_frame *frame)
{
    if (!frame->fingerprint)
        return;

    char *hash = sr_sha1_hash_string(frame->fingerprint);
    free(frame->fingerprint);
    frame->fingerprint = hash;
    frame->fingerprint_hashed = true;
}

void
sr_core_fingerprint_hash(struct sr_core_stacktrace *stacktrace)
{
    struct sr_core_thread *thread = stacktrace->threads;
    while (thread)
    {
        struct sr_core_frame *frame = thread->frames;
        while (frame)
        {
            hash_frame(frame);

            frame = frame->next;
        }

        thread = thread->next;
    }
}
