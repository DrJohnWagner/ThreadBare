"""Runs the full generation pipeline end to end: Intake -> Serial Reference ->
Parallelizer -> Failure Planter -> Harness -> Analyser -> a fully assembled Run.

Any agent failing — a bad response, a validation error, an API error — fails the
whole thing. No partial Run is ever persisted; see AGENTS.md's "Mid-pipeline failure"
decision. The caller (backend/app/routers/runs.py) doesn't catch anything either — an
exception here becomes a plain 500.

URL fetching is not wired in yet (see agents/url_fetch.py) — sourceMaterial.url is
accepted by the request schema but its content is never fetched or passed to Intake.
"""

from datetime import UTC, datetime
from uuid import uuid4

from ..schemas import GenerationRequest, Run
from . import analyser, failure_planter, harness, intake, parallelizer, serial_reference


def generate_run(request: GenerationRequest) -> Run:
    source = request.source_material
    spec = intake.run(
        code=source.code if source else None,
        text=source.text if source else None,
        url_content=None,
    )

    serial = serial_reference.run(spec=spec)

    correct_parallel = parallelizer.run(
        spec=spec,
        serial_code=serial.code,
        serial_signature=serial.signature,
    )

    planted = failure_planter.run(
        spec=spec,
        correct_code=correct_parallel.code,
        signature=correct_parallel.signature,
        serial_code=serial.code,
        failure_type_keys=request.failure_modes,
    )

    test_harness = harness.run(
        spec=spec,
        serial_signature=serial.signature,
        parallel_signature=correct_parallel.signature,
        planted_failures=planted.planted_failures,
    )

    analysis = analyser.run(
        serial_reference=serial.code,
        parallel_version=planted.code,
        test_harness=test_harness.code,
    )

    return Run(
        id=str(uuid4()),
        created_at=datetime.now(UTC).isoformat(),
        request=request,
        serial_reference=serial.code,
        parallel_version=planted.code,
        parallel_version_fixed=correct_parallel.code,
        test_harness=test_harness.code,
        planted_failures=planted.planted_failures,
        report=analysis.report,
    )
