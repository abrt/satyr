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
    SR_DISTANCE_DAMERAU_LEVENSHTEIN
};

float
sr_distance(enum sr_distance_type distance_type,
            struct sr_thread *thread1,
            struct sr_thread *thread2);

#ifdef __cplusplus
}
#endif

#endif
