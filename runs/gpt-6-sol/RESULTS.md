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

- Compiled each `parallel.c` and `parallel_fixed.c` on its own (`-c`) with Apple clang and Homebrew libomp at `-O2`. `gcc-14` isn't installed here. The harnesses were read but not built or run.
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
4. Six of the Analyser's 16 matches carry a neighbouring label. For SAFE-01, SAFE-07, SAFE-08 and LIVE-05 the Analyser's label describes the code at least as well as the requested type.
5. Nothing was executed. Whether each harness actually observes its failure is still unverified.
