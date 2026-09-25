"""Failure Planter agent — corrupts a copy of the correct parallel version.

See AGENTS.md ("4. Failure Planter"). This is the core of the product: given a known-
correct OpenMP function, introduce one or more specific, requested concurrency
failures with nothing in the code hinting at them, and report back exactly what was
planted, using the same PlantedFailure shape stored on Run.plantedFailures.
"""

from .runner import call_agent
from .shared import BUILD_COMMAND, THREADBARE_CONTEXT, format_taxonomy
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
4. Plant exactly the requested failure mode(s), nothing else. Every failure except a \
liveness failure must leave the program terminating normally.
5. The result must compile with exactly this command, and the planted failure must \
survive it: {BUILD_COMMAND}. At -O2 the compiler deletes code whose result is never \
used, so a change that only adds work nobody reads plants nothing.
6. The failure must be observable by running the program. A safety failure must change \
the function's output on at least some runs. A performance failure must make it \
measurably slower, or scale worse as threads are added. A liveness failure must stop \
it finishing on at least some runs. A change that output, timing and termination \
cannot reveal — for example, calling a thread-unsafe function and discarding its \
result — plants nothing.
7. A liveness failure must depend on thread scheduling or thread count, not hang on \
every run regardless. A loop waiting for a condition that can never become true is a \
plain infinite loop, not any of the liveness types. Deadlock: threads each hold \
something another needs, for example two locks taken in opposite orders. Livelock: \
threads keep running and changing state in response to each other but never advance. \
Starvation: the program as a whole progresses while one thread is repeatedly denied \
what it waits for. Incorrect termination detection: the program decides it has \
finished too early, or fails to notice that it has. Broken progress assumptions: the \
code relies on a scheduling or fairness guarantee that OpenMP does not make.
8. If a requested failure mode cannot be planted naturally — this computation has no \
structure it could apply to — do not invent one and do not describe existing code as \
a planted change. Return the correct code unchanged, an empty "plantedFailures" and \
explain why in "declineReason".
9. If you are given no specific failure modes (an empty list), choose one yourself \
from the taxonomy below that is clearly applicable to this particular computation's \
actual structure. Do not force a failure that doesn't fit — for example, don't plant \
a false-sharing failure into a computation that has no per-thread scalar accumulators.

For every failure mode you actually plant, report back one entry in \
"plantedFailures", with:

- typeKey — the exact taxonomy key, verbatim, from the taxonomy below.
- implementationNote — for an instructor who will see this next to the answer key. \
First quote, verbatim and in backticks, every line you added or changed to plant this \
failure, exactly as it appears in your code. Then, in one or two sentences, explain \
mechanically why that change causes this specific failure: name the missing barrier, \
the racing variables, the scheduling clause, or whatever the actual mechanism is. Do \
not just restate the taxonomy's description of the category.

Taxonomy of concurrency failures you may choose from — "typeKey" must be one of the \
"key" values below, verbatim:
{format_taxonomy()}

Respond with exactly three fields: "code" (the complete corrupted C source), \
"plantedFailures" (one entry per failure mode actually planted, as described above) \
and "declineReason" (an empty string unless you declined under rule 8)."""


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
