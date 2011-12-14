#include "lib/backtrace.h"
#include "lib/thread.h"
#include "lib/frame.h"
#include "lib/utils.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/normalize.h"
#include "lib/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sysexits.h>
#include <assert.h>

int main(int argc, char **argv)
{
    bool optimized = false;
    int i;
    char *filename1 = NULL;
    char *filename2 = NULL;
    long file_size;

   /*higher number here means higher accuracy of the computed values but slower
     computing*/
    int accuracy = 8;

  // getting backtrace files names as arguments
    for (i = 1; i < argc; i++)
    {
        if (0 == strcmp("--optimized", argv[i])) optimized = true;
        else if (!filename1) filename1 = argv[i];
        else if (!filename2) filename2 = argv[i];
        else
        {
            fprintf(stderr, "Too many arguments\n");
            exit(1);
        }
    }
    if (!filename1 || !filename2)
    {
        fprintf(stderr, "You must provide two files");
        exit(1);
    }

  // opening files
    FILE *file1 = fopen(filename1, "r");
    if (!file1)
    {
        fprintf(stderr, "Unable to open file %s.\n", filename1);
        exit(1);
    }
    FILE *file2 = fopen(filename2, "r");
    if (!file2)
    {
        fprintf(stderr, "Unable to open file %s.\n", filename2);
        exit(1);
    }

  // getting contents of the first file
    fseek(file1, 0, SEEK_END);
    file_size = ftell(file1);
    rewind(file1);
    char *text1 = (char*) malloc (sizeof(char)*(file_size + 1));
    if (!text1)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    size_t result = fread(text1, 1, file_size, file1);
    if (result != file_size)
    {
        fprintf(stderr, "Error reading from file %s.\n", filename1);
        exit(1);
    }
    fclose(file1);
    text1[file_size] = '\0';

  // getting contents of the second file
    fseek(file2, 0, SEEK_END);
    file_size = ftell(file2);
    rewind(file2);
    char *text2 = (char*) malloc (sizeof(char)*(file_size + 1));
    if (!text2)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    result = fread(text2, 1, file_size, file2);
    if (result != file_size)
    {
        fprintf(stderr, "Error reading from file %s.\n", filename2);
        exit(1);
    }
    fclose(file2);
    text2[file_size] = '\0';

    struct btp_thread *crash_thread1;
    struct btp_thread *crash_thread2;


    // having optimized versions of the backtraces
        if (optimized)
        {
            crash_thread1 = btp_thread_parse_funs(text1);
            crash_thread2 = btp_thread_parse_funs(text2);
            free(text1);
            free(text2);
        }
    // backtraces in files weren't parsed yet, calling btp_bt_parse
        else
        {
            struct btp_backtrace *backtrace;
            struct btp_backtrace *backtrace2;
            const char *ptr = text1;
            const char *ptr2 = text2;

            struct btp_location location;
            btp_location_init(&location);
            backtrace = btp_backtrace_parse(&ptr, &location);
            if (!backtrace)
            {
                char *location_str = btp_location_to_string(&location);
                fprintf(stderr, "Failed to parse the backtrace.\n  %s\n",
                        location_str);
                free(location_str);
                exit(1);
            }
            free(text1);

            struct btp_location location2;
            btp_location_init(&location2);
            backtrace2 = btp_backtrace_parse(&ptr2, &location2);
            if (!backtrace2)
            {
                char *location_str = btp_location_to_string(&location2);
                fprintf(stderr, "Failed to parse the backtrace no.2.\n  %s\n",
                        location_str);
                free(location_str);
                exit(1);
            }
            free(text2);

            crash_thread1 = btp_backtrace_find_crash_thread(backtrace);
            crash_thread2 = btp_backtrace_find_crash_thread(backtrace2);
        }

    if (!crash_thread1)
    {
        fprintf(stderr, "Error getting the crash thread for first bug.\n");
        exit(1);
    }
    if (!crash_thread2)
    {
        fprintf(stderr, "Error getting the crash thread for second bug.\n");
        exit(1);
    }

    // removing threads not needed for comparison
    btp_thread_remove_frames_below_n(crash_thread1, accuracy);
    btp_thread_remove_frames_below_n(crash_thread2, accuracy);

  // renaming paired unknown function names
    btp_normalize_paired_unknown_function_names(crash_thread1, crash_thread2);

  // printing results of all the metrics
    printf("Levenshtein distance of these two backtraces is %d. \n",
           btp_thread_levenshtein_distance(crash_thread1, crash_thread2, true));
    printf("Jaccard distance of these two backtraces is %f. \n",
           btp_thread_jaccard_distance(crash_thread1, crash_thread2));
    printf("Jaro-Winkler distance of these two backtraces is %f. \n\n",
           btp_thread_jarowinkler_distance(crash_thread1, crash_thread2));

    return 0;
}

