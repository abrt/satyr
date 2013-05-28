/*
    metrics.c

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
#include "gdb_thread.h"
#include "gdb_frame.h"
#include "utils.h"
#include "metrics.h"
#include "normalize.h"
#include "thread.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

float
sr_gdb_thread_jarowinkler_distance_custom(struct sr_gdb_thread *thread1,
                                          struct sr_gdb_thread *thread2,
                                          sr_gdb_frame_cmp_type compare_func)
{
    int frame1_count = sr_thread_frame_count((struct sr_thread*) thread1);
    int frame2_count = sr_thread_frame_count((struct sr_thread*) thread2);

    if (frame1_count == 0 && frame2_count == 0)
    {
        return 1.0;
    }

    int max_frame_count = (frame2_count > frame1_count ? frame2_count : frame1_count);
    int i = 0, j, prefix_len = 0;
    bool match, still_prefix = true;
    float k, trans_count = 0, match_count = 0, dist_jaro, dist;


    struct sr_gdb_frame *curr_frame = thread1->frames;
    for (i = 1; curr_frame; i++)
    {
        match = false;
        struct sr_gdb_frame *curr_frame2 = thread2->frames;
        for (j = 1; !match && curr_frame2; j++)
        {
            /*whether the prefix continues to be the same for both threads or not*/
            if (i == j && 0 != compare_func(curr_frame, curr_frame2))
                still_prefix = false;

            /*getting a match only if not too far away from each other
              and if functions aren't both unpaired unknown functions */

            if (abs(i - j) <= max_frame_count / 2 - 1 &&
              0 == compare_func(curr_frame, curr_frame2))
            {
                match = true;
                if(i != j)trans_count++;  // transposition in place
            }
            curr_frame2 = curr_frame2->next;
        }

        if (still_prefix)
            prefix_len++;

        if (match)
            match_count++;

        curr_frame = curr_frame->next;
    }

    trans_count = trans_count / 2;

    if (prefix_len > 4)
        prefix_len = 4;

    if (match_count == 0)return 0;  // so as not to divide by 0

    dist_jaro = (match_count / (float)frame1_count +
                 match_count / (float)frame2_count +
                (match_count - trans_count) / match_count) / 3;

    k = 0.2;  /* how much weight we give to having common prefixes
               * (always k < 0.25) */

    dist = dist_jaro + (float)prefix_len * k * (1 - dist_jaro);

    return dist;
}

static bool
frames_contain(struct sr_gdb_frame *frames,
               struct sr_gdb_frame *frame,
               sr_gdb_frame_cmp_type compare_func)
{
    while (frames)
    {
        // checking if functions are the same but not both "??"
        if (!compare_func(frames, frame))
            return true;

        frames = frames->next;
    }

    return false;
}

float
sr_gdb_thread_jaccard_distance_custom(struct sr_gdb_thread *thread1,
                                      struct sr_gdb_thread *thread2,
                                      sr_gdb_frame_cmp_type compare_func)
{
    int union_size, intersection_size = 0, set1_size = 0, set2_size = 0;
    float j_distance;
    struct sr_gdb_frame *curr_frame;

    for (curr_frame = thread1->frames; curr_frame; curr_frame = curr_frame->next)
    {
        if (frames_contain(curr_frame->next, curr_frame, compare_func))
            continue; // not last, skip

        set1_size++;

        if (frames_contain(thread2->frames, curr_frame, compare_func))
            intersection_size++;
    }

    for (curr_frame = thread2->frames; curr_frame; curr_frame = curr_frame->next)
    {
        if (frames_contain(curr_frame->next, curr_frame, compare_func))
            continue; // not last, skip

        set2_size++;
    }

    union_size = set1_size + set2_size - intersection_size;
    if (!union_size)
        return 0.0;

    j_distance = 1.0 - intersection_size / (float)union_size;
    if (j_distance < 0.0)
        j_distance = 0.0;

    return j_distance;
}


int
sr_gdb_thread_levenshtein_distance_custom(struct sr_gdb_thread *thread1,
                                          struct sr_gdb_thread *thread2,
                                          bool transposition,
                                          sr_gdb_frame_cmp_type compare_func)
{
    int m = sr_thread_frame_count((struct sr_thread*) thread1) + 1;
    int n = sr_thread_frame_count((struct sr_thread*) thread2) + 1;

    // store only two last rows and columns instead of whole 2D array
    int dist[m + n + 1], dist1[m + n + 1], dist2;

    int i, j, l, cost = 0;

    // first row and column having distance equal to their position
    for (i = m; i > 0; i--)
        dist[m - i] = i;

    for (; i <= n; i++)
        dist[m + i] = i;

    struct sr_gdb_frame *curr_frame2 = thread2->frames;
    struct sr_gdb_frame *prev_frame = NULL;
    struct sr_gdb_frame *prev_frame2 = NULL;

    for (j = 1; curr_frame2; j++)
    {
        struct sr_gdb_frame *curr_frame = thread1->frames;
        for (i = 1; curr_frame; i++)
        {
            l = m + j - i;

            dist2 = dist1[l];
            dist1[l] = dist[l];

            /*similar characters have distance equal to the previous one diagonally,
              "??" functions aren't taken as similar */
            if (0 == compare_func(curr_frame, curr_frame2))
                cost = 0;
            // different ones takes the lowest value of all previous distances
            else
            {
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
                 0 == compare_func(curr_frame, prev_frame2) &&
                 0 == compare_func(prev_frame, curr_frame2)))
            {
                dist[l] = dist2 + cost;
            }

            prev_frame = curr_frame;
            curr_frame = curr_frame->next;
        }

        prev_frame2 = curr_frame2;
        curr_frame2 = curr_frame2->next;
    }

    return dist[n];
}

float
sr_gdb_thread_jarowinkler_distance(struct sr_gdb_thread *thread1,
                                   struct sr_gdb_thread *thread2)
{
    return sr_gdb_thread_jarowinkler_distance_custom(thread1,
                                                     thread2,
                                                     sr_gdb_frame_cmp_distance);
}

float
sr_gdb_thread_jaccard_distance(struct sr_gdb_thread *thread1,
                               struct sr_gdb_thread *thread2)
{
    return sr_gdb_thread_jaccard_distance_custom(thread1,
                                                 thread2,
                                                 sr_gdb_frame_cmp_distance);
}

int
sr_gdb_thread_levenshtein_distance(struct sr_gdb_thread *thread1,
                                   struct sr_gdb_thread *thread2,
                                   bool transposition)
{
    return sr_gdb_thread_levenshtein_distance_custom(thread1,
                                                     thread2,
                                                     transposition,
                                                     sr_gdb_frame_cmp_distance);
}

float
sr_gdb_thread_levenshtein_distance_f(struct sr_gdb_thread *thread1,
                                     struct sr_gdb_thread *thread2)
{
    int frame_count1, frame_count2, max_frame_count;

    frame_count1 = sr_thread_frame_count((struct sr_thread*) thread1);
    frame_count2 = sr_thread_frame_count((struct sr_thread*) thread2);
    max_frame_count = frame_count1 > frame_count2 ? frame_count1 : frame_count2;

    if (!max_frame_count)
        return 1.0;

    return (float)sr_gdb_thread_levenshtein_distance_custom(thread1,
                                                            thread2,
                                                            true,
                                                            sr_gdb_frame_cmp_distance) / max_frame_count;
}

static int
get_distance_position(const struct sr_distances *distances, int i, int j)
{
    /* The array holds only matrix entries (i, j) where i < j,
     * locate the position in the array. */
    assert(i < j && i >= 0 && i < distances->m && j < distances->n);

    int h = distances->n, l = distances->n - i;

    return ((h * h - h) - (l * l - l)) / 2 + j - 1;
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
    distances->distances = sr_malloc(sizeof(*distances->distances) *
                                      (get_distance_position(distances, m - 1, n - 1) + 1));

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

struct sr_distances *
sr_gdb_threads_compare(struct sr_gdb_thread **threads,
                       int m,
                       int n,
                       sr_dist_thread_type dist_func)
{
    struct sr_distances *distances;
    struct sr_gdb_thread *thread1, *thread2;
    int i, j, ok, all;

    distances = sr_distances_new(m, n);

    for (i = 0; i < m; i++)
    {
        for (j = i + 1; j < n; j++)
        {
            ok = all = 0;
            sr_gdb_thread_quality_counts(threads[i], &ok, &all);
            sr_gdb_thread_quality_counts(threads[j], &ok, &all);

            if (ok == all)
            {
                thread1 = threads[i];
                thread2 = threads[j];
            }
            else
            {
                /* There are some unknown function names, try to pair them, but
                 * the threads need to be copied first. */
                thread1 = sr_gdb_thread_dup(threads[i], false);
                thread2 = sr_gdb_thread_dup(threads[j], false);
                sr_normalize_gdb_paired_unknown_function_names(thread1, thread2);
            }

            distances->distances[get_distance_position(distances, i, j)] = dist_func(thread1, thread2);

            if (ok != all)
            {
                sr_gdb_thread_free(thread1);
                sr_gdb_thread_free(thread2);
            }
        }
    }

    return distances;
}
