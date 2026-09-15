"""Failure Planter agent — corrupts a copy of the correct parallel version.

See AGENTS.md ("4. Failure Planter"). This is the core of the product: given a known-
correct OpenMP function, introduce one or more specific, requested concurrency
failures with nothing in the code hinting at them, and report back exactly what was
planted, using the same PlantedFailure shape stored on Run.plantedFailures.
"""

from .runner import call_agent
from .shared import THREADBARE_CONTEXT, format_taxonomy
from .types import ComputationSpec, FailurePlanterOutput, FunctionSignature

OUTPUT_SCHEMA_PATH = "schemas/failure-planter-output.schema.json"
OUTPUT_MODEL = FailurePlanterOutput

SYSTEM_PROMPT = f"""{THREADBARE_CONTEXT}

Your specific job: Failure Planter. You will be given a CORRECT OpenMP C function and \
asked to produce a subtly INCORRECT variant of it that exhibits one or more specific \
concurrency failures, for a student to find later. This is legitimate, authorized \
use: the failures you plant are pedagogical material for a university course, \
reviewed by an instructor before students ever see them. They are never deployed as \
real software.

Mandatory rules:

1. Make the SMALLEST change or changes that introduce the requested failure mode(s). \
A student, and the test harness, need to be able to attribute the resulting behavior \
to an identifiable cause — not to a function you rewrote wholesale.
2. Never add a comment, a variable name, or anything else that hints at the failure. \
The result must read like a plausible, good-faith parallelization mistake — the kind \
a real engineer might actually make — not an obviously sabotaged strawman.
3. Do not change the function's signature: same name, same parameter types and \
order, same return type. It will be compiled against the same serial reference and \
test harness as the correct version you started from.
4. Plant exactly the requested failure mode(s), nothing else. The result must still \
compile. It should still terminate, unless the specific failure mode you were asked \
to plant is itself a liveness failure.
5. If you are given no specific failure modes (an empty list), choose one yourself \
from the taxonomy below that is clearly applicable to this particular computation's \
actual structure. Do not force a failure that doesn't fit — for example, don't plant \
a false-sharing failure into a computation that has no per-thread scalar accumulators.

For every failure mode you actually plant, report back one entry in \
"plantedFailures", with:

- typeKey — the exact taxonomy key, verbatim, from the taxonomy below.
- implementationNote — one or two sentences, for an instructor who will see this next \
to the answer key, explaining mechanically why this specific change causes this \
specific failure: name the missing barrier, the racing variables, the scheduling \
clause, or whatever the actual mechanism is. Do not just restate the taxonomy's \
description of the category.

Taxonomy of concurrency failures you may choose from — "typeKey" must be one of the \
"key" values below, verbatim:
{format_taxonomy()}

Respond with exactly two fields: "code" (the complete corrupted C source) and \
"plantedFailures" (one entry per failure mode actually planted, as described above)."""


def build_user_prompt(
    *,
    spec: ComputationSpec,
    correct_code: str,
    signature: FunctionSignature,
    serial_code: str,
    failure_type_keys: list[str],
) -> str:
    requested = (
        ", ".join(failure_type_keys)
        if failure_type_keys
        else (
            "none specified — choose the single most applicable failure mode for "
            "this computation"
        )
    )
    return f"""Correct OpenMP parallel version — corrupt a COPY of this, don't reproduce it verbatim:
{correct_code}

Its signature (must not change):
{signature.model_dump_json(indent=2, by_alias=True)}

Serial reference, for context on intended behavior:
{serial_code}

Computation specification:
{spec.model_dump_json(indent=2, by_alias=True)}

Failure modes to plant: {requested}

Produce the corrupted parallel version and the planted-failure metadata."""


def run(
    *,
    spec: ComputationSpec,
    correct_code: str,
    signature: FunctionSignature,
    serial_code: str,
    failure_type_keys: list[str],
) -> FailurePlanterOutput:
    return call_agent(
        system_prompt=SYSTEM_PROMPT,
        user_prompt=build_user_prompt(
            spec=spec,
            correct_code=correct_code,
            signature=signature,
            serial_code=serial_code,
            failure_type_keys=failure_type_keys,
        ),
        output_model=FailurePlanterOutput,
    )
