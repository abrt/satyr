/*
    btparser.c - backtrace parsing tool

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
#include "lib/backtrace.h"
#include "lib/thread.h"
#include "lib/frame.h"
#include "lib/utils.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/metrics.h"
#include "lib/cluster.h"
#include "lib/normalize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sysexits.h>
#include <assert.h>

const char *argp_program_version = "btparser " VERSION;
const char *argp_program_bug_address = "<kklic@redhat.com>";

static char doc[] = "btparser -- backtrace parser";

/* A description of the arguments we accept. */
static char args_doc[] = "[FILE...]";

static struct argp_option options[] = {
    {"stdin"           , 'i', 0, 0, "Use standard input rather than input file"},
    {"rate"            , 'r', 0, 0, "Prints the backtrace rating from 0 to 1"},
    {"crash-function"  , 'c', 0, 0, "Prints crash function"},
    {"duplication-hash", 'h', 0, 0, "Prints normalized string useful for hash calculation"},
    {"debug"           , 'd', 0, 0, "Prints parser debug information"},
    {"verbose"         , 'v', 0, 0, "Prints human-friendly superfluous output."},
    {"comparison-optimized", 'o', 0, 0, "Prints or parses backtrace optimized for comparison.", 1},
    {"max-frames"      , 'm', "FRAMES", 0, "Sets maximum number of frames in optimized backtrace (default 8).", 2},
    {"distances"       , 's', 0, 0, "Prints distance matrix.", 3},
    {"dendrogram"      , 'g', 0, 0, "Prints ASCII dendrogram.", 4},
    {"clusters"        , 'l', "LEVEL", 0, "Prints clusters cut at LEVEL.", 5},
    {"min-cluster-size", 'z', "SIZE", 0, "Sets minimum size of printed clusters (default 2).", 6},
    { 0 }
};

enum what_to_output
{
    BACKTRACE,
    RATE,
    CRASH_FUNCTION,
    DUPLICATION_HASH,
    DISTANCES,
    CLUSTERS,
    DENDROGRAM
};

struct arguments
{
    enum what_to_output output;
    bool debug;
    bool verbose;
    bool stdin;
    bool optimized;
    char **filenames;
    int num_filenames;
    int max_frames;
    int min_cluster_size;
    float cut_level;
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments *arguments = (struct arguments*)state->input;

    switch (key)
    {
    case 'r': arguments->output = RATE; break;
    case 'c': arguments->output = CRASH_FUNCTION; break;
    case 'h': arguments->output = DUPLICATION_HASH; break;
    case 'd': arguments->debug = true; break;
    case 'v': arguments->verbose = true; break;
    case 'i': arguments->stdin = true; break;
    case 'o': arguments->optimized = true; break;
    case 'm': arguments->max_frames = atoi(arg); break;
    case 's': arguments->output = DISTANCES; break;
    case 'g': arguments->output = DENDROGRAM; break;
    case 'l': arguments->output = CLUSTERS; arguments->cut_level = atof(arg); break;
    case 'z': arguments->min_cluster_size = atoi(arg); break;
    case ARGP_KEY_ARGS:
        arguments->filenames = state->argv + state->next;
        arguments->num_filenames = state->argc - state->next;
        break;
    case ARGP_KEY_END:
        switch (arguments->output)
        {
        case DISTANCES:
        case CLUSTERS:
        case DENDROGRAM:
            if (arguments->num_filenames < 2 || arguments->stdin) {
                /* At least two filenames required */
                argp_usage(state);
                exit(EX_USAGE); /* Invalid argument */
            }
            break;
        default:
            if ((arguments->num_filenames != 1 && !arguments->stdin) ||
                    (arguments->num_filenames != 0 && arguments->stdin)) {
                /* Argument required */
                argp_usage(state);
                exit(EX_USAGE); /* Invalid argument */
            }
            break;
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/** Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

void print_dendrogram(struct btp_dendrogram *dendrogram, char **names)
{
    int i, j, l, width = 40;
    float min, max;

    for (i = 1, min = max = dendrogram->merge_levels[0];
            i + 1 < dendrogram->size; i++)
        if (min > dendrogram->merge_levels[i])
            min = dendrogram->merge_levels[i];
        else if (max < dendrogram->merge_levels[i])
            max = dendrogram->merge_levels[i];

    if (min == max)
        max = min + 1.0;

    for (i = 0; i < dendrogram->size; i++)
    {
        putchar('|');
        if (i + 1 < dendrogram->size)
        {
            l = (max - dendrogram->merge_levels[i]) / (max - min) * width;
            for (j = 0; j < l; j++)
                putchar('_');
            for (; j < width; j++)
                putchar(' ');
            printf("| %6.3f | %s\n", dendrogram->merge_levels[i], names[dendrogram->order[i]]);
        }
        else
        {
            for (j = 0; j < width; j++)
                putchar(' ');
            printf("|        | %s\n", names[dendrogram->order[i]]);
        }
    }
}

char *read_file(char *filename)
{
    char *text = NULL;
    if (!filename)
    {
        struct btp_strbuf *input = btp_strbuf_new();
        int c;
        while ((c = getchar()) != EOF && c != '\0')
            btp_strbuf_append_char(input, (char)c);

        btp_strbuf_append_char(input, '\0');
        text = btp_strbuf_free_nobuf(input);
    }
    else
    {
        /* Load the input file to a string. */
        text = btp_file_to_string(filename);
        if (!text)
        {
            fprintf(stderr, "Failed to read the input file.\n");
            exit(1);
        }
    }

    return text;
}

struct btp_backtrace *parse_backtrace(char *filename)
{
    char *text = read_file(filename);
    /* Parse the input string. */
    const char *ptr = text;
    struct btp_location location;
    btp_location_init(&location);
    struct btp_backtrace *backtrace = btp_backtrace_parse(&ptr, &location);
    if (!backtrace)
    {
        char *location_str = btp_location_to_string(&location);
        fprintf(stderr, "Failed to parse the backtrace.\n  %s\n",
                location_str);
        free(location_str);
        exit(1);
    }
    free(text);

    return backtrace;
}

struct btp_thread *get_optimized_thread(struct btp_backtrace *backtrace, int max_frames)
{
    struct btp_thread *thread = btp_backtrace_get_optimized_thread(backtrace, max_frames);

    if (!thread)
    {
        fprintf(stderr, "Failed to find the crash thread.\n");
        exit(1);
    }

    return thread;
}

int main(int argc, char **argv)
{
    /* Set options default values and parse program command line. */
    struct arguments arguments;
    arguments.output = BACKTRACE;
    arguments.debug = false;
    arguments.verbose = false;
    arguments.stdin = false;
    arguments.optimized = false;
    arguments.filenames = NULL;
    arguments.num_filenames = 0;
    arguments.max_frames = 8;
    arguments.min_cluster_size = 2;
    arguments.cut_level = 0.0;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (arguments.debug)
        btp_debug_parser = true;

    switch (arguments.output)
    {
    case BACKTRACE:
    case RATE:
    case CRASH_FUNCTION:
    case DUPLICATION_HASH:
    {
        struct btp_backtrace *backtrace;

        if (arguments.stdin)
            backtrace = parse_backtrace(NULL);
        else if (arguments.num_filenames == 1)
            backtrace = parse_backtrace(arguments.filenames[0]);
        else
            assert(0);

        switch (arguments.output)
        {
        case BACKTRACE:
        {
            if (!arguments.optimized)
            {
                char *text_parsed = btp_backtrace_to_text(backtrace, false);
                puts(text_parsed);
                free(text_parsed);
            }
            else
            {
                struct btp_thread *crash_thread = get_optimized_thread(backtrace, arguments.max_frames);
                char *funs = btp_thread_format_funs(crash_thread);
                printf("%s", funs);
                free(funs);
                btp_thread_free(crash_thread);
            }
            break;
        }
        case RATE:
        {
            float q = btp_backtrace_quality_complex(backtrace);
            printf("%.2f", q);
            break;
        }
        case CRASH_FUNCTION:
        {
            struct btp_frame *frame = btp_backtrace_get_crash_frame(backtrace);
            if (!frame)
            {
                fprintf(stderr, "Failed to find the crash function.\n");
                exit(1);
            }
            if (frame->function_name)
                puts(frame->function_name);
            else
            {
                assert(frame->signal_handler_called);
                puts("signal handler");
            }
            btp_frame_free(frame);
            break;
        }
        case DUPLICATION_HASH:
        {
            char *hash = btp_backtrace_get_duplication_hash(backtrace);
            puts(hash);
            free(hash);
            break;
        }
        default:
            assert(0);
        }

        btp_backtrace_free(backtrace);

        break;
    }
    case DISTANCES:
    case CLUSTERS:
    case DENDROGRAM:
    {
        assert(arguments.num_filenames >= 2);

        int i, j, n = arguments.num_filenames;
        struct btp_thread *threads[n];

        for (i = 0; i < n; i++)
        {
            if (arguments.optimized)
            {
                char *text = read_file(arguments.filenames[i]);
                threads[i] = btp_thread_parse_funs(text);
                free(text);
            }
            else
            {
                struct btp_backtrace *backtrace = parse_backtrace(arguments.filenames[i]);
                threads[i] = get_optimized_thread(backtrace, arguments.max_frames);
                btp_backtrace_free(backtrace);
            }
        }

        struct btp_distances *distances = btp_threads_compare(threads, n - 1, n, btp_thread_levenshtein_distance_f);

        for (i = 0; i < n; i++)
            btp_thread_free(threads[i]);

        if (arguments.output == DISTANCES)
        {
            for (i = 0; i < n; i++)
            {
                for (j = 0; j < n; j++)
                    printf("%.3f\t", btp_distances_get_distance(distances, i, j));
                printf("\n");
            }
            btp_distances_free(distances);
            break;
        }

        struct btp_dendrogram *dendrogram = btp_distances_cluster_objects(distances);

        btp_distances_free(distances);

        if (arguments.output == DENDROGRAM)
        {
            print_dendrogram(dendrogram, arguments.filenames);
            btp_dendrogram_free(dendrogram);
            break;
        }

        struct btp_cluster *cluster, *clusters = btp_dendrogram_cut(dendrogram,
                arguments.cut_level, arguments.min_cluster_size);

        btp_dendrogram_free(dendrogram);

        if (arguments.output == CLUSTERS)
        {
            while (clusters)
            {
                for (i = 0; i < clusters->size; i++)
                    printf("%s\t", arguments.filenames[clusters->objects[i]]);
                cluster = clusters;
                clusters = clusters->next;
                btp_cluster_free(cluster);
                printf("\n");
            }
            break;
        }

        assert(0);
        break;
    }
    default:
        fprintf(stderr, "Unexpected operation.");
        exit(1);
    }

    return 0;
}
