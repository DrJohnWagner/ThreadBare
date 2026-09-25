# gpt-5.4-nano — 19 failure types

Generated 2026-09-25 by `run-prompts.py`, one run per taxonomy type, with the model set to `gpt-5.4-nano` on OpenAI. All 19 runs returned without an error, in 18.5 minutes in total (32–94 s each).

Nothing here was compiled or executed. The "Planted" column is my reading of the diff between `parallel.c` and `parallel_fixed.c` against the requested type. The "Analyser" column records whether any report finding carries the requested `typeKey`.

## Summary

| | count |
|---|---|
| planted bug matches the requested type | 10 |
| planted, but weak or partly wrong | 2 |
| nothing planted, or the wrong failure planted | 7 |
| Analyser reports the requested type (runs with a real plant) | 4 of 17 |
| Harness strategy matches the category | 19 |
| runs with at least one report `typeKey` outside the taxonomy | 14 |

## Safety — differential harness

| code | prompt | diff lines | planted | Analyser |
|---|---|---|---|---|
| SAFE-01 incorrect synchronization | k-means | 0 | No. `parallel.c` is identical to `parallel_fixed.c`. The planted note describes a race that isn't in the code. | reports it, but there is nothing to find |
| SAFE-02 incorrect task partitioning | k-means | 9 | Yes — the assignment loop runs `i < n - 1` and skips the last point. | found the bug, filed it as `poor-task-partitioning-scheduling` |
| SAFE-03 incorrect reductions | k-means | 0 | No. Identical files, as in SAFE-01. | reports it, but there is nothing to find |
| SAFE-04 dependency assumptions | k-means | 4 | Yes — writes `A_prev[i]` inside the parallel loop that reads it. | yes |
| SAFE-05 resource lifetime | BFS | 2 | Yes — `free(queue)` inside the level loop; later levels use it after it's freed. | no — generic `safety` findings, none mentions the free |
| SAFE-06 thread-unsafe components | pipeline | 4 | Weak — `(void)rand()` in the parallel loop. The result is discarded, so the race never reaches the output and a differential check can't observe it. | yes |
| SAFE-07 error / cancellation | pipeline | 1 | No. The only difference is an `#include <stdlib.h>`. The skip-on-`malloc`-failure path the note describes is identical in both versions, and `malloc` doesn't fail at these sizes. | no |
| SAFE-08 unintended nondeterminism | BFS | 2 | Partly — `schedule(dynamic)` vs `schedule(static)`. The fixed version keeps the same CAS-then-`parent[w]` race, so it is nondeterministic too. | yes |

## Performance — scaling harness

| code | prompt | diff lines | planted | Analyser |
|---|---|---|---|---|
| PERF-01 excessive synchronization | k-means | 11 | Yes — one `critical` per centroid (64 per thread per iteration) in place of one. | yes |
| PERF-02 poor partitioning | BFS | 2 | Inverted. The planted version uses `schedule(guided, 1)` and the fixed version uses `schedule(static)`. Static is the split that leaves workers idle on power-law degrees. | no |
| PERF-03 oversubscription | pipeline | 9 | Yes — `num_threads(nt * 4)` on both parallel loops. | no — findings are about an unrelated race |
| PERF-04 memory bottlenecks | k-means | 8 | Probably none. The added loop's result is discarded (`(void)waste`), so `-O2` will likely delete it. It also isn't the cache-line sharing the table asked for. | no |
| PERF-05 shared-resource contention | BFS | 18 | Yes, by a different mechanism — one global `critical` around every edge's CAS, not allocator contention. | found the lock, filed it as `performance` |
| PERF-06 insufficient parallelism | k-means | 12 | Yes — the centroid update is serialised under `omp single`. | no |

## Liveness — timeout harness

| code | prompt | diff lines | planted | Analyser |
|---|---|---|---|---|
| LIVE-01 deadlock | pipeline | 9 | Yes, by a different mechanism. The last file's thread spins inside a `critical` inside the `omp for`. The other threads wait at the loop's implicit barrier, before the atomic that would release it. That circular wait hangs every run, but it isn't the lock-order inversion the table asked for. | no — filed as `liveness` |
| LIVE-02 livelock | pipeline | 4 | Wrong failure. Each thread spins on its own row's count, which is still zero, before incrementing it, so the run hangs on the first record. That's a guaranteed hang, not livelock: nothing retries or changes state. | no — filed as `liveness` |
| LIVE-03 starvation | BFS | 14 | Wrong failure. Thread 0 spins on `fsz`, which changes only after the parallel region, so every search hangs at its last level. That's a guaranteed hang, not starvation. | no — `incorrect-termination-detection` and `livelock` |
| LIVE-04 termination detection | BFS | 2 | Yes, reversed from the plan — `frontierSize = 1` after every level, so the loop never exits. The table asked for a frontier that looks empty too early. | no |
| LIVE-05 progress assumptions | pipeline | 6 | Yes — thread 0 waits for other threads' atomic add, which only happens with more than one thread; with `OMP_NUM_THREADS=1` it spins forever. | no — filed as `livelock` |

## Problems in the pipeline

1. No check that a failure was planted. SAFE-01 and SAFE-03 went through Harness and Analyser with identical files. A check that `parallel.c` differs from `parallel_fixed.c` would turn these into failed runs.
2. The Analyser's `typeKey` is not validated. 14 runs contain findings keyed `safety`, `performance` or `liveness` (category names), and SAFE-04 has the non-existent `incorrect-task-partitioning-scheduling`. The Run schema accepts any string.
3. The Analyser mostly misses the planted failure. It reports 3–7 findings per run, most of them speculative ("relies on implicit barrier semantics…"). It reports the requested type in only 4 of the 17 runs with a real plant.
4. Liveness plants are guaranteed hangs, not the intended failures. LIVE-01 to LIVE-04 hang on every run, whatever the scheduling; only LIVE-05 depends on the thread count. A timeout harness can't tell livelock or starvation from any other infinite loop.
