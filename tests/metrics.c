#include <distance.h>
#include <gdb/frame.h>
#include <gdb/thread.h>
#include <glib.h>
#include <math.h>
#include <normalize.h>
#include <stdbool.h>
#include <utils.h>

typedef struct
{
    bool normalize;

    float expected_jaro_winkler_distance;
    float expected_jaccard_distance;
    float expected_levenshtein_distance;
    float expected_damerau_levenshtein_distance;

    struct sr_gdb_thread *threads[2];
} DistanceTestData;

static struct sr_gdb_thread *
create_threadv(size_t  frame_count,
               char **function_names)
{
    struct sr_gdb_thread *thread;

    thread = sr_gdb_thread_new();

    for (size_t i = 0; i < frame_count; i++)
    {
        struct sr_gdb_frame *frame;

        frame = sr_gdb_frame_new();

        frame->function_name = g_strdup(function_names[i]);

        if (NULL == thread->frames)
        {
            thread->frames = frame;
        }
        else
        {
            sr_gdb_frame_append(thread->frames, frame);
        }
    }

    return thread;
}

static struct sr_gdb_thread *
create_thread(size_t frame_count,
              ...)
{
    struct sr_gdb_thread *thread;
    char **function_names = g_malloc_n(frame_count + 1, sizeof (*function_names));
    va_list argp;

    va_start(argp, frame_count);
    for (size_t i = 0; i < frame_count; i++)
    {
        function_names[i] = g_strdup(va_arg(argp, char *));
    }
    va_end(argp);
    function_names[frame_count] = NULL;
    thread = create_threadv(frame_count, function_names);
    g_strfreev(function_names);

    return thread;
}

static DistanceTestData *
distance_test_data_new(bool        normalize,
                       double      expected_jaro_winkler_distance,
                       double      expected_jaccard_distance,
                       double      expected_levenshtein_distance,
                       double      expected_damerau_levenshtein_distance,
                       size_t      frame_counts[2],
                       ...)
{
    DistanceTestData *test_data;
    va_list argp;

    test_data = g_new0(DistanceTestData, 1);

    test_data->normalize = normalize;
    test_data->expected_jaro_winkler_distance = expected_jaro_winkler_distance;
    test_data->expected_jaccard_distance = expected_jaccard_distance;
    test_data->expected_levenshtein_distance = expected_levenshtein_distance;
    test_data->expected_damerau_levenshtein_distance = expected_damerau_levenshtein_distance;

    va_start(argp, frame_counts);

    for (size_t i = 0; i < G_N_ELEMENTS(test_data->threads); i++)
    {
        char **function_names = g_malloc_n(frame_counts[i] + 1, sizeof (*function_names));
        for (size_t j = 0; j < frame_counts[i]; j++)
        {
            function_names[j] = g_strdup(va_arg(argp, char *));
        }
        function_names[frame_counts[i]] = NULL;
        test_data->threads[i] = create_threadv(frame_counts[i], function_names);
        g_strfreev(function_names);
    }

    va_end(argp);

    return test_data;
}

static void
distance_test_data_free(DistanceTestData *data)
{
    for (size_t i = 0; i < G_N_ELEMENTS(data->threads); i++)
    {
        sr_gdb_thread_free(data->threads[i]);
    }

    free(data);
}

static void
test(const void *user_data)
{
    const DistanceTestData *test_data;
    float distance;

    test_data = (const DistanceTestData *)user_data;

    if (test_data->normalize)
    {
        sr_normalize_gdb_paired_unknown_function_names(test_data->threads[0],
                                                       test_data->threads[1]);
    }

    if (isfinite(test_data->expected_jaro_winkler_distance))
    {
        distance = sr_distance(SR_DISTANCE_JARO_WINKLER,
                               (struct sr_thread *)test_data->threads[0],
                               (struct sr_thread *)test_data->threads[1]);
        g_assert_cmpfloat_with_epsilon(distance,
                                       test_data->expected_jaro_winkler_distance,
                                       FLT_EPSILON);
    }

    if (isfinite(test_data->expected_jaccard_distance))
    {
        distance = sr_distance(SR_DISTANCE_JACCARD,
                               (struct sr_thread *)test_data->threads[0],
                               (struct sr_thread *)test_data->threads[1]);
        g_assert_cmpfloat_with_epsilon(distance,
                                       test_data->expected_jaccard_distance,
                                       FLT_EPSILON);
    }

    if (isfinite(test_data->expected_levenshtein_distance))
    {
        distance = sr_distance(SR_DISTANCE_LEVENSHTEIN,
                               (struct sr_thread *)test_data->threads[0],
                               (struct sr_thread *)test_data->threads[1]);
        g_assert_cmpfloat_with_epsilon(distance,
                                       test_data->expected_levenshtein_distance,
                                       FLT_EPSILON);
    }

    if (isfinite(test_data->expected_damerau_levenshtein_distance))
    {
        distance = sr_distance(SR_DISTANCE_DAMERAU_LEVENSHTEIN,
                               (struct sr_thread *)test_data->threads[0],
                               (struct sr_thread *)test_data->threads[1]);
        g_assert_cmpfloat_with_epsilon(distance,
                                       test_data->expected_damerau_levenshtein_distance,
                                       FLT_EPSILON);
    }
}

static void
test_distances_basic_properties(void)
{
    const int m = 50;
    const int n = 100;
    struct sr_distances *distances;

    distances = sr_distances_new(m, n);

    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            sr_distances_set_distance(distances, i, j, i + j / 1000.0);
        }
    }

    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float distance;

            distance = sr_distances_get_distance(distances, i, j);

            if (i == j)
            {
                g_assert_cmpfloat(distance, ==, 0.0);
            }
            else if (j < m && i < j)
            {
                g_assert_cmpfloat_with_epsilon(distance,
                                               (float)(j + (i / 1000.0)),
                                               FLT_EPSILON);
            }
            else
            {
                g_assert_cmpfloat_with_epsilon(distance,
                                               (float)(i + (j / 1000.0)),
                                               FLT_EPSILON);
            }
        }
    }
    sr_distances_free(distances);
}

static void
prepare_threads(struct sr_gdb_thread *threads[static 8])
{
    threads[0] = create_thread(4, "asd", "agd", "das", "??");
    threads[1] = create_thread(3, "asd", "agd", "??");
    threads[2] = create_thread(3, "asd", "agd", "??");
    threads[3] = create_thread(3, "dg", "??", "??");
    threads[4] = create_thread(2, "foo", "??");
    threads[5] = create_thread(5, "bbb", "??", "foo", "bar", "baz");
    threads[6] = create_thread(1, "dg");
    threads[7] = create_thread(3, "bar", "das", "baz");
}

static void
test_distances_threads_compare(void)
{
    struct sr_gdb_thread *threads[8];
    struct sr_distances *distances;
    float distance;

    prepare_threads(threads);

    distances = sr_threads_compare((struct sr_thread **)threads, 3, 4, SR_DISTANCE_JACCARD);

    distance = sr_distances_get_distance(distances, 0, 1);
    g_assert_cmpfloat_with_epsilon(distance, 0.6, FLT_EPSILON);

    distance = sr_distances_get_distance(distances, 0, 2);
    g_assert_cmpfloat_with_epsilon(distance, 0.6, FLT_EPSILON);

    distance = sr_distances_get_distance(distances, 0, 3);
    g_assert_cmpfloat_with_epsilon(distance, 1.0, FLT_EPSILON);

    distance = sr_distances_get_distance(distances, 1, 2);
    g_assert_cmpfloat_with_epsilon(distance, 0.0, FLT_EPSILON);

    distance = sr_distances_get_distance(distances, 1, 3);
    g_assert_cmpfloat_with_epsilon(distance, 1.0, FLT_EPSILON);

    distance = sr_distances_get_distance(distances, 2, 3);
    g_assert_cmpfloat_with_epsilon(distance, 1.0, FLT_EPSILON);

    sr_distances_free(distances);

    for (size_t i = 0; i < G_N_ELEMENTS(threads); i++)
    {
        sr_gdb_thread_free(threads[i]);
    }
}

static void
test_distances_part_divide(void)
{
    int test_data[][3] =
    {
        { 3, 8, 1, },
        { 3, 8, 2, },
        { 3, 8, 3, },
        { 3, 8, 4, },
        { 3, 8, 5, },
        { 3, 8, 6, },
        { 3, 8, 7, },
        { 3, 8, 8, },
        { 3, 8, 9, },
        { 3, 8, 10, },
        { 3, 8, 11, },
        { 3, 8, 12, },
        { 3, 8, 13, },
        { 3, 8, 18, },
        { 3, 8, 19, },
        { 3, 8, 5000, },
        { 50, 50, 5, },
        { 50, 50, 10, },
        { 50, 50, 20, },
        { 50, 50, 50, },
        { 1, 50, 50, },
        { 1, 50, 3000, },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        struct sr_distances_part *parts;
        struct sr_distances_part *cursor;
        size_t nelems;
        size_t expected_parts;

        parts = sr_distances_part_create(test_data[i][0], test_data[i][1],
                                         SR_DISTANCE_LEVENSHTEIN,
                                         test_data[i][2]);
        cursor = parts;
        nelems = (test_data[i][0] * (test_data[i][0] - 1)) / 2;
        nelems += test_data[i][0] * (test_data[i][1] - test_data[i][0]);
        expected_parts = MIN(test_data[i][2], nelems);

        for (size_t j = 0; j < expected_parts; j++, cursor = cursor->next)
        {
            g_assert_nonnull(cursor);
        }

        g_assert_null(cursor);

        sr_distances_part_free(parts, true);
    }
}

static void
test_distances_part_conquer(void)
{
    int test_data[][3] =
    {
        { 3, 4, 1, },
        { 3, 4, 2, },
        { 3, 4, 3, },
        { 3, 4, 4, },

        { 2, 5, 1, },
        { 2, 5, 2, },
        { 2, 5, 3, },
        { 2, 5, 4, },

        { 7, 8, 1, },
        { 7, 8, 2, },
        { 7, 8, 4, },
        { 7, 8, 8, },
        { 7, 8, 16, },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        int m;
        int n;
        int nparts;
        struct sr_gdb_thread *threads[8];
        struct sr_distances *reference;
        struct sr_distances_part *parts;
        struct sr_distances *distances;

        prepare_threads(threads);

        m = test_data[i][0];
        n = test_data[i][1];
        nparts = test_data[i][2];
        reference = sr_threads_compare((struct sr_thread **)threads, m, n, SR_DISTANCE_JACCARD);
        parts = sr_distances_part_create(m, n, SR_DISTANCE_JACCARD, nparts);

        for (struct sr_distances_part *it = parts; it != NULL; it = it->next)
        {
            sr_distances_part_compute(it, (struct sr_thread **)threads);
        }

        distances = sr_distances_part_merge(parts);

        sr_distances_part_free(parts, true);

        g_assert_nonnull(distances);

        for (int j = 0; j < m; j++)
        {
            for (int k = j + 1; k < n; k++)
            {
                float lhs;
                float rhs;

                lhs = sr_distances_get_distance(distances, j, k);
                rhs = sr_distances_get_distance(reference, j, k);

                g_assert_cmpfloat_with_epsilon(lhs, rhs, FLT_EPSILON);
            }
        }
        sr_distances_free(reference);
        sr_distances_free(distances);

        for (size_t i = 0; i < G_N_ELEMENTS(threads); i++)
        {
            sr_gdb_thread_free(threads[i]);
        }
    }
}

int
main(int    argc,
     char **argv)
{
    int exit_code;
    DistanceTestData *test_data[] =
    {
        distance_test_data_new(true,
                               0.0, 1.0, 1.0, INFINITY,
                               (size_t [2]){ 0, 3 },
                               "??", "ds", "main"),
        distance_test_data_new(true,
                               1.0, 0.0, 0.0, INFINITY,
                               (size_t [2]){ 0, 0 }),

        distance_test_data_new(true,
                               0.0, 1.0, 1.0, INFINITY,
                               (size_t [2]){ 2, 3 },
                               "??", "sd",
                               "??", "ds", "main"),
        distance_test_data_new(true,
                               0.0, 1.0, 1.0, INFINITY,
                               (size_t [2]){ 1, 3 },
                               "??",
                               "??", "ds", "main"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 0.0, INFINITY,
                               (size_t [2]){ 3, 3 },
                               "??", "ds", "main",
                               "??", "ds", "main"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 0.0, INFINITY,
                               (size_t [2]){ 4, 4 },
                               "dg", "??", "ds", "main",
                               "dg", "??", "ds", "main"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 0.25, INFINITY,
                               (size_t [2]){ 3, 4 },
                               "dg", "??", "ds",
                               "dg", "??", "ds", "main"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 2.0 / 3.0, INFINITY,
                               (size_t [2]){ 2, 3 },
                               "dg", "??",
                               "dg", "??", "main"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 0.75, INFINITY,
                               (size_t [2]){ 3, 4 },
                               "dg", "??", "??",
                               "dg", "??", "??", "main"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 1.0, INFINITY,
                               (size_t [2]){ 3, 2 },
                               "??", "??", "main",
                               "??", "??"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 0.4, INFINITY,
                               (size_t [2]){ 5, 5 },
                               "ale", "asd", "??", "agd", "dsa",
                               "ale", "huga", "asd", "??", "agd"),
        distance_test_data_new(true,
                               INFINITY, INFINITY, 0.5, INFINITY,
                               (size_t [2]){ 4, 4 },
                               "asd", "??", "agd", "dsa",
                               "asd", "agd", "??", "dsa"),
        distance_test_data_new(true,
                               INFINITY, 0.0, INFINITY, INFINITY,
                               (size_t [2]){ 4, 3 },
                               "asd", "agd", "agd", "??",
                               "asd", "agd", "??"),

        distance_test_data_new(false,
                               INFINITY, INFINITY, 0.75, 0.5,
                               (size_t [2]){ 4, 4 },
                               "a", "b", "c", "d",
                               "b", "a", "d", "c"),
        distance_test_data_new(false,
                               INFINITY, 2.0 / 3.0, INFINITY, INFINITY,
                               (size_t [2]){ 3, 2 },
                               "asd", "agd", "agd",
                               "sad", "agd"),
        distance_test_data_new(false,
                               INFINITY, 0.6, INFINITY, INFINITY,
                               (size_t [2]){ 5, 4 },
                               "asd", "agd", "gad", "sad", "abc",
                               "sad", "sad", "sad", "gad"),
    };

    g_test_init(&argc, &argv, NULL);

    g_test_add_data_func("/distances/empty-threads/1", test_data[0], test);
    g_test_add_data_func("/distances/empty-threads/2", test_data[1], test);

    g_test_add_data_func("/distances/unknown-frames/1", test_data[2], test);
    g_test_add_data_func("/distances/unknown-frames/2", test_data[3], test);
    g_test_add_data_func("/distances/unknown-frames/3", test_data[4], test);
    g_test_add_data_func("/distances/unknown-frames/4", test_data[5], test);
    g_test_add_data_func("/distances/unknown-frames/5", test_data[6], test);
    g_test_add_data_func("/distances/unknown-frames/6", test_data[7], test);
    g_test_add_data_func("/distances/unknown-frames/7", test_data[8], test);
    g_test_add_data_func("/distances/unknown-frames/8", test_data[9], test);
    g_test_add_data_func("/distances/unknown-frames/9", test_data[10], test);
    g_test_add_data_func("/distances/unknown-frames/10", test_data[11], test);
    g_test_add_data_func("/distances/unknown-frames/11", test_data[12], test);

    g_test_add_data_func("/distances/random/1", test_data[13], test);
    g_test_add_data_func("/distances/random/2", test_data[14], test);
    g_test_add_data_func("/distances/random/3", test_data[15], test);


    g_test_add_func("/distances/basic-properties", test_distances_basic_properties);
    g_test_add_func("/distances/threads-compare", test_distances_threads_compare);

    g_test_add_func("/distances/part/divide", test_distances_part_divide);
    g_test_add_func("/distances/part/conquer", test_distances_part_conquer);

    exit_code = g_test_run();

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        distance_test_data_free(test_data[i]);
    }

    return exit_code;
}
