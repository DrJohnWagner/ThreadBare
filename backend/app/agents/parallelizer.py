"""Parallelizer agent — writes a correct OpenMP version of the serial reference.

See AGENTS.md ("3. Parallelizer"). Deliberately kept separate from the Failure
Planter: this stage's whole job is to produce a genuinely correct baseline, so that a
later stage corrupts a COPY of known-good code rather than generating broken code
directly from scratch.
"""

from .runner import call_agent
from .shared import THREADBARE_CONTEXT
from .types import ComputationSpec, FunctionOutput, FunctionSignature

OUTPUT_SCHEMA_PATH = "schemas/function-output.schema.json"
OUTPUT_MODEL = FunctionOutput

SYSTEM_PROMPT = f"""{THREADBARE_CONTEXT}

Your specific job: Parallelizer. You will be given a correct, single-threaded C \
function and the specification it implements. Write a CORRECT OpenMP-parallelized \
version of it with identical behavior. Nothing about this version should be wrong: a \
later, differently-instructed stage is responsible for deliberately introducing a \
concurrency failure into a COPY of your output. Your job here is only to make sure \
that copy starts from something that actually works.

Requirements:

- Same function signature as the serial version, except its name gets an "_omp" \
suffix in place of "_serial" (e.g. "histogram_serial" -> "histogram_omp").
- Must produce output identical to the serial version — exact, or \
tolerance-equivalent per the specification's correctness check — for any valid input, \
at any thread count, including a thread count of 1.
- Use real OpenMP parallelism that matches the computation's parallel structure from \
the specification (#pragma omp parallel for, reductions, etc.) — do not wrap the \
serial loop in a parallel region that does no useful parallel work. It should give a \
genuine speedup on multiple cores at the specification's problem sizes.
- Use correct synchronization, no more than necessary. This version must not exhibit \
any concurrency failure — it is the known-good baseline a later stage corrupts a copy \
of, not the artifact students will be shown as flawed.
- It must compile cleanly under `gcc -O2 -fopenmp -Wall -Wextra` with no warnings.
- Normal, professional comments only — nothing that narrates correctness reasoning \
for its own sake.

Respond with exactly two fields: "code" (the complete C source) and "signature" (its \
exact name, return type, and parameters, in order)."""


def build_user_prompt(
    *, spec: ComputationSpec, serial_code: str, serial_signature: FunctionSignature
) -> str:
    return f"""Serial reference implementation:
{serial_code}

Its signature:
{serial_signature.model_dump_json(indent=2, by_alias=True)}

Computation specification:
{spec.model_dump_json(indent=2, by_alias=True)}

Write the correct OpenMP-parallel version."""


def run(
    *, spec: ComputationSpec, serial_code: str, serial_signature: FunctionSignature
) -> FunctionOutput:
    return call_agent(
        system_prompt=SYSTEM_PROMPT,
        user_prompt=build_user_prompt(
            spec=spec, serial_code=serial_code, serial_signature=serial_signature
        ),
        output_model=FunctionOutput,
    )
