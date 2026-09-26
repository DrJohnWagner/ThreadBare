#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    unsigned char *packed;
    size_t record_count;
    int64_t counts[16];
    int64_t sums[16];
} SensorFileWork;

void sensor_log_ingest_omp(const unsigned char *const *compressed_files,
                           const size_t *compressed_sizes,
                           size_t file_count,
                           size_t records_per_file,
                           int64_t *rows,
                           int64_t *summary)
{
    SensorFileWork work[file_count ? file_count : 1];
    unsigned char decoded_ready[file_count ? file_count : 1];
    unsigned char counted_ready[file_count ? file_count : 1];
    unsigned char decode_slots[8] = {0};
    unsigned char append_slots[8] = {0};

    #pragma omp parallel
    {
        #pragma omp single
        {
            for (size_t file = 0; file < file_count; ++file) {
                #pragma omp task firstprivate(file) depend(inout: decode_slots[file % 8]) depend(out: decoded_ready[file])
                {
                    unsigned char *packed = malloc(3 * records_per_file);
                    if (packed == NULL && records_per_file != 0) {
                        abort();
                    }

                    size_t record_count = 0;
                    const unsigned char *input = compressed_files[file];
                    for (size_t pos = 0; pos < compressed_sizes[file]; pos += 4) {
                        unsigned int run_length = input[pos];
                        unsigned char sensor = input[pos + 1];
                        unsigned char low = input[pos + 2];
                        unsigned char high = input[pos + 3];

                        for (unsigned int i = 0; i < run_length; ++i) {
                            size_t offset = 3 * record_count++;
                            packed[offset] = sensor;
                            packed[offset + 1] = low;
                            packed[offset + 2] = high;
                        }
                    }
                    work[file].packed = packed;
                    work[file].record_count = record_count;
                }

                #pragma omp task firstprivate(file) depend(in: decoded_ready[file]) depend(inout: decode_slots[file % 8], append_slots[file % 8]) depend(out: counted_ready[file])
                {
                    int64_t counts[16] = {0};
                    int64_t sums[16] = {0};
                    const unsigned char *packed = work[file].packed;

                    for (size_t record = 0; record < work[file].record_count; ++record) {
                        size_t offset = 3 * record;
                        unsigned int sensor = packed[offset];
                        unsigned int bits = (unsigned int)packed[offset + 1] |
                                            ((unsigned int)packed[offset + 2] << 8);
                        int32_t reading = bits < 0x8000U ? (int32_t)bits :
                                          (int32_t)bits - 0x10000;
                        ++counts[sensor];
                        sums[sensor] += reading;
                    }

                    for (size_t sensor = 0; sensor < 16; ++sensor) {
                        work[file].counts[sensor] = counts[sensor];
                        work[file].sums[sensor] = sums[sensor];
                    }
                    free(work[file].packed);
                }

                #pragma omp task firstprivate(file) depend(in: counted_ready[file]) depend(inout: append_slots[file % 8])
                {
                    for (size_t sensor = 0; sensor < 16; ++sensor) {
                        size_t row = 4 * (16 * file + sensor);
                        rows[row] = (int64_t)file;
                        rows[row + 1] = (int64_t)sensor;
                        rows[row + 2] = work[file].counts[sensor];
                        rows[row + 3] = work[file].sums[sensor];
                    }
                }
            }
        }
    }

    int64_t total_records = 0;
    int64_t total_sum = 0;
    for (size_t file = 0; file < file_count; ++file) {
        total_records += (int64_t)work[file].record_count;
        for (size_t sensor = 0; sensor < 16; ++sensor) {
            total_sum += work[file].sums[sensor];
        }
    }

    summary[0] = (int64_t)file_count;
    summary[1] = total_records;
    summary[2] = (int64_t)(16 * file_count);
    summary[3] = total_sum;
}
