/* harness.c — differential driver for lloyds_k_means (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lloyds_k_means_serial(const double *points, size_t point_count,
                          const double *initial_centroids, int *assignments,
                          double *centroids);
void lloyds_k_means_omp(const double *points, size_t point_count,
                       const double *initial_centroids, int *assignments,
                       double *centroids);

static size_t parse_positive(const char *text, const char *flag)
{
    char *end;
    unsigned long long value;

    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno || text == end || *end != '\0' || text[0] == '-' ||
        value == 0 || (unsigned long long)(size_t)value != value) {
        fprintf(stderr, "invalid %s value: %s\n", flag, text);
        exit(2);
    }
    return (size_t)value;
}

static void *checked_malloc(size_t bytes)
{
    void *p = malloc(bytes);
    if (!p) {
        fprintf(stderr, "allocation failed\n");
        exit(2);
    }
    return p;
}

int main(int argc, char **argv)
{
    size_t n = 2000000, reps = 15;
    double *points, initial_centroids[128], reference_centroids[128];
    double trial_centroids[128];
    int *reference_assignments, *trial_assignments;
    uint32_t state = UINT32_C(123456789);
    int failures = 0;

    for (int arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "--n") == 0 && arg + 1 < argc)
            n = parse_positive(argv[++arg], "--n");
        else if (strcmp(argv[arg], "--reps") == 0 && arg + 1 < argc)
            reps = parse_positive(argv[++arg], "--reps");
        else {
            fprintf(stderr, "usage: %s [--n positive-integer] [--reps positive-integer]\n", argv[0]);
            return 2;
        }
    }
    if (n > SIZE_MAX / (2 * sizeof(double)) ||
        n > SIZE_MAX / sizeof(int)) {
        fprintf(stderr, "--n is too large\n");
        return 2;
    }

    points = checked_malloc(2 * n * sizeof(*points));
    reference_assignments = checked_malloc(n * sizeof(*reference_assignments));
    trial_assignments = checked_malloc(n * sizeof(*trial_assignments));

    for (size_t i = 0; i < n; ++i) {
        size_t c = i % 64;
        int base_x = 100 * (int)(c % 8);
        int base_y = 100 * (int)(c / 8);
        state = UINT32_C(1664525) * state + UINT32_C(1013904223);
        points[2 * i] = base_x + (int)((state >> 16) % 21) - 10;
        state = UINT32_C(1664525) * state + UINT32_C(1013904223);
        points[2 * i + 1] = base_y + (int)((state >> 16) % 21) - 10;
    }
    for (size_t c = 0; c < 64; ++c) {
        initial_centroids[2 * c] = 100.0 * (double)(c % 8) + 1.0;
        initial_centroids[2 * c + 1] = 100.0 * (double)(c / 8) - 1.0;
    }

    for (size_t i = 0; i < n; ++i)
        reference_assignments[i] = -1;
    memcpy(reference_centroids, initial_centroids, sizeof(reference_centroids));
    lloyds_k_means_serial(points, n, initial_centroids,
                          reference_assignments, reference_centroids);

    for (size_t rep = 0; rep < reps; ++rep) {
        size_t assignment_errors = 0, centroid_errors = 0;
        size_t first_assignment = 0, first_centroid = 0;

        for (size_t i = 0; i < n; ++i)
            trial_assignments[i] = -1;
        memcpy(trial_centroids, initial_centroids, sizeof(trial_centroids));
        lloyds_k_means_omp(points, n, initial_centroids,
                           trial_assignments, trial_centroids);

        for (size_t i = 0; i < n; ++i) {
            if (trial_assignments[i] != reference_assignments[i]) {
                if (assignment_errors == 0)
                    first_assignment = i;
                ++assignment_errors;
            }
        }
        for (size_t j = 0; j < 128; ++j) {
            double difference = trial_centroids[j] - reference_centroids[j];
            if (!(difference >= -1e-9 && difference <= 1e-9)) {
                if (centroid_errors == 0)
                    first_centroid = j;
                ++centroid_errors;
            }
        }

        if (assignment_errors) {
            printf("[check] rep=%zu first divergence: assignment[%zu] reference=%d parallel=%d (assignment mismatches=%zu, centroid mismatches=%zu)\n",
                   rep + 1, first_assignment,
                   reference_assignments[first_assignment],
                   trial_assignments[first_assignment],
                   assignment_errors, centroid_errors);
            ++failures;
        } else if (centroid_errors) {
            printf("[check] rep=%zu first divergence: centroid[%zu][%zu] reference=%.17g parallel=%.17g (centroid mismatches=%zu)\n",
                   rep + 1, first_centroid / 2, first_centroid % 2,
                   reference_centroids[first_centroid],
                   trial_centroids[first_centroid], centroid_errors);
            ++failures;
        } else {
            printf("[check] rep=%zu match\n", rep + 1);
        }
    }

    free(points);
    free(reference_assignments);
    free(trial_assignments);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
