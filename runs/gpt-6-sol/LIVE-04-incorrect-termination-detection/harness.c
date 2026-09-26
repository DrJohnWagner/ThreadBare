/* harness.c — timeout driver for breadth_first_search (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <omp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int breadth_first_search_serial(int *sources, int *distance, int *parent);
int breadth_first_search_omp(int *sources, int *distance, int *parent);

enum { VERTICES = 310000, SEARCHES = 20, EDGES = 5000000 };

static uint64_t state = UINT64_C(0x243F6A8885A308D3);

static uint64_t draw(void)
{
    uint64_t z = (state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static int make_sources(int sources[SEARCHES])
{
    int *reservoir = malloc((size_t)2 * EDGES * sizeof(*reservoir));
    if (!reservoir) return -1;

    size_t length = 0;
    for (int i = 0; i < 17; ++i) {
        for (int j = i + 1; j < 17; ++j) {
            reservoir[length++] = i;
            reservoir[length++] = j;
        }
    }
    for (int v = 17; v < VERTICES; ++v) {
        const size_t eligible = length;
        const int degree = v < 17 + 40136 ? 17 : 16;
        int selected[17];
        int count = 0;
        while (count < degree) {
            int target = reservoir[draw() % eligible];
            int duplicate = 0;
            for (int k = 0; k < count; ++k)
                if (selected[k] == target) { duplicate = 1; break; }
            if (duplicate) continue;
            selected[count++] = target;
            reservoir[length++] = v;
            reservoir[length++] = target;
        }
    }
    free(reservoir);
    if (length != (size_t)2 * EDGES) return -1;

    for (int i = 0; i < SEARCHES;) {
        int candidate = (int)(draw() % VERTICES);
        int duplicate = 0;
        for (int j = 0; j < i; ++j)
            if (sources[j] == candidate) { duplicate = 1; break; }
        if (!duplicate) sources[i++] = candidate;
    }
    return 0;
}

static double seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int positive_arg(const char *text)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < 1 || value > 10000)
        return -1;
    return (int)value;
}

int main(int argc, char **argv)
{
    int reps = 4, threads = 4, timeout = 90, failures = 0;
    for (int i = 1; i < argc; ++i) {
        int value;
        if (i + 1 >= argc || (value = positive_arg(argv[i + 1])) < 0) {
            fprintf(stderr, "usage: %s [--reps N] [--threads N] [--timeout SECONDS]\n", argv[0]);
            return 2;
        }
        if (!strcmp(argv[i], "--reps")) reps = value;
        else if (!strcmp(argv[i], "--threads")) threads = value;
        else if (!strcmp(argv[i], "--timeout")) timeout = value;
        else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 2;
        }
        ++i;
    }

    int sources[SEARCHES];
    if (make_sources(sources)) {
        fprintf(stderr, "source generation failed\n");
        return 2;
    }

    const size_t count = (size_t)VERTICES * SEARCHES;
    const size_t bytes = count * sizeof(int);
    int *reference_distance = malloc(bytes);
    int *reference_parent = malloc(bytes);
    size_t shared_bytes = 2 * bytes + sizeof(int);
    void *shared = mmap(NULL, shared_bytes, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!reference_distance || !reference_parent || shared == MAP_FAILED) {
        fprintf(stderr, "output allocation failed\n");
        return 2;
    }
    int *distance = shared;
    int *parent = distance + count;
    int *parallel_result = parent + count;

    memset(reference_distance, 0xa5, bytes);
    memset(reference_parent, 0xa5, bytes);
    int reference_result = breadth_first_search_serial(sources, reference_distance,
                                                        reference_parent);

    for (int rep = 0; rep < reps; ++rep) {
        memset(distance, 0xa5, bytes);
        memset(parent, 0xa5, bytes);
        *parallel_result = 0;
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            ++failures;
            break;
        }
        if (child == 0) {
            omp_set_dynamic(0);
            omp_set_num_threads(threads);
            *parallel_result = breadth_first_search_omp(sources, distance, parent);
            _exit(0);
        }

        int status = 0, timed_out = 0;
        double deadline = seconds() + timeout;
        for (;;) {
            pid_t done = waitpid(child, &status, WNOHANG);
            if (done == child) break;
            if (done < 0) {
                if (errno == EINTR) continue;
                perror("waitpid");
                kill(child, SIGKILL);
                waitpid(child, &status, 0);
                ++failures;
                timed_out = -1;
                break;
            }
            if (seconds() >= deadline) {
                kill(child, SIGKILL);
                waitpid(child, &status, 0);
                timed_out = 1;
                break;
            }
            struct timespec pause_time = { 0, 10000000L };
            nanosleep(&pause_time, NULL);
        }
        if (timed_out == 1) {
            printf("[check] repetition %d: parallel traversal exceeded %d-second timeout\n",
                   rep + 1, timeout);
            ++failures;
            continue;
        }
        if (timed_out < 0) break;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("[check] repetition %d: parallel traversal exited abnormally\n", rep + 1);
            ++failures;
            continue;
        }
        if (*parallel_result != reference_result) {
            printf("[check] repetition %d: return value differs (serial=%d parallel=%d)\n",
                   rep + 1, reference_result, *parallel_result);
            ++failures;
        }
        size_t pos;
        for (pos = 0; pos < count; ++pos)
            if (distance[pos] != reference_distance[pos] ||
                parent[pos] != reference_parent[pos]) break;
        if (pos < count) {
            printf("[check] repetition %d: source[%zu]=%d vertex=%zu: "
                   "distance %d/%d, parent %d/%d (serial/parallel)\n",
                   rep + 1, pos / VERTICES, sources[pos / VERTICES],
                   pos % VERTICES, reference_distance[pos], distance[pos],
                   reference_parent[pos], parent[pos]);
            ++failures;
        } else if (*parallel_result == reference_result) {
            printf("[check] repetition %d: all %zu distance/parent pairs match\n",
                   rep + 1, count);
        }
    }

    munmap(shared, shared_bytes);
    free(reference_distance);
    free(reference_parent);
    printf("[verdict] %s\n", failures ? "LIVE-01 exposed" : "LIVE-01 not triggered");
    return failures ? 1 : 0;
}
