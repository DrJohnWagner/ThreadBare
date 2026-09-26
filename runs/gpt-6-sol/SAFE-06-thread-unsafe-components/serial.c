#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char sensor_id;
    unsigned char reading;
} sensor_log_record;

void sensor_log_pipeline_serial(size_t file_count,
                                const unsigned char *const *compressed_files,
                                const size_t *compressed_sizes,
                                uint64_t *results)
{
    for (size_t file = 0; file < file_count; ++file) {
        unsigned char expanded[23000 * 2];
        sensor_log_record decoded[23000];
        uint64_t counts[16] = {0};
        uint64_t sums[16] = {0};
        size_t record_count = 0;

        for (size_t offset = 0; offset < compressed_sizes[file]; offset += 3) {
            unsigned int run_length = compressed_files[file][offset];
            unsigned char sensor_id = compressed_files[file][offset + 1];
            unsigned char reading = compressed_files[file][offset + 2];

            for (unsigned int n = 0; n < run_length; ++n) {
                expanded[2 * record_count] = sensor_id;
                expanded[2 * record_count + 1] = reading;
                ++record_count;
            }
        }

        for (size_t record = 0; record < record_count; ++record) {
            decoded[record].sensor_id = expanded[2 * record];
            decoded[record].reading = expanded[2 * record + 1];
        }

        for (size_t record = 0; record < record_count; ++record) {
            unsigned char sensor_id = decoded[record].sensor_id;
            ++counts[sensor_id];
            sums[sensor_id] += decoded[record].reading;
        }

        /* Each row contains file index, sensor ID, count, and reading sum. */
        for (size_t sensor = 0; sensor < 16; ++sensor) {
            size_t row = (file * 16 + sensor) * 4;
            results[row] = file;
            results[row + 1] = sensor;
            results[row + 2] = counts[sensor];
            results[row + 3] = sums[sensor];
        }
    }
}
