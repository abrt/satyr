/*
    gdb_frame.h

    Copyright (C) 2010, 2011, 2012  Red Hat, Inc.

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
#ifndef SATYR_GDB_FRAME_H
#define SATYR_GDB_FRAME_H

/**
 * @file
 * @brief Single frame of GDB stack trace thread.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "../report_type.h"
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

struct sr_location;

typedef uint64_t sr_gdb_frame_address_t;

/**
 * @brief A function call of a GDB-produced stack trace.
 *
 * A frame representing a function call or a signal handler on a call
 * stack of a thread.
 */
struct sr_gdb_frame
{
    enum sr_report_type type;

    /**
     * A function name or NULL. If it's NULL, signal_handler_called is
     * true.
     */
    char *function_name;

    /**
     * A function type, or NULL if it isn't present.
     */
    char *function_type;

    /**
     * A frame number in a thread. It does not necessarily show the
     * actual position in the thread, as this number is set by the
     * parser and never updated.
     */
    uint32_t number;

    /**
     * The name of the source file containing the function definition,
     * or the name of the binary file (.so) with the binary code of
     * the function, or NULL.
     */
    char *source_file;

    /**
     * A line number in the source file, determining the position of
     * the function definition, or -1 when unknown.
     */
    uint32_t source_line;

    /**
     * Signal handler was called on this frame.
     */
    bool signal_handler_called;

    /**
     * The function address in the computer memory, or -1 when the
     * address is unknown. Address is unknown when the frame
     * represents inlined function.
     */
    sr_gdb_frame_address_t address;

    /**
     * A library name or NULL.
     */
    char *library_name;

    /**
     * A sibling frame residing below this one, or NULL if this is the
     * last frame in the parent thread.
     */
    struct sr_gdb_frame *next;
};

/**
 * Creates and initializes a new frame structure.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function sr_gdb_frame_free().
 */
struct sr_gdb_frame *
sr_gdb_frame_new(void);

/**
 * Initializes all members of the frame structure to their default
 * values.  No memory is released, members are simply overwritten.
 * This is useful for initializing a frame structure placed on the
 * stack.
 */
void
sr_gdb_frame_init(struct sr_gdb_frame *frame);

/**
 * Releases the memory held by the frame. The frame siblings are not
 * released.
 * @param frame
 * If the frame is NULL, no operation is performed.
 */
void
sr_gdb_frame_free(struct sr_gdb_frame *frame);

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
 * be released by calling the function sr_gdb_frame_free().
 */
struct sr_gdb_frame *
sr_gdb_frame_dup(struct sr_gdb_frame *frame,
                 bool siblings);

/**
 * Checks whether the frame represents a call of function with certain
 * function name.
 * @param frame
 *   A stack trace frame.
 * @param ...
 *   Names of source files or shared libaries that should contain the
 *   function name.  The list needs to be terminated by NULL.  Just
 *   NULL can be provided, and source file cannot be present in order
 *   to succeed.  An empty string will cause ANY source file to match
 *   and succeed.  The name of source file is searched as a substring.
 * @returns
 *   True if the frame corresponds to a function with function_name,
 *   residing in a source file.
 */
bool
sr_gdb_frame_calls_func(struct sr_gdb_frame *frame,
                        const char *function_name,
                        ...);

/**
 * Compares two frames.
 * @param frame1
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param frame2
 * It must be non-NULL pointer. It's not modified by calling this
 * function.
 * @param compare_number
 * Indicates whether to include the frame numbers in the
 * comparsion. If set to false, the frame numbers are ignored.
 * @returns
 * Returns 0 if the frames are same.  Returns negative number if
 * frame1 is found to be 'less' than frame2.  Returns positive number
 * if frame1 is found to be 'greater' than frame2.
 */
int
sr_gdb_frame_cmp(struct sr_gdb_frame *frame1,
                 struct sr_gdb_frame *frame2,
                 bool compare_number);

/**
 * Compares two frames, but only by their function and library names.
 * Two unknown functions ("??") are assumed to be different and unknown
 * library names to be the same.
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
sr_gdb_frame_cmp_distance(struct sr_gdb_frame *frame1,
                          struct sr_gdb_frame *frame2);

/**
 * Appends 'item' at the end of the list 'dest'.
 * @returns
 * This function returns the 'dest' frame.  If 'dest' is NULL, it
 * returns the 'item' frame.
 */
struct sr_gdb_frame *
sr_gdb_frame_append(struct sr_gdb_frame *dest,
                    struct sr_gdb_frame *item);

/**
 * Appends the textual representation of the frame to the string
 * buffer.
 * @param frame
 * It must be a non-NULL pointer.  It's not modified by calling this
 * function.
 */
void
sr_gdb_frame_append_to_str(struct sr_gdb_frame *frame,
                           GString *dest,
                           bool verbose);

/**
 * If the input contains a complete frame, this function parses the
 * frame text, returns it in a structure, and moves the input pointer
 * after the frame.  If the input does not contain proper, complete
 * frame, the function does not modify input and returns NULL.
 * @returns
 * Allocated pointer with a frame structure. The pointer should be
 * released by sr_gdb_frame_free().
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  When this function returns NULL, the structure will contain
 * the error line, column, and message.  The line and column members
 * of the location are gradually increased as the parser handles the
 * input, so the location should be initialized before calling this
 * function to get reasonable values.
 */
struct sr_gdb_frame *
sr_gdb_frame_parse(const char **input,
                   struct sr_location *location);

/**
 * If the input contains a proper frame start section, parse the frame
 * number, and move the input pointer after this section. Otherwise do
 * not modify input.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a frame start.
 * @code
 * "#1 "
 * "#255  "
 * @endcode
 */
int
sr_gdb_frame_parse_frame_start(const char **input,
                               uint32_t *number);

/**
 * Parses C++ operator on input.  Supports even 'operator new[]' and
 * 'operator delete[]'.
 * @param target
 * The parsed operator name is appened to the string buffer provided,
 * if an operator is found. Otherwise the string buffer is not
 * changed.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain operator.
 */
int
sr_gdb_frame_parseadd_operator(const char **input,
                               GString *target);

/**
 * Parses a part of function name from the input.
 * @param target
 * Pointer to a non-allocated pointer. This function will set the
 * pointer to newly allocated memory containing the name chunk, if it
 * returns positive, nonzero value.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a part of function name.
 */
int
sr_gdb_frame_parse_function_name_chunk(const char **input,
                                       bool space_allowed,
                                       char **target);

/**
 * If the input buffer contains part of function name containing
 * braces, for example "(anonymous namespace)", parse it, append the
 * contents to target and move input after the braces.  Otherwise do
 * not modify the input and the target.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a braced part of function name.
 */
int
sr_gdb_frame_parse_function_name_braces(const char **input,
                                        char **target);

/**
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a template part of function name.
 */
int
sr_gdb_frame_parse_function_name_template(const char **input,
                                          char **target);

/**
 * If the input buffer contains part of function name containing template args,
 * for example
 * " [with Object = bmalloc::Heap; Function = void (bmalloc::Heap::*)()]", parse
 * it, append the contents to the target and move the input after the trailing
 * square brace. Otherwise do not modify the input and the target.
 * @returns
 * The number of characters parsed from input. 0 if the input does not
 * contain a template args part of function name.
 */
int
sr_gdb_frame_parse_function_name_template_args(const char **input,
                                               char **target);

/**
 * Parses the function name, which is a part of the frame header, from
 * the input. If the frame header contains also the function type,
 * it's also parsed.
 * @param function_name
 * A pointer pointing to an uninitialized pointer. This function
 * allocates a string and sets the pointer to it if it parses the
 * function name from the input successfully.  The memory returned
 * this way must be released by the caller using the function free().
 * If this function returns true, this pointer is guaranteed to be
 * non-NULL.
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 * @returns
 * True if the input stream contained a function name, which has been
 * parsed. False otherwise.
 */
bool
sr_gdb_frame_parse_function_name(const char **input,
                                 char **function_name,
                                 char **function_type,
                                 struct sr_location *location);

/**
 * Skips function arguments which are a part of the frame header, in
 * the input stream.
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
sr_gdb_frame_skip_function_args(const char **input,
                                struct sr_location *location);

/**
 * If the input contains proper function call, parse the function
 * name and store it to result, move the input pointer after whole
 * function call, and return true. Otherwise do not modify the input
 * and return false.
 *
 * If this function returns true, the caller is responsible to free
 * the the function_name.
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
sr_gdb_frame_parse_function_call(const char **input,
                                 char **function_name,
                                 char **function_type,
                                 struct sr_location *location);

/**
 * If the input contains address and function call, parse them, move
 * the input pointer after this sequence, and return true.
 * Otherwise do not modify the input and return false.
 *
 * If this function returns true, the caller is responsible to free
 * the parameter function.
 *
 * @code
 * 0x000000322160e7fd in fsync ()
 * 0x000000322222987a in write_to_temp_file (
 * filename=0x18971b0 "/home/jfclere/.recently-used.xbel",
 * contents=<value optimized out>, length=29917, error=0x7fff3cbe4110)
 * @endcode
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
sr_gdb_frame_parse_address_in_function(const char **input,
                                       uint64_t *address,
                                       char **function_name,
                                       char **function_type,
                                       struct sr_location *location);

/**
 * If the input contains sequence "from path/to/file:fileline" or "at
 * path/to/file:fileline", parse it, move the input pointer after this
 * sequence and return true. Otherwise do not modify the input and
 * return false.
 *
 * The ':' followed by line number is optional. If it is not present,
 * the fileline is set to -1.
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 */
bool
sr_gdb_frame_parse_file_location(const char **input,
                                 char **file,
                                 uint32_t *file_line,
                                 struct sr_location *location);

/**
 * If the input contains proper frame header, this function
 * parses the frame header text, moves the input pointer
 * after the frame header, and returns a frame struct.
 * If the input does not contain proper frame header, this function
 * returns NULL and does not modify input.
 * @param location
 * The caller must provide a pointer to an instance of sr_location
 * here.  The line and column members of the location are gradually
 * increased as the parser handles the input, so the location should
 * be initialized before calling this function to get reasonable
 * values.  When this function returns false (an error occurred), the
 * structure will contain the error line, column, and message.
 * @returns
 * Newly created frame struct or NULL. The returned frame struct
 * should be released by sr_gdb_frame_free().
 */
struct sr_gdb_frame *
sr_gdb_frame_parse_header(const char **input,
                          struct sr_location *location);

#ifdef __cplusplus
}
#endif

#endif
