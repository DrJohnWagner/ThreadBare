/* harness.c — timeout driver for sensor_log_ingest (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <omp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct sensor_log_row sensor_log_row;
int sensor_log_ingest_serial(const char *const *paths, size_t nfiles,
                             sensor_log_row *rows, uint64_t *summary);
int sensor_log_ingest_omp(const char *const *paths, size_t nfiles,
                          sensor_log_row *rows, uint64_t *summary);

#define RECORDS_PER_FILE 23000u
#define SENSORS 16u
#define RAW_BYTES (RECORDS_PER_FILE * 3u)
#define ROW_STORAGE_BYTES 128u

static uint32_t next_state(uint32_t *state)
{
    *state = UINT32_C(1664525) * *state + UINT32_C(1013904223);
    return *state;
}

static size_t run_length(const unsigned char *data, size_t pos, size_t len)
{
    size_t count = 1;
    while (count < 130 && pos + count < len &&
           data[pos + count] == data[pos])
        ++count;
    return count;
}

static size_t encode_rle(const unsigned char *raw, size_t len,
                         unsigned char *compressed)
{
    size_t pos = 0, out = 0;
    while (pos < len) {
        size_t run = run_length(raw, pos, len);
        if (run >= 3) {
            compressed[out++] = (unsigned char)(128 + run - 3);
            compressed[out++] = raw[pos];
            pos += run;
        } else {
            size_t start = pos++;
            while (pos < len && pos - start < 128 &&
                   run_length(raw, pos, len) < 3)
                ++pos;
            compressed[out++] = (unsigned char)(pos - start - 1);
            memcpy(compressed + out, raw + start, pos - start);
            out += pos - start;
        }
    }
    return out;
}

static double seconds_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    size_t nfiles = 400, made = 0;
    unsigned timeout_seconds = 8;
    char directory[] = "/tmp/threadbare-sensor-XXXXXX";
    char **paths = NULL;
    unsigned char *raw = NULL, *compressed = NULL;
    void *serial_rows = NULL;
    uint64_t serial_summary[3] = {0, 0, 0};
    uint64_t expected_sum = 0;
    int failures = 0, setup_ok = 1, directory_created = 0;

    for (int a = 1; a < argc; ++a) {
        if (strcmp(argv[a], "--n") == 0 && a + 1 < argc) {
            char *end;
            unsigned long v = strtoul(argv[++a], &end, 10);
            if (*end || v < 3 || v > 10000) setup_ok = 0;
            else nfiles = (size_t)v;
        } else if (strcmp(argv[a], "--timeout") == 0 && a + 1 < argc) {
            char *end;
            unsigned long v = strtoul(argv[++a], &end, 10);
            if (*end || v < 1 || v > 3600) setup_ok = 0;
            else timeout_seconds = (unsigned)v;
        } else {
            setup_ok = 0;
        }
    }
    if (!setup_ok) {
        fprintf(stderr, "usage: %s [--n 3..10000] [--timeout seconds]\n", argv[0]);
        goto cleanup;
    }

    paths = calloc(nfiles, sizeof(*paths));
    raw = malloc(RAW_BYTES);
    compressed = malloc(2 * RAW_BYTES + 256);
    serial_rows = calloc(nfiles * SENSORS, ROW_STORAGE_BYTES);
    if (!paths || !raw || !compressed || !serial_rows || !mkdtemp(directory)) {
        perror("input setup");
        goto cleanup;
    }
    directory_created = 1;

    for (size_t f = 0; f < nfiles; ++f) {
        uint32_t state = UINT32_C(0x9e3779b9) ^
                         ((uint32_t)(f + 1) * UINT32_C(0x85ebca6b));
        for (size_t r = 0; r < RECORDS_PER_FILE; ++r) {
            unsigned sensor = (next_state(&state) >> 16) % SENSORS;
            unsigned value = (next_state(&state) >> 16) % 1000u;
            raw[3 * r] = (unsigned char)sensor;
            raw[3 * r + 1] = (unsigned char)value;
            raw[3 * r + 2] = (unsigned char)(value >> 8);
            expected_sum += value;
        }
        size_t length = encode_rle(raw, RAW_BYTES, compressed);
        paths[f] = malloc(strlen(directory) + 40);
        if (!paths[f]) {
            perror("path allocation");
            goto cleanup;
        }
        snprintf(paths[f], strlen(directory) + 40, "%s/%zu.bin", directory, f);
        FILE *file = fopen(paths[f], "wb");
        if (!file) {
            perror("file creation");
            goto cleanup;
        }
        ++made;
        int written = fwrite(compressed, 1, length, file) == length;
        if (fclose(file) != 0 || !written) {
            perror("file write");
            goto cleanup;
        }
    }

    int serial_status = sensor_log_ingest_serial((const char *const *)paths,
                                                 nfiles, serial_rows,
                                                 serial_summary);
    printf("[check] serial returned %d; summary=(%" PRIu64 ",%" PRIu64
           ",%" PRIu64 "); expected=(%" PRIu64 ",%" PRIu64 ",%" PRIu64 ")\n",
           serial_status, serial_summary[0], serial_summary[1],
           serial_summary[2], (uint64_t)nfiles * SENSORS,
           (uint64_t)nfiles * RECORDS_PER_FILE, expected_sum);
    fflush(stdout);

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        goto cleanup;
    }
    if (child == 0) {
        uint64_t summary[3] = {0, 0, 0};
        void *rows = calloc(nfiles * SENSORS, ROW_STORAGE_BYTES);
        if (!rows) _exit(2);
        omp_set_dynamic(0);
        omp_set_num_threads(2);
        int status = sensor_log_ingest_omp((const char *const *)paths,
                                           nfiles, rows, summary);
        printf("[check] parallel returned %d; summary=(%" PRIu64 ",%" PRIu64
               ",%" PRIu64 ")\n", status, summary[0], summary[1], summary[2]);
        fflush(stdout);
        free(rows);
        _exit(0);
    }

    double deadline = seconds_now() + timeout_seconds;
    int status = 0;
    for (;;) {
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            printf("[check] parallel process exited before timeout (status %d)\n",
                   WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            break;
        }
        if (result < 0 && errno != EINTR) {
            perror("waitpid");
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            break;
        }
        if (seconds_now() >= deadline) {
            failures = 1;
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            printf("[check] parallel call exceeded %u-second wall-clock timeout with 2 threads\n",
                   timeout_seconds);
            break;
        }
        struct timespec pause_time = {0, 10000000L};
        nanosleep(&pause_time, NULL);
    }

cleanup:
    for (size_t f = 0; f < made; ++f)
        if (paths[f]) unlink(paths[f]);
    if (directory_created) rmdir(directory);
    if (paths) {
        for (size_t f = 0; f < nfiles; ++f) free(paths[f]);
    }
    free(paths);
    free(raw);
    free(compressed);
    free(serial_rows);
    printf("[verdict] %s\n", failures ? "LIVE-01 exposed" : "LIVE-01 not triggered");
    return failures ? 1 : 0;
}
