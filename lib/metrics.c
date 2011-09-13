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
frame_compare(struct btp_frame *frame1, struct btp_frame *frame2)
{
    if (btp_strcmp0(frame1->function_name, "??") == 0 &&
       btp_strcmp0(frame2->function_name, "??") == 0)
    {
        return -1;
    }
    return(btp_strcmp0(frame1->function_name, frame2->function_name));
}


float
thread_jarowinkler_distance(struct btp_thread *thread1, struct btp_thread *thread2)
{
    int frame1_count = btp_thread_get_frame_count(thread1);
    int frame2_count = btp_thread_get_frame_count(thread2);

    if (0 == frame1_count || 0 == frame2_count)
    {
        return -1;
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
            if (i == j && 0 != frame_compare(curr_frame, curr_frame2))
                still_prefix = false;

            /*getting a match only if not too far away from each other
              and if functions aren't both unpaired unknown functions */

            if (abs(i - j) <= max_frame_count / 2 - 1 &&
              0 == frame_compare(curr_frame, curr_frame2))
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



float
thread_jaccard_index(struct btp_thread *thread1, struct btp_thread *thread2)
{
    int frame1_count = btp_thread_get_frame_count(thread1);
    int frame2_count = btp_thread_get_frame_count(thread2);

   //case when one of the threads is empty
    if (frame1_count == 0 || frame2_count == 0)
    {
        return(-1);
    }

    int i,j, union_size, intersection_size = 0;
    float j_index;

    struct btp_frame *curr_frame = thread1->frames;

    for (i = 1; curr_frame; i++)
    {
        struct btp_frame *curr_frame2 = thread2->frames;
        for (j = 1; curr_frame2; j++)
        {
            // checking if functions are the same but not both "??"

            if (0 == frame_compare(curr_frame, curr_frame2))
                intersection_size++;
            curr_frame2 = curr_frame2->next;
        }
        curr_frame = curr_frame->next;
    }

    union_size = frame1_count + frame2_count - intersection_size;
    j_index = intersection_size / (float)union_size;

    return j_index;
}


int
thread_levenshtein_distance(struct btp_thread *thread1, struct btp_thread *thread2, bool transposition)
{
    int frame1_count = btp_thread_get_frame_count(thread1);
    int frame2_count = btp_thread_get_frame_count(thread2);

   // case when one of the threads is empty
    if (frame1_count == 0 || frame2_count == 0)
    {
        return(-1);
    }


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
            if (0 == frame_compare(curr_frame, curr_frame2))
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
                 0 == frame_compare(curr_frame, prev_frame2) &&
                 0 == frame_compare(prev_frame, curr_frame2)) &&
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
