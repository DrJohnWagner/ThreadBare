/* harness.c — scaling driver for lloyd_kmeans (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lloyd_kmeans_serial(size_t n, const double *points,
                         const double *initial_centroids,
                         int *assignments, double *centroids);
void lloyd_kmeans_omp(size_t n, const double *points,
                      const double *initial_centroids,
                      int *assignments, double *centroids);

int main(int argc, char **argv)
{
    size_t n = 1000000;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") != 0 || ++i == argc) {
            fprintf(stderr, "usage: %s [--n positive-integer]\n", argv[0]);
            return 2;
        }
        errno = 0;
        char *end;
        unsigned long long value = strtoull(argv[i], &end, 10);
        if (errno || end == argv[i] || *end || value == 0 ||
            value > SIZE_MAX / (2 * sizeof(double)) ||
            value > SIZE_MAX / sizeof(int)) {
            fprintf(stderr, "invalid problem size\n");
            return 2;
        }
        n = (size_t)value;
    }

    double *points = malloc(2 * n * sizeof(*points));
    int *reference = malloc(n * sizeof(*reference));
    int *result = malloc(n * sizeof(*result));
    double initial[128], expected_centroids[128], result_centroids[128];
    if (!points || !reference || !result) {
        fprintf(stderr, "allocation failed\n");
        free(points);
        free(reference);
        free(result);
        return 2;
    }

    for (size_t i = 0; i < n; ++i) {
        unsigned g = (unsigned)(i % 64);
        uint32_t s1 = UINT32_C(1664525) * (uint32_t)(i + 1) +
                      UINT32_C(1013904223);
        uint32_t s2 = UINT32_C(1664525) * s1 + UINT32_C(1013904223);
        points[2 * i] = 10.0 * (g % 8) + ((int)(s1 % 1001) - 500) / 1000.0;
        points[2 * i + 1] = 10.0 * (g / 8) + ((int)(s2 % 1001) - 500) / 1000.0;
    }
    for (unsigned g = 0; g < 64; ++g) {
        initial[2 * g] = 10.0 * (g % 8) + 0.25;
        initial[2 * g + 1] = 10.0 * (g / 8) + 0.25;
    }

    /* Untimed full kernel pass, including input and output first touches. */
    for (size_t i = 0; i < n; ++i) reference[i] = -1;
    memcpy(expected_centroids, initial, sizeof(initial));
    lloyd_kmeans_serial(n, points, initial, reference, expected_centroids);

    for (size_t i = 0; i < n; ++i) reference[i] = -1;
    memcpy(expected_centroids, initial, sizeof(initial));
    double start = omp_get_wtime();
    lloyd_kmeans_serial(n, points, initial, reference, expected_centroids);
    double serial_seconds = omp_get_wtime() - start;
    printf("[scale] serial seconds=%.6f\n", serial_seconds);

    int max_threads = omp_get_max_threads();
    int failures = 0;
    for (int threads = 1; ; ) {
        omp_set_dynamic(0);
        omp_set_num_threads(threads);
        for (size_t i = 0; i < n; ++i) result[i] = -1;
        memcpy(result_centroids, initial, sizeof(initial));
        start = omp_get_wtime();
        lloyd_kmeans_omp(n, points, initial, result, result_centroids);
        double seconds = omp_get_wtime() - start;
        double speedup = serial_seconds / seconds;
        double efficiency = speedup / threads;
        printf("[scale] threads=%d seconds=%.6f speedup=%.3f efficiency=%.3f\n",
               threads, seconds, speedup, efficiency);

        if (threads == max_threads) {
            /* Check the final parallel output once, outside the timed region. */
            size_t different = 0;
            for (size_t i = 0; i < n; ++i)
                different += result[i] != reference[i];
            for (size_t i = 0; i < 128; ++i)
                different += !isfinite(result_centroids[i]) ||
                             fabs(result_centroids[i] - expected_centroids[i]) > 1e-9;
            printf("[check] differing outputs=%zu\n", different);
            if (different || (max_threads > 1 && efficiency < 0.45))
                failures = 1;
            break;
        }
        threads = threads > max_threads / 2 ? max_threads : threads * 2;
    }

    free(points);
    free(reference);
    free(result);
    printf("[verdict] %s\n", failures ? "PERF-01 exposed" : "PERF-01 not triggered");
    return failures ? 1 : 0;
}
