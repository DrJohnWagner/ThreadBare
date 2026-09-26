#include <stddef.h>

void lloyd_kmeans_serial(size_t n, const double *points,
                         const double *initial_centroids,
                         int *assignments, double *centroids)
{
    for (size_t i = 0; i < n; ++i)
        assignments[i] = -1;

    for (int c = 0; c < 64; ++c) {
        centroids[2 * c] = initial_centroids[2 * c];
        centroids[2 * c + 1] = initial_centroids[2 * c + 1];
    }

    for (;;) {
        double sums[64][2] = {{0.0}};
        size_t counts[64] = {0};
        int changed = 0;

        for (size_t i = 0; i < n; ++i) {
            double x = points[2 * i];
            double y = points[2 * i + 1];
            int nearest = 0;
            double dx = x - centroids[0];
            double dy = y - centroids[1];
            double minimum = dx * dx + dy * dy;

            for (int c = 1; c < 64; ++c) {
                dx = x - centroids[2 * c];
                dy = y - centroids[2 * c + 1];
                double distance = dx * dx + dy * dy;
                if (distance < minimum) {
                    minimum = distance;
                    nearest = c;
                }
            }

            if (assignments[i] != nearest)
                changed = 1;
            assignments[i] = nearest;
            sums[nearest][0] += x;
            sums[nearest][1] += y;
            ++counts[nearest];
        }

        for (int c = 0; c < 64; ++c) {
            if (counts[c] != 0) {
                centroids[2 * c] = sums[c][0] / (double)counts[c];
                centroids[2 * c + 1] = sums[c][1] / (double)counts[c];
            }
        }

        if (!changed)
            break;
    }
}
