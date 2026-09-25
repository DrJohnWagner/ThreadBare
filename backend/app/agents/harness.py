"""Harness agent — writes harness.c, a real compilable driver.

See AGENTS.md ("5. Harness"). Chooses a differential, scaling, or timeout check
strategy based on the planted failure's category, matching the convention originally
worked out by hand in the six examples this agent replaces.

Raises ValueError before ever calling the model if a planted failure's typeKey isn't
a real taxonomy key, or if the planted failures span more than one category — the
prompt below only knows how to pick a single strategy per run.
"""

from ..schemas import PlantedFailure
from ..taxonomy import get_category_for_type
from .runner import call_agent
from .shared import BUILD_COMMAND, THREADBARE_CONTEXT
from .types import ComputationSpec, FunctionSignature, HarnessOutput

OUTPUT_SCHEMA_PATH = "schemas/harness-output.schema.json"
OUTPUT_MODEL = HarnessOutput

SYSTEM_PROMPT = f"""{THREADBARE_CONTEXT}

Your specific job: Harness. You will be given a serial reference function's \
signature, a parallel version's signature (that version deliberately contains one or \
more planted concurrency failures), a computation specification, and the ground \
truth for what was planted and in which failure category. Write harness.c: a \
standalone C program with its own main() that actually demonstrates whether the \
planted failure is present — not a description of how a person could check.

Requirements:

1. Start the file with a comment header in exactly this style:

   /* harness.c — <differential|scaling|timeout> driver for <computation name> (generated)
    *
    *   build: {BUILD_COMMAND}
    *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
    */

   The two files you link against are always named exactly serial.c and parallel.c, \
regardless of the computation's name — never use computation-specific filenames in \
the build line, because the files really will be named that on disk.

2. Forward-declare the serial and parallel functions yourself, matching their real \
signatures exactly — do not assume or require a header file.
3. Generate deterministic synthetic input yourself, using a fixed-seed PRNG or a \
formula, following the computation specification's input-generation strategy. Never \
read stdin or an external file — the harness must be fully self-contained and produce \
the same result on every run.
4. Choose your check strategy from the planted failure's category:
   - Category "safety": use a DIFFERENTIAL check. Call the serial reference once for \
ground truth, then call the parallel version across repetitions (a --reps flag, \
default 15-30), comparing its full output against the reference each time and \
printing a "[check]" line reporting the first point of divergence whenever they \
mismatch. Default the problem size (a --n flag) to the largest value in the \
computation specification's problemSizes, unless that's impractically slow for the \
repetition count above — that field was already chosen to make the failure \
observable within a few seconds.
   - Category "performance": use a SCALING check. Before timing anything, run one \
untimed warmup pass over the input (call the kernel once and discard the result, or \
explicitly touch every element of the input arrays) so first-touch page faults and \
cold-cache costs land outside the measurements — otherwise the serial baseline, which \
runs first, absorbs that one-time cost and the apparent speedup is inflated by it, \
not by anything the parallel version actually did. Then time the serial reference \
once, then time the parallel version at increasing thread counts (1, 2, 4, ... up to \
omp_get_max_threads()), printing a "[scale]" line per thread count with elapsed time, \
speedup, and efficiency. Do one light correctness check only, since the output \
should already be correct — the point is that it's slow, not wrong. Base the \
pass/fail verdict on parallel efficiency (speedup ÷ thread count) at the highest \
thread count tested, not on absolute elapsed time — efficiency is machine-independent, \
elapsed time isn't. Choose a numeric efficiency threshold appropriate to how severe \
this specific failure mode's slowdown should be.
   - Category "liveness": the parallel call may hang forever and never return. Run \
it under a hard wall-clock timeout (alarm() or a watchdog thread) and treat hitting \
that timeout itself as the failure signal — a differential or scaling check alone \
would just hang the whole harness process.
5. End with exactly one line: printf("[verdict] %s\\n", failures ? "<CODE> exposed" \
: "<pass message>"); and return 1 if the failure was detected, 0 otherwise. Set \
<CODE> to the category's prefix — SAFE, PERF, or LIVE — followed by a two-digit \
number you choose, e.g. "LIVE-01"; never print the literal characters "NN". Use that \
exact same <CODE> in both the exposed-branch and the pass-branch message, e.g. \
"LIVE-01 exposed" / "LIVE-01 not triggered".
6. The file must compile cleanly under the exact build line you printed in the \
header comment. Add -lm to that line if the computation needs libm — spell it out \
when it's needed and omit it entirely otherwise; never print literal square brackets \
around it.

Respond with exactly two fields: "code" (the complete harness.c source) and \
"checkStrategy" (whichever of "differential", "scaling", or "timeout" you actually \
used)."""


def build_user_prompt(
    *,
    spec: ComputationSpec,
    serial_signature: FunctionSignature,
    parallel_signature: FunctionSignature,
    planted_failures: list[PlantedFailure],
) -> str:
    categories = set()
    planted_lines = []
    for f in planted_failures:
        category = get_category_for_type(f.type_key)
        if category is None:
            raise ValueError(
                f"unknown taxonomy key {f.type_key!r} from Failure Planter — cannot "
                "pick a check strategy for it"
            )
        categories.add(category)
        planted_lines.append(
            f"- typeKey: {f.type_key}\n"
            f"  category: {category}\n"
            f"  implementationNote: {f.implementation_note}"
        )
    if len(categories) > 1:
        raise ValueError(
            f"planted failures span multiple categories ({sorted(categories)}) — "
            "the harness prompt only supports one check strategy per run"
        )
    planted_text = "\n".join(planted_lines)
    return f"""Serial reference signature:
{serial_signature.model_dump_json(indent=2, by_alias=True)}

Parallel version signature (same as serial apart from the name suffix):
{parallel_signature.model_dump_json(indent=2, by_alias=True)}

Computation specification:
{spec.model_dump_json(indent=2, by_alias=True)}

Planted failure(s):
{planted_text}

Write the test harness."""


def run(
    *,
    spec: ComputationSpec,
    serial_signature: FunctionSignature,
    parallel_signature: FunctionSignature,
    planted_failures: list[PlantedFailure],
) -> HarnessOutput:
    return call_agent(
        system_prompt=SYSTEM_PROMPT,
        user_prompt=build_user_prompt(
            spec=spec,
            serial_signature=serial_signature,
            parallel_signature=parallel_signature,
            planted_failures=planted_failures,
        ),
        output_model=HarnessOutput,
    )
