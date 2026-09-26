#include <stddef.h>

void lloyd_kmeans_serial(const double *points, size_t n,
                         const double *initial_centroids, int *assignments,
                         double *centroids)
{
    for (size_t c = 0; c < 64; ++c) {
        centroids[2 * c] = initial_centroids[2 * c];
        centroids[2 * c + 1] = initial_centroids[2 * c + 1];
    }
    for (size_t i = 0; i < n; ++i)
        assignments[i] = -1;

    int changed;
    do {
        double sums[64][2] = {{0.0}};
        size_t counts[64] = {0};
        changed = 0;

        for (size_t i = 0; i < n; ++i) {
            double dx = points[2 * i] - centroids[0];
            double dy = points[2 * i + 1] - centroids[1];
            double best_distance = dx * dx + dy * dy;
            int best = 0;

            for (int c = 1; c < 64; ++c) {
                dx = points[2 * i] - centroids[2 * c];
                dy = points[2 * i + 1] - centroids[2 * c + 1];
                double distance = dx * dx + dy * dy;
                if (distance < best_distance) {
                    best_distance = distance;
                    best = c;
                }
            }

            if (assignments[i] != best) {
                assignments[i] = best;
                changed = 1;
            }
            sums[best][0] += points[2 * i];
            sums[best][1] += points[2 * i + 1];
            ++counts[best];
        }

        for (size_t c = 0; c < 64; ++c) {
            if (counts[c] != 0) {
                centroids[2 * c] = sums[c][0] / (double)counts[c];
                centroids[2 * c + 1] = sums[c][1] / (double)counts[c];
            }
        }
    } while (changed);
}
