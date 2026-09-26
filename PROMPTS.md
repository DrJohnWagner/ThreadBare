# Source prompts and failure assignments

Used by `run-prompts.py`: one run per failure type, each with the prompt assigned below.

## The three prompts

### 1 · k-means

> Lloyd's k-means over one million two-dimensional points and 64 centroids. Each iteration assigns every point to its nearest centroid, recomputes each centroid as the mean of its members, then checks whether any assignment changed; repeat until none does.

### 2 · BFS

> Breadth-first search over a 5-million-edge power-law graph with about 310,000 vertices, recording the hop distance and parent of every reachable vertex. Build the graph once up front, then traverse from 20 different source vertices per measured run.

### 3 · Ingest pipeline

> A three-stage ingest pipeline over 200 sensor-log files of roughly 23,000 records each, generated from a seed rather than shipped. Stage one reads and decompresses a file, stage two decodes its records, stage three appends one summary row per file per sensor to a shared results table; a bounded queue sits between each pair of stages. Report a short fixed summary at the end and nothing per file or per record.

## Safety failures

| code | failure | prompt | why this prompt can show it |
|---|---|---|---|
| SAFE-01 | Incorrect synchronization | k-means | Every thread adds points into the same 64 centroid sums. Unsynchronised updates to those shared sums lose contributions, and the centroids come out wrong. |
| SAFE-02 | Incorrect task partitioning | k-means | The assignment loop splits one million points across threads. An off-by-one in the split skips or duplicates points, which changes cluster membership. |
| SAFE-03 | Incorrect reductions | k-means | Each centroid needs two reductions, sums and member counts. Combining them inconsistently gives a wrong mean even when each reduction is right on its own. |
| SAFE-04 | Dependency assumptions | k-means | Each iteration has three phases: assign, recompute, check. Overlapping the recompute with the phase that reads the centroids breaks a real dependency that looks like independent work. |
| SAFE-05 | Resource lifetime | BFS | The frontier grows level by level, which invites reallocating its buffer while workers still hold pointers into the old one. Not in the prompt: nothing requires a shared, reallocated frontier. gpt-6-sol declined this type because the natural result is a crash, not wrong output. |
| SAFE-06 | Thread-unsafe components | ingest pipeline | Stage two decodes records in several threads. A shared decoder handle, or a decoder with static scratch state, corrupts records that are decoded concurrently. |
| SAFE-07 | Error / cancellation handling | ingest pipeline | Files are read, decompressed and decoded, each of which can fail. Dropping a worker's error, or leaving the results table half-written after one, gives a wrong summary or a false success. Not in the prompt: it never mentions corrupt files, so the harness has to inject one. |
| SAFE-08 | Unintended nondeterminism | BFS | Many frontier vertices can discover the same vertex at the same level. Which parent wins depends on thread timing, so parents change from run to run while distances stay correct. |

## Performance failures

| code | failure | prompt | why this prompt can show it |
|---|---|---|---|
| PERF-01 | Excessive synchronization | k-means | A million updates to 64 shared accumulators per iteration tempt a lock per point. That serialises the main loop where a reduction would not. |
| PERF-02 | Poor partitioning or scheduling | BFS | Power-law degrees make work per vertex highly uneven. A static split of the frontier leaves most threads idle while a few process the hubs. |
| PERF-03 | Oversubscription | ingest pipeline | Three stages each want their own threads. Giving every stage a full thread team runs three times as many threads as cores. |
| PERF-04 | Memory bottlenecks | k-means | Per-thread accumulators for 64 centroids are small arrays. Packed together, several threads' accumulators share cache lines, and every update moves the line between cores. |
| PERF-05 | Shared-resource contention | BFS | Frontiers are built and grown on every level of 20 traversals. Allocating from one heap, or appending to one shared structure, makes the threads queue on the same lock. |
| PERF-06 | Insufficient parallelism | k-means | Each iteration ends with a convergence check before the next can start. A serial check, or serial recompute, between parallel phases caps the speedup. |

## Liveness failures

| code | failure | prompt | why this prompt can show it |
|---|---|---|---|
| LIVE-01 | Deadlock | ingest pipeline | The pipeline holds two kinds of lock: queue locks and the results-table lock. Two stages taking them in opposite orders can each hold what the other needs. |
| LIVE-02 | Livelock | ingest pipeline | Bounded queues fill and empty. Producers that retry on a full queue while consumers back off on an empty one can keep running without moving data. Not in the prompt: no retry or back-off protocol is asked for. gpt-6-sol declined this type because the pipeline it was given had none. |
| LIVE-03 | Starvation | BFS | Uneven traversal work invites work-stealing, and a biased choice of which thread to steal from can leave one thread without work indefinitely. Not in the prompt: work-stealing isn't asked for, and 20 independent traversals need no shared resource. gpt-6-sol declined this type for that reason. |
| LIVE-04 | Termination detection | BFS | Each traversal stops when its frontier is empty. Deciding that too early — while other threads are still adding vertices, or from another traversal's state — stops the search with vertices unvisited. |
| LIVE-05 | Progress assumptions | ingest pipeline | Stages wait on each other through queues. Spin-waiting that assumes the runtime will schedule the thread being waited on can hang when every thread is busy waiting. |
