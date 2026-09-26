/* harness.c — scaling driver for multi_source_bfs (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <omp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int multi_source_bfs_serial(int *distance, int *parent);
int multi_source_bfs_omp(int *distance, int *parent);

enum { V = 310000, SOURCES = 20 };

static int check_outputs(const int *reference_distance,
                         const int *parallel_distance,
                         const int *parallel_parent)
{
    for (int search = 0; search < SOURCES; ++search) {
        int source = (int)((long long)search * V / SOURCES);
        size_t base = (size_t)search * V;

        for (int vertex = 0; vertex < V; ++vertex) {
            size_t index = base + (size_t)vertex;
            int got = parallel_distance[index];
            int expected = reference_distance[index];
            int parent = parallel_parent[index];

            if (got != expected) {
                printf("[check] search=%d vertex=%d distance=%d expected=%d\n",
                       search, vertex, got, expected);
                return 0;
            }
            if (vertex == source || got == -1) {
                if (parent != -1) {
                    printf("[check] search=%d vertex=%d parent=%d expected=-1\n",
                           search, vertex, parent);
                    return 0;
                }
            } else if (parent < 0 || parent >= V ||
                       parallel_distance[base + (size_t)parent] != got - 1) {
                printf("[check] search=%d vertex=%d invalid parent=%d\n",
                       search, vertex, parent);
                return 0;
            }
        }
    }
    printf("[check] all distances and parent levels match\n");
    return 1;
}

int main(void)
{
    const size_t count = (size_t)V * SOURCES;
    int failures = 0;
    int *reference_distance = malloc(count * sizeof *reference_distance);
    int *reference_parent = malloc(count * sizeof *reference_parent);
    int *parallel_distance = malloc(count * sizeof *parallel_distance);
    int *parallel_parent = malloc(count * sizeof *parallel_parent);

    if (!reference_distance || !reference_parent ||
        !parallel_distance || !parallel_parent) {
        printf("[check] allocation failed\n");
        failures = 1;
        goto done;
    }

    /* The kernels construct the specified seeded graph internally; their
       signatures expose only the arrays for the 20 source results. */
    omp_set_dynamic(0);
    omp_set_num_threads(1);
    if (multi_source_bfs_serial(reference_distance, reference_parent) != 0 ||
        multi_source_bfs_omp(parallel_distance, parallel_parent) != 0) {
        printf("[check] warmup kernel returned an error\n");
        failures = 1;
        goto done;
    }

    double start = omp_get_wtime();
    if (multi_source_bfs_serial(reference_distance, reference_parent) != 0) {
        printf("[check] serial kernel returned an error\n");
        failures = 1;
        goto done;
    }
    double serial_seconds = omp_get_wtime() - start;
    printf("[scale] serial seconds=%.6f\n", serial_seconds);

    int max_threads = omp_get_max_threads();
    double last_efficiency = 0.0;
    for (int threads = 1;;) {
        omp_set_num_threads(threads);
        start = omp_get_wtime();
        int status = multi_source_bfs_omp(parallel_distance, parallel_parent);
        double seconds = omp_get_wtime() - start;
        if (status != 0) {
            printf("[check] parallel kernel returned an error at %d threads\n",
                   threads);
            failures = 1;
            goto done;
        }

        double speedup = seconds > 0.0 ? serial_seconds / seconds : 0.0;
        last_efficiency = speedup / threads;
        printf("[scale] threads=%d seconds=%.6f speedup=%.3f efficiency=%.3f\n",
               threads, seconds, speedup, last_efficiency);

        if (threads == max_threads)
            break;
        threads = threads > max_threads / 2 ? max_threads : threads * 2;
    }

    if (!check_outputs(reference_distance, parallel_distance, parallel_parent))
        failures = 1;
    if (max_threads > 1 && last_efficiency < 0.65)
        failures = 1;

done:
    free(reference_distance);
    free(reference_parent);
    free(parallel_distance);
    free(parallel_parent);
    printf("[verdict] %s\n", failures ? "PERF-01 exposed" : "PERF-01 not triggered");
    return failures ? 1 : 0;
}
