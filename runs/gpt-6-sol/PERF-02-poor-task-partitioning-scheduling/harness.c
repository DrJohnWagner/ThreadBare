/* harness.c — scaling driver for breadth_first_search (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void breadth_first_search_serial(const int *offsets, const int *neighbors,
                                 const int *sources, int *distance, int *parent);
void breadth_first_search_omp(const int *offsets, const int *neighbors,
                              const int *sources, int *distance, int *parent);

enum { N = 310000, M = 5000000, NSOURCES = 20 };

static uint64_t rng_state = 1;

static uint64_t next_random(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * UINT64_C(2685821657736338717);
}

static void add_weight(int *tree, int vertex, int amount)
{
    for (int i = vertex + 1; i <= N; i += i & -i)
        tree[i] += amount;
}

/* Return a zero-based vertex selected by its current degree. */
static int select_vertex(const int *tree, uint64_t total)
{
    uint64_t target = next_random() % total;
    int index = 0;
    for (int bit = 1 << 18; bit != 0; bit >>= 1) {
        int candidate = index + bit;
        if (candidate <= N && (uint64_t)tree[candidate] <= target) {
            target -= (uint64_t)tree[candidate];
            index = candidate;
        }
    }
    return index;
}

static void build_graph(int *offsets, int *neighbors)
{
    int *degree = calloc(N, sizeof(*degree));
    int *tree = calloc(N + 1, sizeof(*tree));
    int *seen = calloc(N, sizeof(*seen));
    int *ends_a = malloc((size_t)M * sizeof(*ends_a));
    int *ends_b = malloc((size_t)M * sizeof(*ends_b));
    int *cursor = malloc((size_t)N * sizeof(*cursor));
    if (!degree || !tree || !seen || !ends_a || !ends_b || !cursor) {
        fprintf(stderr, "graph allocation failed\n");
        exit(2);
    }

    int edges = 0;
    for (int a = 0; a < 17; ++a) {
        for (int b = a + 1; b < 17; ++b) {
            ends_a[edges] = a;
            ends_b[edges++] = b;
        }
        degree[a] = 16;
        add_weight(tree, a, 16);
    }

    uint64_t total = 17 * 16;
    for (int v = 17; v < N; ++v) {
        int count = 16 + (v < 17 + 40136);
        int chosen[17];
        for (int j = 0; j < count; ++j) {
            int neighbor;
            do {
                neighbor = select_vertex(tree, total);
            } while (seen[neighbor] == v + 1);
            seen[neighbor] = v + 1;
            chosen[j] = neighbor;
            ends_a[edges] = v;
            ends_b[edges++] = neighbor;
        }
        for (int j = 0; j < count; ++j) {
            ++degree[chosen[j]];
            add_weight(tree, chosen[j], 1);
        }
        degree[v] = count;
        add_weight(tree, v, count);
        total += (uint64_t)2 * count;
    }
    if (edges != M) {
        fprintf(stderr, "incorrect graph edge count: %d\n", edges);
        exit(2);
    }

    offsets[0] = 0;
    for (int v = 0; v < N; ++v)
        offsets[v + 1] = offsets[v] + degree[v];
    memcpy(cursor, offsets, (size_t)N * sizeof(*cursor));
    for (int e = 0; e < M; ++e) {
        int a = ends_a[e], b = ends_b[e];
        neighbors[cursor[a]++] = b;
        neighbors[cursor[b]++] = a;
    }

    free(degree);
    free(tree);
    free(seen);
    free(ends_a);
    free(ends_b);
    free(cursor);
}

static int check_outputs(const int *offsets, const int *neighbors,
                         const int *sources, const int *reference,
                         const int *distance, const int *parent)
{
    for (int s = 0; s < NSOURCES; ++s) {
        size_t base = (size_t)s * N;
        for (int v = 0; v < N; ++v) {
            int d = distance[base + v];
            int p = parent[base + v];
            if (d != reference[base + v]) {
                printf("[check] source=%d vertex=%d distance=%d expected=%d\n",
                       s, v, d, reference[base + v]);
                return 0;
            }
            if (v == sources[s]) {
                if (d != 0 || p != v) {
                    printf("[check] source=%d vertex=%d invalid source parent=%d\n",
                           s, v, p);
                    return 0;
                }
            } else if (d == -1) {
                if (p != -1) {
                    printf("[check] source=%d vertex=%d unreachable parent=%d\n",
                           s, v, p);
                    return 0;
                }
            } else {
                int adjacent = 0;
                if (p >= 0 && p < N && distance[base + p] == d - 1) {
                    for (int e = offsets[v]; e < offsets[v + 1]; ++e) {
                        if (neighbors[e] == p) {
                            adjacent = 1;
                            break;
                        }
                    }
                }
                if (!adjacent) {
                    printf("[check] source=%d vertex=%d distance=%d invalid parent=%d\n",
                           s, v, d, p);
                    return 0;
                }
            }
        }
    }
    printf("[check] all distances and BFS parents valid\n");
    return 1;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "310000") != 0) {
                fprintf(stderr, "only --n 310000 is supported\n");
                return 2;
            }
        } else {
            fprintf(stderr, "usage: %s [--n 310000]\n", argv[0]);
            return 2;
        }
    }

    int *offsets = malloc((size_t)(N + 1) * sizeof(*offsets));
    int *neighbors = malloc((size_t)(2 * M) * sizeof(*neighbors));
    int *reference = malloc((size_t)NSOURCES * N * sizeof(*reference));
    int *reference_parent = malloc((size_t)NSOURCES * N * sizeof(*reference_parent));
    int *distance = malloc((size_t)NSOURCES * N * sizeof(*distance));
    int *parent = malloc((size_t)NSOURCES * N * sizeof(*parent));
    if (!offsets || !neighbors || !reference || !reference_parent ||
        !distance || !parent) {
        fprintf(stderr, "allocation failed\n");
        return 2;
    }

    build_graph(offsets, neighbors);
    int sources[NSOURCES];
    for (int i = 0; i < NSOURCES; ++i)
        sources[i] = (7 + 15427 * i) % N;

    /* Touch every input element before measuring either implementation. */
    volatile uint64_t touched = 0;
    for (int i = 0; i <= N; ++i)
        touched += (unsigned)offsets[i];
    for (int i = 0; i < 2 * M; ++i)
        touched += (unsigned)neighbors[i];
    for (int i = 0; i < NSOURCES; ++i)
        touched += (unsigned)sources[i];
    (void)touched;

    double start = omp_get_wtime();
    breadth_first_search_serial(offsets, neighbors, sources,
                                reference, reference_parent);
    double serial_time = omp_get_wtime() - start;
    printf("[scale] serial seconds=%.6f\n", serial_time);

    int maximum = omp_get_max_threads();
    omp_set_dynamic(0);
    int tested = 0;
    double highest_efficiency = 1.0;
    for (int threads = 1; ; ) {
        omp_set_num_threads(threads);
        start = omp_get_wtime();
        breadth_first_search_omp(offsets, neighbors, sources, distance, parent);
        double elapsed = omp_get_wtime() - start;
        double speedup = serial_time / elapsed;
        highest_efficiency = speedup / threads;
        printf("[scale] threads=%d seconds=%.6f speedup=%.3f efficiency=%.3f\n",
               threads, elapsed, speedup, highest_efficiency);
        if (!tested)
            (void)check_outputs(offsets, neighbors, sources, reference,
                                distance, parent);
        tested = 1;
        if (threads == maximum)
            break;
        threads = threads > maximum / 2 ? maximum : threads * 2;
    }

    int failures = maximum > 1 && highest_efficiency < 0.70;
    free(offsets);
    free(neighbors);
    free(reference);
    free(reference_parent);
    free(distance);
    free(parent);
    printf("[verdict] %s\n", failures ? "PERF-01 exposed" : "PERF-01 not triggered");
    return failures ? 1 : 0;
}
