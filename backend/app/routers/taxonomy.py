from fastapi import APIRouter

from ..schemas import Taxonomy
from ..taxonomy import get_taxonomy

router = APIRouter()


@router.get("/api/taxonomy", response_model=Taxonomy)
def read_taxonomy() -> Taxonomy:
    return get_taxonomy()
