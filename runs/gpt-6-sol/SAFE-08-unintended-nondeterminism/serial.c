#include <stdint.h>
#include <stdlib.h>

static uint64_t splitmix64_next(uint64_t *state)
{
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

void breadth_first_search_serial(int *distance, int *parent)
{
    enum { V = 310000, M = 5000000, SEARCHES = 20 };
    int *head = malloc((size_t)V * sizeof *head);
    int *next = malloc((size_t)M * sizeof *next);
    int *destination = malloc((size_t)M * sizeof *destination);
    int *queue = malloc((size_t)V * sizeof *queue);
    uint64_t state = UINT64_C(0x123456789abcdef0);

    if (head == NULL || next == NULL || destination == NULL || queue == NULL) {
        free(head);
        free(next);
        free(destination);
        free(queue);
        abort();
    }

    for (int v = 0; v < V; ++v)
        head[v] = -1;

    for (int v = 0; v < V; ++v) {
        destination[v] = (v + 1) % V;
        next[v] = head[v];
        head[v] = v;
    }

    for (int e = V; e < M; ++e) {
        int from = (int)(splitmix64_next(&state) % (uint64_t)V);
        uint32_t x = (uint32_t)(splitmix64_next(&state) >> 32);
        uint32_t r = x;
        for (int i = 0; i < 3; ++i)
            r = (uint32_t)(((uint64_t)r * x) >> 32);
        destination[e] = (int)(((uint64_t)V * r) >> 32);
        next[e] = head[from];
        head[from] = e;
    }

    for (int k = 0; k < SEARCHES; ++k) {
        int *dist = distance + (size_t)k * V;
        int *par = parent + (size_t)k * V;
        int source = k * V / SEARCHES;
        int front = 0;
        int back = 0;

        for (int v = 0; v < V; ++v) {
            dist[v] = -1;
            par[v] = -1;
        }
        dist[source] = 0;
        queue[back++] = source;

        while (front < back) {
            int from = queue[front++];
            int candidate_distance = dist[from] + 1;
            for (int e = head[from]; e != -1; e = next[e]) {
                int to = destination[e];
                if (dist[to] == -1) {
                    dist[to] = candidate_distance;
                    par[to] = from;
                    queue[back++] = to;
                } else if (dist[to] == candidate_distance && from < par[to]) {
                    par[to] = from;
                }
            }
        }
    }

    free(queue);
    free(destination);
    free(next);
    free(head);
}
