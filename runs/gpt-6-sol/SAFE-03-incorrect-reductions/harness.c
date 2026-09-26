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
#include <omp.h>

void lloyd_kmeans_serial(const double *points, size_t n,
                         const double *initial_centroids,
                         int *assignments, double *centroids);
void lloyd_kmeans_omp(const double *points, size_t n,
                      const double *initial_centroids,
                      int *assignments, double *centroids);

static uint32_t hash32(uint32_t j)
{
    uint32_t x = j * 1664525u + 1013904223u;
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    return x;
}

static int parse_positive(const char *text, size_t *value)
{
    char *end;
    unsigned long long parsed;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed == 0 ||
        parsed > (unsigned long long)SIZE_MAX)
        return 0;
    *value = (size_t)parsed;
    return 1;
}

static double absolute(double x)
{
    return x < 0.0 ? -x : x;
}

int main(int argc, char **argv)
{
    size_t n = 1048576, reps = 15;
    double initial[128];
    double *points, *reference_centroids, *actual_centroids;
    int *reference_assignments, *actual_assignments;
    int failures = 0;
    int stability_checked = 0;

    for (int a = 1; a < argc; ++a) {
        size_t value;
        if (a + 1 >= argc || !parse_positive(argv[a + 1], &value)) {
            fprintf(stderr, "usage: %s [--n N] [--reps R]\n", argv[0]);
            return 2;
        }
        if (strcmp(argv[a], "--n") == 0)
            n = value;
        else if (strcmp(argv[a], "--reps") == 0)
            reps = value;
        else {
            fprintf(stderr, "unknown flag: %s\n", argv[a]);
            return 2;
        }
        ++a;
    }
    if (n < 64 || n > SIZE_MAX / (2 * sizeof(double)) ||
        n > SIZE_MAX / sizeof(int) || n > UINT32_MAX / 2) {
        fprintf(stderr, "invalid problem size\n");
        return 2;
    }

    points = malloc(2 * n * sizeof(*points));
    reference_centroids = malloc(sizeof(initial));
    actual_centroids = malloc(sizeof(initial));
    reference_assignments = malloc(n * sizeof(*reference_assignments));
    actual_assignments = malloc(n * sizeof(*actual_assignments));
    if (!points || !reference_centroids || !actual_centroids ||
        !reference_assignments || !actual_assignments) {
        fprintf(stderr, "allocation failed\n");
        free(points);
        free(reference_centroids);
        free(actual_centroids);
        free(reference_assignments);
        free(actual_assignments);
        return 2;
    }

    for (size_t i = 0; i < n; ++i) {
        size_t c = i % 64;
        points[2 * i] = 100.0 * (double)(c % 8) +
            ((int)(hash32((uint32_t)(2 * i)) % 2001u) - 1000) / 1000.0;
        points[2 * i + 1] = 100.0 * (double)(c / 8) +
            ((int)(hash32((uint32_t)(2 * i + 1)) % 2001u) - 1000) / 1000.0;
    }
    memcpy(initial, points, sizeof(initial));
    for (size_t i = 0; i < n; ++i)
        reference_assignments[i] = -1;
    memcpy(reference_centroids, initial, sizeof(initial));
    lloyd_kmeans_serial(points, n, initial,
                        reference_assignments, reference_centroids);

    omp_set_dynamic(0);
    for (size_t rep = 0; rep < reps; ++rep) {
        int mismatch = 0;
        for (size_t i = 0; i < n; ++i)
            actual_assignments[i] = -1;
        memcpy(actual_centroids, initial, sizeof(initial));
        lloyd_kmeans_omp(points, n, initial,
                         actual_assignments, actual_centroids);

        for (size_t i = 0; i < n; ++i) {
            if (actual_assignments[i] != reference_assignments[i]) {
                printf("[check] rep=%zu assignment[%zu]: expected=%d actual=%d\n",
                       rep + 1, i, reference_assignments[i], actual_assignments[i]);
                mismatch = 1;
                break;
            }
        }
        if (!mismatch) {
            for (size_t j = 0; j < 128; ++j) {
                double difference = absolute(actual_centroids[j] - reference_centroids[j]);
                if (!(difference <= 1e-9)) {
                    printf("[check] rep=%zu centroid[%zu][%zu]: expected=%.17g actual=%.17g\n",
                           rep + 1, j / 2, j % 2,
                           reference_centroids[j], actual_centroids[j]);
                    mismatch = 1;
                    break;
                }
            }
        }
        if (!mismatch && !stability_checked) {
            stability_checked = 1;
            for (size_t i = 0; i < n; ++i) {
                int nearest = 0;
                double dx = points[2 * i] - actual_centroids[0];
                double dy = points[2 * i + 1] - actual_centroids[1];
                double best = dx * dx + dy * dy;
                for (int c = 1; c < 64; ++c) {
                    dx = points[2 * i] - actual_centroids[2 * c];
                    dy = points[2 * i + 1] - actual_centroids[2 * c + 1];
                    double distance = dx * dx + dy * dy;
                    if (distance < best) {
                        best = distance;
                        nearest = c;
                    }
                }
                if (nearest != actual_assignments[i]) {
                    printf("[check] rep=%zu unstable assignment[%zu]: reported=%d nearest=%d\n",
                           rep + 1, i, actual_assignments[i], nearest);
                    mismatch = 1;
                    break;
                }
            }
        }
        if (mismatch)
            ++failures;
        else
            printf("[check] rep=%zu match\n", rep + 1);
    }

    free(points);
    free(reference_centroids);
    free(actual_centroids);
    free(reference_assignments);
    free(actual_assignments);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
