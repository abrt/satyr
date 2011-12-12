/*
    cluster.c

    Copyright (C) 2011  Red Hat, Inc.

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

#include "cluster.h"
#include "metrics.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct btp_dendrogram *
btp_dendrogram_new(int size)
{
    struct btp_dendrogram *dendrogram = btp_malloc(sizeof(struct btp_dendrogram));

    assert(size > 1);

    dendrogram->size = size;
    dendrogram->order = btp_malloc(sizeof(*dendrogram->order) * size);
    dendrogram->merge_levels = btp_malloc(sizeof(*dendrogram->merge_levels) * (size - 1));

    return dendrogram;
}

void
btp_dendrogram_free(struct btp_dendrogram *dendrogram)
{
    if (!dendrogram)
        return;
    free(dendrogram->order);
    free(dendrogram->merge_levels);
    free(dendrogram);
}

struct cluster
{
    int size;
    int alloced;
    int *objects;
};

static void
cluster_init(struct cluster *cluster)
{
    cluster->size = cluster->alloced = 0;
    cluster->objects = NULL;
}

static void
cluster_clean(struct cluster *cluster)
{
    free(cluster->objects);
    cluster_init(cluster);
}

static void
cluster_add_index(struct cluster *cluster, int index)
{
    if (cluster->size >= cluster->alloced)
    {
        cluster->alloced = cluster->alloced >= 1 ? cluster->alloced * 2 : 1;
        cluster->objects = realloc(cluster->objects, sizeof (*cluster->objects)
                * cluster->alloced);
    }
    cluster->objects[cluster->size++] = index;
}

static void
cluster_merge(struct cluster *dest, struct cluster *src)
{
    int i;

    for (i = 0; i < src->size; i++)
        cluster_add_index(dest, src->objects[i]);
}

static void
cluster_reverse(struct cluster *cluster)
{
    int i, t, s;

    for (i = 0, s = cluster->size; i < s / 2; i++) {
        t = cluster->objects[i];
        cluster->objects[i] = cluster->objects[s - i - 1];
        cluster->objects[s - i - 1] = t;
    }
}

struct btp_dendrogram *
btp_distances_cluster_objects(struct btp_distances *distances)
{
    int i, j, merges, m = distances->m, n = distances->n;

    struct btp_distances *cluster_distances;
    struct cluster clusters[n];

    float merge_levels[n];

    cluster_distances = btp_distances_dup(distances);

    /* Start with one cluster per each object. */
    for (i = 0; i < n; i++)
    {
        cluster_init(&clusters[i]);
        cluster_add_index(&clusters[i], i);
    }

    /* Merge clusters n - 1 times so there will be only one cluster left. */
    for (merges = 0; merges + 1 < n; merges++)
    {
        bool first, reverse1, reverse2;
        int c1, c2;
        float dist, min_dist, dists[4];

        /* Quiet compiler. */
        c1 = c2 = 0;
        min_dist = 0.0;

        /* Find two clusters with minimal distance. */
        for (i = 0, first = true; i < m; i++)
        {
            if (!clusters[i].size)
                continue;

            for (j = i + 1; j < n; j++)
            {
                if (!clusters[j].size)
                    continue;

                dist = btp_distances_get_distance(cluster_distances, i, j);

                if (first || min_dist > dist)
                {
                    min_dist = dist;
                    c1 = i;
                    c2 = j;
                    first = false;
                }
            }
        }

        assert(!first && c1 < c2);

        /* Update distances of the new cluster to other clusters. */
        for (i = 0; i < n; i++)
        {
            if (!clusters[i].size || i == c1 || i == c2)
                continue;

            /* Skip if distance between clusters i and c2 is unknown. */
            if (!(c2 < m || i < m))
                continue;

            dists[0] = btp_distances_get_distance(cluster_distances, i, c1);
            dists[1] = btp_distances_get_distance(cluster_distances, i, c2);

            /* The cluster distance algorithm is hardcoded for now. */
            switch (3)
            {
                case 1:
                    /* maximum */
                    dist = dists[0] > dists[1] ? dists[0] : dists[1];
                    break;
                case 2:
                    /* minimum */
                    dist = dists[0] < dists[1] ? dists[0] : dists[1];
                    break;
                case 3:
                    /* average */
                    dist = (dists[0] * clusters[c1].size + dists[1] * clusters[c2].size) / (clusters[c1].size + clusters[c2].size);
                    break;
            }

            btp_distances_set_distance(cluster_distances, i, c1, dist);
        }

        /* With full distance matrix, merge the sequences of the two clusters
         * so outer objects with minimal distance will be next to each other. */
        if (m + 1 == n)
        {
            dists[0] = btp_distances_get_distance(distances,
                    clusters[c1].objects[0], clusters[c2].objects[0]);
            dists[1] = btp_distances_get_distance(distances,
                    clusters[c1].objects[clusters[c1].size - 1],
                    clusters[c2].objects[0]);
            dists[2] = btp_distances_get_distance(distances,
                    clusters[c1].objects[0],
                    clusters[c2].objects[clusters[c2].size - 1]);
            dists[3] = btp_distances_get_distance(distances,
                    clusters[c1].objects[clusters[c1].size - 1],
                    clusters[c2].objects[clusters[c2].size - 1]);
            if (dists[1] <= dists[0] && dists[1] <= dists[2] &&
                    dists[1] <= dists[3])
                reverse1 = false, reverse2 = false;
            else if (dists[0] <= dists[1] && dists[0] <= dists[2] &&
                    dists[0] <= dists[3])
                reverse1 = true, reverse2 = false;
            else if (dists[2] <= dists[0] && dists[2] <= dists[1] &&
                    dists[2] <= dists[3])
                reverse1 = true, reverse2 = true;
            else
                reverse1 = false, reverse2 = true;
        }
        else
            reverse1 = false, reverse2 = false;

        /* If the cluster sequences need to be reversed, shift the merge levels
         * as they will be pointing to the opposide direction. */
        if (reverse1)
        {
            for (i = 1; i < clusters[c1].size; i++)
                merge_levels[clusters[c1].objects[i - 1]] =
                    merge_levels[clusters[c1].objects[i]];
            cluster_reverse(&clusters[c1]);
        }
        if (reverse2)
        {
            for (i = 1; i < clusters[c2].size; i++)
                merge_levels[clusters[c2].objects[i - 1]] =
                    merge_levels[clusters[c2].objects[i]];
            cluster_reverse(&clusters[c2]);
        }

        /* Save the level at which the cluster is merged. */
        merge_levels[clusters[c2].objects[0]] = min_dist;

        /* Merge the two clusters. */
        cluster_merge(&clusters[c1], &clusters[c2]);
        cluster_clean(&clusters[c2]);
    }

    struct btp_dendrogram *dendrogram = btp_dendrogram_new(n);

    for (i = 0; i < n; i++)
        dendrogram->order[i] = clusters[0].objects[i];
    /* Save the merge levels in the same order as the objects. */
    for (i = 1; i < n; i++)
        dendrogram->merge_levels[i - 1] = merge_levels[clusters[0].objects[i]];

    cluster_clean(&clusters[0]);
    btp_distances_free(cluster_distances);

    return dendrogram;
}

struct btp_cluster *
btp_cluster_new(int size)
{
    struct btp_cluster *cluster = btp_malloc(sizeof(struct btp_cluster));

    cluster->size = size;
    cluster->objects = btp_malloc(sizeof(*cluster->objects) * size);
    cluster->next = NULL;

    return cluster;
}

void
btp_cluster_free(struct btp_cluster *cluster)
{
    if (!cluster)
        return;
    free(cluster->objects);
    free(cluster);
}

struct btp_cluster *
btp_dendrogram_cut(struct btp_dendrogram *dendrogram, float level, int min_size)
{
    int first;
    struct btp_cluster *cluster = NULL, *cluster_tmp;
    int i, j;

    for (i = first = 0; i < dendrogram->size; i++)
    {
        if (!(i + 1 < dendrogram->size && dendrogram->merge_levels[i] <= level))
        {
            /* End of the current cluster found, save it. */
            if (min_size <= i - first + 1)
            {
                cluster_tmp = btp_cluster_new(i - first + 1);
                for (j = first; j <= i; j++)
                    cluster_tmp->objects[j - first] = dendrogram->order[j];

                cluster_tmp->next = cluster;
                cluster = cluster_tmp;
            }

            first = i + 1;
        }
    }

    return cluster;
}
