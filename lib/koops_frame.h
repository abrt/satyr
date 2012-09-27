/*
    koops_frame.h

    Copyright (C) 2012  ABRT Team
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
#ifndef BTPARSER_KOOPS_FRAME_H
#define BTPARSER_KOOPS_FRAME_H

/**
 * @file
 * @brief Kernel oops stack frame.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Kernel oops stack frame.
 */
struct btp_koops_frame
{
    /**
     * Address of the function in memory.
     */
    uint64_t address;

    /**
     * http://git.kernel.org/?p=linux/kernel/git/torvalds/linux.git;a=blob;f=arch/x86/kernel/dumpstack.c
     * printk_address(unsigned long address, int reliable)
     */
    bool reliable;

    char *function_name;

    uint64_t function_offset;

    uint64_t function_length;

    uint64_t from_address;

    char *from_function_name;

    uint64_t from_function_offset;

    uint64_t from_function_length;

    char *module_name;

    struct btp_koops_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_koops_frame_free().
 */
struct btp_koops_frame *
btp_koops_frame_new();

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
btp_koops_frame_init(struct btp_koops_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
btp_koops_frame_free(struct btp_koops_frame *frame);

/**
 * Creates a duplicate of the frame.
 * @param frame
 * It must be non-NULL pointer. The frame is not modified by calling
 * this function.
 * @param siblings
 * Whether to duplicate also siblings referenced by frame->next.  If
 * false, frame->next is not duplicated for the new frame, but it is
 * set to NULL.
 * @returns
 * This function never returns NULL. The returned duplicate frame must
 * be released by calling the function btp_koops_frame_free().
 */
struct btp_koops_frame *
btp_koops_frame_dup(struct btp_koops_frame *frame,
                    bool siblings);

/**
 * Compares two frames.
 * @param frame1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param frame2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @returns
 * Returns 0 if the frames are same.  Returns negative number if
 * frame1 is found to be 'less' than frame2.  Returns positive number
 * if frame1 is found to be 'greater' than frame2.
 */
int
btp_koops_frame_cmp(struct btp_koops_frame *frame1,
                    struct btp_koops_frame *frame2);

#ifdef __cplusplus
}
#endif

#endif
