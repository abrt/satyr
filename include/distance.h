/*
    distance.h

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
#ifndef SATYR_DISTANCE_H
#define SATYR_DISTANCE_H

/**
 * @file
 * @brief Distance between stack trace threads.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct sr_thread;

enum sr_distance_type
{
    /* Jaro-Winkler distance:
     *
     * Gets number of matching function names(match_count) from both
     * threads and number of transpositions in place(trans_count) if
     * the transpositioned function names are not farther away than
     * frame_count/2 - 1.  Then computes the Jaro-Winkler distance
     * according to the formula.  NOTE: The Jaro-Winkler distance is
     * not a metric distance as it does not satisfy the triangle
     * inequality.  Returns a number between 0 and 1: 0 = no
     * similarity, 1 = similar threads.
     */
    SR_DISTANCE_JARO_WINKLER,

    /* Jaccard distance:
     *
     * Gives a number representing the difference of the size of the
     * intersection and union divided by the size of the union of two
     * threads.  Function names positions in the thread are not taken
     * into account.  Returns a number between 0 and 1: 0 = similar
     * threads, 1 = no similarity.
     */
    SR_DISTANCE_JACCARD,

    /* Levenshtein distance:
     *
     * Computes the distance of two threads creating a matrix of
     * distances between all the frames in each thread.  Computes a
     * number representing how many function names need to be
     * different in one thread to match all the function names in
     * second thread at each respective positions or how many function
     * names need to be different to have a match.  The number is
     * always between 0 and n, where n is the frame count of longer
     * thread 0 = similar threads, n = no similar function names.
     * This is normalized to a result between 0 and 1 that is
     * returned.
     */
    SR_DISTANCE_LEVENSHTEIN,

    /* Damerau-Levenshtein distance:
     *
     * Like the Levenshtein distance, but with transpositions. NOTE:
     * The triangle inequality does not hold.
     */
    SR_DISTANCE_DAMERAU_LEVENSHTEIN,

    /* Sentinel, keep it the last entry. */
    SR_DISTANCE_NUM
};

float
sr_distance(enum sr_distance_type distance_type,
            struct sr_thread *thread1,
            struct sr_thread *thread2);

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
 * Creates a distances structure by comparing threads.
 * @param threads
 * Array of threads. They are not modified by calling this function.
 * @param m
 * Compare first m threads from the array with other threads.
 * @param n
 * Number of threads in the passed array.
 * @param dist_type
 * Type of distance to compute.
 * @returns
 * This function never returns NULL.
 */
struct sr_distances *
sr_threads_compare(struct sr_thread **threads, int m, int n,
                   enum sr_distance_type dist_type);
#ifdef __cplusplus
}
#endif

#endif
