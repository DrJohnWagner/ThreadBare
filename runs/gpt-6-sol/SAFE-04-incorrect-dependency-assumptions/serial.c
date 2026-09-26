#include <stddef.h>

void lloyds_k_means_serial(const double *points, size_t point_count,
                          const double *initial_centroids, int *assignments,
                          double *centroids)
{
    for (size_t c = 0; c < 64; ++c) {
        centroids[2 * c] = initial_centroids[2 * c];
        centroids[2 * c + 1] = initial_centroids[2 * c + 1];
    }

    for (size_t i = 0; i < point_count; ++i)
        assignments[i] = -1;

    for (;;) {
        double sum_x[64] = {0};
        double sum_y[64] = {0};
        size_t counts[64] = {0};
        int changed = 0;

        for (size_t i = 0; i < point_count; ++i) {
            double x = points[2 * i];
            double y = points[2 * i + 1];
            int nearest = 0;
            double dx = x - centroids[0];
            double dy = y - centroids[1];
            double best_distance = dx * dx + dy * dy;

            for (int c = 1; c < 64; ++c) {
                dx = x - centroids[2 * c];
                dy = y - centroids[2 * c + 1];
                double distance = dx * dx + dy * dy;
                if (distance < best_distance) {
                    best_distance = distance;
                    nearest = c;
                }
            }

            if (assignments[i] != nearest) {
                assignments[i] = nearest;
                changed = 1;
            }
            sum_x[nearest] += x;
            sum_y[nearest] += y;
            ++counts[nearest];
        }

        for (size_t c = 0; c < 64; ++c) {
            if (counts[c] != 0) {
                centroids[2 * c] = sum_x[c] / (double)counts[c];
                centroids[2 * c + 1] = sum_y[c] / (double)counts[c];
            }
        }

        if (!changed)
            break;
    }
}
