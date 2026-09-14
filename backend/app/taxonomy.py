"""Loads the canonical taxonomy from schemas/taxonomy.json — the repo root's schemas/
directory, not a backend-local copy. Single source of truth: see schemas/README.md.
"""

import json
from functools import lru_cache
from pathlib import Path

from .schemas import FailureCategory, Taxonomy

TAXONOMY_PATH = Path(__file__).resolve().parents[2] / "schemas" / "taxonomy.json"


@lru_cache
def get_taxonomy() -> Taxonomy:
    with TAXONOMY_PATH.open() as f:
        data = json.load(f)
    return Taxonomy.model_validate(data)


@lru_cache
def get_category_for_type(type_key: str) -> FailureCategory | None:
    for category in get_taxonomy().categories:
        for item in category.items:
            if item.key == type_key:
                return item.category
    return None
