#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct sensor_log_row {
    uint64_t file_index;
    uint64_t sensor_id;
    uint64_t record_count;
    uint64_t value_sum;
} sensor_log_row;

enum {
    RECORDS_PER_FILE = 23000,
    SENSOR_COUNT = 16,
    DECOMPRESSED_SIZE = RECORDS_PER_FILE * 3
};

static int sensor_log_process_file(const char *path, size_t file_index,
                                   sensor_log_row *rows, uint64_t *file_total)
{
    unsigned char decoded[DECOMPRESSED_SIZE];
    uint64_t counts[SENSOR_COUNT] = {0};
    uint64_t sums[SENSOR_COUNT] = {0};
    size_t used = 0;
    int control;
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return -1;

    while ((control = fgetc(file)) != EOF) {
        size_t length;

        if (control < 128) {
            length = (size_t)control + 1;
            if (length > DECOMPRESSED_SIZE - used ||
                fread(decoded + used, 1, length, file) != length) {
                fclose(file);
                return -1;
            }
        } else {
            int value = fgetc(file);
            length = (size_t)(control - 128) + 3;
            if (value == EOF || length > DECOMPRESSED_SIZE - used) {
                fclose(file);
                return -1;
            }
            memset(decoded + used, value, length);
        }
        used += length;
    }

    if (ferror(file) || used != DECOMPRESSED_SIZE) {
        fclose(file);
        return -1;
    }
    fclose(file);

    for (size_t i = 0; i < DECOMPRESSED_SIZE; i += 3) {
        unsigned int sensor = decoded[i];
        uint64_t value = (uint64_t)decoded[i + 1] |
                         ((uint64_t)decoded[i + 2] << 8);
        if (sensor >= SENSOR_COUNT)
            return -1;
        ++counts[sensor];
        sums[sensor] += value;
    }

    *file_total = 0;
    for (size_t sensor = 0; sensor < SENSOR_COUNT; ++sensor) {
        sensor_log_row *row = &rows[file_index * SENSOR_COUNT + sensor];
        row->file_index = (uint64_t)file_index;
        row->sensor_id = (uint64_t)sensor;
        row->record_count = counts[sensor];
        row->value_sum = sums[sensor];
        *file_total += sums[sensor];
    }
    return 0;
}

int sensor_log_ingest_omp(const char *const *paths, size_t nfiles,
                          sensor_log_row *rows, uint64_t *summary)
{
    uint64_t total_values = 0;
    int failed = 0;

    #pragma omp parallel for reduction(+:total_values) reduction(|:failed)
    for (size_t f = 0; f < nfiles; ++f) {
        uint64_t file_total;
        if (sensor_log_process_file(paths[f], f, rows, &file_total) != 0)
            failed = 1;
        else
            total_values += file_total;
    }

    if (failed)
        return -1;

    summary[0] = (uint64_t)nfiles * SENSOR_COUNT;
    summary[1] = (uint64_t)nfiles * RECORDS_PER_FILE;
    summary[2] = total_values;
    return 0;
}
