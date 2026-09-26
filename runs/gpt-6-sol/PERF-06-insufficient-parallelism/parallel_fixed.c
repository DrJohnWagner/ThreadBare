#include <stddef.h>
#include <omp.h>

void lloyd_kmeans_omp(size_t n, const double *points,
                      const double *initial_centroids,
                      int *assignments, double *centroids)
{
    for (size_t j = 0; j < 128; ++j) {
        centroids[j] = initial_centroids[j];
    }

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; ++i) {
        assignments[i] = -1;
    }

    int changed;
    do {
        double sums[128] = {0};
        size_t counts[64] = {0};
        changed = 0;

        #pragma omp parallel for schedule(static) reduction(+:sums[:128], counts[:64]) reduction(|:changed)
        for (size_t i = 0; i < n; ++i) {
            double x = points[2 * i];
            double y = points[2 * i + 1];
            int nearest = 0;
            double dx = x - centroids[0];
            double dy = y - centroids[1];
            double best_distance = dx * dx + dy * dy;

            for (int j = 1; j < 64; ++j) {
                dx = x - centroids[2 * j];
                dy = y - centroids[2 * j + 1];
                double distance = dx * dx + dy * dy;
                if (distance < best_distance) {
                    best_distance = distance;
                    nearest = j;
                }
            }

            if (assignments[i] != nearest) {
                assignments[i] = nearest;
                changed = 1;
            }
            sums[2 * nearest] += x;
            sums[2 * nearest + 1] += y;
            ++counts[nearest];
        }

        for (int j = 0; j < 64; ++j) {
            if (counts[j] != 0) {
                centroids[2 * j] = sums[2 * j] / (double)counts[j];
                centroids[2 * j + 1] = sums[2 * j + 1] / (double)counts[j];
            }
        }
    } while (changed);
}
