#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const unsigned char *data;
    size_t size;
} sensor_log_file;

typedef struct {
    size_t file_index;
    uint16_t sensor_id;
    uint64_t count;
    uint64_t value_sum;
} sensor_log_row;

typedef struct {
    uint16_t sensor_id;
    uint16_t value;
} sensor_log_record;

int sensor_log_ingest_serial(const sensor_log_file *files, size_t file_count,
                             sensor_log_row *rows)
{
    enum { SENSOR_COUNT = 32, RECORD_COUNT = 23000,
           DECODED_BYTES = RECORD_COUNT * 4 };
    unsigned char decoded_bytes[DECODED_BYTES];
    sensor_log_record records[RECORD_COUNT];

    if (files == NULL || rows == NULL || file_count > SIZE_MAX / SENSOR_COUNT)
        return -1;

    for (size_t f = 0; f < file_count; ++f) {
        const unsigned char *data = files[f].data;
        size_t input_pos = 0;
        size_t output_pos = 0;
        uint64_t counts[SENSOR_COUNT] = {0};
        uint64_t sums[SENSOR_COUNT] = {0};

        if (data == NULL)
            return -1;

        while (input_pos < files[f].size) {
            unsigned char control = data[input_pos++];
            size_t length = (size_t)(control & 127u) + 1;

            if (length > DECODED_BYTES - output_pos)
                return -1;

            if (control & 128u) {
                if (input_pos == files[f].size)
                    return -1;
                memset(decoded_bytes + output_pos, data[input_pos++], length);
            } else {
                if (length > files[f].size - input_pos)
                    return -1;
                memcpy(decoded_bytes + output_pos, data + input_pos, length);
                input_pos += length;
            }
            output_pos += length;
        }

        if (output_pos != DECODED_BYTES)
            return -1;

        for (size_t i = 0; i < RECORD_COUNT; ++i) {
            size_t offset = i * 4;
            records[i].sensor_id = (uint16_t)(
                (unsigned int)decoded_bytes[offset] |
                ((unsigned int)decoded_bytes[offset + 1] << 8));
            records[i].value = (uint16_t)(
                (unsigned int)decoded_bytes[offset + 2] |
                ((unsigned int)decoded_bytes[offset + 3] << 8));
            if (records[i].sensor_id >= SENSOR_COUNT)
                return -1;
        }

        for (size_t i = 0; i < RECORD_COUNT; ++i) {
            uint16_t sensor = records[i].sensor_id;
            ++counts[sensor];
            sums[sensor] += records[i].value;
        }

        for (size_t sensor = 0; sensor < SENSOR_COUNT; ++sensor) {
            sensor_log_row *row = &rows[f * SENSOR_COUNT + sensor];
            row->file_index = f;
            row->sensor_id = (uint16_t)sensor;
            row->count = counts[sensor];
            row->value_sum = sums[sensor];
        }
    }

    return 0;
}
