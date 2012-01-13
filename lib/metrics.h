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

struct btp_frame;
struct btp_thread;

/* Jaro-Winkler distance:
 * Gets number of matching function names(match_count) from both threads and
 * number of transpositions in place(trans_count)
 * if the transpositioned function names are not farther away than frame_count/2 - 1.
 * Then computes the Jaro-Winkler distance according to the formula.
 * NOTE: The Jaro-Winkler distance is not a metric distance as it does not
 * satisfy the triangle inequality.
 * Returns a number between 0 and 1:
 * 0 = no similarity, 1 = similar threads
 */

float
btp_thread_jarowinkler_distance(struct btp_thread *thread1, struct btp_thread *thread2);


/* Jaccard distance:
 * Gives a number representing the difference of the size of the intersection
 * and union divided by the size of the union of two threads.
 * Function names positions in the thread are not taken into account.
 * Returns a number between 0 and 1:
 * 0 = similar threads, 1 = no similarity
 */

float
btp_thread_jaccard_distance(struct btp_thread *thread1, struct btp_thread *thread2);


/* Levenshtein distance:
 * Computes the distance of two threads creating a matrix of distances
 * between all the frames in each thread.
 * Returns a number representing how many function names need to be different
 * in one thread to match all the function names in second thread at each respective
 * positions or how many function names need to be different/transpositioned
 * to have a match if the transposition argument is enabled.
 * NOTE: With transpositions enabled the triangle inequality does not hold.
 * The distance is always between 0 and n, where n is the frame count of longer thread
 * 0 = similar threads, n = no similar function names
 */

int
btp_thread_levenshtein_distance(struct btp_thread *thread1, struct btp_thread *thread2, bool transposition);

/* Levenshtein distance returned with transpositions enabled and
 * returned in interval [0, 1] */
float
btp_thread_levenshtein_distance_f(struct btp_thread *thread1, struct btp_thread *thread2);

typedef int (*btp_frame_cmp_type)(struct btp_frame*, struct btp_frame*);

/* Following three functions are equivalent to the three above except
 * that they take frame comparison function as an argument argument
 */
float
btp_thread_jarowinkler_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        btp_frame_cmp_type compare_func);
float
btp_thread_jaccard_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        btp_frame_cmp_type compare_func);
int
btp_thread_levenshtein_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2, bool transposition,
        btp_frame_cmp_type compare_func);

/**
 * Represents an m-by-n distance matrix.
 * (only entries (i, j) where i < j are actually stored)
 */
struct btp_distances
{
    int m;
    int n;
    float *distances;
};

/**
 * Creates and initializes a new distances structure.
 * @param m
 * Number of rows.
 * @param n
 * Number of columns.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * calling the function btp_distances_free().
 */
struct btp_distances *
btp_distances_new(int m, int n);

/**
 * Creates a duplicate of the distances structure.
 * @param distances
 * It must be non-NULL pointer. The structure is not modified by calling
 * this function.
 * @returns
 * This function never returns NULL.
 */
struct btp_distances *
btp_distances_dup(struct btp_distances *distances);

/**
 * Releases the memory held by the distances structure.
 * @param distances
 * If the distances is NULL, no operation is performed.
 */
void
btp_distances_free(struct btp_distances *distances);

/**
 * Gets the entry (i, j) from the distance matrix.
 * @param distances
 * It must be non-NULL pointer.
 * @param i
 * Row in the matrix.
 * @param j
 * Column in the matrix.
 * @returns
 * For entries (i, i) zero distance is returned and values returned for
 * entries (i, j) and (j, i) are the same.
 */
float
btp_distances_get_distance(struct btp_distances *distances, int i, int j);

/**
 * Sets the entry (i, j) from the distance matrix.
 * @param distances
 * It must be non-NULL pointer.
 * @param i
 * Row in the matrix.
 * @param j
 * Column in the matrix.
 * @param d
 * Distance.
 */
void
btp_distances_set_distance(struct btp_distances *distances, int i, int j, float d);

/**
 * A function which compares two threads.
 */
typedef float (*btp_dist_thread_type)(struct btp_thread *, struct btp_thread *);

/**
 * Creates a distances structure by comparing threads.
 * @param threads
 * Array of threads. They are not modified by calling this function.
 * @param m
 * Compare first m threads from the array with other threads.
 * @param n
 * Number of threads in the passed array.
 * @param dist_func
 * Distance function which will be used to compare the threads. It's assumed to
 * be symmetric and return zero distance for equal threads.
 * @returns
 * This function never returns NULL.
 */
struct btp_distances *
btp_threads_compare(struct btp_thread **threads, int m, int n, btp_dist_thread_type dist_func);

#ifdef __cplusplus
}
#endif

#endif
