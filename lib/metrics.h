/*
    metrics.h

    Copyright (C) 2010  Red Hat, Inc.

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
#ifndef BTPARSER_METRICS_H
#define BTPARSER_METRICS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compares two frames taking unknown functions ("??") as different ones.*/
int
frame_compare(struct btp_frame *frame1, struct btp_frame *frame2);


/* Jaro-Winkler distance:
 * Gets number of matching function names(match_count) from both threads and
 * number of transpositions in place(trans_count)
 * if the transpositioned function names are not farther away than frame_count/2 - 1.
 * Then computes the Jaro-Winkler distance according to the formula.
 * Returns a number between 0 and 1:
 * 0 = no similarity, 1 = similar threads
 */

float
thread_jarowinkler_distance(struct btp_thread *thread1, struct btp_thread *thread2);


/* Jaccard index:
 * Gives a number representing the difference of the size of the intersection
 * and union divided by the size of the union of two threads.
 * Function names positions in the thread are not taken into account.
 * Returns a number between 0 and 1:
 * 0 = no similarity, 1 = similar threads
 */

float
thread_jaccard_index(struct btp_thread *thread1, struct btp_thread *thread2);


/* Levenshtein distance:
 * Computes the distance of two threads creating a matrix of distances
 * between all the frames in each thread.
 * Returns a number representing how many function names need to be different
 * in one thread to match all the function names in second thread at each respective
 * positions or how many function names need to be different/transpositioned
 * to have a match if the transposition argument is enabled.
 * The distance is always between 0 and n, where n is the frame count of longer thread
 * 0 = similar threads, n = no similar function names
 */

int
thread_levenshtein_distance(struct btp_thread *thread1, struct btp_thread *thread2, bool transposition);

typedef int (*frame_cmp_type)(struct btp_frame*, struct btp_frame*);

/* Following three functions are equivalent to the three above except
 * that they take frame comparison function as an argument argument
 */
float
thread_jarowinkler_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        frame_cmp_type compare_func);
float
thread_jaccard_index_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        frame_cmp_type compare_func);
int
thread_levenshtein_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2, bool transposition,
        frame_cmp_type compare_func);

#ifdef __cplusplus
}
#endif

#endif
