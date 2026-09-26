/* harness.c — timeout driver for sensor_log_ingest (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void sensor_log_ingest_serial(const unsigned char *const *compressed_files,
                              const size_t *compressed_sizes, size_t file_count,
                              size_t records_per_file, int64_t *rows,
                              int64_t *summary);
void sensor_log_ingest_omp(const unsigned char *const *compressed_files,
                           const size_t *compressed_sizes, size_t file_count,
                           size_t records_per_file, int64_t *rows,
                           int64_t *summary);

static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static double seconds_now(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(2);
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static unsigned parse_positive(const char *text, unsigned limit)
{
    char *end;
    unsigned long value;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno || end == text || *end != '\0' || value < 1 || value > limit) {
        fprintf(stderr, "invalid positive integer: %s\n", text);
        exit(2);
    }
    return (unsigned)value;
}

int main(int argc, char **argv)
{
    const size_t records_per_file = 23000;
    size_t file_count = 200;
    unsigned timeout_seconds = 12;
    const unsigned char **files;
    size_t *sizes;
    int64_t *serial_rows, *parallel_rows;
    int64_t serial_summary[4] = {0, 0, 0, 0};
    int64_t parallel_summary[4] = {0, 0, 0, 0};
    pid_t child;
    int status = 0, failures = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc)
            file_count = parse_positive(argv[++i], 800);
        else if (strncmp(argv[i], "--n=", 4) == 0)
            file_count = parse_positive(argv[i] + 4, 800);
        else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
            timeout_seconds = parse_positive(argv[++i], 120);
        else if (strncmp(argv[i], "--timeout=", 10) == 0)
            timeout_seconds = parse_positive(argv[i] + 10, 120);
        else {
            fprintf(stderr, "usage: %s [--n 1..800] [--timeout 1..120]\n", argv[0]);
            return 2;
        }
    }

    files = calloc(file_count, sizeof(*files));
    sizes = calloc(file_count, sizeof(*sizes));
    /* Space for at least four int64_t values per output row. */
    serial_rows = calloc(file_count * 16 * 4, sizeof(*serial_rows));
    parallel_rows = calloc(file_count * 16 * 4, sizeof(*parallel_rows));
    if (!files || !sizes || !serial_rows || !parallel_rows) {
        fprintf(stderr, "allocation failed\n");
        return 2;
    }

    for (size_t f = 0; f < file_count; ++f) {
        unsigned char *data = malloc(records_per_file * 4);
        size_t remaining = records_per_file, used = 0;
        uint32_t state = UINT32_C(0x9E3779B9) ^
                         ((uint32_t)(f + 1) * UINT32_C(0x85EBCA6B));
        if (!data) {
            fprintf(stderr, "input allocation failed\n");
            return 2;
        }
        if (state == 0) state = 1;
        while (remaining) {
            size_t run = 1 + next_random(&state) % 4;
            uint8_t sensor;
            int16_t reading;
            if (run > remaining) run = remaining;
            sensor = (uint8_t)(next_random(&state) % 16);
            reading = (int16_t)((int32_t)(next_random(&state) % 2001) - 1000);
            data[used++] = (unsigned char)run;
            data[used++] = sensor;
            data[used++] = (unsigned char)((uint16_t)reading & 255u);
            data[used++] = (unsigned char)((uint16_t)reading >> 8);
            remaining -= run;
        }
        files[f] = data;
        sizes[f] = used;
    }

    sensor_log_ingest_serial(files, sizes, file_count, records_per_file,
                             serial_rows, serial_summary);

    child = fork();
    if (child < 0) {
        perror("fork");
        return 2;
    }
    if (child == 0) {
        sensor_log_ingest_omp(files, sizes, file_count, records_per_file,
                              parallel_rows, parallel_summary);
        _exit(memcmp(serial_summary, parallel_summary,
                     sizeof(serial_summary)) == 0 ? 0 : 3);
    }

    {
        double deadline = seconds_now() + timeout_seconds;
        for (;;) {
            pid_t result = waitpid(child, &status, WNOHANG);
            if (result == child) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                    printf("[check] parallel call completed; summary matches reference\n");
                else
                    printf("[check] parallel call exited without matching the reference summary\n");
                break;
            }
            if (result < 0 && errno != EINTR) {
                perror("waitpid");
                return 2;
            }
            if (seconds_now() >= deadline) {
                kill(child, SIGKILL);
                while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                printf("[check] parallel call exceeded %u-second wall-clock timeout\n",
                       timeout_seconds);
                failures = 1;
                break;
            }
            struct timespec pause_time = {0, 10000000L};
            nanosleep(&pause_time, NULL);
        }
    }

    for (size_t f = 0; f < file_count; ++f) free((void *)files[f]);
    free(files);
    free(sizes);
    free(serial_rows);
    free(parallel_rows);
    printf("[verdict] %s\n", failures ? "LIVE-01 exposed" : "LIVE-01 not triggered");
    return failures ? 1 : 0;
}