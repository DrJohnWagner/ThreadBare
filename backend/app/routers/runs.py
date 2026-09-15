from fastapi import APIRouter, HTTPException, Response

from .. import store
from ..agents.pipeline import generate_run
from ..conversions import to_history_record
from ..schemas import GenerationRequest, HistoryRecord, Run
from ..zips import build_history_zip, build_run_zip

router = APIRouter()


@router.post("/api/runs", response_model=Run)
def create_run(body: GenerationRequest) -> Run:
    run = generate_run(body)
    store.add_run(run)
    return run


@router.get("/api/runs", response_model=list[HistoryRecord])
def list_runs() -> list[HistoryRecord]:
    return [to_history_record(r) for r in store.list_runs()]


@router.delete("/api/runs", status_code=204)
def clear_runs() -> None:
    store.clear_runs()


@router.get("/api/runs/export")
def export_runs() -> Response:
    data = build_history_zip(store.list_runs())
    return Response(
        content=data,
        media_type="application/zip",
        headers={
            "Content-Disposition": 'attachment; filename="threadbare-history.zip"'
        },
    )


@router.get("/api/runs/{run_id}", response_model=Run)
def get_run(run_id: str) -> Run:
    run = store.get_run(run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    return run


@router.get("/api/runs/{run_id}/download")
def download_run(run_id: str) -> Response:
    run = store.get_run(run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    data = build_run_zip(run)
    return Response(
        content=data,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="{run.id}.zip"'},
    )
