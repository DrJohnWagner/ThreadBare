"""Pydantic models for agent structured outputs — mirror schemas/*.schema.json exactly.

See schemas/README.md for why function-output.schema.json, failure-planter-output, etc.
exist as their own files rather than ad hoc shapes: these classes ARE those schemas,
typed for Python, the same way backend/app/schemas.py types the request/response
schemas. FailurePlanterOutput and AnalyserOutput reuse PlantedFailure and ReportFinding
directly rather than redefining their shape.
"""

import re
from typing import Literal

from pydantic import BaseModel, ConfigDict, field_validator
from pydantic.alias_generators import to_camel

from ..schemas import PlantedFailure, ReportFinding


class CamelModel(BaseModel):
    model_config = ConfigDict(alias_generator=to_camel, populate_by_name=True)


# A handful of C keywords worth guarding against as a whole identifier — cheap
# insurance against the Intake agent picking a computation name that happens to
# collide with one (e.g. "sort" is fine, "for" is not).
_C_KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "int",
    "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile",
    "while",
}  # fmt: skip

_VALID_IDENTIFIER = re.compile(r"^[a-z][a-z0-9_]*$")


def sanitize_computation_name(raw: str) -> str:
    """Coerce an arbitrary string into a safe fragment for a C identifier.

    ComputationSpec.name is used verbatim to build real function names
    (`<name>_serial`, `<name>_omp`) in three downstream agent prompts. The schema
    can't enforce this with a `pattern` — OpenAI's Structured Outputs strict mode
    doesn't reliably support it (see schemas/README.md) — so it's enforced here
    instead, every time a ComputationSpec is constructed, whether that's from a real
    model response or a test fixture.
    """
    slug = re.sub(r"[^a-z0-9]+", "_", raw.strip().lower()).strip("_")
    if not slug or not slug[0].isalpha():
        slug = f"computation_{slug}" if slug else "computation"
    if slug in _C_KEYWORDS:
        slug = f"{slug}_computation"
    if not _VALID_IDENTIFIER.match(slug):
        # Should be unreachable given the substitution above, but never silently
        # hand a later agent an identifier that won't compile.
        raise ValueError(
            f"could not derive a valid C identifier from computation name {raw!r}"
        )
    return slug


# --- function-signature.schema.json / function-output.schema.json -------------------


class FunctionParameter(CamelModel):
    type: str
    name: str


class FunctionSignature(CamelModel):
    name: str
    return_type: str
    parameters: list[FunctionParameter]


class FunctionOutput(CamelModel):
    """Structured output shared by the Serial Reference and Parallelizer agents."""

    code: str
    signature: FunctionSignature


# --- computation-spec.schema.json -----------------------------------------------------


class ComputationSpec(CamelModel):
    """Structured output of the Intake agent."""

    name: str
    description: str
    input_description: str
    correctness_check: str
    parallel_decomposition: str
    problem_sizes: list[int]
    assumptions: list[str]

    @field_validator("name")
    @classmethod
    def _sanitize_name(cls, value: str) -> str:
        return sanitize_computation_name(value)


# --- failure-planter-output.schema.json -----------------------------------------------


class FailurePlanterOutput(CamelModel):
    """Structured output of the Failure Planter agent."""

    code: str
    planted_failures: list[PlantedFailure]


# --- harness-output.schema.json -------------------------------------------------------


class HarnessOutput(CamelModel):
    """Structured output of the Harness agent."""

    code: str
    check_strategy: Literal["differential", "scaling", "timeout"]


# --- analyser-output.schema.json ------------------------------------------------------


class AnalyserOutput(CamelModel):
    """Structured output of the Analyser agent."""

    report: list[ReportFinding]
