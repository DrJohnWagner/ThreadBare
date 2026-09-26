/* harness.c — scaling driver for lloyd_kmeans (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

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

static uint64_t advance(uint64_t *state)
{
    *state = *state * UINT64_C(6364136223846793005)
                    + UINT64_C(1442695040888963407);
    return *state;
}

static void reset_output(size_t n, int *assignments, double *centroids,
                         const double *initial_centroids)
{
    for (size_t i = 0; i < n; ++i)
        assignments[i] = -1;
    memcpy(centroids, initial_centroids, 128 * sizeof(double));
}

int main(int argc, char **argv)
{
    size_t n = 2000000;
    for (int i = 1; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc)
            value = argv[++i];
        else if (strncmp(argv[i], "--n=", 4) == 0)
            value = argv[i] + 4;
        if (!value) {
            fprintf(stderr, "usage: %s [--n positive-integer]\n", argv[0]);
            return 2;
        }
        char *end;
        unsigned long long requested = strtoull(value, &end, 10);
        if (!*value || *end || requested == 0 || requested > SIZE_MAX / (2 * sizeof(double))) {
            fprintf(stderr, "invalid --n value\n");
            return 2;
        }
        n = (size_t)requested;
    }

    double *points = malloc(2 * n * sizeof(*points));
    int *reference = malloc(n * sizeof(*reference));
    int *candidate = malloc(n * sizeof(*candidate));
    double initial[128], serial_centroids[128], parallel_centroids[128];
    if (!points || !reference || !candidate) {
        fprintf(stderr, "allocation failed\n");
        free(points);
        free(reference);
        free(candidate);
        return 2;
    }

    uint64_t state = 1;
    const double unit = 1.0 / 9007199254740992.0;
    for (size_t i = 0; i < n; ++i) {
        unsigned c = (unsigned)(advance(&state) >> 58);
        double ux = (double)(advance(&state) >> 11) * unit;
        double uy = (double)(advance(&state) >> 11) * unit;
        points[2 * i] = ((double)(c % 8) + 0.5) / 8.0 + 0.05 * (ux - 0.5);
        points[2 * i + 1] = ((double)(c / 8) + 0.5) / 8.0 + 0.05 * (uy - 0.5);
    }
    for (unsigned j = 0; j < 64; ++j) {
        initial[2 * j] = ((double)(j % 8) + 0.5) / 8.0;
        initial[2 * j + 1] = ((double)(j / 8) + 0.5) / 8.0;
    }

    /* Untimed kernel pass faults in input and output pages before measurement. */
    reset_output(n, reference, serial_centroids, initial);
    lloyd_kmeans_serial(n, points, initial, reference, serial_centroids);
    reset_output(n, candidate, parallel_centroids, initial);

    reset_output(n, reference, serial_centroids, initial);
    double start = omp_get_wtime();
    lloyd_kmeans_serial(n, points, initial, reference, serial_centroids);
    double serial_seconds = omp_get_wtime() - start;
    printf("[scale] serial seconds=%.6f\n", serial_seconds);

    omp_set_dynamic(0);
    int max_threads = omp_get_max_threads();
    double final_efficiency = 1.0;
    int checked = 0;
    int threads = 1;
    for (;;) {
        omp_set_num_threads(threads);
        reset_output(n, candidate, parallel_centroids, initial);
        start = omp_get_wtime();
        lloyd_kmeans_omp(n, points, initial, candidate, parallel_centroids);
        double seconds = omp_get_wtime() - start;
        double speedup = serial_seconds / seconds;
        final_efficiency = speedup / (double)threads;
        printf("[scale] threads=%d seconds=%.6f speedup=%.3f efficiency=%.3f\n",
               threads, seconds, speedup, final_efficiency);

        if (!checked) {
            size_t mismatch = n;
            for (size_t i = 0; i < n; ++i) {
                if (reference[i] != candidate[i]) {
                    mismatch = i;
                    break;
                }
            }
            if (mismatch != n) {
                printf("[check] assignment mismatch at point %zu: serial=%d parallel=%d\n",
                       mismatch, reference[mismatch], candidate[mismatch]);
            } else {
                int coordinate = -1;
                for (int j = 0; j < 128; ++j) {
                    double difference = serial_centroids[j] - parallel_centroids[j];
                    if (!(difference >= -1e-9 && difference <= 1e-9)) {
                        coordinate = j;
                        break;
                    }
                }
                if (coordinate >= 0)
                    printf("[check] centroid coordinate %d differs\n", coordinate);
                else
                    printf("[check] output matches reference\n");
            }
            checked = 1;
        }

        if (threads == max_threads)
            break;
        threads = threads > max_threads / 2 ? max_threads : threads * 2;
    }

    /* The two-worker cap can only be diagnosed by scaling beyond two workers. */
    int failures = max_threads >= 4 && final_efficiency < 0.65;
    if (max_threads < 4)
        printf("[check] at least four available threads required to test scaling\n");
    free(points);
    free(reference);
    free(candidate);
    printf("[verdict] %s\n", failures ? "PERF-01 exposed" : "PERF-01 not triggered");
    return failures ? 1 : 0;
}