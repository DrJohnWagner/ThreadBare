#include <stddef.h>
#include <string.h>

void lloyd_kmeans_omp(size_t n, const int *points,
                      const double *initial_centroids, int *assignments,
                      double *centroids)
{
    memcpy(centroids, initial_centroids, 128 * sizeof(*centroids));

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; ++i)
        assignments[i] = -1;

    for (;;) {
        double sums[128] = {0.0};
        size_t counts[64] = {0};
        int changed = 0;

        #pragma omp parallel for schedule(static) reduction(+:counts[:64]) reduction(|:changed)
        for (size_t i = 0; i < n; ++i) {
            double x = (double)points[2 * i];
            double y = (double)points[2 * i + 1];
            int nearest = 0;
            double dx = x - centroids[0];
            double dy = y - centroids[1];
            double best_distance = dx * dx + dy * dy;

            for (int k = 1; k < 64; ++k) {
                dx = x - centroids[2 * k];
                dy = y - centroids[2 * k + 1];
                double distance = dx * dx + dy * dy;
                if (distance < best_distance) {
                    best_distance = distance;
                    nearest = k;
                }
            }

            if (assignments[i] != nearest)
                changed = 1;
            assignments[i] = nearest;
            sums[2 * nearest] += x;
            sums[2 * nearest + 1] += y;
            ++counts[nearest];
        }

        for (int k = 0; k < 64; ++k) {
            if (counts[k] != 0) {
                centroids[2 * k] = sums[2 * k] / (double)counts[k];
                centroids[2 * k + 1] = sums[2 * k + 1] / (double)counts[k];
            }
        }

        if (!changed)
            break;
    }
}
