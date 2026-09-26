/* harness.c — differential driver for sensor_log_pipeline (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#include <inttypes.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sensor_log_pipeline_serial(size_t file_count,
                                const unsigned char *const *compressed_files,
                                const size_t *compressed_sizes,
                                uint64_t *results);
void sensor_log_pipeline_omp(size_t file_count,
                             const unsigned char *const *compressed_files,
                             const size_t *compressed_sizes,
                             uint64_t *results);

enum { RECORDS_PER_FILE = 23000, SENSORS = 16, WORDS_PER_ROW = 4 };

typedef struct {
    uint64_t file_index, sensor_id, count, reading_sum;
} row;

static uint32_t draw(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static int row_order(const void *a, const void *b)
{
    const row *x = a, *y = b;
    if (x->file_index != y->file_index)
        return (x->file_index > y->file_index) - (x->file_index < y->file_index);
    return (x->sensor_id > y->sensor_id) - (x->sensor_id < y->sensor_id);
}

static size_t option_size(const char *text, const char *name)
{
    char *end;
    unsigned long long value = strtoull(text, &end, 10);
    if (!*text || *end || value == 0 || value > 10000) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(2);
    }
    return (size_t)value;
}

static void *allocated(size_t bytes)
{
    void *p = malloc(bytes);
    if (!p) {
        fprintf(stderr, "allocation failed\n");
        exit(2);
    }
    return p;
}

int main(int argc, char **argv)
{
    size_t n = 400, reps = 20;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc)
            n = option_size(argv[++i], "--n");
        else if (strcmp(argv[i], "--reps") == 0 && i + 1 < argc)
            reps = option_size(argv[++i], "--reps");
        else {
            fprintf(stderr, "usage: %s [--n files] [--reps repetitions]\n", argv[0]);
            return 2;
        }
    }

    /* Make a single-threaded environment usable for testing concurrent decoding. */
    omp_set_dynamic(0);
    if (omp_get_max_threads() < 4)
        omp_set_num_threads(8);

    const unsigned char **files = allocated(n * sizeof(*files));
    size_t *sizes = allocated(n * sizeof(*sizes));
    uint64_t generated_sum = 0;
    for (size_t f = 0; f < n; ++f) {
        unsigned char *data = allocated(3 * (size_t)RECORDS_PER_FILE);
        uint32_t state = UINT32_C(0xA341316C) + (uint32_t)f;
        size_t used = 0, records = 0;
        while (records < RECORDS_PER_FILE) {
            size_t length = 1 + draw(&state) % 32;
            if (length > RECORDS_PER_FILE - records)
                length = RECORDS_PER_FILE - records;
            unsigned char sensor = (unsigned char)(draw(&state) % SENSORS);
            unsigned char reading = (unsigned char)(draw(&state) % 256);
            data[used++] = (unsigned char)length;
            data[used++] = sensor;
            data[used++] = reading;
            generated_sum += length * (uint64_t)reading;
            records += length;
        }
        files[f] = data;
        sizes[f] = used;
    }

    size_t row_count = n * SENSORS;
    size_t words = row_count * WORDS_PER_ROW;
    uint64_t *reference = allocated(words * sizeof(*reference));
    uint64_t *actual = allocated(words * sizeof(*actual));
    row *reference_rows = allocated(row_count * sizeof(*reference_rows));
    row *actual_rows = allocated(row_count * sizeof(*actual_rows));
    for (size_t i = 0; i < words; ++i)
        reference[i] = UINT64_MAX;
    sensor_log_pipeline_serial(n, files, sizes, reference);
    memcpy(reference_rows, reference, row_count * sizeof(*reference_rows));
    qsort(reference_rows, row_count, sizeof(*reference_rows), row_order);

    uint64_t reference_count = 0, reference_sum = 0;
    for (size_t i = 0; i < row_count; ++i) {
        row r = reference_rows[i];
        if (r.file_index != i / SENSORS || r.sensor_id != i % SENSORS) {
            fprintf(stderr, "reference has a missing, duplicate, or invalid row at %zu\n", i);
            exit(2);
        }
        reference_count += r.count;
        reference_sum += r.reading_sum;
    }
    if (reference_count != (uint64_t)n * RECORDS_PER_FILE ||
        reference_sum != generated_sum) {
        fprintf(stderr, "reference summary disagrees with generated input\n");
        exit(2);
    }

    int failures = 0;
    for (size_t rep = 0; rep < reps; ++rep) {
        for (size_t i = 0; i < words; ++i)
            actual[i] = UINT64_MAX;
        sensor_log_pipeline_omp(n, files, sizes, actual);
        memcpy(actual_rows, actual, row_count * sizeof(*actual_rows));
        qsort(actual_rows, row_count, sizeof(*actual_rows), row_order);

        uint64_t total_count = 0, total_sum = 0;
        size_t first = row_count;
        for (size_t i = 0; i < row_count; ++i) {
            const row *a = &actual_rows[i], *r = &reference_rows[i];
            total_count += a->count;
            total_sum += a->reading_sum;
            if (first == row_count &&
                (a->file_index != r->file_index || a->sensor_id != r->sensor_id ||
                 a->count != r->count || a->reading_sum != r->reading_sum))
                first = i;
        }
        if (first != row_count || total_count != reference_count ||
            total_sum != reference_sum) {
            ++failures;
            if (first != row_count) {
                const row *a = &actual_rows[first], *r = &reference_rows[first];
                printf("[check] rep=%zu first row=%zu expected=(%" PRIu64 ",%" PRIu64
                       ",%" PRIu64 ",%" PRIu64 ") actual=(%" PRIu64 ",%" PRIu64
                       ",%" PRIu64 ",%" PRIu64 ")\n",
                       rep + 1, first, r->file_index, r->sensor_id, r->count,
                       r->reading_sum, a->file_index, a->sensor_id, a->count,
                       a->reading_sum);
            } else {
                printf("[check] rep=%zu first divergence in totals: expected=(%" PRIu64
                       ",%" PRIu64 ") actual=(%" PRIu64 ",%" PRIu64 ")\n",
                       rep + 1, reference_count, reference_sum, total_count, total_sum);
            }
        }
    }

    printf("[summary] records=%" PRIu64 " reading_sum=%" PRIu64 " rows=%zu\n",
           reference_count, reference_sum, row_count);
    for (size_t f = 0; f < n; ++f)
        free((void *)files[f]);
    free(files);
    free(sizes);
    free(reference);
    free(actual);
    free(reference_rows);
    free(actual_rows);
    printf("[verdict] %s\n", failures ? "SAFE-01 exposed" : "SAFE-01 not triggered");
    return failures ? 1 : 0;
}
