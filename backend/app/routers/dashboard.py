from fastapi import APIRouter

from .. import store
from ..schemas import CategoryCounts, DashboardStats
from ..taxonomy import get_category_for_type

router = APIRouter()


@router.get("/api/dashboard", response_model=DashboardStats)
def get_dashboard() -> DashboardStats:
    runs = store.list_runs()

    by_category = {"safety": 0, "performance": 0, "liveness": 0}
    by_type: dict[str, int] = {}
    for run in runs:
        for failure in run.planted_failures:
            by_type[failure.type_key] = by_type.get(failure.type_key, 0) + 1
            category = get_category_for_type(failure.type_key)
            if category:
                by_category[category] += 1

    return DashboardStats(
        total_runs=len(runs),
        by_category=CategoryCounts(**by_category),
        by_type=by_type,
    )
