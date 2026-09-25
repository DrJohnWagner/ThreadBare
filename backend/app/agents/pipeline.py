"""Runs the full generation pipeline end to end: Intake -> Serial Reference ->
Parallelizer -> Failure Planter -> Harness -> Analyser -> a fully assembled Run.

Any agent failing — a bad response, a validation error, an API error — fails the
whole thing. No partial Run is ever persisted; see AGENTS.md's "Mid-pipeline failure"
decision. The caller (backend/app/routers/runs.py) doesn't catch anything either — an
exception here becomes a plain 500.

URL fetching is not wired in yet (see agents/url_fetch.py) — sourceMaterial.url is
accepted by the request schema but its content is never fetched or passed to Intake.
"""

import time
from contextlib import contextmanager
from datetime import UTC, datetime
from uuid import uuid4

from ..schemas import GenerationRequest, Run
from . import analyser, failure_planter, harness, intake, parallelizer, serial_reference
from .runner import AgentCallError


@contextmanager
def _stage(name: str):
    """Print progress to the console around one agent call.

    The free/slow models this pipeline sometimes runs against can take minutes per
    call — without this, a long run looks identical to a hung one.
    """
    print(f"[pipeline] {name}: calling...", flush=True)
    started = time.monotonic()
    yield
    print(f"[pipeline] {name}: done ({time.monotonic() - started:.1f}s)", flush=True)


def generate_run(request: GenerationRequest) -> Run:
    source = request.source_material
    with _stage("Intake"):
        spec = intake.run(
            code=source.code if source else None,
            text=source.text if source else None,
            url_content=None,
        )

    with _stage("Serial Reference"):
        serial = serial_reference.run(spec=spec)

    with _stage("Parallelizer"):
        correct_parallel = parallelizer.run(
            spec=spec,
            serial_code=serial.code,
            serial_signature=serial.signature,
        )

    with _stage("Failure Planter"):
        planted = failure_planter.run(
            spec=spec,
            correct_code=correct_parallel.code,
            signature=correct_parallel.signature,
            serial_code=serial.code,
            failure_type_keys=request.failure_modes,
        )

    if not planted.planted_failures:
        raise AgentCallError(
            f"Failure Planter declined to plant: {planted.decline_reason}"
        )
    if "".join(planted.code.split()) == "".join(correct_parallel.code.split()):
        raise AgentCallError(
            "Failure Planter reported planted failures but returned the correct "
            "parallel version unchanged (ignoring whitespace)"
        )

    with _stage("Harness"):
        test_harness = harness.run(
            spec=spec,
            serial_signature=serial.signature,
            parallel_signature=correct_parallel.signature,
            planted_failures=planted.planted_failures,
        )

    with _stage("Analyser"):
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
