/* harness.c — scaling driver for sensor_log_ingest (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <omp.h>

struct sensor_log_row;
struct sensor_log_summary;

int sensor_log_ingest_serial(const char *const *file_paths, size_t file_count,
                             struct sensor_log_row *rows,
                             struct sensor_log_summary *summary);
int sensor_log_ingest_omp(const char *const *file_paths, size_t file_count,
                          struct sensor_log_row *rows,
                          struct sensor_log_summary *summary);

#define GROUPS 5750u
#define SENSORS 16u
#define ROW_STORAGE_BYTES 128u
#define SUMMARY_STORAGE_BYTES 256u
#define SUMMARY_CHECK_BYTES 32u

static int make_input(char **paths, size_t n, const char *directory)
{
    unsigned char tokens[GROUPS * 4u];

    for (size_t f = 0; f < n; ++f) {
        size_t length = strlen(directory) + 40u;
        paths[f] = malloc(length);
        if (!paths[f]) return -1;
        snprintf(paths[f], length, "%s/file_%06zu.bin", directory, f);

        uint32_t state = UINT32_C(0x12345678) ^
                         (UINT32_C(0x9e3779b9) * (uint32_t)(f + 1u));
        for (size_t g = 0; g < GROUPS; ++g) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            uint16_t reading = (uint16_t)((state >> 8) & 4095u);
            tokens[4u * g] = 4u;
            tokens[4u * g + 1u] = (unsigned char)((state >> 24) & 15u);
            tokens[4u * g + 2u] = (unsigned char)reading;
            tokens[4u * g + 3u] = (unsigned char)(reading >> 8);
        }

        FILE *file = fopen(paths[f], "wb");
        if (!file) return -1;
        int ok = fwrite(tokens, 1, sizeof tokens, file) == sizeof tokens;
        if (fclose(file) != 0) ok = 0;
        if (!ok) return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    size_t n = 800u;
    int failures = 0;
    char directory[] = "/tmp/threadbare_sensor_XXXXXX";
    char **paths = NULL;
    void *serial_rows = NULL, *parallel_rows = NULL;
    void *serial_summary = NULL, *parallel_summary = NULL;
    int directory_created = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            char *end;
            errno = 0;
            unsigned long value = strtoul(argv[++i], &end, 10);
            if (errno || *end || value == 0 || value > 800u) {
                fprintf(stderr, "--n must be an integer from 1 to 800\n");
                failures = 1;
                goto done;
            }
            n = (size_t)value;
        } else {
            fprintf(stderr, "usage: %s [--n 1..800]\n", argv[0]);
            failures = 1;
            goto done;
        }
    }

    paths = calloc(n, sizeof *paths);
    serial_rows = calloc(n * SENSORS, ROW_STORAGE_BYTES);
    parallel_rows = calloc(n * SENSORS, ROW_STORAGE_BYTES);
    serial_summary = calloc(1, SUMMARY_STORAGE_BYTES);
    parallel_summary = calloc(1, SUMMARY_STORAGE_BYTES);
    if (!paths || !serial_rows || !parallel_rows || !serial_summary ||
        !parallel_summary || !mkdtemp(directory)) {
        perror("setup");
        failures = 1;
        goto done;
    }
    directory_created = 1;
    if (make_input(paths, n, directory) != 0) {
        perror("generate input");
        failures = 1;
        goto done;
    }

    /* Untimed pass reads every generated file and first-touches result pages. */
    int warmup = sensor_log_ingest_serial((const char *const *)paths, n,
                                          serial_rows, serial_summary);
    if (warmup != 0) {
        fprintf(stderr, "[check] serial warmup returned %d\n", warmup);
        failures = 1;
        goto done;
    }

    memset(serial_summary, 0, SUMMARY_STORAGE_BYTES);
    double start = omp_get_wtime();
    int reference_status = sensor_log_ingest_serial((const char *const *)paths, n,
                                                     serial_rows, serial_summary);
    double serial_seconds = omp_get_wtime() - start;
    if (reference_status != 0 || serial_seconds <= 0.0) {
        fprintf(stderr, "[check] serial reference returned %d\n", reference_status);
        failures = 1;
        goto done;
    }

    int max_threads = omp_get_max_threads();
    if (max_threads < 1) max_threads = 1;
    int threads = 1;
    for (;;) {
        omp_set_num_threads(threads);
        memset(parallel_summary, 0, SUMMARY_STORAGE_BYTES);
        start = omp_get_wtime();
        int parallel_status = sensor_log_ingest_omp((const char *const *)paths, n,
                                                    parallel_rows, parallel_summary);
        double parallel_seconds = omp_get_wtime() - start;
        double speedup = parallel_seconds > 0.0
                             ? serial_seconds / parallel_seconds : 0.0;
        double efficiency = speedup / (double)threads;
        printf("[scale] threads=%d serial=%.6f parallel=%.6f speedup=%.3f efficiency=%.3f\n",
               threads, serial_seconds, parallel_seconds, speedup, efficiency);
        fflush(stdout);

        if (threads == 1) {
            int match = parallel_status == reference_status &&
                        memcmp(serial_summary, parallel_summary,
                               SUMMARY_CHECK_BYTES) == 0;
            printf("[check] return status and aggregate summary: %s\n",
                   match ? "match" : "mismatch");
            if (!match) failures = 1;
        }
        if (threads == max_threads) {
            if (efficiency < 0.65) failures = 1;
            break;
        }
        threads = threads > max_threads / 2 ? max_threads : threads * 2;
    }

done:
    if (paths) {
        for (size_t f = 0; f < n; ++f) {
            if (paths[f]) {
                unlink(paths[f]);
                free(paths[f]);
            }
        }
    }
    if (directory_created) rmdir(directory);
    free(paths);
    free(serial_rows);
    free(parallel_rows);
    free(serial_summary);
    free(parallel_summary);
    printf("[verdict] %s\n", failures ? "PERF-01 exposed" : "PERF-01 not triggered");
    return failures ? 1 : 0;
}
