#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <omp.h>

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

struct sensor_log_raw_slot {
    uint8_t records[23000 * 3];
};

struct sensor_log_decoded_slot {
    uint64_t counts[16];
    uint64_t sums[16];
};

static int sensor_log_read_expand(const char *path,
                                  struct sensor_log_raw_slot *slot)
{
    uint8_t tokens[5750 * 4];
    size_t raw_length = 0;

    if (path == NULL)
        return -1;

    FILE *file = fopen(path, "rb");
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
            slot->records[raw_length++] = sensor;
            slot->records[raw_length++] = tokens[offset + 2];
            slot->records[raw_length++] = tokens[offset + 3];
        }
    }

    return 0;
}

static void sensor_log_decode(const struct sensor_log_raw_slot *raw,
                              struct sensor_log_decoded_slot *decoded)
{
    for (size_t sensor = 0; sensor < 16; ++sensor) {
        decoded->counts[sensor] = 0;
        decoded->sums[sensor] = 0;
    }

    for (size_t offset = 0; offset < sizeof raw->records; offset += 3) {
        uint8_t sensor = raw->records[offset];
        uint16_t reading = (uint16_t)((uint16_t)raw->records[offset + 1] |
                                      ((uint16_t)raw->records[offset + 2] << 8));
        ++decoded->counts[sensor];
        decoded->sums[sensor] += reading;
    }
}

int sensor_log_ingest_omp(const char *const *file_paths, size_t file_count,
                          struct sensor_log_row *rows,
                          struct sensor_log_summary *summary)
{
    struct sensor_log_raw_slot raw_slots[8];
    struct sensor_log_decoded_slot decoded_slots[8];
    int status[8];
    int failed = 0;

    if (file_paths == NULL || rows == NULL || summary == NULL)
        return -1;

    summary->processed_files = 0;
    summary->table_rows = 0;
    summary->total_record_count = 0;
    summary->total_reading_sum = 0;

    #pragma omp parallel num_threads(2 * omp_get_num_procs())
    {
        #pragma omp single
        {
            for (size_t base = 0; base < file_count && !failed; ) {
                size_t batch_size = file_count - base;
                if (batch_size > 8)
                    batch_size = 8;

                for (size_t slot = 0; slot < batch_size; ++slot) {
                    size_t file_index = base + slot;

                    #pragma omp task firstprivate(slot, file_index) depend(out: raw_slots[slot])
                    {
                        status[slot] = sensor_log_read_expand(file_paths[file_index],
                                                              &raw_slots[slot]);
                    }

                    #pragma omp task firstprivate(slot) depend(in: raw_slots[slot]) depend(out: decoded_slots[slot])
                    {
                        if (status[slot] == 0)
                            sensor_log_decode(&raw_slots[slot], &decoded_slots[slot]);
                    }

                    #pragma omp task firstprivate(slot, file_index) depend(in: decoded_slots[slot])
                    {
                        if (status[slot] == 0) {
                            for (size_t sensor = 0; sensor < 16; ++sensor) {
                                struct sensor_log_row *row =
                                    &rows[file_index * 16 + sensor];
                                row->file_index = file_index;
                                row->sensor_id = (uint8_t)sensor;
                                row->record_count = decoded_slots[slot].counts[sensor];
                                row->reading_sum = decoded_slots[slot].sums[sensor];
                            }
                        }
                    }
                }

                #pragma omp taskwait
                for (size_t slot = 0; slot < batch_size; ++slot) {
                    if (status[slot] != 0)
                        failed = 1;
                }
                base += batch_size;
            }
        }
    }

    if (failed)
        return -1;

    for (size_t file_index = 0; file_index < file_count; ++file_index) {
        for (size_t sensor = 0; sensor < 16; ++sensor) {
            const struct sensor_log_row *row = &rows[file_index * 16 + sensor];
            ++summary->table_rows;
            summary->total_record_count += row->record_count;
            summary->total_reading_sum += row->reading_sum;
        }
        ++summary->processed_files;
    }

    return 0;
}
