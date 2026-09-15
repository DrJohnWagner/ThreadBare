"""Serial Reference agent — writes the correct single-threaded C oracle.

See AGENTS.md ("2. Serial Reference").
"""

from .runner import call_agent
from .shared import THREADBARE_CONTEXT
from .types import ComputationSpec, FunctionOutput

OUTPUT_SCHEMA_PATH = "schemas/function-output.schema.json"
OUTPUT_MODEL = FunctionOutput

SYSTEM_PROMPT = f"""{THREADBARE_CONTEXT}

Your specific job: Serial Reference. You will be given a computation specification — \
name, description, input strategy, correctness check, parallel decomposition, problem \
sizes, and assumptions — produced by an earlier stage. Write a correct, \
single-threaded C implementation of exactly that computation. This implementation is \
the oracle every other version of this computation gets checked against, and it will \
later be compiled together with a test harness written by a different stage — so it \
must be a plain, linkable function, not a program with its own main().

Requirements:

- Write a single function (plus any small private helper functions it needs). Name \
the primary function "<name>_serial", where <name> is the computation's name from the \
specification, e.g. "histogram_serial".
- The function takes all inputs as parameters (no global state, no file or console \
I/O, no printf) and either returns the result or writes it through an output \
parameter — whichever makes it straightforward for a test harness to compare its \
result against another version's result later.
- It must compile cleanly under `gcc -O2 -Wall -Wextra` with no warnings.
- Implement exactly the computation described in the specification. Do not add \
functionality the specification doesn't call for, do not "improve" on the \
specification, and do not add comments beyond what a competent C programmer would \
write for code this straightforward.
- Standard library only. No OpenMP pragmas, no threads, no #include <omp.h> — this \
file must compile without -fopenmp.
- State the exact function signature you chose — its name, return type, and every \
parameter's type and name, in order — as the structured "signature" field. A later \
stage calls this function by that signature without re-reading your source, so it \
must be complete and exact.

Respond with exactly two fields: "code" (the complete C source) and "signature" (the \
structured signature described above)."""


def build_user_prompt(*, spec: ComputationSpec) -> str:
    return f"""Computation specification:
{spec.model_dump_json(indent=2, by_alias=True)}

Write the serial reference implementation."""


def run(*, spec: ComputationSpec) -> FunctionOutput:
    return call_agent(
        system_prompt=SYSTEM_PROMPT,
        user_prompt=build_user_prompt(spec=spec),
        output_model=FunctionOutput,
    )
