# gpt-5.4-nano — 19 failure types

Generated 2026-09-25 by `run-prompts.py runs/gpt-5.4-nano-02`, one run per taxonomy type. The Failure Planter's prompt and the pipeline around it had these rules:

- it may decline a type it can't plant naturally (new `declineReason` field);
- the planted failure must survive `gcc-14 -O2 -fopenmp …` and be observable in output, timing or termination;
- liveness failures must depend on scheduling or thread count, with each liveness type defined;
- each `implementationNote` must first quote the changed lines;
- the pipeline rejects a run whose planted code equals the correct parallel version, or whose `plantedFailures` is empty.

18 of 19 runs were saved. LIVE-03 failed on all three attempts; see the liveness table.

## Method

- Compiled each `parallel.c` and `parallel_fixed.c` on its own (`-c`) with Apple clang and Homebrew libomp at `-O2`. `gcc-14` isn't installed here. The harnesses were neither built nor run.
- The "planted" column is my reading of the diff between the two files, checked against the requested type.
- "Analyser" records whether any report finding carries the requested `typeKey`.

## Summary

| | runs |
|---|---|
| saved | 18 of 19 |
| planted version doesn't compile | 5 |
| … of which the fixed version doesn't compile either (Parallelizer's fault) | 3 |
| compiles, and plants the requested failure | 5 |
| compiles, plant is weak or partly right | 3 |
| compiles, wrong failure or no effect | 5 |
| unchanged output | 0 |
| planted-only comments that point at the failure | 2 |
| declines | 0 |
| Analyser reports the requested type | 2 of 18 |
| runs with a report `typeKey` outside the taxonomy | 13 |

Each type was run once, so a single row says little about how reliably the pipeline handles that type.

## Safety — differential harness

| code | prompt | compiles | planted | Analyser |
|---|---|---|---|---|
| SAFE-01 incorrect synchronization | k-means | no — `nowait` isn't allowed on `parallel for` | Would be none anyway: the parallel region still ends with its own barrier. | no |
| SAFE-02 incorrect task partitioning | k-means | yes | Yes — `for (i = 0; i < N; i += 2)` updates only every other `prevAssign`. | no — filed as `safety` |
| SAFE-03 incorrect reductions | k-means | yes | Yes — `count[j] = ctot / nthreads` combines the per-thread counts wrongly. The output is wrong on every run, including with one thread. | found the bug, filed it as `safety` |
| SAFE-04 dependency assumptions | k-means | yes | No effect. It adds `cx[A[i]] += 0.0f; /* depend on updated assignments */`: adding zero never changes `cx`, and the comment gives it away. The note describes `A_prev[i] = A[i]`, which is in both versions. | no |
| SAFE-05 resource lifetime | BFS | yes | Weak — reads `localQueue[0]` into a `volatile` after `free(localQueue)`, then discards it. The output can't change. | no |
| SAFE-06 thread-unsafe components | pipeline | yes | Partly — `z ^= (uint64_t)rand()` reaches the output. But the output differs from the serial version even with one thread, so a differential check detects `rand()`'s value, not its race. | yes |
| SAFE-07 error / cancellation | pipeline | yes | Wrong failure — writes `0` to the total in place of `total_sum`. No error or cancellation path is involved. | no — filed as `incorrect-reductions` |
| SAFE-08 unintended nondeterminism | BFS | no — neither version compiles (malformed `atomic capture`) | `schedule(dynamic, 1)` vs `schedule(static)`. The fixed version has the same CAS/parent race. | no |

## Performance — scaling harness

| code | prompt | compiles | planted | Analyser |
|---|---|---|---|---|
| PERF-01 excessive synchronization | k-means | no — a `barrier` may not be nested inside an `omp for` | A barrier per loop iteration would be the right kind of failure. | no — filed as `performance` |
| PERF-02 poor partitioning | BFS | no — neither version compiles (OpenMP construct inside an `atomic`) | Inverted: `schedule(dynamic, 1)` planted and `static` fixed. Static is the split that leaves workers idle on power-law degrees. | no |
| PERF-03 oversubscription | pipeline | yes | Yes — `num_threads(4 * omp_get_max_threads())`. | no |
| PERF-04 memory bottlenecks | k-means | yes | Yes — a second full pass over `points` and `new_assignments` per iteration. It doubles both the sums and the counts, so the means don't change. It comes with the comment "Deliberately stress bandwidth". | found the extra pass, filed it as `performance` |
| PERF-05 shared-resource contention | BFS | yes | Yes — an empty `#pragma omp critical { ; }` on every edge. It works, but no engineer would write it by accident. | no — filed as `excessive-synchronization` |
| PERF-06 insufficient parallelism | k-means | yes | Wrong failure — `single` in place of `critical` merges only one thread's partial sums. The centroids come out wrong, which is a safety failure, not a slowdown. | found the serialised merge, filed it as `performance`; missed the wrong result |

## Liveness — timeout harness

| code | prompt | compiles | planted | Analyser |
|---|---|---|---|---|
| LIVE-01 deadlock | pipeline | no — calls `omp_malloc`, which isn't an OpenMP function; in both versions | Yes — the lock-order inversion the assignment table asked for: thread 0 takes A then B, thread 1 takes B then A. Barriers force the interleaving, so it hangs on every run with 2 or more threads. | no — filed as `liveness` |
| LIVE-02 livelock | pipeline | yes | Wrong failure — spins while its own `counts[sensorId]` is even. That count starts at 0, so it hangs on the first record every run. | no — filed as `liveness` |
| LIVE-03 starvation | BFS | — | Not saved: 500 on three attempts. One direct run showed the Planter returning `typeKey` `liveness-starvation`, which the Harness rejects. | — |
| LIVE-04 termination detection | BFS | yes | Partly — `if (qcur_n > 0) qcur_n -= 1;` drops one frontier vertex per level. The program finishes with wrong hops and parents, which a timeout harness can't detect. | yes |
| LIVE-05 progress assumptions | pipeline | yes | Wrong failure — thread 0 starts at `f == 0`, where it can never set `waitFlag`, so every thread spins forever on every run. | no — filed as `liveness` |

## Problems in the pipeline

1. The unchanged-output check never fired, and the model never declined. Every run changed the code, but SAFE-04 and SAFE-05 changed it in ways the output can't show. Rule 6 alone didn't prevent that.
2. The Planter introduces compile errors of its own. SAFE-01 and PERF-01 use OpenMP constructs the standard forbids. The build command in the prompt tells the model how the code is compiled, but nothing checks that it compiles.
3. The Parallelizer's output doesn't always compile. Three runs fail in the fixed version too, so the Planter started from broken code.
4. Two liveness plants hang on every run (LIVE-02, LIVE-05), and LIVE-01's forced deadlock depends only on having two or more threads. The notes for LIVE-02 and LIVE-05 claim a dependence on scheduling that the code doesn't have.
5. Quoting worked as a format. All 18 notes quote code, which made SAFE-04's mismatch visible: its quote is mostly lines that exist in both versions.
6. A wrong `typeKey` from the Planter surfaces as a bare 500 from the Harness (LIVE-03). Nothing checks the Planter's keys against the requested ones.
7. The Analyser reports the requested type in 2 of 18 runs, and 13 runs have findings keyed to something outside the taxonomy.

A compile check on both versions after the Parallelizer and after the Planter would have rejected 5 of these 18 runs.
