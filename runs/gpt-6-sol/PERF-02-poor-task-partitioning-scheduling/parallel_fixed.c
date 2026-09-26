#include <stdlib.h>

enum { VERTEX_COUNT = 310000, SOURCE_COUNT = 20 };

void breadth_first_search_omp(const int *offsets, const int *neighbors,
                              const int *sources, int *distance, int *parent)
{
    #pragma omp parallel for schedule(dynamic, 1)
    for (int s = 0; s < SOURCE_COUNT; ++s) {
        int *queue = malloc((size_t)VERTEX_COUNT * sizeof(*queue));
        if (queue == NULL) {
            abort();
        }

        int base = s * VERTEX_COUNT;
        int source = sources[s];
        int head = 0;
        int tail = 0;

        for (int v = 0; v < VERTEX_COUNT; ++v) {
            distance[base + v] = -1;
            parent[base + v] = -1;
        }

        distance[base + source] = 0;
        parent[base + source] = source;
        queue[tail++] = source;

        while (head < tail) {
            int v = queue[head++];
            for (int i = offsets[v]; i < offsets[v + 1]; ++i) {
                int neighbor = neighbors[i];
                if (distance[base + neighbor] == -1) {
                    distance[base + neighbor] = distance[base + v] + 1;
                    parent[base + neighbor] = v;
                    queue[tail++] = neighbor;
                }
            }
        }

        free(queue);
    }
}