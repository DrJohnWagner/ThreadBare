"""Canned example bugs used to stub POST /api/runs until the real generation pipeline
exists. Ported from DESIGN.html's BUGS array, reshaped into the Run model's plant/report
split: `explanation` is written as an independent analyser's observation, not
documentation of the plant — see DESIGN_NOTES.md and ENGINEERING.md's Report tab design.

serial_code and buggy_code/fixed_code are linkable functions, not self-contained
programs — each test_harness is a real driver (its own main()) that links against
whichever one it's given and checks it. That's the actual test-harness artifact, not a
description of one: see the "actual code" note in ENGINEERING.md.

finding_lines are 1-indexed line numbers into buggy_code — the report is structured
JSON (typeKey + lines + explanation per finding), not prose describing the bug's
location, so those lines have to actually point at the bug.
"""

import random
from dataclasses import dataclass


@dataclass(frozen=True)
class Fixture:
    type_key: str
    serial_code: str
    buggy_code: str
    fixed_code: str
    test_harness: str
    implementation_note: str
    finding_lines: list[int]
    explanation: str


FIXTURES: list[Fixture] = [
    Fixture(
        type_key="incorrect-dependency-assumptions",
        serial_code="""void prefix_sum_serial(const int *input, int n, int *output)
{
    output[0] = input[0];
    for (int i = 1; i < n; i++) {
        output[i] = output[i - 1] + input[i];
    }
}""",
        buggy_code="""#include <omp.h>

void prefix_sum_omp(const int *input, int n, int *output)
{
    #pragma omp parallel for
    for (int i = 1; i < n; i++) {
        output[i] = output[i - 1] + input[i];
    }
    output[0] = input[0];
}""",
        fixed_code="""#include <omp.h>

void prefix_sum_omp(const int *input, int n, int *output)
{
    output[0] = input[0];
    for (int i = 1; i < n; i++) {
        output[i] = output[i - 1] + input[i];
    }
}""",
        test_harness="""/* harness.c — differential driver for prefix_sum (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=8 ./harness --reps 20 --n 20000000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void prefix_sum_serial(const int *, int, int *);
void prefix_sum_omp   (const int *, int, int *);

static int first_mismatch(const int *a, const int *b, int n)
{
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return i + 1;
    return 0;
}

int main(int argc, char **argv)
{
    int n = 20000000, reps = 20;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--n"))    n    = atoi(argv[++i]);
        if (!strcmp(argv[i], "--reps")) reps = atoi(argv[++i]);
    }

    int *input = malloc(n * sizeof(int));
    int *ref   = malloc(n * sizeof(int));
    int *got   = malloc(n * sizeof(int));
    for (int i = 0; i < n; ++i) input[i] = (i % 7) - 3;

    prefix_sum_serial(input, n, ref);

    int failures = 0;
    for (int r = 0; r < reps; ++r) {
        memset(got, 0, n * sizeof(int));
        prefix_sum_omp(input, n, got);
        int bad = first_mismatch(ref, got, n);
        if (bad) {
            ++failures;
            printf("[check]   rep %02d  MISMATCH  first divergence at output[%d]\\n",
                   r + 1, bad - 1);
        }
    }

    printf("[verdict] %s\\n", failures ? "SAFE-01 exposed" : "no mismatch observed");
    free(input); free(ref); free(got);
    return failures ? 1 : 0;
}""",
        implementation_note=(
            "#pragma omp parallel for hands each thread a slice of the iteration "
            "space on the assumption that no slice needs another slice's results. "
            "Here every output[i] reads output[i-1], which is only guaranteed "
            "correct once the loop has run sequentially up to that point."
        ),
        finding_lines=[5, 7, 9],
        explanation=(
            "Loop iterations are executed out of order relative to a data "
            "dependency between output[i] and output[i-1]."
        ),
    ),
    Fixture(
        type_key="incorrect-synchronization",
        serial_code="""long counter_serial(long n)
{
    long counter = 0;
    for (long i = 0; i < n; i++) {
        counter++;
    }
    return counter;
}""",
        buggy_code="""#include <omp.h>

long counter_omp(long n)
{
    long counter = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        counter++;
    }
    return counter;
}""",
        fixed_code="""#include <omp.h>

long counter_omp(long n)
{
    long counter = 0;
    #pragma omp parallel for reduction(+:counter)
    for (long i = 0; i < n; i++) {
        counter++;
    }
    return counter;
}""",
        test_harness="""/* harness.c — differential driver for counter (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=8 ./harness --reps 20 --n 10000000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long counter_serial(long);
long counter_omp(long);

int main(int argc, char **argv)
{
    long n = 10000000;
    int reps = 20;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--n"))    n    = strtol(argv[++i], NULL, 10);
        if (!strcmp(argv[i], "--reps")) reps = atoi(argv[++i]);
    }

    long expected = counter_serial(n);
    printf("[harness] serial reference: %ld\\n", expected);

    int failures = 0;
    for (int r = 0; r < reps; ++r) {
        long got = counter_omp(n);
        if (got != expected) {
            ++failures;
            printf("[check]   rep %02d  MISMATCH  got %ld, expected %ld\\n",
                   r + 1, got, expected);
        }
    }

    printf("[verdict] %s\\n", failures ? "SAFE-02 exposed" : "no mismatch observed");
    return failures ? 1 : 0;
}""",
        implementation_note=(
            "counter++ is a read-modify-write on shared state with no "
            "synchronization; concurrent increments from different threads "
            "interleave and overwrite each other."
        ),
        finding_lines=[6, 8],
        explanation=(
            "Multiple threads update the same counter without coordination; "
            "increments are lost."
        ),
    ),
    Fixture(
        type_key="incorrect-synchronization",
        serial_code="""void reverse_scale_serial(int n, double *a, double *b)
{
    for (int i = 0; i < n; i++) a[i] = i * 1.5;
    for (int i = 0; i < n; i++) b[i] = a[n - 1 - i] * 2.0;
}""",
        buggy_code="""#include <omp.h>

void reverse_scale_omp(int n, double *a, double *b)
{
    #pragma omp parallel
    {
        #pragma omp for nowait
        for (int i = 0; i < n; i++) {
            a[i] = i * 1.5;
        }

        #pragma omp for
        for (int i = 0; i < n; i++) {
            b[i] = a[n - 1 - i] * 2.0;
        }
    }
}""",
        fixed_code="""#include <omp.h>

void reverse_scale_omp(int n, double *a, double *b)
{
    #pragma omp parallel
    {
        #pragma omp for
        for (int i = 0; i < n; i++) {
            a[i] = i * 1.5;
        }

        #pragma omp for
        for (int i = 0; i < n; i++) {
            b[i] = a[n - 1 - i] * 2.0;
        }
    }
}""",
        test_harness="""/* harness.c — differential driver for reverse_scale (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=8 ./harness --reps 50 --n 2000000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void reverse_scale_serial(int, double *, double *);
void reverse_scale_omp   (int, double *, double *);

static int first_mismatch(const double *a, const double *b, int n)
{
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return i + 1;
    return 0;
}

int main(int argc, char **argv)
{
    int n = 2000000, reps = 50;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--n"))    n    = atoi(argv[++i]);
        if (!strcmp(argv[i], "--reps")) reps = atoi(argv[++i]);
    }

    double *a_ref = malloc(n * sizeof(double));
    double *b_ref = malloc(n * sizeof(double));
    double *a_got = malloc(n * sizeof(double));
    double *b_got = malloc(n * sizeof(double));

    reverse_scale_serial(n, a_ref, b_ref);

    int failures = 0;
    for (int r = 0; r < reps; ++r) {
        reverse_scale_omp(n, a_got, b_got);
        int bad = first_mismatch(b_ref, b_got, n);
        if (bad) {
            ++failures;
            printf("[check]   rep %02d  MISMATCH  first divergence at b[%d]\\n",
                   r + 1, bad - 1);
        }
    }

    printf("[verdict] %s\\n", failures ? "SAFE-03 exposed" : "no mismatch observed");
    free(a_ref); free(b_ref); free(a_got); free(b_got);
    return failures ? 1 : 0;
}""",
        implementation_note=(
            "nowait removes the implicit barrier at the end of the first omp for, "
            "so a fast thread can enter the second loop and read a[n-1-i] before "
            "the thread responsible for writing it has done so."
        ),
        finding_lines=[7, 14],
        explanation=(
            "One thread group starts reading a before another has finished "
            "writing all of it."
        ),
    ),
    Fixture(
        type_key="excessive-synchronization",
        serial_code="""#include <math.h>

double trig_sum_serial(long n)
{
    double total = 0.0;
    for (long i = 1; i <= n; i++) {
        total += sqrt((double)i) * sin((double)i);
    }
    return total;
}""",
        buggy_code="""#include <omp.h>
#include <math.h>

double trig_sum_omp(long n)
{
    double total = 0.0;
    #pragma omp parallel for
    for (long i = 1; i <= n; i++) {
        double value = sqrt((double)i) * sin((double)i);
        #pragma omp critical
        {
            total += value;
        }
    }
    return total;
}""",
        fixed_code="""#include <omp.h>
#include <math.h>

double trig_sum_omp(long n)
{
    double total = 0.0;
    #pragma omp parallel for reduction(+:total)
    for (long i = 1; i <= n; i++) {
        double value = sqrt((double)i) * sin((double)i);
        total += value;
    }
    return total;
}""",
        test_harness="""/* harness.c — scaling driver for trig_sum (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness -lm
 *   run:   OMP_NUM_THREADS=8 ./harness --n 20000000
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

double trig_sum_serial(long);
double trig_sum_omp   (long);

int main(int argc, char **argv)
{
    long n = 20000000;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--n")) n = strtol(argv[++i], NULL, 10);
    }

    double t0 = omp_get_wtime();
    double ref = trig_sum_serial(n);
    double serial_s = omp_get_wtime() - t0;
    printf("[harness] serial reference: %.3f s  (total = %.6f)\\n", serial_s, ref);

    omp_set_num_threads(1);
    double t1 = omp_get_wtime();
    double got1 = trig_sum_omp(n);
    double one_thread_s = omp_get_wtime() - t1;
    (void)one_thread_s;
    if (fabs(got1 - ref) > 1e-6 * fabs(ref)) {
        printf("[check]   MISMATCH  1-thread total %.6f vs reference %.6f\\n", got1, ref);
    }

    int max_threads = omp_get_max_threads();
    double best_speedup = 0.0;
    for (int t = 1; t <= max_threads; t *= 2) {
        omp_set_num_threads(t);
        double s = omp_get_wtime();
        trig_sum_omp(n);
        double e = omp_get_wtime() - s;
        double speedup = serial_s / e;
        if (speedup > best_speedup) best_speedup = speedup;
        printf("[scale]   %2d threads  %.3f s  speedup %.2fx  efficiency %3.0f%%\\n",
               t, e, speedup, 100.0 * speedup / t);
    }

    int failures = best_speedup < (double)max_threads * 0.25;
    printf("[verdict] %s\\n", failures ? "PERF-01 exposed" : "scales as expected");
    return failures ? 1 : 0;
}""",
        implementation_note=(
            "The critical section wraps only total += value, but every thread "
            "enters it once per iteration, right next to the expensive sqrt/sin "
            "work — effectively serializing the whole loop."
        ),
        finding_lines=[10, 12],
        explanation=(
            "Every thread serializes on a single critical section once per "
            "iteration, negating parallel speedup."
        ),
    ),
    Fixture(
        type_key="memory-bottlenecks",
        serial_code="""long count_serial(long n)
{
    long total = 0;
    for (long i = 0; i < n; i++) {
        total++;
    }
    return total;
}""",
        buggy_code="""#include <omp.h>
#include <stdlib.h>

long count_omp(long n)
{
    int nthreads = omp_get_max_threads();
    int *counts = calloc(nthreads, sizeof(int));

    #pragma omp parallel num_threads(nthreads)
    {
        int tid = omp_get_thread_num();
        for (long i = tid; i < n; i += nthreads) {
            counts[tid]++;
        }
    }

    long total = 0;
    for (int t = 0; t < nthreads; t++) total += counts[t];
    free(counts);
    return total;
}""",
        fixed_code="""#include <omp.h>
#include <stdlib.h>

long count_omp(long n)
{
    int nthreads = omp_get_max_threads();
    int *counts = calloc(nthreads, sizeof(int));

    #pragma omp parallel num_threads(nthreads)
    {
        int tid = omp_get_thread_num();
        int local = 0;
        for (long i = tid; i < n; i += nthreads) {
            local++;
        }
        counts[tid] = local;
    }

    long total = 0;
    for (int t = 0; t < nthreads; t++) total += counts[t];
    free(counts);
    return total;
}""",
        test_harness="""/* harness.c — scaling driver for count (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=8 ./harness --n 200000000
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long count_serial(long);
long count_omp   (long);

int main(int argc, char **argv)
{
    long n = 200000000;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--n")) n = strtol(argv[++i], NULL, 10);
    }

    double t0 = omp_get_wtime();
    long ref = count_serial(n);
    double serial_s = omp_get_wtime() - t0;
    printf("[harness] serial reference: %.3f s  (total = %ld)\\n", serial_s, ref);

    int max_threads = omp_get_max_threads();
    double best_speedup = 0.0;
    for (int t = 1; t <= max_threads; t *= 2) {
        omp_set_num_threads(t);
        double s = omp_get_wtime();
        long got = count_omp(n);
        double e = omp_get_wtime() - s;
        if (got != ref) {
            printf("[check]   %2d threads  MISMATCH  got %ld, expected %ld\\n", t, got, ref);
        }
        double speedup = serial_s / e;
        if (speedup > best_speedup) best_speedup = speedup;
        printf("[scale]   %2d threads  %.3f s  speedup %.2fx  efficiency %3.0f%%\\n",
               t, e, speedup, 100.0 * speedup / t);
    }

    int failures = best_speedup < (double)max_threads * 0.25;
    printf("[verdict] %s\\n", failures ? "PERF-02 exposed" : "scales as expected");
    return failures ? 1 : 0;
}""",
        implementation_note=(
            "counts[0..nthreads-1] are adjacent ints, almost certainly within one "
            "cache line; every increment invalidates that line for every other "
            "core (false sharing)."
        ),
        finding_lines=[7, 13],
        explanation=(
            "Per-thread counters share a cache line, so every increment "
            "invalidates it for other cores."
        ),
    ),
    Fixture(
        type_key="poor-task-partitioning-scheduling",
        serial_code="""long triangular_sum_serial(long n)
{
    long total = 0;
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < i; j++) {
            total += (i * j) % 7;
        }
    }
    return total;
}""",
        buggy_code="""#include <omp.h>

long triangular_sum_omp(long n)
{
    long total = 0;
    #pragma omp parallel for reduction(+:total) schedule(static)
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < i; j++) {
            total += (i * j) % 7;
        }
    }
    return total;
}""",
        fixed_code="""#include <omp.h>

long triangular_sum_omp(long n)
{
    long total = 0;
    #pragma omp parallel for reduction(+:total) schedule(dynamic, 64)
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < i; j++) {
            total += (i * j) % 7;
        }
    }
    return total;
}""",
        test_harness="""/* harness.c — scaling driver for triangular_sum (generated)
 *
 *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
 *   run:   OMP_NUM_THREADS=8 ./harness --n 40000
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long triangular_sum_serial(long);
long triangular_sum_omp   (long);

int main(int argc, char **argv)
{
    long n = 40000;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--n")) n = strtol(argv[++i], NULL, 10);
    }

    double t0 = omp_get_wtime();
    long ref = triangular_sum_serial(n);
    double serial_s = omp_get_wtime() - t0;
    printf("[harness] serial reference: %.3f s  (total = %ld)\\n", serial_s, ref);

    int max_threads = omp_get_max_threads();
    double best_speedup = 0.0;
    for (int t = 1; t <= max_threads; t *= 2) {
        omp_set_num_threads(t);
        double s = omp_get_wtime();
        long got = triangular_sum_omp(n);
        double e = omp_get_wtime() - s;
        if (got != ref) {
            printf("[check]   %2d threads  MISMATCH  got %ld, expected %ld\\n", t, got, ref);
        }
        double speedup = serial_s / e;
        if (speedup > best_speedup) best_speedup = speedup;
        printf("[scale]   %2d threads  %.3f s  speedup %.2fx  efficiency %3.0f%%\\n",
               t, e, speedup, 100.0 * speedup / t);
    }

    int failures = best_speedup < (double)max_threads * 0.4;
    printf("[verdict] %s\\n", failures ? "PERF-03 exposed" : "scales as expected");
    return failures ? 1 : 0;
}""",
        implementation_note=(
            "schedule(static) divides the outer loop's iteration count evenly, "
            "but per-iteration cost grows with i (the inner loop runs i times), so "
            "the chunk containing the largest i values does far more total work."
        ),
        finding_lines=[6],
        explanation=(
            "Static scheduling splits the loop into equal-sized chunks despite "
            "highly uneven per-iteration cost."
        ),
    ),
]


def pick_fixture(failure_modes: list[str]) -> Fixture:
    if failure_modes:
        matching = [f for f in FIXTURES if f.type_key in failure_modes]
        if matching:
            return random.choice(matching)
    return random.choice(FIXTURES)
