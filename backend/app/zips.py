"""Builds the downloadable .zip archives for a run and for the full history.

Stdlib zipfile — no hand-rolled archive format needed on this side, unlike the frontend
mock's dependency-free client-side ZIP writer, which this endpoint replaces entirely.
"""

import io
import json
import zipfile

from .schemas import Run


def _write_run_files(zf: zipfile.ZipFile, run: Run, prefix: str = "") -> None:
    zf.writestr(f"{prefix}serial.c", run.serial_reference)
    zf.writestr(f"{prefix}parallel.c", run.parallel_version)
    zf.writestr(f"{prefix}harness.c", run.test_harness)
    if run.fixed:
        zf.writestr(f"{prefix}parallel_fixed.c", run.parallel_version_fixed)
    zf.writestr(
        f"{prefix}request.json", run.request.model_dump_json(indent=2, by_alias=True)
    )


def build_run_zip(run: Run) -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        _write_run_files(zf, run)
    return buf.getvalue()


def build_history_zip(runs: list[Run]) -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        summary = [
            {
                "id": r.id,
                "createdAt": r.created_at,
                "plantedTypeKeys": [b.type_key for b in r.planted_bugs],
                "fixed": r.fixed,
            }
            for r in runs
        ]
        zf.writestr("history-summary.json", json.dumps(summary, indent=2))
        for idx, run in enumerate(runs, start=1):
            _write_run_files(zf, run, prefix=f"run-{idx}-{run.id}/")
    return buf.getvalue()
