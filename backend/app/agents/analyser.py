"""Analyser agent — the blind report.

See AGENTS.md ("6. Analyser"). Structurally restricted to three inputs: the serial
reference, the parallel version, and the test harness. It is never given the planted-
failure ground truth, and it always runs — it is not conditional on anything. Whatever
code calls this agent must not have PlantedFailure data in scope at all; comparing its
findings against that ground truth happens elsewhere, in code this agent has no
knowledge of.
"""

from .runner import call_agent
from .shared import THREADBARE_CONTEXT, format_taxonomy
from .types import AnalyserOutput

OUTPUT_SCHEMA_PATH = "schemas/analyser-output.schema.json"
OUTPUT_MODEL = AnalyserOutput

SYSTEM_PROMPT = f"""{THREADBARE_CONTEXT}

Your specific job: Analyser. You are acting as a code reviewer examining a parallel C \
program for concurrency defects. You have not been told what, if anything, is wrong \
with it, and you have no access to any information about how it was produced — you \
are seeing it exactly the way a student in the course would.

You will be given three files: a single-threaded reference implementation (assume it \
is correct), a multithreaded OpenMP version of the same computation, and a test \
harness that exercises both. Study all three and identify every concurrency-related \
defect you can find in the parallel version specifically — anything that could cause \
it to produce the wrong output, run less efficiently than it should, or fail to make \
progress, relative to what the serial reference and the way it's used would suggest.

For each defect you find, report one entry with:

- typeKey — classify the defect against the taxonomy below: the single best-matching \
key. If something is genuinely wrong but doesn't cleanly fit any one category, choose \
the closest key and say so explicitly in your explanation rather than forcing a \
confident-sounding match.
- lines — the 1-indexed line number(s) in the parallel version's source where the \
defect actually lives. Point at the specific lines responsible, not just the \
enclosing function.
- explanation — one or two sentences, in your own words, stating the mechanism by \
which this causes a problem. Do not just restate the taxonomy's description of the \
category.

Taxonomy of concurrency failures — "typeKey" must be one of the "key" values below, \
verbatim:
{format_taxonomy()}

Be precise and conservative: report defects you can actually substantiate by reading \
the code in front of you, not speculative concerns. Reporting zero findings is a \
perfectly good answer if you genuinely find nothing wrong. It is also fine — \
expected, even — to report something the test harness doesn't explicitly check for, \
if you can see it directly in the code.

Do not infer anything from variable names, comments, or code style about what might \
have been deliberately inserted versus accidental — treat every line of code the same \
way, on its own merits. There are no hints anywhere in what you are given.

Respond with exactly one field: "report", a list of findings as described above \
(an empty list if you find nothing)."""


def build_user_prompt(
    *, serial_reference: str, parallel_version: str, test_harness: str
) -> str:
    return f"""Serial reference (serial.c):
{serial_reference}

Parallel version (parallel.c):
{parallel_version}

Test harness (harness.c):
{test_harness}

Identify every concurrency defect you can find in the parallel version."""


def run(
    *, serial_reference: str, parallel_version: str, test_harness: str
) -> AnalyserOutput:
    return call_agent(
        system_prompt=SYSTEM_PROMPT,
        user_prompt=build_user_prompt(
            serial_reference=serial_reference,
            parallel_version=parallel_version,
            test_harness=test_harness,
        ),
        output_model=AnalyserOutput,
    )
