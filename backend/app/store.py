"""In-memory run storage — a plain module-level dict, lost on process restart.

Deliberate MVP simplification: no database. See ENGINEERING.md's API contract section.
"""

from .schemas import Run

_runs: dict[str, Run] = {}


def add_run(run: Run) -> None:
    _runs[run.id] = run


def list_runs() -> list[Run]:
    return sorted(_runs.values(), key=lambda r: r.created_at, reverse=True)


def get_run(run_id: str) -> Run | None:
    return _runs.get(run_id)


def clear_runs() -> None:
    _runs.clear()
