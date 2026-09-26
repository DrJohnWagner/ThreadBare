#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct sensor_log_row {
    size_t file_index;
    uint8_t sensor_id;
    uint64_t record_count;
    uint64_t reading_sum;
};

struct sensor_log_summary {
    size_t processed_files;
    size_t table_rows;
    uint64_t total_record_count;
    uint64_t total_reading_sum;
};

int sensor_log_ingest_serial(const char *const *file_paths, size_t file_count,
                             struct sensor_log_row *rows,
                             struct sensor_log_summary *summary)
{
    uint8_t tokens[5750 * 4];
    uint8_t raw[23000 * 3];

    if (file_paths == NULL || rows == NULL || summary == NULL)
        return -1;

    summary->processed_files = 0;
    summary->table_rows = 0;
    summary->total_record_count = 0;
    summary->total_reading_sum = 0;

    for (size_t f = 0; f < file_count; ++f) {
        uint64_t counts[16] = {0};
        uint64_t sums[16] = {0};
        size_t raw_length = 0;

        if (file_paths[f] == NULL)
            return -1;

        FILE *file = fopen(file_paths[f], "rb");
        if (file == NULL)
            return -1;

        size_t bytes_read = fread(tokens, 1, sizeof tokens, file);
        int trailing_byte = fgetc(file);
        int read_error = ferror(file);
        int close_error = fclose(file);
        if (bytes_read != sizeof tokens || trailing_byte != EOF ||
            read_error || close_error != 0)
            return -1;

        for (size_t group = 0; group < 5750; ++group) {
            size_t offset = group * 4;
            uint8_t repeat = tokens[offset];
            uint8_t sensor = tokens[offset + 1];

            if (repeat != 4 || sensor >= 16)
                return -1;

            for (unsigned int record = 0; record < repeat; ++record) {
                raw[raw_length++] = sensor;
                raw[raw_length++] = tokens[offset + 2];
                raw[raw_length++] = tokens[offset + 3];
            }
        }

        for (size_t offset = 0; offset < raw_length; offset += 3) {
            uint8_t sensor = raw[offset];
            uint16_t reading = (uint16_t)((uint16_t)raw[offset + 1] |
                                          ((uint16_t)raw[offset + 2] << 8));
            ++counts[sensor];
            sums[sensor] += reading;
        }

        for (size_t sensor = 0; sensor < 16; ++sensor) {
            struct sensor_log_row *row = &rows[f * 16 + sensor];
            row->file_index = f;
            row->sensor_id = (uint8_t)sensor;
            row->record_count = counts[sensor];
            row->reading_sum = sums[sensor];
            ++summary->table_rows;
            summary->total_record_count += counts[sensor];
            summary->total_reading_sum += sums[sensor];
        }
        ++summary->processed_files;
    }

    return 0;
}
