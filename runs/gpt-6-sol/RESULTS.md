# gpt-6-sol — 19 failure types

Generated 2026-09-25 by `run-prompts.py runs/gpt-6-sol`, one run per taxonomy type, in about 55 minutes (121–235 s per run). The Failure Planter's prompt and the pipeline around it had these rules:

- it may decline a type it can't plant naturally (`declineReason`);
- the planted failure must survive `gcc-14 -O2 -fopenmp …` and be observable in output, timing or termination;
- every failure except a liveness failure must leave the program terminating normally;
- liveness failures must depend on scheduling or thread count, with each liveness type defined;
- each `implementationNote` must first quote the changed lines;
- the pipeline rejects a run whose planted code equals the correct parallel version, or whose `plantedFailures` is empty.

16 of 19 runs were saved. The Planter declined the other three.

## Method

- Compiled each `parallel.c` and `parallel_fixed.c` on its own (`-c`) with Apple clang and Homebrew libomp at `-O2`, before `gcc-14` was installed. The harnesses were built and run afterwards; see RUN SUMMARY.
- The "planted" column is my reading of the diff between the two files, checked against the requested type.
- "Analyser" records whether a report finding carries the requested `typeKey`, and where not, how it labelled the planted change.

## Summary

| | runs |
|---|---|
| saved | 16 of 19 |
| declined by the Planter | 3 |
| planted and fixed versions both compile | 16 |
| plants the requested failure | 15 |
| plant partly right | 1 |
| planted-only comments that point at the failure | 0 |
| Analyser reports the requested type | 10 of 16 |
| Analyser finds the planted change under any label | 16 of 16 |
| runs with a report `typeKey` outside the taxonomy | 0 |

Each type was run once, so a single row says little about how reliably the pipeline handles that type.

## Safety — differential harness

| code | prompt | planted | Analyser |
|---|---|---|---|
| SAFE-01 incorrect synchronization | k-means | Yes — drops `sums[:128]` from the loop's `reduction`, so threads race on the centroid sums and lose updates. | found it, filed as `incorrect-reductions` |
| SAFE-02 incorrect task partitioning | k-means | Yes — `for (int k = 1; k < 63; ++k)` never considers centroid 63. The output is wrong on every run, including with one thread. | yes |
| SAFE-03 incorrect reductions | k-means | Yes — adds `#pragma omp parallel for` to the accumulation loop, which updates shared `sums` and `counts` with no reduction. | yes |
| SAFE-04 dependency assumptions | k-means | Yes — puts accumulation and centroid update in separate `omp section`s, so the update can read partial sums. | found it, filed as `incorrect-synchronization` |
| SAFE-05 resource lifetime | BFS | Declined: "Freeing it while another search still uses it would likely cause a use-after-free crash, rather than an observable wrong output with normal termination." The terminate-normally rule is what blocked it. | — |
| SAFE-06 thread-unsafe components | pipeline | Yes — the decoder works in a `static` scratch array shared by all decode workers. | found it, filed as `incorrect-synchronization` |
| SAFE-07 error / cancellation | pipeline | Yes — `firstprivate(failed)` in place of `reduction(\|:failed)` drops a worker's error flag, so a malformed file returns success. The harness feeds in a malformed block and checks for `-1`. | found it, filed as `incorrect-reductions` |
| SAFE-08 unintended nondeterminism | BFS | Partly — picks the source from `omp_get_thread_num()` in place of `k`. Which thread runs slot `k` varies from run to run, but the result is also wrong against the serial reference on almost every run, so it reads more as a partitioning bug. | found it, filed as `incorrect-task-partitioning` |

## Performance — scaling harness

| code | prompt | planted | Analyser |
|---|---|---|---|
| PERF-01 excessive synchronization | k-means | Yes — a `critical` around each point's nearest-centroid search serialises the main loop. | yes |
| PERF-02 poor partitioning | BFS | Yes — `schedule(static, 16)` over 20 searches gives one thread 16 searches, another 4 and the rest none. | yes |
| PERF-03 oversubscription | pipeline | Yes — `num_threads(2 * omp_get_num_procs())`. | yes |
| PERF-04 memory bottlenecks | k-means | Yes — `schedule(static, 1)` puts adjacent points on different threads, so their writes to adjacent `assignments` entries share cache lines. | yes |
| PERF-05 shared-resource contention | BFS | Yes — every search `fprintf`s each visited vertex to one shared `FILE *` on `/dev/null`, so threads contend for the stream's lock. | yes |
| PERF-06 insufficient parallelism | k-means | Yes — `num_threads(2)` caps the assignment loop at two threads. | yes |

## Liveness — timeout harness

| code | prompt | planted | Analyser |
|---|---|---|---|
| LIVE-01 deadlock | pipeline | Yes — counting tasks take lock 0 then lock 1, append tasks take lock 1 then lock 0. It deadlocks only when tasks for two files overlap. | yes |
| LIVE-02 livelock | pipeline | Declined: "The queue pipeline has no retry or reciprocal backoff protocol in which workers could naturally keep changing state without advancing." | — |
| LIVE-03 starvation | BFS | Declined: "The 20 traversals are independent … There is no contended lock or resource through which one worker could indefinitely deny another required work." | — |
| LIVE-04 termination detection | BFS | Yes — one `finished` flag shared across independent searches, so one search's completion can make another exit early. The program terminates with wrong distances; the harness compares every distance and parent after its timeout check, so it catches this. | yes |
| LIVE-05 progress assumptions | pipeline | Yes — under `schedule(static, 1)`, file `f` waits for file `f + 1`, which belongs to a thread already spinning. It hangs whenever there are 2 or more threads and more files than threads; the one-thread path skips the wait. | found it, filed as `deadlock` — also a fair label |

## Problems remaining

1. My terminate-normally rule blocks SAFE-05. A lifetime bug naturally crashes, and the rule forbids that. Safety failures should be allowed to end in a crash as well as a wrong output.
2. LIVE-02 and LIVE-03 depend on structure the source prompts don't ask for. The Parallelizer wrote independent BFS traversals and a pipeline with no retry protocol, so there was nothing to livelock or starve. Getting these planted means describing work-stealing (BFS) and retry-on-full queues (pipeline) in the prompts.
3. Three safety plants give wrong output with one thread as well: SAFE-02 (skipped centroid), SAFE-07 (`firstprivate` loses the flag on one thread too) and SAFE-08 (every search starts at vertex 0). A differential check against the serial reference can't separate "wrong because of concurrency" from "wrong"; rerunning the harness with `OMP_NUM_THREADS=1` would.
4. Six of the Analyser's 16 matches carry a neighbouring label. Every one of those labels is a taxonomy type in the same category as the requested one; none crosses categories:

   | run | requested | Analyser's label |
   |---|---|---|
   | SAFE-01 | incorrect-synchronization | incorrect-reductions |
   | SAFE-04 | incorrect-dependency-assumptions | incorrect-synchronization |
   | SAFE-06 | thread-unsafe-components | incorrect-synchronization |
   | SAFE-07 | incorrect-error-handling | incorrect-reductions |
   | SAFE-08 | unintended-nondeterminism | incorrect-task-partitioning |
   | LIVE-05 | broken-progress-assumptions | deadlock |

   For SAFE-01, SAFE-07, SAFE-08 and LIVE-05 the Analyser's label describes the code at least as well as the requested type. The mix-ups fall where the taxonomy's boundaries overlap: a missing reduction is both a synchronisation and a reduction bug, and a spin-wait that can never be satisfied is both a deadlock and a broken progress assumption. Nothing in the pipeline checks the Analyser's labels against the taxonomy, so keeping to valid keys is the model's doing.
5. See RUN SUMMARY for the build-and-run results.

## RUN SUMMARY

Every saved run was built and executed on 2026-09-26. Each `<run>/` subdirectory holds the extracted zip plus three logs:

- `output.txt` — the harness built and run against `parallel.c` (the planted version);
- `output-fixed.txt` — the same harness against `parallel_fixed.c`, as a control;
- `output-1thread.txt` (safety runs only) — the planted version with `OMP_NUM_THREADS=1 --reps 1`.

Setup: Homebrew gcc 14.4.0, the macOS 26 SDK from Xcode 26.3 (`SDKROOT`), an 8-core / 16-thread Intel i9-9980HK and `OMP_NUM_THREADS=8`, with each harness's default flags. Every build used the harness header's command unchanged, except LIVE-04 and LIVE-05. Those define `_POSIX_C_SOURCE`, which on macOS hides `MAP_ANONYMOUS` and `mkdtemp`, so they needed `-D_DARWIN_C_SOURCE`; on Linux they would build as written. Each configuration ran once, so timings carry run-to-run noise: the serial PERF-03 reference took 0.086 s in one run and 0.163 s in the other.

| code | planted | fixed (control) | one thread | outcome |
|---|---|---|---|---|
| SAFE-01 | no output; killed at 300 s | passes | not tested: the harness forces at least 4 threads | failure real, not reported |
| SAFE-02 | exposed: `assignment[39]` 39 vs 31 | passes | exposed | detected; sequential bug |
| SAFE-03 | exposed: `centroid[0][0]` differs | passes | passes | detected |
| SAFE-04 | exposed: all 128 centroid values differ | passes | passes | detected |
| SAFE-06 | exposed: row 0 counts 1175 vs 1149 | passes | not tested: the harness forces 8 threads | detected |
| SAFE-07 | exposed: malformed file returns 0, not −1 | passes | exposed | detected; sequential bug |
| SAFE-08 | exposed: 3,578,638 mismatches | passes | exposed | detected; sequential bug |
| PERF-01 | exposed: 0.20× at 8 threads | passes: 4.89× | — | detected |
| PERF-02 | exposed: 1.15× at 8 threads | passes: 5.63× | — | detected |
| PERF-03 | exposed: 2.18× at 8 threads | also exposed: 4.16× | — | plant real; harness can't tell |
| PERF-04 | passes: 5.58× at 8 threads | passes: 5.59× | — | plant has no effect |
| PERF-05 | passes: measured at 1 thread only | passes | — | harness bug; untested |
| PERF-06 | exposed: 1.47× at 8 threads | also exposed: 3.91× | — | plant real; harness can't tell |
| LIVE-01 | exposed: timed out at 12 s | passes in 0.8 s | — | detected |
| LIVE-04 | exposed: parent mismatches in all 4 reps | passes: 6.2M pairs match | — | detected |
| LIVE-05 | exposed: timed out at 8 s with 2 threads | passes in 1.3 s | — | detected |

### What the runs show

1. 11 of the 16 harnesses separate the planted version from the fixed one: exposed on the plant, passing on the control. Combined with the 3 declines, 11 of the 19 requested failure types were generated and confirmed end to end.
2. Three of those 11 are sequential bugs. SAFE-02, SAFE-07 and SAFE-08 fail with one thread as well, so the harness detects a wrong result, not a concurrency failure. SAFE-03 and SAFE-04 pass with one thread and fail with eight, which confirms they depend on concurrency. That leaves 8 concurrency failures confirmed end to end: SAFE-03, SAFE-04, SAFE-06, PERF-01, PERF-02, LIVE-01, LIVE-04 and LIVE-05.
3. SAFE-01's race turns into non-termination. The k-means loop is `for (;;) { … if (!changed) break; }` with no iteration cap. Lost updates shift the centroids every iteration, so assignments never settle and the first repetition never returns. The differential harness has no timeout, so it never prints a verdict. Differential harnesses need a timeout as well.
4. PERF-03 and PERF-06 plant real slowdowns (2.18× vs 4.16×, and 1.47× vs 3.91×, at 8 threads), but their harnesses require 65% efficiency at the highest thread count. On this machine the fixed versions only reach 52% and 49%, so both versions are flagged. An absolute threshold depends on the machine; comparing against the fixed version, or against a measured baseline, would not.
5. PERF-04's `schedule(static, 1)` has no measurable effect: 5.58× against 5.59×. Sharing cache lines when writing `int` assignments costs nothing next to 64 distance calculations per point.
6. PERF-05's harness never runs more than one thread. It calls `omp_set_num_threads(1)` for the warm-up, then reads `omp_get_max_threads()` as the top of its sweep, which now returns 1. The planted `fprintf` makes the single-thread run 1.6× slower than the fixed version, but lock contention needs more than one thread and was never measured.
7. Every verdict line names the first code in its category (`SAFE-01`, `PERF-01`, `LIVE-01`), whatever the actual run. The Harness prompt asks for a code, but the pipeline never tells the Harness which run it's writing for. This is cosmetic, but it makes the logs misleading when read on their own.
