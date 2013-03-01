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
#ifndef SATYR_CLUSTER_H
#define SATYR_CLUSTER_H

/**
 * @file
 * @brief Clustering for stack trace threads.
 *
 * The implemented clustering algorithm assigns a set of stack trace
 * threads into groups.  Each group represents a single program flaw.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct sr_distances;

/**
 * @brief A dendrogram created by clustering.
 */
struct sr_dendrogram
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
 * sr_dendrogram_free().
 */
struct sr_dendrogram *
sr_dendrogram_new(int size);

/**
 * Releases the memory held by the dendrogram.
 * @param dendrogram
 * If dendrogram is NULL, no operation is performed.
 */
void
sr_dendrogram_free(struct sr_dendrogram *dendrogram);

/**
 * Performs hierarchical agglomerative clustering on objects.
 * @param distances
 * Distances between the objects. The structure is not modified by
 * calling this function.
 */
struct sr_dendrogram *
sr_distances_cluster_objects(struct sr_distances *distances);

/**
 * @brief A cluster of objects from a dendrogram.
 */
struct sr_cluster
{
    /* Number of objects in the cluster. */
    int size;
    /* Array of indices of the objects. */
    int *objects;
    /* A sibling cluster, or NULL if this is the last cluster. */
    struct sr_cluster *next;
};

/**
 * Creates and initializes a new cluster.
 * @param size
 * Number of objects in the cluster.
 * @returns
 * It never returns NULL. The returned pointer must be released by
 * sr_cluster_free().
 */
struct sr_cluster *
sr_cluster_new(int size);

/**
 * Releases the memory held by the cluster.
 * @param dendrogram
 * If cluster is NULL, no operation is performed.
 */
void
sr_cluster_free(struct sr_cluster *cluster);

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
struct sr_cluster *
sr_dendrogram_cut(struct sr_dendrogram *dendrogram,
                  float level,
                  int min_size);

#ifdef __cplusplus
}
#endif

#endif
