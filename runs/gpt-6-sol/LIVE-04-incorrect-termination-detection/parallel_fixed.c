#include <stdint.h>
#include <stdlib.h>

static uint64_t next_random(uint64_t *state)
{
    uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

int breadth_first_search_omp(int *sources, int *distance, int *parent)
{
    enum { vertices = 310000, edges = 5000000, searches = 20 };
    uint64_t state = UINT64_C(0x243F6A8885A308D3);
    int *reservoir = malloc(2 * (size_t)edges * sizeof *reservoir);
    int *degree = calloc(vertices, sizeof *degree);
    int *offset = malloc((vertices + 1) * sizeof *offset);
    int *position = malloc(vertices * sizeof *position);
    int *neighbor = malloc(2 * (size_t)edges * sizeof *neighbor);
    int *queues = malloc((size_t)searches * vertices * sizeof *queues);
    size_t length = 0;

    if (!reservoir || !degree || !offset || !position || !neighbor || !queues) {
        free(reservoir);
        free(degree);
        free(offset);
        free(position);
        free(neighbor);
        free(queues);
        return -1;
    }

    for (int u = 0; u < 17; ++u) {
        for (int v = u + 1; v < 17; ++v) {
            reservoir[length++] = u;
            reservoir[length++] = v;
            ++degree[u];
            ++degree[v];
        }
    }

    for (int v = 17; v < vertices; ++v) {
        size_t eligible = length;
        int selected[17];
        int count = 0;
        int required = v < 17 + 40136 ? 17 : 16;

        while (count < required) {
            int u = reservoir[next_random(&state) % eligible];
            int duplicate = 0;
            for (int i = 0; i < count; ++i) {
                if (selected[i] == u) {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate)
                continue;

            selected[count++] = u;
            reservoir[length++] = v;
            reservoir[length++] = u;
            ++degree[v];
            ++degree[u];
        }
    }

    for (int i = 0; i < searches; ++i) {
        int candidate;
        int duplicate;
        do {
            candidate = (int)(next_random(&state) % vertices);
            duplicate = 0;
            for (int j = 0; j < i; ++j) {
                if (sources[j] == candidate) {
                    duplicate = 1;
                    break;
                }
            }
        } while (duplicate);
        sources[i] = candidate;
    }

    offset[0] = 0;
    for (int v = 0; v < vertices; ++v) {
        offset[v + 1] = offset[v] + degree[v];
        position[v] = offset[v];
    }
    for (size_t i = 0; i < length; i += 2) {
        int u = reservoir[i];
        int v = reservoir[i + 1];
        neighbor[position[u]++] = v;
        neighbor[position[v]++] = u;
    }
    free(reservoir);
    free(degree);
    free(position);

    #pragma omp parallel for schedule(dynamic, 1)
    for (int run = 0; run < searches; ++run) {
        int *run_distance = distance + (size_t)run * vertices;
        int *run_parent = parent + (size_t)run * vertices;
        int *queue = queues + (size_t)run * vertices;
        int head = 0;
        int tail = 0;
        int source = sources[run];

        for (int v = 0; v < vertices; ++v) {
            run_distance[v] = -1;
            run_parent[v] = -1;
        }
        run_distance[source] = 0;
        run_parent[source] = source;
        queue[tail++] = source;

        while (head < tail) {
            int u = queue[head++];
            int next_distance = run_distance[u] + 1;
            for (int i = offset[u]; i < offset[u + 1]; ++i) {
                int v = neighbor[i];
                if (run_distance[v] == -1) {
                    run_distance[v] = next_distance;
                    run_parent[v] = u;
                    queue[tail++] = v;
                } else if (run_distance[v] == next_distance && u < run_parent[v]) {
                    run_parent[v] = u;
                }
            }
        }
    }

    free(offset);
    free(neighbor);
    free(queues);
    return 0;
}
