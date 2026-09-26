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
                         const double *initial_centroids, int *assignments,
                         double *final_centroids, int *iterations);
void lloyd_kmeans_omp(size_t n, const double *points,
                      const double *initial_centroids, int *assignments,
                      double *final_centroids, int *iterations);

static uint32_t next_state(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

int main(int argc, char **argv)
{
    size_t n = 1000000;
    int failures = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            char *end;
            unsigned long long value = strtoull(argv[++i], &end, 10);
            if (*end != '\0' || value < 64 || value > SIZE_MAX / (2 * sizeof(double))) {
                fprintf(stderr, "Invalid --n value\n");
                goto verdict;
            }
            n = (size_t)value;
        } else {
            fprintf(stderr, "Usage: %s [--n N]\n", argv[0]);
            goto verdict;
        }
    }

    double *points = malloc(2 * n * sizeof(*points));
    double *initial = malloc(128 * sizeof(*initial));
    double *serial_centroids = malloc(128 * sizeof(*serial_centroids));
    double *parallel_centroids = malloc(128 * sizeof(*parallel_centroids));
    int *serial_assignments = malloc(n * sizeof(*serial_assignments));
    int *parallel_assignments = malloc(n * sizeof(*parallel_assignments));
    if (!points || !initial || !serial_centroids || !parallel_centroids ||
        !serial_assignments || !parallel_assignments) {
        fprintf(stderr, "Allocation failed\n");
        free(points);
        free(initial);
        free(serial_centroids);
        free(parallel_centroids);
        free(serial_assignments);
        free(parallel_assignments);
        goto verdict;
    }

    for (size_t i = 0; i < n; ++i) {
        unsigned g = (unsigned)(i % 64);
        uint32_t state = UINT32_C(0x9e3779b9) ^ (uint32_t)(i + 1);
        uint32_t a = next_state(&state);
        uint32_t b = next_state(&state);
        points[2 * i] = 100.0 * (g % 8) + ((int)(a % 2001u) - 1000) / 100.0;
        points[2 * i + 1] = 100.0 * (g / 8) + ((int)(b % 2001u) - 1000) / 100.0;
    }
    memcpy(initial, points, 128 * sizeof(*initial));

    /* Touch input and both output buffers before starting the measurements. */
    volatile double touched = 0.0;
    for (size_t i = 0; i < 2 * n; ++i)
        touched += points[i];
    for (size_t i = 0; i < 128; ++i) {
        touched += initial[i];
        serial_centroids[i] = 0.0;
        parallel_centroids[i] = 0.0;
    }
    for (size_t i = 0; i < n; ++i) {
        serial_assignments[i] = -1;
        parallel_assignments[i] = -1;
    }
    (void)touched;

    omp_set_dynamic(0);
    int serial_iterations = 0;
    int parallel_iterations = 0;

    /* Untimed kernel warmup also removes first-use costs from the baseline. */
    lloyd_kmeans_serial(n, points, initial, serial_assignments,
                        serial_centroids, &serial_iterations);

    double start = omp_get_wtime();
    lloyd_kmeans_serial(n, points, initial, serial_assignments,
                        serial_centroids, &serial_iterations);
    double serial_seconds = omp_get_wtime() - start;
    printf("[scale] serial seconds=%.6f\n", serial_seconds);

    int max_threads = omp_get_max_threads();
    double highest_efficiency = 1.0;
    for (int threads = 1; ; ) {
        omp_set_num_threads(threads);
        start = omp_get_wtime();
        lloyd_kmeans_omp(n, points, initial, parallel_assignments,
                         parallel_centroids, &parallel_iterations);
        double seconds = omp_get_wtime() - start;
        double speedup = seconds > 0.0 ? serial_seconds / seconds : 0.0;
        double efficiency = speedup / threads;
        printf("[scale] threads=%d seconds=%.6f speedup=%.3f efficiency=%.3f\n",
               threads, seconds, speedup, efficiency);

        if (threads == max_threads) {
            highest_efficiency = efficiency;
            break;
        }
        threads = threads > max_threads / 2 ? max_threads : threads * 2;
    }

    /* One correctness check, using the output from the highest thread count. */
    if (serial_iterations != parallel_iterations) {
        printf("[check] iterations: serial=%d parallel=%d\n",
               serial_iterations, parallel_iterations);
    } else {
        size_t i;
        for (i = 0; i < n; ++i) {
            if (serial_assignments[i] != parallel_assignments[i]) {
                printf("[check] assignment[%zu]: serial=%d parallel=%d\n",
                       i, serial_assignments[i], parallel_assignments[i]);
                break;
            }
        }
        if (i == n) {
            for (i = 0; i < 128; ++i) {
                double delta = serial_centroids[i] - parallel_centroids[i];
                if (delta < -1e-8 || delta > 1e-8) {
                    printf("[check] centroid[%zu]: serial=%.17g parallel=%.17g\n",
                           i, serial_centroids[i], parallel_centroids[i]);
                    break;
                }
            }
            if (i == 128)
                printf("[check] outputs match\n");
        }
    }

    /* The verdict measures scaling, not absolute runtime. */
    failures = max_threads > 1 && highest_efficiency < 0.55;

    free(points);
    free(initial);
    free(serial_centroids);
    free(parallel_centroids);
    free(serial_assignments);
    free(parallel_assignments);

verdict:
    printf("[verdict] %s\n", failures ? "PERF-01 exposed" : "PERF-01 not triggered");
    return failures ? 1 : 0;
}
