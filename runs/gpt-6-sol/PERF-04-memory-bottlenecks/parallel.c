#include <stddef.h>

void lloyd_kmeans_omp(size_t n, const double *points,
                      const double *initial_centroids, int *assignments,
                      double *final_centroids, int *iterations)
{
    for (size_t c = 0; c < 64; ++c) {
        final_centroids[2 * c] = initial_centroids[2 * c];
        final_centroids[2 * c + 1] = initial_centroids[2 * c + 1];
    }

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; ++i)
        assignments[i] = -1;

    *iterations = 0;
    int changed;
    do {
        double sums[128] = {0.0};
        size_t counts[64] = {0};
        changed = 0;

        #pragma omp parallel for schedule(static,1) reduction(+:sums[:128], counts[:64]) reduction(|:changed)
        for (size_t i = 0; i < n; ++i) {
            double x = points[2 * i];
            double y = points[2 * i + 1];
            int nearest = 0;
            double dx = x - final_centroids[0];
            double dy = y - final_centroids[1];
            double best_distance = dx * dx + dy * dy;

            for (int c = 1; c < 64; ++c) {
                dx = x - final_centroids[2 * c];
                dy = y - final_centroids[2 * c + 1];
                double distance = dx * dx + dy * dy;
                if (distance < best_distance) {
                    best_distance = distance;
                    nearest = c;
                }
            }

            if (assignments[i] != nearest)
                changed = 1;
            assignments[i] = nearest;
            sums[2 * nearest] += x;
            sums[2 * nearest + 1] += y;
            ++counts[nearest];
        }

        for (size_t c = 0; c < 64; ++c) {
            if (counts[c] != 0) {
                final_centroids[2 * c] = sums[2 * c] / (double)counts[c];
                final_centroids[2 * c + 1] = sums[2 * c + 1] / (double)counts[c];
            }
        }
        ++*iterations;
    } while (changed);
}
