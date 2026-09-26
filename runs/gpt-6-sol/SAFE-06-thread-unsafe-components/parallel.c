#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <omp.h>

typedef struct {
    unsigned char sensor_id;
    unsigned char reading;
} sensor_log_record;

typedef struct {
    size_t file;
    size_t record_count;
    unsigned char expanded[23000 * 2];
    sensor_log_record decoded[23000];
} sensor_log_job;

typedef struct {
    sensor_log_job *items[8];
    unsigned int head;
    unsigned int count;
    unsigned int producers;
    omp_lock_t lock;
} sensor_log_queue;

static void sensor_log_expand(sensor_log_job *job,
                              const unsigned char *compressed,
                              size_t compressed_size)
{
    size_t record_count = 0;

    for (size_t offset = 0; offset < compressed_size; offset += 3) {
        unsigned int run_length = compressed[offset];
        unsigned char sensor_id = compressed[offset + 1];
        unsigned char reading = compressed[offset + 2];

        for (unsigned int n = 0; n < run_length; ++n) {
            job->expanded[2 * record_count] = sensor_id;
            job->expanded[2 * record_count + 1] = reading;
            ++record_count;
        }
    }
    job->record_count = record_count;
}

static void sensor_log_decode(sensor_log_job *job)
{
    static sensor_log_record decoded[23000];
    for (size_t record = 0; record < job->record_count; ++record) {
        decoded[record].sensor_id = job->expanded[2 * record];
        decoded[record].reading = job->expanded[2 * record + 1];
    }
    for (size_t record = 0; record < job->record_count; ++record) {
        job->decoded[record] = decoded[record];
    }
}

static void sensor_log_aggregate(const sensor_log_job *job, uint64_t *results)
{
    uint64_t counts[16] = {0};
    uint64_t sums[16] = {0};

    for (size_t record = 0; record < job->record_count; ++record) {
        unsigned char sensor_id = job->decoded[record].sensor_id;
        ++counts[sensor_id];
        sums[sensor_id] += job->decoded[record].reading;
    }

    for (size_t sensor = 0; sensor < 16; ++sensor) {
        size_t row = (job->file * 16 + sensor) * 4;
        results[row] = job->file;
        results[row + 1] = sensor;
        results[row + 2] = counts[sensor];
        results[row + 3] = sums[sensor];
    }
}

static void sensor_log_queue_push(sensor_log_queue *queue, sensor_log_job *job)
{
    for (;;) {
        omp_set_lock(&queue->lock);
        if (queue->count < 8) {
            queue->items[(queue->head + queue->count) % 8] = job;
            ++queue->count;
            omp_unset_lock(&queue->lock);
            return;
        }
        omp_unset_lock(&queue->lock);
        #pragma omp taskyield
    }
}

/* Returns 1 for a job, 0 while waiting, and -1 after all producers finish. */
static int sensor_log_queue_pop(sensor_log_queue *queue, sensor_log_job **job)
{
    int status;

    omp_set_lock(&queue->lock);
    if (queue->count != 0) {
        *job = queue->items[queue->head];
        queue->head = (queue->head + 1) % 8;
        --queue->count;
        status = 1;
    } else {
        status = queue->producers == 0 ? -1 : 0;
    }
    omp_unset_lock(&queue->lock);
    return status;
}

static void sensor_log_queue_producer_done(sensor_log_queue *queue)
{
    omp_set_lock(&queue->lock);
    --queue->producers;
    omp_unset_lock(&queue->lock);
}

void sensor_log_pipeline_omp(size_t file_count,
                             const unsigned char *const *compressed_files,
                             const size_t *compressed_sizes,
                             uint64_t *results)
{
    sensor_log_queue expanded_queue = {0};
    sensor_log_queue decoded_queue = {0};
    size_t next_file = 0;
    int thread_limit = omp_get_max_threads();

    if (thread_limit > 12) {
        thread_limit = 12;
    }
    omp_init_lock(&expanded_queue.lock);
    omp_init_lock(&decoded_queue.lock);

    #pragma omp parallel num_threads(thread_limit)
    {
        int thread = omp_get_thread_num();
        int threads = omp_get_num_threads();
        int expand_workers = threads / 3;
        int decode_workers = threads / 3;

        #pragma omp single
        {
            expanded_queue.producers = (unsigned int)expand_workers;
            decoded_queue.producers = (unsigned int)decode_workers;
        }

        if (threads < 3) {
            #pragma omp for schedule(static)
            for (size_t file = 0; file < file_count; ++file) {
                sensor_log_job job;
                job.file = file;
                sensor_log_expand(&job, compressed_files[file], compressed_sizes[file]);
                sensor_log_decode(&job);
                sensor_log_aggregate(&job, results);
            }
        } else if (thread < expand_workers) {
            for (;;) {
                size_t file;
                sensor_log_job *job;

                #pragma omp atomic capture
                { file = next_file; ++next_file; }
                if (file >= file_count) {
                    break;
                }
                job = malloc(sizeof(*job));
                job->file = file;
                sensor_log_expand(job, compressed_files[file], compressed_sizes[file]);
                sensor_log_queue_push(&expanded_queue, job);
            }
            sensor_log_queue_producer_done(&expanded_queue);
        } else if (thread < expand_workers + decode_workers) {
            for (;;) {
                sensor_log_job *job;
                int status = sensor_log_queue_pop(&expanded_queue, &job);
                if (status < 0) {
                    break;
                }
                if (status == 0) {
                    #pragma omp taskyield
                    continue;
                }
                sensor_log_decode(job);
                sensor_log_queue_push(&decoded_queue, job);
            }
            sensor_log_queue_producer_done(&decoded_queue);
        } else {
            for (;;) {
                sensor_log_job *job;
                int status = sensor_log_queue_pop(&decoded_queue, &job);
                if (status < 0) {
                    break;
                }
                if (status == 0) {
                    #pragma omp taskyield
                    continue;
                }
                sensor_log_aggregate(job, results);
                free(job);
            }
        }
    }

    omp_destroy_lock(&decoded_queue.lock);
    omp_destroy_lock(&expanded_queue.lock);
}
