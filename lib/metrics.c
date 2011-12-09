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

#include "thread.h"
#include "frame.h"
#include "utils.h"
#include "metrics.h"
#include <stdio.h>
#include <limits.h>

int
btp_frame_compare(struct btp_frame *frame1, struct btp_frame *frame2)
{
    if (btp_strcmp0(frame1->function_name, "??") == 0 &&
       btp_strcmp0(frame2->function_name, "??") == 0)
    {
        return -1;
    }
    return(btp_strcmp0(frame1->function_name, frame2->function_name));
}


float
btp_thread_jarowinkler_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        btp_frame_cmp_type compare_func)
{
    int frame1_count = btp_thread_get_frame_count(thread1);
    int frame2_count = btp_thread_get_frame_count(thread2);

    if (frame1_count == 0 && frame2_count == 0)
    {
        return 1.0;
    }

    int max_frame_count = (frame2_count > frame1_count ? frame2_count : frame1_count);
    int i = 0, j, prefix_len = 0;
    bool match, still_prefix = true;
    float k, trans_count = 0, match_count = 0, dist_jaro, dist;


    struct btp_frame *curr_frame = thread1->frames;
    for (i = 1; curr_frame; i++)
    {
        match = false;
        struct btp_frame *curr_frame2 = thread2->frames;
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

    if (match_count == 0)return 0;  // so as not to divide by 0

    dist_jaro = (match_count / (float)frame1_count +
                 match_count / (float)frame2_count +
                (match_count - trans_count) / match_count) / 3;

    k = 0.2;  /*how much weight we give to having common prefixes (always k<0.25)*/
    dist = dist_jaro + (float)prefix_len * k * (1 - dist_jaro);

    return dist;
}

static bool
frames_contain(struct btp_frame *frames, struct btp_frame *frame,
        btp_frame_cmp_type compare_func)
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
btp_thread_jaccard_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        btp_frame_cmp_type compare_func)
{
    int union_size, intersection_size = 0, set1_size = 0, set2_size = 0;
    float j_distance;
    struct btp_frame *curr_frame;

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
btp_thread_levenshtein_distance_custom(struct btp_thread *thread1, struct btp_thread *thread2,
        bool transposition, btp_frame_cmp_type compare_func)
{
    int frame1_count = btp_thread_get_frame_count(thread1);
    int frame2_count = btp_thread_get_frame_count(thread2);

    int dist[frame1_count+1][frame2_count+1];
    int i, j, k, l, lowest = INT_MAX, cost = 0;

    // first row and column having distance equal to their position
    for (i = 0; i <= frame1_count; i++)
    {
        dist[i][0] = i;
    }
    for (j = 0; j <= frame2_count; j++)
    {
        dist[0][j] = j;
    }
    struct btp_frame *curr_frame2 = thread2->frames;
    struct btp_frame *prev_frame = NULL;
    struct btp_frame *prev_frame2 = NULL;

    for (j = 1; curr_frame2; j++)
    {

        struct btp_frame *curr_frame = thread1->frames;
        for (i = 1; curr_frame; i++)
        {

            /*similar characters have distance equal to the previous one diagonally,
              "??" functions aren't taken as similar */
            if (0 == compare_func(curr_frame, curr_frame2))
            {
                cost = 0;
                dist[i][j] = dist[i-1][j-1];
            }
            // different ones takes the lowest value of all previous distances
            else
            {
                cost = 1;
                // checking left, top and topleft values in matrix
                for (k = -1; k <= 0;k++)
                {
                    for (l = -1; l <= 0;l++)
                    {
                        if (k == 0 && l == 0) continue;
                        if (lowest > dist[i+k][j+l]) lowest = dist[i+k][j+l];
                    }
                }
                dist[i][j] = lowest + 1;
                lowest = INT_MAX;
            }

            /*checking for transposition of two characters in both ways
              taking into account that "??" functions are not similar*/
            if (transposition &&
                (i == 2 && j == 2 &&
                 0 == compare_func(curr_frame, prev_frame2) &&
                 0 == compare_func(prev_frame, curr_frame2)) &&
                dist[i][j] > (dist[i-2][j-2] + cost))
                  dist[i][j] = (dist[i-2][j-2] + cost);

            prev_frame = curr_frame;
            curr_frame = curr_frame->next;
        }

        prev_frame2 = curr_frame2;
        curr_frame2 = curr_frame2->next;
    }

    return(dist[frame1_count][frame2_count]);
}

float
btp_thread_jarowinkler_distance(struct btp_thread *thread1, struct btp_thread *thread2)
{
    return btp_thread_jarowinkler_distance_custom(thread1, thread2, btp_frame_compare);
}

float
btp_thread_jaccard_distance(struct btp_thread *thread1, struct btp_thread *thread2)
{
    return btp_thread_jaccard_distance_custom(thread1, thread2, btp_frame_compare);
}

int
btp_thread_levenshtein_distance(struct btp_thread *thread1, struct btp_thread *thread2, bool transposition)
{
    return btp_thread_levenshtein_distance_custom(thread1, thread2, transposition, btp_frame_compare);
}
