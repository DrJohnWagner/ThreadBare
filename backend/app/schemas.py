"""Pydantic models mirroring schemas/*.schema.json.

These are hand-written, not generated, for now — see schemas/README.md. Field names are
snake_case in Python and camelCase on the wire (via CamelModel's alias generator), so
they match the JSON Schema field names exactly in every request/response body.
"""

from typing import Literal, Optional

from pydantic import BaseModel, ConfigDict
from pydantic.alias_generators import to_camel

FailureCategory = Literal["safety", "performance", "liveness"]


class CamelModel(BaseModel):
    model_config = ConfigDict(alias_generator=to_camel, populate_by_name=True)


# --- taxonomy.schema.json -----------------------------------------------------------


class TaxonomyItem(CamelModel):
    key: str
    label: str
    category: FailureCategory
    summary: str
    description: str


class TaxonomyCategory(CamelModel):
    key: FailureCategory
    label: str
    blurb: str
    items: list[TaxonomyItem]


class Taxonomy(CamelModel):
    categories: list[TaxonomyCategory]


# --- generation-request.schema.json --------------------------------------------------


class SourceMaterial(CamelModel):
    code: Optional[str] = None
    text: Optional[str] = None
    url: Optional[str] = None


class GenerationRequest(CamelModel):
    language: Literal["c-openmp"]
    failure_modes: list[str] = []
    source_material: Optional[SourceMaterial] = None


# --- planted-bug.schema.json / report-finding.schema.json ---------------------------


class PlantedBug(CamelModel):
    type_key: str
    implementation_note: str


class ReportFinding(CamelModel):
    type_key: str
    lines: list[int]
    explanation: str


# --- run.schema.json ------------------------------------------------------------------


class Run(CamelModel):
    id: str
    created_at: str
    request: GenerationRequest
    serial_reference: str
    parallel_version: str
    parallel_version_fixed: str
    test_harness: str
    planted_bugs: list[PlantedBug]
    report: list[ReportFinding]
    fixed: bool = False


# --- history-record.schema.json --------------------------------------------------------


class HistoryRecord(CamelModel):
    id: str
    created_at: str
    language: Literal["c-openmp"]
    requested_failure_modes: list[str]
    planted_type_keys: list[str]
    fixed: bool


# --- dashboard-stats.schema.json -------------------------------------------------------


class CategoryCounts(CamelModel):
    safety: int = 0
    performance: int = 0
    liveness: int = 0


class DashboardStats(CamelModel):
    total_runs: int
    fixed_count: int
    fix_rate: Optional[float]
    by_category: CategoryCounts
    by_type: dict[str, int]


# --- request bodies with no dedicated schema file -------------------------------------


class PatchRunBody(CamelModel):
    fixed: bool
