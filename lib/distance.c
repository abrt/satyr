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
#include "gdb_thread.h"
#include "gdb_frame.h"
#include "core_thread.h"
#include "core_frame.h"
#include "java_thread.h"
#include "java_frame.h"
#include "koops_stacktrace.h"
#include "koops_frame.h"
#include "python_stacktrace.h"
#include "python_frame.h"
#include "thread.h"
#include <stdlib.h>

typedef int (*frame_cmp_function)(void*, void*);
typedef void* (*frame_next_function)(void*);
typedef void* (*thread_first_frame_function)(void*);

struct type_specific_functions
{
    frame_cmp_function frame_cmp;
    frame_next_function frame_next;
    thread_first_frame_function thread_first_frame;
};

float
distance_jaro_winkler(struct type_specific_functions functions,
                      void *thread1,
                      void *thread2)
{
    int frame1_count = sr_thread_frame_count((struct sr_thread*) thread1);
    int frame2_count = sr_thread_frame_count((struct sr_thread*) thread2);

    if (frame1_count == 0 && frame2_count == 0)
        return 1.0;

    int max_frame_count = frame2_count;
    if (max_frame_count < frame1_count)
        max_frame_count = frame1_count;

    int prefix_len = 0;
    bool still_prefix = true;
    float trans_count = 0, match_count = 0;

    void *curr_frame = functions.thread_first_frame(thread1);
    for (int i = 1; curr_frame; ++i)
    {
        bool match = false;
        void *curr_frame2 = functions.thread_first_frame(thread2);
        for (int j = 1; !match && curr_frame2; ++j)
        {
            /* Whether the prefix continues to be the same for both
             * threads or not.
             */
            if (i == j && 0 != functions.frame_cmp(curr_frame, curr_frame2))
                still_prefix = false;

            /* Getting a match only if not too far away from each
             * other and if functions aren't both unpaired unknown
             * functions.
             */
            if (abs(i - j) <= max_frame_count / 2 - 1 &&
                0 == functions.frame_cmp(curr_frame, curr_frame2))
            {
                match = true;
                if (i != j)
                    ++trans_count;  // transposition in place
            }

            curr_frame2 = functions.frame_next(curr_frame2);
        }

        if (still_prefix)
            ++prefix_len;

        if (match)
            ++match_count;

        curr_frame = functions.frame_next(curr_frame);
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
distance_jaccard_frames_contain(struct type_specific_functions functions,
                                void *frames,
                                void *frame)
{
    while (frames)
    {
        // Checking if functions are the same but not both "??".
        if (!functions.frame_cmp(frames, frame))
            return true;

        frames = functions.frame_next(frames);
    }

    return false;
}

float
distance_jaccard(struct type_specific_functions functions,
                 void *thread1,
                 void *thread2)
{
    int intersection_size = 0, set1_size = 0, set2_size = 0;

    for (void *curr_frame = functions.thread_first_frame(thread1);
         curr_frame;
         curr_frame = functions.frame_next(curr_frame))
    {
        if (distance_jaccard_frames_contain(
                functions,
                functions.frame_next(curr_frame),
                curr_frame))
        {
            continue; // not last, skip
        }

        ++set1_size;

        if (distance_jaccard_frames_contain(
                functions,
                functions.thread_first_frame(thread2),
                curr_frame))
        {
            ++intersection_size;
        }
    }

    for (void *curr_frame = functions.thread_first_frame(thread2);
         curr_frame;
         curr_frame = functions.frame_next(curr_frame))
    {
        if (distance_jaccard_frames_contain(
                functions,
                functions.frame_next(curr_frame),
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
distance_levenshtein(struct type_specific_functions functions,
                     void *thread1,
                     void *thread2,
                     bool transposition)
{
    int m = sr_thread_frame_count((struct sr_thread*) thread1) + 1;
    int n = sr_thread_frame_count((struct sr_thread*) thread2) + 1;

    // store only two last rows and columns instead of whole 2D array
    int dist[m + n + 1], dist1[m + n + 1];

    // first row and column having distance equal to their position
    for (int i = m; i > 0; --i)
        dist[m - i] = i;

    for (int i = 0; i <= n; ++i)
        dist[m + i] = i;

    void *curr_frame2 = functions.thread_first_frame(thread2);
    void *prev_frame = NULL;
    void *prev_frame2 = NULL;

    for (int j = 1; curr_frame2; ++j)
    {
        void *curr_frame = functions.thread_first_frame(thread1);
        for (int i = 1; curr_frame; ++i)
        {
            int l = m + j - i;

            int dist2 = dist1[l];
            dist1[l] = dist[l];

            int cost;

            /*similar characters have distance equal to the previous
              one diagonally, "??" functions aren't taken as
              similar */
            if (0 == functions.frame_cmp(curr_frame, curr_frame2))
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
                 0 == functions.frame_cmp(curr_frame, prev_frame2) &&
                 0 == functions.frame_cmp(prev_frame, curr_frame2)))
            {
                dist[l] = dist2 + cost;
            }

            prev_frame = curr_frame;
            curr_frame = functions.frame_next(curr_frame);
        }

        prev_frame2 = curr_frame2;
        curr_frame2 = functions.frame_next(curr_frame2);
    }

    return dist[n];
}

static float
distance(enum sr_distance_type distance_type,
         struct type_specific_functions functions,
         void *thread1,
         void *thread2)
{
    switch (distance_type)
    {
    case SR_DISTANCE_JARO_WINKLER:
        return distance_jaro_winkler(functions, thread1, thread2);
    case SR_DISTANCE_JACCARD:
        return distance_jaccard(functions, thread1, thread2);
    case SR_DISTANCE_LEVENSHTEIN:
        return distance_levenshtein(functions, thread1, thread2, false);
    case SR_DISTANCE_DAMERAU_LEVENSHTEIN:
        return distance_levenshtein(functions, thread1, thread2, true);
    default:
        return 1.0f;
    }
}

static int
frame_cmp_gdb(void *frame1, void *frame2)
{
    return sr_gdb_frame_cmp_distance((struct sr_gdb_frame*)frame1,
                                     (struct sr_gdb_frame*)frame2);
}

static void *
frame_next_gdb(void *frame)
{
    return ((struct sr_gdb_frame*)frame)->next;
}

static void *
thread_first_frame_gdb(void *thread)
{
    return ((struct sr_gdb_thread*)thread)->frames;
}

struct type_specific_functions gdb_specific_functions = {
    frame_cmp_gdb,
    frame_next_gdb,
    thread_first_frame_gdb,
};

float
sr_distance_gdb(enum sr_distance_type distance_type,
                struct sr_gdb_thread *thread1,
                struct sr_gdb_thread *thread2)
{
    return distance(distance_type,
                    gdb_specific_functions,
                    thread1,
                    thread2);
}

static int
frame_cmp_core(void *frame1, void *frame2)
{
    return sr_core_frame_cmp_distance((struct sr_core_frame*)frame1,
                                      (struct sr_core_frame*)frame2);
}

static void *
frame_next_core(void *frame)
{
    return ((struct sr_core_frame*)frame)->next;
}

static void *
thread_first_frame_core(void *thread)
{
    return ((struct sr_core_thread*)thread)->frames;
}

struct type_specific_functions core_specific_functions = {
    frame_cmp_core,
    frame_next_core,
    thread_first_frame_core,
};

float
sr_distance_core(enum sr_distance_type distance_type,
                 struct sr_core_thread *thread1,
                 struct sr_core_thread *thread2)
{
    return distance(distance_type,
                    core_specific_functions,
                    thread1,
                    thread2);
}

static int
frame_cmp_java(void *frame1, void *frame2)
{
    return sr_java_frame_cmp_distance((struct sr_java_frame*)frame1,
                                      (struct sr_java_frame*)frame2);
}

static void *
frame_next_java(void *frame)
{
    return ((struct sr_java_frame*)frame)->next;
}

static void *
thread_first_frame_java(void *thread)
{
    return ((struct sr_java_thread*)thread)->frames;
}

struct type_specific_functions java_specific_functions = {
    frame_cmp_java,
    frame_next_java,
    thread_first_frame_java,
};

float
sr_distance_java(enum sr_distance_type distance_type,
                 struct sr_java_thread *thread1,
                 struct sr_java_thread *thread2)
{
    return distance(distance_type,
                    java_specific_functions,
                    thread1,
                    thread2);
}

static int
frame_cmp_koops(void *frame1, void *frame2)
{
    return sr_koops_frame_cmp_distance((struct sr_koops_frame*)frame1,
                                       (struct sr_koops_frame*)frame2);
}

static void *
frame_next_koops(void *frame)
{
    return ((struct sr_koops_frame*)frame)->next;
}

static void *
thread_first_frame_koops(void *stacktrace)
{
    return ((struct sr_koops_stacktrace*)stacktrace)->frames;
}

struct type_specific_functions koops_specific_functions = {
    frame_cmp_koops,
    frame_next_koops,
    thread_first_frame_koops,
};

float
sr_distance_koops(enum sr_distance_type distance_type,
                  struct sr_koops_stacktrace *stacktrace1,
                  struct sr_koops_stacktrace *stacktrace2)
{
    return distance(distance_type,
                    koops_specific_functions,
                    stacktrace1,
                    stacktrace2);
}


static int
frame_cmp_python(void *frame1, void *frame2)
{
    return sr_python_frame_cmp_distance((struct sr_python_frame*)frame1,
                                        (struct sr_python_frame*)frame2);
}

static void *
frame_next_python(void *frame)
{
    return ((struct sr_python_frame*)frame)->next;
}

static void *
thread_first_frame_python(void *stacktrace)
{
    return ((struct sr_python_stacktrace*)stacktrace)->frames;
}

struct type_specific_functions python_specific_functions = {
    frame_cmp_python,
    frame_next_python,
    thread_first_frame_python,
};

float
sr_distance_python(enum sr_distance_type distance_type,
                   struct sr_python_stacktrace *stacktrace1,
                   struct sr_python_stacktrace *stacktrace2)
{
    return distance(distance_type,
                    python_specific_functions,
                    stacktrace1,
                    stacktrace2);
}

