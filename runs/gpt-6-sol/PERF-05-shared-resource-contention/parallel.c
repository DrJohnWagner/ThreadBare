#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

enum { VERTICES = 310000, EDGES = 5000000, SOURCES = 20 };

static uint64_t next_random(uint64_t *state)
{
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return *state * UINT64_C(2685821657736338717);
}

static void tree_add(uint32_t *tree, uint32_t vertex, uint32_t amount)
{
    for (uint32_t i = vertex + 1; i <= VERTICES; i += i & -i)
        tree[i] += amount;
}

static uint32_t tree_select(const uint32_t *tree, uint32_t rank)
{
    uint32_t index = 0;
    uint32_t bit = 1;
    while (bit <= VERTICES / 2)
        bit <<= 1;

    while (bit != 0) {
        uint32_t next = index + bit;
        if (next <= VERTICES && tree[next] <= rank) {
            rank -= tree[next];
            index = next;
        }
        bit >>= 1;
    }
    return index;
}

int multi_source_bfs_omp(int *distance, int *parent)
{
    uint32_t *edge_u = malloc((size_t)EDGES * sizeof *edge_u);
    uint32_t *edge_v = malloc((size_t)EDGES * sizeof *edge_v);
    uint32_t *offset = calloc((size_t)VERTICES + 1, sizeof *offset);
    uint32_t *tree = calloc((size_t)VERTICES + 1, sizeof *tree);
    uint32_t *neighbors = malloc((size_t)2 * EDGES * sizeof *neighbors);

    if (!edge_u || !edge_v || !offset || !tree || !neighbors) {
        free(edge_u);
        free(edge_v);
        free(offset);
        free(tree);
        free(neighbors);
        return -1;
    }

    uint64_t state = UINT64_C(0x9E3779B97F4A7C15);
    uint32_t weight = 1;
    uint32_t count = 0;
    tree_add(tree, 0, 1);

    for (uint32_t v = 1; v < VERTICES; ++v) {
        uint32_t connections = v < 16 ? v : 16;
        for (uint32_t j = 0; j < connections; ++j) {
            uint32_t u = tree_select(tree, (uint32_t)(next_random(&state) % weight));
            edge_u[count] = v;
            edge_v[count] = u;
            ++count;
            tree_add(tree, u, 1);
            ++weight;
        }
        tree_add(tree, v, connections + 1);
        weight += connections + 1;
    }

    while (count < EDGES) {
        uint32_t u = tree_select(tree, (uint32_t)(next_random(&state) % weight));
        uint32_t v;
        do {
            v = tree_select(tree, (uint32_t)(next_random(&state) % weight));
        } while (v == u);
        edge_u[count] = u;
        edge_v[count] = v;
        ++count;
        tree_add(tree, u, 1);
        tree_add(tree, v, 1);
        weight += 2;
    }

    for (uint32_t e = 0; e < EDGES; ++e) {
        ++offset[edge_u[e] + 1];
        ++offset[edge_v[e] + 1];
    }
    for (uint32_t v = 0; v < VERTICES; ++v)
        offset[v + 1] += offset[v];

    for (uint32_t v = 0; v < VERTICES; ++v)
        tree[v] = offset[v];
    for (uint32_t e = 0; e < EDGES; ++e) {
        uint32_t u = edge_u[e];
        uint32_t v = edge_v[e];
        neighbors[tree[u]++] = v;
        neighbors[tree[v]++] = u;
    }
    free(edge_u);
    free(edge_v);
    free(tree);

    FILE *stream = fopen("/dev/null", "w");
    if (!stream) {
        free(offset);
        free(neighbors);
        return -1;
    }
    int failed = 0;
    #pragma omp parallel for schedule(dynamic, 1) reduction(|:failed)
    for (int i = 0; i < SOURCES; ++i) {
        uint32_t *queue = malloc((size_t)VERTICES * sizeof *queue);
        if (!queue) {
            failed = 1;
            continue;
        }

        int *dist = distance + (size_t)i * VERTICES;
        int *pred = parent + (size_t)i * VERTICES;
        for (uint32_t v = 0; v < VERTICES; ++v) {
            dist[v] = -1;
            pred[v] = -1;
        }

        uint32_t source = (uint32_t)(i * VERTICES / SOURCES);
        uint32_t head = 0;
        uint32_t tail = 0;
        dist[source] = 0;
        queue[tail++] = source;

        while (head < tail) {
            uint32_t u = queue[head++];
            fprintf(stream, "%u\n", u);
            for (uint32_t p = offset[u]; p < offset[u + 1]; ++p) {
                uint32_t v = neighbors[p];
                if (dist[v] == -1) {
                    dist[v] = dist[u] + 1;
                    pred[v] = (int)u;
                    queue[tail++] = v;
                }
            }
        }
        free(queue);
    }

    fclose(stream);
    free(offset);
    free(neighbors);
    return failed ? -1 : 0;
}
