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
#ifndef SATYR_METRICS_H
#define SATYR_METRICS_H

/**
 * @file
 * @brief Distance between stack trace threads.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct sr_gdb_frame;
struct sr_gdb_thread;

/* Jaro-Winkler distance:
 *
 * Gets number of matching function names(match_count) from both
 * threads and number of transpositions in place(trans_count) if the
 * transpositioned function names are not farther away than
 * frame_count/2 - 1.  Then computes the Jaro-Winkler distance
 * according to the formula.  NOTE: The Jaro-Winkler distance is not a
 * metric distance as it does not satisfy the triangle inequality.
 * Returns a number between 0 and 1: 0 = no similarity, 1 = similar
 * threads
 */
float
sr_gdb_thread_jarowinkler_distance(struct sr_gdb_thread *thread1,
                                   struct sr_gdb_thread *thread2);


/* Jaccard distance:
 *
 * Gives a number representing the difference of the size of the
 * intersection and union divided by the size of the union of two
 * threads.  Function names positions in the thread are not taken into
 * account.  Returns a number between 0 and 1: 0 = similar threads, 1
 * = no similarity
 */
float
sr_gdb_thread_jaccard_distance(struct sr_gdb_thread *thread1,
                               struct sr_gdb_thread *thread2);


/* Levenshtein distance:
 *
 * Computes the distance of two threads creating a matrix of distances
 * between all the frames in each thread.  Returns a number
 * representing how many function names need to be different in one
 * thread to match all the function names in second thread at each
 * respective positions or how many function names need to be
 * different/transpositioned to have a match if the transposition
 * argument is enabled.  NOTE: With transpositions enabled the
 * triangle inequality does not hold.  The distance is always between
 * 0 and n, where n is the frame count of longer thread 0 = similar
 * threads, n = no similar function names
 */
int
sr_gdb_thread_levenshtein_distance(struct sr_gdb_thread *thread1,
                                   struct sr_gdb_thread *thread2,
                                   bool transposition);

/* Levenshtein distance returned with transpositions enabled and
 * returned in interval [0, 1].
 */
float
sr_gdb_thread_levenshtein_distance_f(struct sr_gdb_thread *thread1,
                                     struct sr_gdb_thread *thread2);

typedef int (*sr_gdb_frame_cmp_type)(struct sr_gdb_frame*,
                                     struct sr_gdb_frame*);

/* Following three functions are equivalent to the three above except
 * that they take frame comparison function as an argument argument
 */
float
sr_gdb_thread_jarowinkler_distance_custom(struct sr_gdb_thread *thread1,
                                          struct sr_gdb_thread *thread2,
                                          sr_gdb_frame_cmp_type compare_func);

float
sr_gdb_thread_jaccard_distance_custom(struct sr_gdb_thread *thread1,
                                      struct sr_gdb_thread *thread2,
                                      sr_gdb_frame_cmp_type compare_func);

int
sr_gdb_thread_levenshtein_distance_custom(struct sr_gdb_thread *thread1,
                                          struct sr_gdb_thread *thread2,
                                          bool transposition,
                                          sr_gdb_frame_cmp_type compare_func);

/**
 * @brief A distance matrix of stack trace threads.
 *
 * The distances are stored in a m-by-n two-dimensional array, where
 * only entries (i, j) where i < j are actually stored.
 */
struct sr_distances
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
 * calling the function sr_distances_free().
 */
struct sr_distances *
sr_distances_new(int m, int n);

/**
 * Creates a duplicate of the distances structure.
 * @param distances
 * It must be non-NULL pointer. The structure is not modified by calling
 * this function.
 * @returns
 * This function never returns NULL.
 */
struct sr_distances *
sr_distances_dup(struct sr_distances *distances);

/**
 * Releases the memory held by the distances structure.
 * @param distances
 * If the distances is NULL, no operation is performed.
 */
void
sr_distances_free(struct sr_distances *distances);

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
sr_distances_get_distance(struct sr_distances *distances, int i, int j);

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
sr_distances_set_distance(struct sr_distances *distances,
                          int i,
                          int j,
                          float d);

/**
 * A function which compares two threads.
 */
typedef float (*sr_dist_thread_type)(struct sr_gdb_thread *,
                                     struct sr_gdb_thread *);

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
struct sr_distances *
sr_gdb_threads_compare(struct sr_gdb_thread **threads,
                       int m, int n,
                       sr_dist_thread_type dist_func);

#ifdef __cplusplus
}
#endif

#endif
