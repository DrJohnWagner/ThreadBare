from .schemas import HistoryRecord, Run


def to_history_record(run: Run) -> HistoryRecord:
    return HistoryRecord(
        id=run.id,
        created_at=run.created_at,
        language=run.request.language,
        requested_failure_modes=run.request.failure_modes,
        planted_type_keys=[f.type_key for f in run.planted_failures],
    )
