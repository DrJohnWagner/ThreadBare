from fastapi import APIRouter

from .. import store
from ..schemas import CategoryCounts, DashboardStats
from ..taxonomy import get_category_for_type

router = APIRouter()


@router.get("/api/dashboard", response_model=DashboardStats)
def get_dashboard() -> DashboardStats:
    runs = store.list_runs()
    total = len(runs)
    fixed_count = sum(1 for r in runs if r.fixed)

    by_category = {"safety": 0, "performance": 0, "liveness": 0}
    by_type: dict[str, int] = {}
    for run in runs:
        for bug in run.planted_bugs:
            by_type[bug.type_key] = by_type.get(bug.type_key, 0) + 1
            category = get_category_for_type(bug.type_key)
            if category:
                by_category[category] += 1

    return DashboardStats(
        total_runs=total,
        fixed_count=fixed_count,
        fix_rate=(fixed_count / total) if total else None,
        by_category=CategoryCounts(**by_category),
        by_type=by_type,
    )
