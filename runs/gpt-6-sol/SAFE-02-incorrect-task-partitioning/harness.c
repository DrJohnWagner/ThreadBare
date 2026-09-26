/* harness.c — differential driver for lloyd_kmeans (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lloyd_kmeans_serial(size_t n, const int *points,
                         const double *initial_centroids, int *assignments,
                         size_t *counts, double *centroids);
void lloyd_kmeans_omp(size_t n, const int *points,
                      const double *initial_centroids, int *assignments,
                      size_t *counts, double *centroids);

static int parse_positive_size(const char *text, size_t *value)
{
    char *end;
    unsigned long long parsed;

    if (*text == '-' || *text == '\0') return 0;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno || *end != '\0' || parsed == 0 || (size_t)parsed != parsed)
        return 0;
    *value = (size_t)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    size_t n = 1000000, reps = 20;
    int *points, *reference, *actual;
    size_t expected_counts[64], actual_counts[64], observed_counts[64];
    double initial_centroids[128], expected_centroids[128], actual_centroids[128];
    uint32_t state = UINT32_C(0xC0FFEE12);
    int failures = 0;

    for (int a = 1; a < argc; ++a) {
        size_t *target;
        if (strcmp(argv[a], "--n") == 0) target = &n;
        else if (strcmp(argv[a], "--reps") == 0) target = &reps;
        else {
            fprintf(stderr, "usage: %s [--n positive-integer] [--reps positive-integer]\n", argv[0]);
            return 2;
        }
        if (++a == argc || !parse_positive_size(argv[a], target)) {
            fprintf(stderr, "invalid or missing value for option\n");
            return 2;
        }
    }

    if (n > SIZE_MAX / (2 * sizeof(int))) {
        fprintf(stderr, "problem size too large\n");
        return 2;
    }
    points = malloc(2 * n * sizeof(*points));
    reference = malloc(n * sizeof(*reference));
    actual = malloc(n * sizeof(*actual));
    if (!points || !reference || !actual) {
        fprintf(stderr, "allocation failed\n");
        free(points);
        free(reference);
        free(actual);
        return 2;
    }

    for (size_t i = 0; i < n; ++i) {
        int g = (int)(i % 64);
        state = UINT32_C(1664525) * state + UINT32_C(1013904223);
        int dx = (int)((state >> 16) % 161) - 80;
        state = UINT32_C(1664525) * state + UINT32_C(1013904223);
        int dy = (int)((state >> 16) % 161) - 80;
        points[2 * i] = 128 * (g % 8) + dx;
        points[2 * i + 1] = 128 * (g / 8) + dy;
        reference[i] = -1;
    }
    for (int g = 0; g < 64; ++g) {
        initial_centroids[2 * g] = 128 * (g % 8) + 4 * ((g % 3) - 1);
        initial_centroids[2 * g + 1] = 128 * (g / 8) + 4 * (((g / 3) % 3) - 1);
    }

    memset(expected_counts, 0, sizeof expected_counts);
    memcpy(expected_centroids, initial_centroids, sizeof expected_centroids);
    lloyd_kmeans_serial(n, points, initial_centroids, reference,
                        expected_counts, expected_centroids);

    for (size_t rep = 0; rep < reps; ++rep) {
        int mismatch = 0;
        for (size_t i = 0; i < n; ++i) actual[i] = -1;
        memset(actual_counts, 0, sizeof actual_counts);
        memset(observed_counts, 0, sizeof observed_counts);
        memcpy(actual_centroids, initial_centroids, sizeof actual_centroids);

        lloyd_kmeans_omp(n, points, initial_centroids, actual,
                         actual_counts, actual_centroids);

        for (size_t i = 0; i < n; ++i) {
            if (actual[i] >= 0 && actual[i] < 64)
                ++observed_counts[actual[i]];
            if (actual[i] != reference[i] && !mismatch) {
                printf("[check] rep=%zu assignment[%zu]: expected=%d actual=%d\n",
                       rep + 1, i, reference[i], actual[i]);
                mismatch = 1;
            }
        }
        for (size_t k = 0; k < 64; ++k) {
            if (actual_counts[k] != expected_counts[k] && !mismatch) {
                printf("[check] rep=%zu count[%zu]: expected=%zu actual=%zu\n",
                       rep + 1, k, expected_counts[k], actual_counts[k]);
                mismatch = 1;
            }
            if (actual_counts[k] != observed_counts[k] && !mismatch) {
                printf("[check] rep=%zu count[%zu]: reported=%zu observed=%zu\n",
                       rep + 1, k, actual_counts[k], observed_counts[k]);
                mismatch = 1;
            }
        }
        for (size_t k = 0; k < 128; ++k) {
            double delta = actual_centroids[k] - expected_centroids[k];
            if (delta < 0) delta = -delta;
            if (!(delta <= 1e-10) && !mismatch) {
                printf("[check] rep=%zu centroid[%zu].%c: expected=%.17g actual=%.17g\n",
                       rep + 1, k / 2, k % 2 ? 'y' : 'x',
                       expected_centroids[k], actual_centroids[k]);
                mismatch = 1;
            }
        }
        failures += mismatch;
    }

    if (!failures) printf("[check] %zu repetitions matched the reference\n", reps);
    free(points);
    free(reference);
    free(actual);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
