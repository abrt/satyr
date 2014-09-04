/*
    distance.c

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
#include "distance.h"
#include "thread.h"
#include "frame.h"
#include "normalize.h"
#include "utils.h"
#include "gdb/thread.h"
#include "internal_utils.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

float
distance_jaro_winkler(struct sr_thread *thread1,
                      struct sr_thread *thread2)
{
    assert(thread1->type == thread2->type);

    int frame1_count = sr_thread_frame_count(thread1);
    int frame2_count = sr_thread_frame_count(thread2);

    if (frame1_count == 0 && frame2_count == 0)
        return 1.0;

    int max_frame_count = frame2_count;
    if (max_frame_count < frame1_count)
        max_frame_count = frame1_count;

    int prefix_len = 0;
    bool still_prefix = true;
    float trans_count = 0, match_count = 0;

    struct sr_frame *curr_frame = sr_thread_frames(thread1);
    for (int i = 1; curr_frame; ++i)
    {
        bool match = false;
        struct sr_frame *curr_frame2 = sr_thread_frames(thread2);
        for (int j = 1; !match && curr_frame2; ++j)
        {
            /* Whether the prefix continues to be the same for both
             * threads or not.
             */
            if (i == j && 0 != sr_frame_cmp_distance(curr_frame, curr_frame2))
                still_prefix = false;

            /* Getting a match only if not too far away from each
             * other and if functions aren't both unpaired unknown
             * functions.
             */
            if (abs(i - j) <= max_frame_count / 2 - 1 &&
                0 == sr_frame_cmp_distance(curr_frame, curr_frame2))
            {
                match = true;
                if (i != j)
                    ++trans_count;  // transposition in place
            }

            curr_frame2 = sr_frame_next(curr_frame2);
        }

        if (still_prefix)
            ++prefix_len;

        if (match)
            ++match_count;

        curr_frame = sr_frame_next(curr_frame);
    }

    trans_count /= 2;

    if (prefix_len > 4)
        prefix_len = 4;

    if (0 == match_count)
        return 0;  // so as not to divide by 0

    float dist_jaro = (match_count / (float)frame1_count +
                       match_count / (float)frame2_count +
                       (match_count - trans_count) / match_count) / 3;

    /* How much weight we give to having common prefixes
     * (always k < 0.25).
     */
    float k = 0.2;

    float dist = dist_jaro + (float)prefix_len * k * (1 - dist_jaro);
    return dist;
}

static bool
distance_jaccard_frames_contain(struct sr_frame *haystack,
                                struct sr_frame *needle)
{
    while (haystack)
    {
        // Checking if functions are the same but not both "??".
        if (!sr_frame_cmp_distance(haystack, needle))
            return true;

        haystack = sr_frame_next(haystack);
    }

    return false;
}

float
distance_jaccard(struct sr_thread *thread1,
                 struct sr_thread *thread2)
{
    assert(thread1->type == thread2->type);

    int intersection_size = 0, set1_size = 0, set2_size = 0;

    for (struct sr_frame *curr_frame = sr_thread_frames(thread1);
         curr_frame;
         curr_frame = sr_frame_next(curr_frame))
    {
        if (distance_jaccard_frames_contain(
                sr_frame_next(curr_frame),
                curr_frame))
        {
            continue; // not last, skip
        }

        ++set1_size;

        if (distance_jaccard_frames_contain(
                sr_thread_frames(thread2),
                curr_frame))
        {
            ++intersection_size;
        }
    }

    for (struct sr_frame *curr_frame = sr_thread_frames(thread2);
         curr_frame;
         curr_frame = sr_frame_next(curr_frame))
    {
        if (distance_jaccard_frames_contain(
                sr_frame_next(curr_frame),
                curr_frame))
        {
            continue; // not last, skip
        }

        ++set2_size;
    }

    int union_size = set1_size + set2_size - intersection_size;
    if (!union_size)
        return 0.0;

    float j_distance = 1.0 - intersection_size / (float)union_size;
    if (j_distance < 0.0)
        j_distance = 0.0;

    return j_distance;
}

float
distance_levenshtein(struct sr_thread *thread1,
                     struct sr_thread *thread2,
                     bool transposition)
{
    assert(thread1->type == thread2->type);

    int frame_count1 = sr_thread_frame_count(thread1);
    int frame_count2 = sr_thread_frame_count(thread2);

    int max_frame_count = frame_count2;
    if (max_frame_count < frame_count1)
        max_frame_count = frame_count1;

    /* Avoid division by zero in case we get two empty threads */
    if (max_frame_count == 0)
        return 0.0;

    int m = frame_count1 + 1;
    int n = frame_count2 + 1;

    // store only two last rows and columns instead of whole 2D array
    SR_ASSERT(n <= SIZE_MAX - 1);
    SR_ASSERT(m <= SIZE_MAX - (n + 1));
    int *dist = sr_malloc_array(sizeof(int), m + n + 1);
    int *dist1 = sr_malloc_array(sizeof(int), m + n + 1);

    // first row and column having distance equal to their position
    for (int i = m; i > 0; --i)
        dist[m - i] = i;

    for (int i = 0; i <= n; ++i)
        dist[m + i] = i;

    struct sr_frame *curr_frame2 = sr_thread_frames(thread2);
    struct sr_frame *prev_frame = NULL;
    struct sr_frame *prev_frame2 = NULL;

    for (int j = 1; curr_frame2; ++j)
    {
        struct sr_frame *curr_frame = sr_thread_frames(thread1);
        for (int i = 1; curr_frame; ++i)
        {
            int l = m + j - i;

            int dist2 = dist1[l];
            dist1[l] = dist[l];

            int cost;

            /*similar characters have distance equal to the previous
              one diagonally, "??" functions aren't taken as
              similar */
            if (0 == sr_frame_cmp_distance(curr_frame, curr_frame2))
                cost = 0;
            else
            {
                // different ones takes the lowest value of all
                // previous distances
                cost = 1;
                dist[l] += 1;
                if (dist[l] > dist[l - 1] + 1)
                    dist[l] = dist[l - 1] + 1;

                if (dist[l] > dist[l + 1] + 1)
                    dist[l] = dist[l + 1] + 1;
            }

            /*checking for transposition of two characters in both ways
              taking into account that "??" functions are not similar*/
            if (transposition &&
                (i >= 2 && j >= 2 && dist[l] > dist2 + cost &&
                 0 == sr_frame_cmp_distance(curr_frame, prev_frame2) &&
                 0 == sr_frame_cmp_distance(prev_frame, curr_frame2)))
            {
                dist[l] = dist2 + cost;
            }

            prev_frame = curr_frame;
            curr_frame = sr_frame_next(curr_frame);
        }

        prev_frame2 = curr_frame2;
        curr_frame2 = sr_frame_next(curr_frame2);
    }

    int result = dist[n];
    free(dist);
    free(dist1);

    return (float)result / max_frame_count;
}

float
sr_distance(enum sr_distance_type distance_type,
            struct sr_thread *thread1,
            struct sr_thread *thread2)
{
    /* Different thread types are always unequal. */
    if (thread1->type != thread2->type)
        return 1.0f;

    switch (distance_type)
    {
    case SR_DISTANCE_JARO_WINKLER:
        return distance_jaro_winkler(thread1, thread2);
    case SR_DISTANCE_JACCARD:
        return distance_jaccard(thread1, thread2);
    case SR_DISTANCE_LEVENSHTEIN:
        return distance_levenshtein(thread1, thread2, false);
    case SR_DISTANCE_DAMERAU_LEVENSHTEIN:
        return distance_levenshtein(thread1, thread2, true);
    default:
        return 1.0f;
    }
}

static int
get_distance_position_mn(int m, int n, int i, int j)
{
    /* The array holds only matrix entries (i, j) where i < j,
     * locate the position in the array. */
    assert(i < j && i >= 0 && i < m && j < n);

    int h = n, l = n - i;

    return ((h * h - h) - (l * l - l)) / 2 + j - 1;
}

static int
get_distance_position(const struct sr_distances *distances, int i, int j)
{
    return get_distance_position_mn(distances->m, distances->n, i, j);
}

struct sr_distances *
sr_distances_new(int m, int n)
{
    struct sr_distances *distances = sr_malloc(sizeof(struct sr_distances));

    /* The number of rows has to be smaller than columns. */
    if (m >= n)
        m = n - 1;

    assert(m > 0 && n > 1 && m < n);

    distances->m = m;
    distances->n = n;
    distances->distances = sr_malloc_array(
                               get_distance_position(distances, m - 1, n - 1) + 1,
                               sizeof(*distances->distances)
                           );

    return distances;
}

struct sr_distances *
sr_distances_dup(struct sr_distances *distances)
{
    struct sr_distances *dup_distances;

    dup_distances = sr_distances_new(distances->m, distances->n);
    memcpy(dup_distances->distances, distances->distances,
            sizeof(*distances->distances) *
            (get_distance_position(distances, distances->m - 1, distances->n - 1) + 1));

    return dup_distances;
}

void
sr_distances_free(struct sr_distances *distances)
{
    if (!distances)
        return;

    free(distances->distances);
    free(distances);
}

float
sr_distances_get_distance(struct sr_distances *distances, int i, int j)
{
    if (i == j)
        return 0.0;

    if (i > j)
    {
        int x;
        x = j, j = i, i = x;
    }

    return distances->distances[get_distance_position(distances, i, j)];
}

void
sr_distances_set_distance(struct sr_distances *distances,
                          int i,
                          int j,
                          float d)
{
    if (i == j)
        return;

    if (i > j)
    {
        int x;
        x = j, j = i, i = x;
    }

    distances->distances[get_distance_position(distances, i, j)] = d;
}

static float
normalize_and_compare(struct sr_thread *t1, struct sr_thread *t2,
                      enum sr_distance_type dist_type)
{
    float dist;

    /* XXX: GDB crashes have a special normalization step for
     * clustering. If there's something similar for other types, we can
     * generalize it -- meanwhile there's a separate case for GDB here
     */
    if (t1->type == SR_REPORT_GDB)
    {
        struct sr_gdb_thread *copy1 = (struct sr_gdb_thread*)t1,
                             *copy2 = (struct sr_gdb_thread*)t2;
        int ok = 0, all = 0;

        sr_gdb_thread_quality_counts(copy1, &ok, &all);
        sr_gdb_thread_quality_counts(copy2, &ok, &all);

        if (ok != all)
        {
            /* There are some unknown function names, try to pair them, but
             * the threads need to be copied first. */
            copy1 = sr_gdb_thread_dup(copy1, false);
            copy2 = sr_gdb_thread_dup(copy2, false);
            sr_normalize_gdb_paired_unknown_function_names(copy1, copy2);
        }

        dist = sr_distance(dist_type, (struct sr_thread*)copy1,
                           (struct sr_thread*)copy2);

        if (ok != all)
        {
            sr_gdb_thread_free(copy1);
            sr_gdb_thread_free(copy2);
        }
    }
    else
        dist = sr_distance(dist_type, t1, t2);

    return dist;
}

struct sr_distances *
sr_threads_compare(struct sr_thread **threads,
                   int m,
                   int n,
                   enum sr_distance_type dist_type)
{
    struct sr_distances *distances;
    int i, j;

    distances = sr_distances_new(m, n);

    if (n <= 0)
        return distances;

    /* Check that all threads are of the same type */
    enum sr_report_type type, prev_type = threads[0]->type;
    for (i = 0; i < n; i++)
    {
        type = threads[i]->type;
        assert(prev_type == type);
        prev_type = type;
    }

    for (i = 0; i < m; i++)
    {
        for (j = i + 1; j < n; j++)
        {

            distances->distances[get_distance_position(distances, i, j)]
                = normalize_and_compare(threads[i], threads[j], dist_type);
        }
    }

    return distances;
}
