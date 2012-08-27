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
#include "core_backtrace.h"
#include "core_frame.h"
#include "core_thread.h"
#include "utils.h"
#include "strbuf.h"
#include "elves.h"
#include "disassembler.h"
#include <ctype.h>
#include <string.h>

/* Global iteration limit for algorithms walking call graphs. */
/*
static const unsigned call_graph_iteration_limit = 512;

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
                     GList *strings)
{
    bool first = true;
    GList *it;

    btp_strbuf_append_strf(buffer, "%s:", name);

    for (it = strings; it != NULL; it = g_list_next(it))
    {
        btp_strbuf_append_strf(buffer, "%s%s", (first ? "" : ","), (char*)it->data);
        first = false;
    }

    if (first)
    {
        btp_strbuf_append_strf(buffer, "-");
    }
}

static void
fp_jump_equality(struct btp_strbuf *buffer, GList *insns,
                 uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"je", "jne", "jz", "jnz", NULL};
    instruction_present(buffer, insns, mnems, "j_eql");
}

static void
fp_jump_signed(struct btp_strbuf *buffer, GList *insns,
               uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"jg", "jl", "jnle", "jnge", "jng", "jnl", "jle", "jge", NULL};
    instruction_present(buffer, insns, mnems, "j_sgn");
}

static void
fp_jump_unsigned(struct btp_strbuf *buffer, GList *insns,
                 uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"ja", "jb", "jnae", "jnbe", "jna", "jnb", "jbe", "jae", NULL};
    instruction_present(buffer, insns, mnems, "j_usn");
}

static void
fp_and_or(struct btp_strbuf *buffer, GList *insns,
          uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"and", "or", NULL};
    instruction_present(buffer, insns, mnems, "and_or");
}

static void
fp_shift(struct btp_strbuf *buffer, GList *insns,
         uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"shl", "shr", NULL};
    instruction_present(buffer, insns, mnems, "shift");
}

static void
fp_has_cycle(struct btp_strbuf *buffer,
             GList *insns,
             uintptr_t begin,
             uintptr_t end,
             GHashTable *call_graph,
             GHashTable *plt)
{
    GList *it;
    static const char *jmp_mnems[] = {"jmp", "jmpb", "jmpw", "jmpl", "jmpq", NULL};
    bool found = false;

    for (it = insns; it != NULL; it = g_list_next(it))
    {
        char *insn = it->data;
        uintptr_t target;

        if (!insn_is_one_of(insn, jmp_mnems))
            continue;

        if (!insn_has_single_addr_operand(insn, &target))
            continue;

        if (begin <= target && target < end)
        {
            found = true;
            break;
        }
    }

    fingerprint_add_bool(buffer, "has_cycle", found);
}
*/

/*
static void
fp_libcalls(struct btp_strbuf *buffer,
            GList *insns,
            uintptr_t begin,
            uintptr_t end,
            GHashTable *call_graph,
            GHashTable *plt)
{
    GList *it, *sym = NULL;
*/
    /* Look up begin in call graph */
/*   GList *callees = g_hash_table_lookup(call_graph, &begin);
 */
    /* Resolve addresses to names if they are in PLT */
/*    for (it = callees; it != NULL; it = g_list_next(it))
    {
        char *s = g_hash_table_lookup(plt, it->data);
        if (s && !g_list_find_custom(sym, s, (GCompareFunc)strcmp))
        {
            sym = g_list_insert_sorted(sym, s, (GCompareFunc)strcmp);
        }
    }
*/
    /* Format the result */
/*   fingerprint_add_list(buffer, "libcalls", sym);
}
*/

/*
static void
fp_calltree_leaves(struct btp_strbuf *buffer,
		   GList *insns,
                   uintptr_t begin,
		   uintptr_t end,
		   GHashTable *call_graph,
		   GHashTable *plt)
{
    unsigned iterations_allowed = call_graph_iteration_limit;
    GHashTable *visited = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, NULL);
    GList *queue = g_list_append(NULL, addr_alloc(begin));
    GList *it, *sym = NULL;

    while (queue != NULL && iterations_allowed)
    {*/
        /* Pop one element */
/*        it = g_list_first(queue);
        queue = g_list_remove_link(queue, it);
        uintptr_t *key = (uintptr_t*)(it->data);*/
        /* uintptr_t addr = *key; */
/*       g_list_free(it);
        iterations_allowed--;
*/
        /* Check if it is not already visited */
/*       if (g_hash_table_lookup_extended(visited, key, NULL, NULL))
        {
            free(key);
            continue;
        }
        g_hash_table_insert(visited, key, key);
*/
        /* Lookup callees */
/*        GList *callees = g_hash_table_lookup(call_graph, key);*/

        /* If callee is PLT, add the corresponding symbols, otherwise
         * extend the worklist */
        /*       for (it = callees; it != NULL; it = g_list_next(it))
        {
            char *s = g_hash_table_lookup(plt, it->data);
            if (s && !g_list_find_custom(sym, s, (GCompareFunc)strcmp))
            {
                sym = g_list_insert_sorted(sym, s, (GCompareFunc)strcmp);
            }
            else if (s == NULL)
            {
                queue = g_list_append(queue, addr_alloc(*(uintptr_t*)(it->data)));
            }
        }
    }
    g_hash_table_destroy(visited);
    list_free_with_free(queue);

    fingerprint_add_list(buffer, "calltree_leaves", sym);
}
        */
bool
btp_core_fingerprint_generate(struct btp_core_backtrace *backtrace,
                              char **error_message)
{
    struct btp_core_thread *thread = backtrace->threads;
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
                    struct btp_elf_frame_description_entry *eh_frame,
                    struct btp_disasm_state *disassembler,
                    char **error_message)
{
    struct btp_elf_frame_description_entry *fde =
        btp_elf_find_fde_for_address(eh_frame, frame->build_id_offset);

    if (!fde)
    {
        *error_message = btp_asprintf("No frame description entry found"
                                      " for an offset.");
        return false;
    }

    char **instructions =
        btp_disasm_get_function_instructions(disassembler,
                                             fde->start_address,
                                             fde->length,
                                             error_message);

    if (!instructions)
        return false;

    struct btp_strbuf *fingerprint = btp_strbuf_new();
/*
    fp_jump_equality(instructions);
    fp_jump_signed(instructions);
    fp_jump_unsigned(instructions);
    fp_and_or(instructions);
    fp_shift(instructions);
    fp_has_cycle(instructions);
    fp_libcalls(instructions);
    fp_calltree_leaves(instructions);
*/
    frame->fingerprint = btp_strbuf_free_nobuf(fingerprint);
    btp_disasm_function_instructions_free(instructions);
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

    struct btp_elf_frame_description_entry *eh_frame =
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

    btp_disasm_free(disassembler);
    btp_elf_procedure_linkage_table_free(plt);
    btp_elf_eh_frame_free(eh_frame);
    return true;
}
