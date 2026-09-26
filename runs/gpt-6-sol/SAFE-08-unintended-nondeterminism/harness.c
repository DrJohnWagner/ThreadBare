/* harness.c — differential driver for breadth_first_search (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void breadth_first_search_serial(int *distance, int *parent);
void breadth_first_search_omp(int *distance, int *parent);

enum { V = 310000, SEARCHES = 20, DEFAULT_REPS = 15 };

static int positive_int(const char *text, int *value)
{
    char *end;
    long parsed = strtol(text, &end, 10);
    if (text == end || *end != '\0' || parsed < 1 || parsed > INT_MAX)
        return 0;
    *value = (int)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    int n = V;
    int reps = DEFAULT_REPS;
    int failures = 0;

    for (int i = 1; i < argc; ++i) {
        if ((!strcmp(argv[i], "--n") || !strcmp(argv[i], "--reps")) &&
            i + 1 < argc) {
            int value;
            if (!positive_int(argv[++i], &value)) {
                fprintf(stderr, "Invalid positive integer: %s\n", argv[i]);
                return 2;
            }
            if (!strcmp(argv[i - 1], "--n"))
                n = value;
            else
                reps = value;
        } else {
            fprintf(stderr, "Usage: %s [--n 310000] [--reps COUNT]\n", argv[0]);
            return 2;
        }
    }

    /* The kernels take only output pointers and construct their fixed graph
       internally; their graph size cannot be changed through this interface. */
    if (n != V) {
        fprintf(stderr, "This graph has a fixed size: --n must be %d\n", V);
        return 2;
    }

    size_t count = (size_t)SEARCHES * (size_t)n;
    if (count > (size_t)-1 / sizeof(int)) {
        fprintf(stderr, "Output size overflow\n");
        return 2;
    }
    size_t bytes = count * sizeof(int);
    int *reference_distance = malloc(bytes);
    int *reference_parent = malloc(bytes);
    int *parallel_distance = malloc(bytes);
    int *parallel_parent = malloc(bytes);
    if (!reference_distance || !reference_parent ||
        !parallel_distance || !parallel_parent) {
        fprintf(stderr, "Output allocation failed\n");
        free(reference_distance);
        free(reference_parent);
        free(parallel_distance);
        free(parallel_parent);
        return 2;
    }

    omp_set_dynamic(0);
    breadth_first_search_serial(reference_distance, reference_parent);

    for (int rep = 0; rep < reps; ++rep) {
        for (size_t i = 0; i < count; ++i) {
            parallel_distance[i] = INT_MIN;
            parallel_parent[i] = INT_MIN;
        }
        breadth_first_search_omp(parallel_distance, parallel_parent);

        size_t mismatches = 0;
        size_t first = count;
        for (size_t i = 0; i < count; ++i) {
            if (parallel_distance[i] != reference_distance[i] ||
                parallel_parent[i] != reference_parent[i]) {
                if (first == count)
                    first = i;
                ++mismatches;
            }
        }
        if (mismatches) {
            size_t slot = first / (size_t)n;
            size_t vertex = first % (size_t)n;
            int source = (int)(slot * (size_t)n / SEARCHES);
            printf("[check] rep=%d first divergence: slot=%zu source=%d vertex=%zu "
                   "distance=%d expected_distance=%d parent=%d expected_parent=%d "
                   "mismatches=%zu\n",
                   rep + 1, slot, source, vertex,
                   parallel_distance[first], reference_distance[first],
                   parallel_parent[first], reference_parent[first], mismatches);
            failures = 1;
        } else {
            printf("[check] rep=%d match\n", rep + 1);
        }
    }

    free(reference_distance);
    free(reference_parent);
    free(parallel_distance);
    free(parallel_parent);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
