"""Generate one run per failure type against a running backend and save each run's zip.

Each of the 19 taxonomy failure types is paired with one of three source prompts. For
each pair this POSTs /api/runs, then GETs /api/runs/{id}/download and writes the zip to
the given directory as <code>-<type key>.zip. A zip that already exists is skipped, so
rerunning after a failure resumes where it stopped. A failed run is reported and the
script moves on to the next.

Usage (backend already running): .venv/bin/python run-prompts.py runs/<name>
"""

import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

API = "http://localhost:8000/api"

KMEANS = (
    "Lloyd's k-means over one million two-dimensional points and 64 centroids. Each "
    "iteration assigns every point to its nearest centroid, recomputes each centroid "
    "as the mean of its members, then checks whether any assignment changed; repeat "
    "until none does."
)
BFS = (
    "Breadth-first search over a 5-million-edge power-law graph with about 310,000 "
    "vertices, recording the hop distance and parent of every reachable vertex. Build "
    "the graph once up front, then traverse from 20 different source vertices per "
    "measured run."
)
PIPELINE = (
    "A three-stage ingest pipeline over 200 sensor-log files of roughly 23,000 records "
    "each, generated from a seed rather than shipped. Stage one reads and decompresses "
    "a file, stage two decodes its records, stage three appends one summary row per "
    "file per sensor to a shared results table; a bounded queue sits between each pair "
    "of stages. Report a short fixed summary at the end and nothing per file or per "
    "record."
)

# (code, taxonomy type key, prompt)
ASSIGNMENTS = [
    ("SAFE-01", "incorrect-synchronization", KMEANS),
    ("SAFE-02", "incorrect-task-partitioning", KMEANS),
    ("SAFE-03", "incorrect-reductions", KMEANS),
    ("SAFE-04", "incorrect-dependency-assumptions", KMEANS),
    ("SAFE-05", "unsafe-resource-lifetime", BFS),
    ("SAFE-06", "thread-unsafe-components", PIPELINE),
    ("SAFE-07", "incorrect-error-handling", PIPELINE),
    ("SAFE-08", "unintended-nondeterminism", BFS),
    ("PERF-01", "excessive-synchronization", KMEANS),
    ("PERF-02", "poor-task-partitioning-scheduling", BFS),
    ("PERF-03", "oversubscription", PIPELINE),
    ("PERF-04", "memory-bottlenecks", KMEANS),
    ("PERF-05", "shared-resource-contention", BFS),
    ("PERF-06", "insufficient-parallelism", KMEANS),
    ("LIVE-01", "deadlock", PIPELINE),
    ("LIVE-02", "livelock", PIPELINE),
    ("LIVE-03", "starvation", BFS),
    ("LIVE-04", "incorrect-termination-detection", BFS),
    ("LIVE-05", "broken-progress-assumptions", PIPELINE),
]


def check_type_keys() -> None:
    """Fail before spending any model calls if a key isn't in the server's taxonomy."""
    with urllib.request.urlopen(f"{API}/taxonomy") as response:
        taxonomy = json.load(response)
    known = {item["key"] for c in taxonomy["categories"] for item in c["items"]}
    unknown = [key for _, key, _ in ASSIGNMENTS if key not in known]
    if unknown:
        sys.exit(f"not in the server's taxonomy: {', '.join(unknown)}")


def create_run(type_key: str, prompt: str) -> str:
    body = json.dumps(
        {
            "language": "c-openmp",
            "failureModes": [type_key],
            "sourceMaterial": {"text": prompt},
        }
    ).encode()
    request = urllib.request.Request(
        f"{API}/runs", data=body, headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(request) as response:
        return json.load(response)["id"]


def download_run(run_id: str) -> bytes:
    with urllib.request.urlopen(f"{API}/runs/{run_id}/download") as response:
        return response.read()


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("usage: run-prompts.py <output directory>")
    out_dir = Path(sys.argv[1])
    check_type_keys()
    out_dir.mkdir(parents=True, exist_ok=True)
    failed = []
    for code, type_key, prompt in ASSIGNMENTS:
        path = out_dir / f"{code}-{type_key}.zip"
        if path.exists():
            print(f"{code}: exists, skipping", flush=True)
            continue
        print(f"{code}: generating...", flush=True)
        started = time.monotonic()
        try:
            run_id = create_run(type_key, prompt)
            path.write_bytes(download_run(run_id))
        except OSError as exc:
            detail = (
                exc.read().decode() if isinstance(exc, urllib.error.HTTPError) else exc
            )
            print(f"{code}: FAILED — {detail}", flush=True)
            failed.append(code)
            continue
        print(
            f"{code}: saved {path.name} ({time.monotonic() - started:.0f}s)", flush=True
        )

    if failed:
        sys.exit(f"{len(failed)} failed: {', '.join(failed)} — rerun to retry them")
    print(f"all {len(ASSIGNMENTS)} runs saved to {out_dir}")


if __name__ == "__main__":
    main()
