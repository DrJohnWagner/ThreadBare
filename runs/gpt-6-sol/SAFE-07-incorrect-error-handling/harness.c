/* harness.c — differential driver for sensor_log_ingest (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

typedef struct {
    unsigned char *data;
    size_t size;
} sensor_log_file;

typedef struct {
    size_t file_index;
    uint16_t sensor_id;
    uint64_t count;
    uint64_t value_sum;
} sensor_log_row;

int sensor_log_ingest_serial(const sensor_log_file *files, size_t file_count,
                             sensor_log_row *rows);
int sensor_log_ingest_omp(const sensor_log_file *files, size_t file_count,
                          sensor_log_row *rows);

enum { RECORDS_PER_FILE = 23000, SENSORS = 32, MAX_FILES = 200 };

static uint32_t next_value(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x & 1023u;
}

static size_t encode(const unsigned char *src, size_t len, unsigned char *dst)
{
    size_t i = 0, out = 0;
    while (i < len) {
        size_t run = 1;
        while (run < 128 && i + run < len && src[i + run] == src[i])
            ++run;
        if (run >= 3) {
            dst[out++] = (unsigned char)(0x80u | (unsigned)(run - 1));
            dst[out++] = src[i];
            i += run;
        } else {
            size_t start = i;
            do {
                ++i;
                if (i == len || i - start == 128)
                    break;
                run = 1;
                while (run < 3 && i + run < len && src[i + run] == src[i])
                    ++run;
            } while (run < 3);
            dst[out++] = (unsigned char)(i - start - 1);
            memcpy(dst + out, src + start, i - start);
            out += i - start;
        }
    }
    return out;
}

static int make_files(sensor_log_file *files, size_t n)
{
    const size_t raw_size = (size_t)RECORDS_PER_FILE * 4;
    unsigned char *raw = malloc(raw_size);
    uint32_t state = 0x12345678u;
    if (!raw)
        return 0;
    for (size_t f = 0; f < n; ++f) {
        for (size_t i = 0; i < RECORDS_PER_FILE; ++i) {
            uint16_t sensor = (uint16_t)((i + f) % SENSORS);
            uint16_t value = (uint16_t)next_value(&state);
            raw[4 * i] = (unsigned char)sensor;
            raw[4 * i + 1] = (unsigned char)(sensor >> 8);
            raw[4 * i + 2] = (unsigned char)value;
            raw[4 * i + 3] = (unsigned char)(value >> 8);
        }
        files[f].data = malloc(raw_size * 2 + 2);
        if (!files[f].data) {
            free(raw);
            return 0;
        }
        files[f].size = encode(raw, raw_size, files[f].data);
    }
    free(raw);
    return 1;
}

static int row_cmp(const void *a, const void *b)
{
    const sensor_log_row *x = a, *y = b;
    if (x->file_index != y->file_index)
        return (x->file_index > y->file_index) ? 1 : -1;
    if (x->sensor_id != y->sensor_id)
        return (x->sensor_id > y->sensor_id) ? 1 : -1;
    return 0;
}

static int rows_differ(sensor_log_row *reference, sensor_log_row *actual,
                       size_t row_count)
{
    qsort(reference, row_count, sizeof(*reference), row_cmp);
    qsort(actual, row_count, sizeof(*actual), row_cmp);
    for (size_t i = 0; i < row_count; ++i) {
        if (reference[i].file_index != actual[i].file_index ||
            reference[i].sensor_id != actual[i].sensor_id ||
            reference[i].count != actual[i].count ||
            reference[i].value_sum != actual[i].value_sum) {
            printf("[check] row %zu: reference=(%zu,%u,%llu,%llu) parallel=(%zu,%u,%llu,%llu)\n",
                   i, reference[i].file_index, (unsigned)reference[i].sensor_id,
                   (unsigned long long)reference[i].count,
                   (unsigned long long)reference[i].value_sum,
                   actual[i].file_index, (unsigned)actual[i].sensor_id,
                   (unsigned long long)actual[i].count,
                   (unsigned long long)actual[i].value_sum);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    size_t n = MAX_FILES;
    int reps = 20, failures = 0;
    sensor_log_file *files = NULL;
    sensor_log_row *expected = NULL, *actual = NULL, *error_rows = NULL;
    unsigned char invalid_block[] = { 0x7f }; /* Literal block requires 128 bytes. */

    for (int i = 1; i < argc; ++i) {
        char *end;
        unsigned long value;
        int is_n = strcmp(argv[i], "--n") == 0;
        int is_reps = strcmp(argv[i], "--reps") == 0;
        if ((!is_n && !is_reps) || ++i == argc) {
            fprintf(stderr, "usage: %s [--n 1..200] [--reps positive-integer]\n", argv[0]);
            goto done;
        }
        value = strtoul(argv[i], &end, 10);
        if (*argv[i] == '\0' || *end != '\0' || value == 0 ||
            (is_n && value > MAX_FILES) || (is_reps && value > 100000)) {
            fprintf(stderr, "invalid option value: %s\n", argv[i]);
            goto done;
        }
        if (is_n) n = (size_t)value;
        else reps = (int)value;
    }

    files = calloc(n, sizeof(*files));
    expected = calloc(n * SENSORS, sizeof(*expected));
    actual = calloc(n * SENSORS, sizeof(*actual));
    error_rows = calloc(n * SENSORS, sizeof(*error_rows));
    if (!files || !expected || !actual || !error_rows || !make_files(files, n)) {
        fprintf(stderr, "input allocation failed\n");
        goto done;
    }

    int valid_reference = sensor_log_ingest_serial(files, n, expected);
    if (valid_reference == -1) {
        fprintf(stderr, "valid input rejected by reference\n");
        goto done;
    }

    /* Obtain the reference error result before repeating the parallel calls. */
    sensor_log_file saved = files[n - 1];
    files[n - 1].data = invalid_block;
    files[n - 1].size = sizeof(invalid_block);
    int invalid_reference = sensor_log_ingest_serial(files, n, error_rows);
    if (invalid_reference != -1) {
        fprintf(stderr, "malformed input not rejected by reference: %d\n",
                invalid_reference);
        files[n - 1] = saved;
        goto done;
    }

    for (int rep = 0; rep < reps; ++rep) {
        memset(actual, 0, n * SENSORS * sizeof(*actual));
        int got = sensor_log_ingest_omp(files, n, actual);
        if (got != invalid_reference) {
            printf("[check] repetition %d: malformed-file return reference=%d parallel=%d\n",
                   rep + 1, invalid_reference, got);
            failures = 1;
        }
    }
    files[n - 1] = saved;

    /* Also compare complete results for one intact parallel run. */
    memset(actual, 0, n * SENSORS * sizeof(*actual));
    int valid_parallel = sensor_log_ingest_omp(files, n, actual);
    if (valid_parallel != valid_reference) {
        printf("[check] valid input: return reference=%d parallel=%d\n",
               valid_reference, valid_parallel);
        failures = 1;
    } else if (rows_differ(expected, actual, n * SENSORS)) {
        failures = 1;
    }

done:
    if (files) {
        for (size_t f = 0; f < n; ++f)
            free(files[f].data);
    }
    free(files);
    free(expected);
    free(actual);
    free(error_rows);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
