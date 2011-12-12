/*
    cluster.h

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
#ifndef BTPARSER_CLUSTER_H
#define BTPARSER_CLUSTER_H

#ifdef __cplusplus
extern "C" {
#endif

struct btp_distances;

/**
 * Represents a dendrogram created by clustering.
 */
struct btp_dendrogram
{
    /* How many objects were clustered. */
    int size;
    /* Indices of objects as they were in the top-level cluster. */
    int *order;  
    /**
     * Levels at which the clusters were merged. The clustering
     * can be reconstructed in order of increasing levels.
     * There are (size - 1) levels.
     */
    float *merge_levels;
};

/**
 * Creates and initializes a new dendrogram structure.
 * @param size
 * Number of objects.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * btp_dendrogram_free().
 */
struct btp_dendrogram *
btp_dendrogram_new(int size);

/**
 * Releases the memory held by the dendrogram.
 * @param dendrogram
 * If dendrogram is NULL, no operation is performed.
 */
void
btp_dendrogram_free(struct btp_dendrogram *dendrogram);

/**
 * Performs hierarchical agglomerative clustering on objects.
 * @param distances
 * Distances between the objects. The structure is not modified by
 * calling this function.
 */
struct btp_dendrogram *
btp_distances_cluster_objects(struct btp_distances *distances);

/**
 * Represents a cluster of objects.
 */
struct btp_cluster
{
    /* Number of objects in the cluster. */
    int size;
    /* Array of indices of the objects. */
    int *objects;
    /* A sibling cluster, or NULL if this is the last cluster. */
    struct btp_cluster *next;
};

/**
 * Creates and initializes a new cluster.
 * @param size
 * Number of objects in the cluster.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * btp_cluster_free().
 */
struct btp_cluster *
btp_cluster_new(int size);

/**
 * Releases the memory held by the cluster.
 * @param dendrogram
 * If cluster is NULL, no operation is performed.
 */
void
btp_cluster_free(struct btp_cluster *cluster);

/**
 * Cuts a dendrogram at specified level.
 * @param dendrogram
 * The dendrogram which should be cut. The structure is not modified
 * by this call.
 * @param level
 * The cutting level of distance.
 * @param min_size
 * The minimum size of clusters which should be returned.
 * @returns
 * List of clusters, NULL if empty.
 */
struct btp_cluster *
btp_dendrogram_cut(struct btp_dendrogram *dendrogram, float level, int min_size);

#ifdef __cplusplus
}
#endif

#endif
