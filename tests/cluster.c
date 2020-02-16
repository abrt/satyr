#include <cluster.h>
#include <distance.h>

#include <glib.h>

static void
test_distances_cluster_objects_1(void)
{
    struct sr_distances *distances;
    struct sr_dendrogram *dendrogram;

    distances = sr_distances_new(3, 4);

    sr_distances_set_distance(distances, 0, 1, 1.0);
    sr_distances_set_distance(distances, 0, 2, 0.5);
    sr_distances_set_distance(distances, 0, 3, 0.0);
    sr_distances_set_distance(distances, 1, 2, 0.1);
    sr_distances_set_distance(distances, 1, 3, 0.3);
    sr_distances_set_distance(distances, 2, 3, 0.7);

    dendrogram = sr_distances_cluster_objects(distances);

    g_assert_cmpint(dendrogram->size, ==, 4);

    g_assert_cmpint(dendrogram->order[0], ==, 0);
    g_assert_cmpint(dendrogram->order[1], ==, 3);
    g_assert_cmpint(dendrogram->order[2], ==, 1);
    g_assert_cmpint(dendrogram->order[3], ==, 2);

    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[0], 0.0, 1e-6);
    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[1], 0.625, 1e-6);
    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[2], 0.1, 1e-6);
}

static void
test_distances_cluster_objects_2(void)
{
    struct sr_distances *distances;
    struct sr_dendrogram *dendrogram;

    distances = sr_distances_new(1, 6);

    sr_distances_set_distance(distances, 0, 1, 1.0);
    sr_distances_set_distance(distances, 0, 2, 0.5);
    sr_distances_set_distance(distances, 0, 3, 0.3);
    sr_distances_set_distance(distances, 0, 4, 0.0);
    sr_distances_set_distance(distances, 0, 5, 0.9);

    dendrogram = sr_distances_cluster_objects(distances);

    g_assert_cmpint(dendrogram->size, ==, 6);

    g_assert_cmpint(dendrogram->order[0], ==, 0);
    g_assert_cmpint(dendrogram->order[1], ==, 4);
    g_assert_cmpint(dendrogram->order[2], ==, 3);
    g_assert_cmpint(dendrogram->order[3], ==, 2);
    g_assert_cmpint(dendrogram->order[4], ==, 5);
    g_assert_cmpint(dendrogram->order[5], ==, 1);

    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[0], 0.0, 1e-6);
    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[1], 0.3, 1e-6);
    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[2], 0.5, 1e-6);
    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[3], 0.9, 1e-6);
    g_assert_cmpfloat_with_epsilon(dendrogram->merge_levels[4], 1.0, 1e-6);
}

static void
test_dendrogram_cut_1(void)
{
    struct sr_dendrogram *dendrogram;
    struct sr_cluster *cluster;

    dendrogram = sr_dendrogram_new(6);

    dendrogram->order[0] = 0;
    dendrogram->order[1] = 3;
    dendrogram->order[2] = 1;
    dendrogram->order[3] = 2;
    dendrogram->order[4] = 4;
    dendrogram->order[5] = 5;

    dendrogram->merge_levels[0] = 0.0;
    dendrogram->merge_levels[1] = 0.6;
    dendrogram->merge_levels[2] = 0.1;
    dendrogram->merge_levels[3] = 0.5;
    dendrogram->merge_levels[4] = 0.3;

    cluster = sr_dendrogram_cut(dendrogram, 0.2, 1);

    g_assert_nonnull(cluster);

    g_assert_cmpint(cluster->size, ==, 1);
    g_assert_cmpint(cluster->objects[0], ==, 5);

    cluster = cluster->next;

    g_assert_nonnull(cluster);

    g_assert_cmpint(cluster->size, ==, 1);
    g_assert_cmpint(cluster->objects[0], ==, 4);

    cluster = cluster->next;

    g_assert_nonnull(cluster);

    g_assert_cmpint(cluster->size, ==, 2);
    g_assert_cmpint(cluster->objects[0], ==, 1);
    g_assert_cmpint(cluster->objects[1], ==, 2);

    cluster = cluster->next;

    g_assert_nonnull(cluster);

    g_assert_cmpint(cluster->size, ==, 2);
    g_assert_cmpint(cluster->objects[0], ==, 0);
    g_assert_cmpint(cluster->objects[1], ==, 3);

    g_assert_null(cluster->next);

    sr_cluster_free(cluster);
    sr_dendrogram_free(dendrogram);
}

static void
test_dendrogram_cut_2(void)
{
    struct sr_dendrogram *dendrogram;
    struct sr_cluster *cluster;

    dendrogram = sr_dendrogram_new(6);

    dendrogram->order[0] = 0;
    dendrogram->order[1] = 3;
    dendrogram->order[2] = 1;
    dendrogram->order[3] = 2;
    dendrogram->order[4] = 4;
    dendrogram->order[5] = 5;

    dendrogram->merge_levels[0] = 0.0;
    dendrogram->merge_levels[1] = 0.6;
    dendrogram->merge_levels[2] = 0.1;
    dendrogram->merge_levels[3] = 0.5;
    dendrogram->merge_levels[4] = 0.3;

    cluster = sr_dendrogram_cut(dendrogram, 0.5, 3);

    g_assert_nonnull(cluster);

    g_assert_cmpint(cluster->size, ==, 4);
    g_assert_cmpint(cluster->objects[0], ==, 1);
    g_assert_cmpint(cluster->objects[1], ==, 2);
    g_assert_cmpint(cluster->objects[2], ==, 4);
    g_assert_cmpint(cluster->objects[3], ==, 5);

    g_assert_null(cluster->next);

    sr_cluster_free(cluster);
    sr_dendrogram_free(dendrogram);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/cluster/objects-distances-1", test_distances_cluster_objects_1);
    g_test_add_func("/cluster/objects-distances-2", test_distances_cluster_objects_2);
    g_test_add_func("/dendrogram/cut-1", test_dendrogram_cut_2);
    g_test_add_func("/dendrogram/cut-2", test_dendrogram_cut_2);

    return g_test_run();
}
