/* harness.c — differential driver for lloyd_kmeans (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness -lm
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

void lloyd_kmeans_serial(size_t n, const int *points,
                          const double *initial_centroids,
                          int *assignments, double *centroids);
void lloyd_kmeans_omp(size_t n, const int *points,
                       const double *initial_centroids,
                       int *assignments, double *centroids);

static uint32_t next_u32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int parse_positive(const char *text, unsigned long long *value)
{
    char *end;
    unsigned long long parsed;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed == 0)
        return 0;
    *value = parsed;
    return 1;
}

int main(int argc, char **argv)
{
    /* A million points multiplied by 15 complete Lloyd runs is expensive;
       use the smaller specified size by default. --n 1000000 tests the larger. */
    size_t n = 250000;
    int reps = 15;
    int failures = 0;
    int *points, *reference_assignments, *parallel_assignments;
    double initial_centroids[128], reference_centroids[128], parallel_centroids[128];
    uint32_t state = UINT32_C(0x9E3779B9);

    for (int arg = 1; arg < argc; ++arg) {
        unsigned long long value;
        if (strcmp(argv[arg], "--n") == 0 && arg + 1 < argc) {
            if (!parse_positive(argv[++arg], &value) ||
                value > SIZE_MAX / (2 * sizeof(int))) {
                fprintf(stderr, "invalid --n\n");
                return 2;
            }
            n = (size_t)value;
        } else if (strcmp(argv[arg], "--reps") == 0 && arg + 1 < argc) {
            if (!parse_positive(argv[++arg], &value) || value > 1000000) {
                fprintf(stderr, "invalid --reps\n");
                return 2;
            }
            reps = (int)value;
        } else {
            fprintf(stderr, "usage: %s [--n positive-integer] [--reps positive-integer]\n", argv[0]);
            return 2;
        }
    }

    if (n > SIZE_MAX / (2 * sizeof(int))) {
        fprintf(stderr, "--n is too large\n");
        return 2;
    }
    points = malloc(2 * n * sizeof(*points));
    reference_assignments = malloc(n * sizeof(*reference_assignments));
    parallel_assignments = malloc(n * sizeof(*parallel_assignments));
    if (!points || !reference_assignments || !parallel_assignments) {
        fprintf(stderr, "allocation failed\n");
        free(points);
        free(reference_assignments);
        free(parallel_assignments);
        return 2;
    }

    for (size_t i = 0; i < n; ++i) {
        points[2 * i] = (int)(next_u32(&state) % 8192u);
        points[2 * i + 1] = (int)(next_u32(&state) % 8192u);
        reference_assignments[i] = -1;
    }
    for (int k = 0; k < 64; ++k) {
        initial_centroids[2 * k] = 512.0 + 1024.0 * (k % 8);
        initial_centroids[2 * k + 1] = 512.0 + 1024.0 * (k / 8);
    }

    /* Avoid a one-thread run silently masking concurrent updates. */
    omp_set_dynamic(0);
    int threads = omp_get_max_threads();
    if (threads < 4) threads = 4;
    if (threads > 8) threads = 8;
    omp_set_num_threads(threads);

    lloyd_kmeans_serial(n, points, initial_centroids,
                        reference_assignments, reference_centroids);
    for (int rep = 1; rep <= reps; ++rep) {
        size_t i;
        for (i = 0; i < n; ++i)
            parallel_assignments[i] = -1;
        lloyd_kmeans_omp(n, points, initial_centroids,
                         parallel_assignments, parallel_centroids);

        for (i = 0; i < n; ++i) {
            if (parallel_assignments[i] != reference_assignments[i]) {
                printf("[check] rep=%d point=%zu reference=%d parallel=%d\n",
                       rep, i, reference_assignments[i], parallel_assignments[i]);
                ++failures;
                break;
            }
        }
        if (i == n) {
            int coordinate;
            for (coordinate = 0; coordinate < 128; ++coordinate) {
                if (!(fabs(parallel_centroids[coordinate] -
                           reference_centroids[coordinate]) <= 1e-9)) {
                    printf("[check] rep=%d centroid=%d coordinate=%d reference=%.17g parallel=%.17g\n",
                           rep, coordinate / 2, coordinate % 2,
                           reference_centroids[coordinate],
                           parallel_centroids[coordinate]);
                    ++failures;
                    break;
                }
            }
            if (coordinate == 128)
                printf("[check] rep=%d match\n", rep);
        }
    }

    free(points);
    free(reference_assignments);
    free(parallel_assignments);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
