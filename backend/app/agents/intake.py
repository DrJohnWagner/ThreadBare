"""Intake agent — turns arbitrary source material into a ComputationSpec.

See AGENTS.md ("1. Intake").
"""

from .runner import call_agent
from .shared import THREADBARE_CONTEXT
from .types import ComputationSpec

OUTPUT_SCHEMA_PATH = "schemas/computation-spec.schema.json"
OUTPUT_MODEL = ComputationSpec

SYSTEM_PROMPT = f"""{THREADBARE_CONTEXT}

Your specific job: Intake. You will be given whatever the user supplied when they \
asked for a new example — source code in any programming language, the fetched text \
of a web page, a plain-English description, or some combination of those, and \
possibly none of them at all. Read it and produce a single, precise, \
language-independent specification of the COMPUTATION it describes. Every later stage \
works from your specification, not from the user's original input — if you are vague \
or wrong here, every artifact built afterward is compromised.

Produce, explicitly:

1. name — a short, lowercase, underscore_separated identifier for the computation \
(e.g. "histogram", "prefix_sum"). Later stages use this to name C functions.
2. description — one or two sentences: what the program computes.
3. inputDescription — the exact input(s): types, shapes/sizes, and a concrete, \
deterministic strategy for generating representative synthetic input (e.g. a seeded \
PRNG formula). This pipeline runs unattended — there is no user available to supply \
real input at generation time, so the strategy must be fully self-contained and \
reproducible.
4. correctnessCheck — the exact output(s) and a precise definition of what makes an \
output right, stated so a later program can check it mechanically: exact equality, a \
tolerance-bounded floating-point comparison, or a specific invariant.
5. parallelDecomposition — the natural unit of parallel work: what the computation \
decomposes into, and whether iterations/chunks have true data dependencies on each \
other. Be specific; this determines which concurrency failures are even applicable \
later.
6. problemSizes — one or more suggested problem sizes, each large enough that timing \
differences and race conditions become actually observable on an 8-16 core machine \
within a few seconds, but small enough to run in an unattended step.
7. assumptions — a list of anything you inferred or guessed rather than were told \
explicitly. Empty list if you made no assumptions. Never silently invent behavior the \
user didn't ask for — write it down here instead of hiding it.

Handling the input:
- If it is source code in a language other than C/C++, describe its logic faithfully \
— understand what the code does, don't just transliterate its syntax.
- If it is the fetched text of a URL, treat it as reference material, not literal \
code to reproduce line-by-line, unless it actually contains source code.
- If it is empty, or only a vague phrase, propose a reasonable, pedagogically useful \
computation yourself (a histogram, a matrix multiply, a numerical integration, etc.) \
that fits common parallel-programming teaching examples. Record that choice in \
assumptions so it's clear you supplied it, not the user.

Respond with exactly the seven fields above. No text outside those fields."""


def build_user_prompt(
    *, code: str | None, text: str | None, url_content: str | None
) -> str:
    """`code`/`text` are GenerationRequest.sourceMaterial's fields verbatim.

    `url_content` is the already-fetched text of sourceMaterial.url — fetching it is a
    plain HTTP call made before this agent runs, not this agent's job.
    """
    return f"""Source material provided by the user:

<code>
{code or "(none provided)"}
</code>

<text>
{text or "(none provided)"}
</text>

<url_content>
{url_content or "(none provided)"}
</url_content>

Target: C/C++ with OpenMP, shared-memory multithreading only.

Produce the computation specification."""


def run(
    *, code: str | None, text: str | None, url_content: str | None
) -> ComputationSpec:
    return call_agent(
        system_prompt=SYSTEM_PROMPT,
        user_prompt=build_user_prompt(code=code, text=text, url_content=url_content),
        output_model=ComputationSpec,
    )
