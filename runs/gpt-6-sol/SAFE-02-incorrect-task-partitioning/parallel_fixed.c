#include <stddef.h>

void lloyd_kmeans_omp(size_t n, const int *points,
                      const double *initial_centroids, int *assignments,
                      size_t *counts, double *centroids)
{
    for (size_t k = 0; k < 64; ++k) {
        centroids[2 * k] = initial_centroids[2 * k];
        centroids[2 * k + 1] = initial_centroids[2 * k + 1];
    }

    for (size_t i = 0; i < n; ++i)
        assignments[i] = -1;

    int changed;
    do {
        double sum_x[64] = {0};
        double sum_y[64] = {0};
        changed = 0;

        for (size_t k = 0; k < 64; ++k)
            counts[k] = 0;

        #pragma omp parallel for reduction(|:changed)
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
        }

        for (size_t i = 0; i < n; ++i) {
            int k = assignments[i];
            ++counts[k];
            sum_x[k] += (double)points[2 * i];
            sum_y[k] += (double)points[2 * i + 1];
        }

        for (size_t k = 0; k < 64; ++k) {
            if (counts[k] != 0) {
                centroids[2 * k] = sum_x[k] / (double)counts[k];
                centroids[2 * k + 1] = sum_y[k] / (double)counts[k];
            }
        }
    } while (changed);
}
