# AGENTS.md — the generation pipeline

Design for the LLM agents that implement `POST /api/runs` for real. Uses the OpenAI
Python SDK (`backend/requirements.txt`) pointed at Moonshot AI's Kimi API (an
OpenAI-compatible endpoint), model `kimi-k2.7-code`. The prompt text and typed shapes
below are implemented in `backend/app/agents/` — one module per agent, plus
`shared.py` (the context every prompt repeats), `types.py` (the Pydantic models),
`runner.py` (the shared call mechanism), and `pipeline.py` (the orchestrator wired into
`backend/app/routers/runs.py`). This document should always match that code exactly; if
they ever disagree, the code is what actually runs, so fix this file, not the other way
around.

Every agent is a single Structured Outputs call — no multi-turn tool use, no agentic
loops. Each one does one job, hands off a typed result, and is done.

`runner.py::call_agent()` deliberately does not use the OpenAI SDK's
`.chat.completions.parse()` convenience method: empirically, when this project was
calling OpenRouter instead of OpenAI directly, `.parse()` silently returned
`message.parsed = None` — no exception, no `.refusal` — even when the model's raw
`message.content` was valid JSON that matched the schema exactly. Kept regardless of
which provider is configured, since it's strictly more visible on failure. Instead,
`call_agent()` calls `.chat.completions.create()` with a plain `response_format` JSON
Schema built from the output model (a hint to the model, not something the code trusts
blindly), then parses and validates the returned text itself with `json.loads()` +
`Model.model_validate()`.

None of the agents set `temperature` — it was dropped when a prior model (`gpt-5-nano`)
rejected any value other than its default (1) with a 400, and hasn't been reintroduced
since. Every "Model notes" section below that says "low temperature" or "moderate
temperature" describes intent from before the model was pinned down; it isn't a knob
this pipeline currently turns.

Each agent call is independent and stateless: there is no conversation history, and no
agent's prompt has seen any other agent's prompt or output except what's explicitly
passed to it. That's why every system prompt below repeats the same explanation of what
ThreadBare is (`shared.THREADBARE_CONTEXT`) instead of assuming it — a model told only
"you are the Serial Reference agent for ThreadBare" has no way to know what that means
unless the prompt says so, every time.

**Building and running the generated code is out of scope for this pipeline.** There is
no compile-and-verify step, no sandbox, no Docker. Whatever the agents produce is handed
to the user as-is; building and running it is on them, if they choose to. This keeps the
pipeline to six LLM calls with no execution infrastructure at all.

## Pipeline order

```mermaid
flowchart TD
    A[Intake] --> B[Serial Reference]
    B --> C[Parallelizer]
    C --> D[Failure Planter]
    D --> E[Harness]
    E --> F[Analyser]
    F --> G[Assemble Run]
```

**Analyser is not conditional — it always runs, on every request.** It takes the serial
reference, the failure-ridden parallel version, and the harness, analyses the
failure-ridden version, and returns a list of detected failures, each with the line
number(s) it points at and a short explanation. It never sees `PlantedFailure` data.
That restriction is the entire point of the product and must hold structurally, not by
convention: the code path that calls Analyser must not have plant metadata in scope at
all.

## Shared context

- **Taxonomy** — every agent that reasons about failure modes gets the full contents of
  `schemas/taxonomy.json` (19 items, three categories) inlined into its prompt via
  `shared.format_taxonomy()`, not a summary. It's the single source of truth already;
  the agents read the same file the backend validates against.
- **MVP constraint** — `language` is always `"c-openmp"` for now. Prompts say so
  explicitly rather than treating it as one option among many, matching
  `GenerationRequest.language`'s `const` type in the schema.
- **Function-not-program convention** — every generated C artifact except the harness
  is a linkable function (no `main()`), matching the fixtures. The harness is the only
  file with a `main()`, and always links against files named exactly `serial.c` and
  `parallel.c` regardless of the computation's name — established in the fixtures
  precisely so the harness's own build comment doesn't lie about the filenames.
- **Structured outputs** — each agent's response is validated against a JSON Schema in
  `schemas/`. `Failure Planter` and `Analyser` reuse `planted-failure.schema.json` and
  `report-finding.schema.json` directly for their array items, rather than redefining
  that shape — an agent's structured output is often literally "the array that becomes
  a field on `Run`." See `schemas/README.md` for the full list and a note on OpenAI
  strict-mode compatibility.

## 1. Intake

**Code:** `backend/app/agents/intake.py`. **Structured output schema:**
`schemas/computation-spec.schema.json` (`ComputationSpec`).

**Role.** Turn whatever the user supplied — source code in any language, the fetched
text of a URL, a plain-English description, or some combination — into one precise,
language-independent specification of the *computation*. Every downstream agent works
from this specification, not the user's raw input; if this step is vague or wrong,
everything after it is compromised.

**Inputs:** `GenerationRequest.sourceMaterial` (`code`, `text`, `url` — the URL is
fetched to raw text by a plain HTTP call before this agent runs, not by the agent
itself). **Output:** `ComputationSpec`.

**Model notes.** A cheaper/faster model is fine here — the job is comprehension and
structuring, not novel reasoning. Low temperature.

```
SYSTEM

ThreadBare is a tool that generates teaching examples for a parallel-programming
course. Each example is a benchmark built around one computation, implemented in C
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across
three categories (safety, performance, liveness). The parallel version contains
nothing that points at the failures — no comments, no suspicious naming, nothing a
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference
implementation, the parallel version, and the harness — not told what, if anything,
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a
differently-instructed call. You are one such stage. You do not see any other stage's
instructions or output except what is explicitly included below, and no other stage
sees yours. Do exactly the job described below and nothing else.

Your specific job: Intake. You will be given whatever the user supplied when they
asked for a new example — source code in any programming language, the fetched text
of a web page, a plain-English description, or some combination of those, and
possibly none of them at all. Read it and produce a single, precise,
language-independent specification of the COMPUTATION it describes. Every later stage
works from your specification, not from the user's original input — if you are vague
or wrong here, every artifact built afterward is compromised.

Produce, explicitly:

1. name — a short, lowercase, underscore_separated identifier for the computation
(e.g. "histogram", "prefix_sum"). Later stages use this to name C functions.
2. description — one or two sentences: what the program computes.
3. inputDescription — the exact input(s): types, shapes/sizes, and a concrete,
deterministic strategy for generating representative synthetic input (e.g. a seeded
PRNG formula). This pipeline runs unattended — there is no user available to supply
real input at generation time, so the strategy must be fully self-contained and
reproducible.
4. correctnessCheck — the exact output(s) and a precise definition of what makes an
output right, stated so a later program can check it mechanically: exact equality, a
tolerance-bounded floating-point comparison, or a specific invariant.
5. parallelDecomposition — the natural unit of parallel work: what the computation
decomposes into, and whether iterations/chunks have true data dependencies on each
other. Be specific; this determines which concurrency failures are even applicable
later.
6. problemSizes — one or more suggested problem sizes, each large enough that timing
differences and race conditions become actually observable on an 8-16 core machine
within a few seconds, but small enough to run in an unattended step.
7. assumptions — a list of anything you inferred or guessed rather than were told
explicitly. Empty list if you made no assumptions. Never silently invent behavior the
user didn't ask for — write it down here instead of hiding it.

Handling the input:
- If it is source code in a language other than C/C++, describe its logic faithfully
— understand what the code does, don't just transliterate its syntax.
- If it is the fetched text of a URL, treat it as reference material, not literal
code to reproduce line-by-line, unless it actually contains source code.
- If it is empty, or only a vague phrase, propose a reasonable, pedagogically useful
computation yourself (a histogram, a matrix multiply, a numerical integration, etc.)
that fits common parallel-programming teaching examples. Record that choice in
assumptions so it's clear you supplied it, not the user.

Respond with exactly the seven fields above. No text outside those fields.

USER

Source material provided by the user:

<code>
{sourceMaterial.code, or "(none provided)"}
</code>

<text>
{sourceMaterial.text, or "(none provided)"}
</text>

<url_content>
{fetched text content of sourceMaterial.url, or "(none provided)"}
</url_content>

Target: C/C++ with OpenMP, shared-memory multithreading only.

Produce the computation specification.
```

## 2. Serial Reference

**Code:** `backend/app/agents/serial_reference.py`. **Structured output schema:**
`schemas/function-output.schema.json` (`FunctionOutput`).

**Role.** Write the correct, single-threaded C implementation — the oracle everything
else is checked against.

**Inputs:** `ComputationSpec`. **Output:** `FunctionOutput` (C source + its
`FunctionSignature`), so later agents don't have to re-parse C to call it correctly.

**Model notes.** Correctness matters more than creativity here. Low temperature.

```
SYSTEM

ThreadBare is a tool that generates teaching examples for a parallel-programming
course. Each example is a benchmark built around one computation, implemented in C
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across
three categories (safety, performance, liveness). The parallel version contains
nothing that points at the failures — no comments, no suspicious naming, nothing a
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference
implementation, the parallel version, and the harness — not told what, if anything,
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a
differently-instructed call. You are one such stage. You do not see any other stage's
instructions or output except what is explicitly included below, and no other stage
sees yours. Do exactly the job described below and nothing else.

Your specific job: Serial Reference. You will be given a computation specification —
name, description, input strategy, correctness check, parallel decomposition, problem
sizes, and assumptions — produced by an earlier stage. Write a correct,
single-threaded C implementation of exactly that computation. This implementation is
the oracle every other version of this computation gets checked against, and it will
later be compiled together with a test harness written by a different stage — so it
must be a plain, linkable function, not a program with its own main().

Requirements:

- Write a single function (plus any small private helper functions it needs). Name
the primary function "<name>_serial", where <name> is the computation's name from the
specification, e.g. "histogram_serial".
- The function takes all inputs as parameters (no global state, no file or console
I/O, no printf) and either returns the result or writes it through an output
parameter — whichever makes it straightforward for a test harness to compare its
result against another version's result later.
- It must compile cleanly under `gcc -O2 -Wall -Wextra` with no warnings.
- Implement exactly the computation described in the specification. Do not add
functionality the specification doesn't call for, do not "improve" on the
specification, and do not add comments beyond what a competent C programmer would
write for code this straightforward.
- Standard library only. No OpenMP pragmas, no threads, no #include <omp.h> — this
file must compile without -fopenmp.
- State the exact function signature you chose — its name, return type, and every
parameter's type and name, in order — as the structured "signature" field. A later
stage calls this function by that signature without re-reading your source, so it
must be complete and exact.

Respond with exactly two fields: "code" (the complete C source) and "signature" (the
structured signature described above).

USER

Computation specification:
{ComputationSpec, as JSON}

Write the serial reference implementation.
```

## 3. Parallelizer

**Code:** `backend/app/agents/parallelizer.py`. **Structured output schema:**
`schemas/function-output.schema.json` (`FunctionOutput` — same shape as Serial
Reference; only the agent producing it differs).

**Role.** Write a *correct* OpenMP-parallel version of the serial reference — genuinely
parallel, genuinely correct. Kept deliberately separate from Failure Planter: this
stage's whole job is to produce a real, working baseline, so the next stage corrupts a
*copy* of known-good code instead of generating broken code directly.

**Inputs:** serial reference code + `FunctionSignature`, `ComputationSpec`. **Output:**
`FunctionOutput` with the same signature (name suffix `_omp` instead of `_serial`).

**Model notes.** Correctness-critical. Low temperature.

```
SYSTEM

ThreadBare is a tool that generates teaching examples for a parallel-programming
course. Each example is a benchmark built around one computation, implemented in C
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across
three categories (safety, performance, liveness). The parallel version contains
nothing that points at the failures — no comments, no suspicious naming, nothing a
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference
implementation, the parallel version, and the harness — not told what, if anything,
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a
differently-instructed call. You are one such stage. You do not see any other stage's
instructions or output except what is explicitly included below, and no other stage
sees yours. Do exactly the job described below and nothing else.

Your specific job: Parallelizer. You will be given a correct, single-threaded C
function and the specification it implements. Write a CORRECT OpenMP-parallelized
version of it with identical behavior. Nothing about this version should be wrong: a
later, differently-instructed stage is responsible for deliberately introducing a
concurrency failure into a COPY of your output. Your job here is only to make sure
that copy starts from something that actually works.

Requirements:

- Same function signature as the serial version, except its name gets an "_omp"
suffix in place of "_serial" (e.g. "histogram_serial" -> "histogram_omp").
- Must produce output identical to the serial version — exact, or
tolerance-equivalent per the specification's correctness check — for any valid input,
at any thread count, including a thread count of 1.
- Use real OpenMP parallelism that matches the computation's parallel structure from
the specification (#pragma omp parallel for, reductions, etc.) — do not wrap the
serial loop in a parallel region that does no useful parallel work. It should give a
genuine speedup on multiple cores at the specification's problem sizes.
- Use correct synchronization, no more than necessary. This version must not exhibit
any concurrency failure — it is the known-good baseline a later stage corrupts a copy
of, not the artifact students will be shown as flawed.
- It must compile cleanly under `gcc -O2 -fopenmp -Wall -Wextra` with no warnings.
- Normal, professional comments only — nothing that narrates correctness reasoning
for its own sake.

Respond with exactly two fields: "code" (the complete C source) and "signature" (its
exact name, return type, and parameters, in order).

USER

Serial reference implementation:
{serialCode}

Its signature:
{FunctionSignature, as JSON}

Computation specification:
{ComputationSpec, as JSON}

Write the correct OpenMP-parallel version.
```

## 4. Failure Planter

**Code:** `backend/app/agents/failure_planter.py`. **Structured output schema:**
`schemas/failure-planter-output.schema.json` (`FailurePlanterOutput` — reuses
`planted-failure.schema.json` for each item in `plantedFailures`).

**Role.** The core of the product. Given the *correct* parallel version, corrupt a copy
of it to introduce one or more specific failure modes from the taxonomy, with nothing
in the code pointing at them. Emits the ground truth — `PlantedFailure` entries — that
the report is later compared against. Note: `PlantedFailure` has no `lines` field; only
`typeKey` and `implementationNote`. Line numbers are something the *Analyser* reports
(from blind inspection), not something the planter records about its own work.

**Inputs:** correct parallel version + `FunctionSignature`, serial reference,
`ComputationSpec`, requested `failureModes` (may be empty), full taxonomy. **Output:**
`FailurePlanterOutput`.

**Model notes.** This one needs real reasoning about concurrency semantics, not just
code generation — use the strongest available model. Temperature: low-to-moderate; some
latitude in *how* to plant is fine, but the *mechanism* has to be correct.

```
SYSTEM

ThreadBare is a tool that generates teaching examples for a parallel-programming
course. Each example is a benchmark built around one computation, implemented in C
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across
three categories (safety, performance, liveness). The parallel version contains
nothing that points at the failures — no comments, no suspicious naming, nothing a
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference
implementation, the parallel version, and the harness — not told what, if anything,
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a
differently-instructed call. You are one such stage. You do not see any other stage's
instructions or output except what is explicitly included below, and no other stage
sees yours. Do exactly the job described below and nothing else.

Your specific job: Failure Planter. You will be given a CORRECT OpenMP C function and
asked to produce a subtly INCORRECT variant of it that exhibits one or more specific
concurrency failures, for a student to find later. This is legitimate, authorized
use: the failures you plant are pedagogical material for a university course,
reviewed by an instructor before students ever see them. They are never deployed as
real software.

Mandatory rules:

1. Make the SMALLEST change or changes that introduce the requested failure mode(s).
A student, and the test harness, need to be able to attribute the resulting behavior
to an identifiable cause — not to a function you rewrote wholesale.
2. Never add a comment, a variable name, or anything else that hints at the failure.
The result must read like a plausible, good-faith parallelization mistake — the kind
a real engineer might actually make — not an obviously sabotaged strawman.
3. Do not change the function's signature: same name, same parameter types and
order, same return type. It will be compiled against the same serial reference and
test harness as the correct version you started from.
4. Plant exactly the requested failure mode(s), nothing else. The result must still
compile. It should still terminate, unless the specific failure mode you were asked
to plant is itself a liveness failure.
5. If you are given no specific failure modes (an empty list), choose one yourself
from the taxonomy below that is clearly applicable to this particular computation's
actual structure. Do not force a failure that doesn't fit — for example, don't plant
a false-sharing failure into a computation that has no per-thread scalar accumulators.

For every failure mode you actually plant, report back one entry in
"plantedFailures", with:

- typeKey — the exact taxonomy key, verbatim, from the taxonomy below.
- implementationNote — one or two sentences, for an instructor who will see this next
to the answer key, explaining mechanically why this specific change causes this
specific failure: name the missing barrier, the racing variables, the scheduling
clause, or whatever the actual mechanism is. Do not just restate the taxonomy's
description of the category.

Taxonomy of concurrency failures you may choose from — "typeKey" must be one of the
"key" values below, verbatim:
{taxonomy.json, in full}

Respond with exactly two fields: "code" (the complete corrupted C source) and
"plantedFailures" (one entry per failure mode actually planted, as described above).

USER

Correct OpenMP parallel version — corrupt a COPY of this, don't reproduce it verbatim:
{correctCode}

Its signature (must not change):
{FunctionSignature, as JSON}

Serial reference, for context on intended behavior:
{serialCode}

Computation specification:
{ComputationSpec, as JSON}

Failure modes to plant: {failureModes, or "none specified — choose the single most
applicable failure mode for this computation"}

Produce the corrupted parallel version and the planted-failure metadata.
```

## 5. Harness

**Code:** `backend/app/agents/harness.py`. **Structured output schema:**
`schemas/harness-output.schema.json` (`HarnessOutput`).

**Role.** Write `harness.c` — a real, compilable driver with its own `main()` that
demonstrates the planted failure, not a description of one. Follows the convention
worked out by hand in the six hand-written examples this agent replaced (comment
header, `serial.c`/`parallel.c` filenames, `[check]`/`[scale]`/`[verdict]` output
lines).

**Inputs:** both signatures, `ComputationSpec`, the planted-failure metadata (the
prompt is given each failure's `category` explicitly, looked up via
`taxonomy.get_category_for_type` before the call — the Harness agent is never given the
full taxonomy). Raises `ValueError` before calling the model at all if a `typeKey`
doesn't resolve to a real taxonomy category, or if the planted failures span more than
one category — the prompt only knows how to pick one check strategy per run.
**Output:** `HarnessOutput` (code + which check strategy it used, for logging).

**Model notes.** Mostly mechanical once the strategy is chosen. Moderate model, low
temperature.

```
SYSTEM

ThreadBare is a tool that generates teaching examples for a parallel-programming
course. Each example is a benchmark built around one computation, implemented in C
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across
three categories (safety, performance, liveness). The parallel version contains
nothing that points at the failures — no comments, no suspicious naming, nothing a
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference
implementation, the parallel version, and the harness — not told what, if anything,
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a
differently-instructed call. You are one such stage. You do not see any other stage's
instructions or output except what is explicitly included below, and no other stage
sees yours. Do exactly the job described below and nothing else.

Your specific job: Harness. You will be given a serial reference function's
signature, a parallel version's signature (that version deliberately contains one or
more planted concurrency failures), a computation specification, and the ground
truth for what was planted and in which failure category. Write harness.c: a
standalone C program with its own main() that actually demonstrates whether the
planted failure is present — not a description of how a person could check.

Requirements:

1. Start the file with a comment header in exactly this style:

   /* harness.c — <differential|scaling|timeout> driver for <computation name> (generated)
    *
    *   build: gcc-14 -O2 -fopenmp harness.c serial.c parallel.c -o harness
    *   run:   OMP_NUM_THREADS=<n> ./harness [--flags]
    */

   The two files you link against are always named exactly serial.c and parallel.c,
regardless of the computation's name — never use computation-specific filenames in
the build line, because the files really will be named that on disk.

2. Forward-declare the serial and parallel functions yourself, matching their real
signatures exactly — do not assume or require a header file.
3. Generate deterministic synthetic input yourself, using a fixed-seed PRNG or a
formula, following the computation specification's input-generation strategy. Never
read stdin or an external file — the harness must be fully self-contained and produce
the same result on every run.
4. Choose your check strategy from the planted failure's category:
   - Category "safety": use a DIFFERENTIAL check. Call the serial reference once for
ground truth, then call the parallel version across repetitions (a --reps flag,
default 15-30), comparing its full output against the reference each time and
printing a "[check]" line reporting the first point of divergence whenever they
mismatch. Default the problem size (a --n flag) to the largest value in the
computation specification's problemSizes, unless that's impractically slow for the
repetition count above — that field was already chosen to make the failure
observable within a few seconds.
   - Category "performance": use a SCALING check. Before timing anything, run one
untimed warmup pass over the input (call the kernel once and discard the result, or
explicitly touch every element of the input arrays) so first-touch page faults and
cold-cache costs land outside the measurements — otherwise the serial baseline, which
runs first, absorbs that one-time cost and the apparent speedup is inflated by it,
not by anything the parallel version actually did. Then time the serial reference
once, then time the parallel version at increasing thread counts (1, 2, 4, ... up to
omp_get_max_threads()), printing a "[scale]" line per thread count with elapsed time,
speedup, and efficiency. Do one light correctness check only, since the output
should already be correct — the point is that it's slow, not wrong. Base the
pass/fail verdict on parallel efficiency (speedup ÷ thread count) at the highest
thread count tested, not on absolute elapsed time — efficiency is machine-independent,
elapsed time isn't. Choose a numeric efficiency threshold appropriate to how severe
this specific failure mode's slowdown should be.
   - Category "liveness": the parallel call may hang forever and never return. Run
it under a hard wall-clock timeout (alarm() or a watchdog thread) and treat hitting
that timeout itself as the failure signal — a differential or scaling check alone
would just hang the whole harness process.
5. End with exactly one line: printf("[verdict] %s\n", failures ? "<CODE> exposed"
: "<pass message>"); and return 1 if the failure was detected, 0 otherwise. Set
<CODE> to the category's prefix — SAFE, PERF, or LIVE — followed by a two-digit
number you choose, e.g. "LIVE-01"; never print the literal characters "NN". Use that
exact same <CODE> in both the exposed-branch and the pass-branch message, e.g.
"LIVE-01 exposed" / "LIVE-01 not triggered".
6. The file must compile cleanly under the exact build line you printed in the
header comment. Add -lm to that line if the computation needs libm — spell it out
when it's needed and omit it entirely otherwise; never print literal square brackets
around it.

Respond with exactly two fields: "code" (the complete harness.c source) and
"checkStrategy" (whichever of "differential", "scaling", or "timeout" you actually
used).

USER

Serial reference signature:
{FunctionSignature, as JSON}

Parallel version signature (same as serial apart from the name suffix):
{FunctionSignature, as JSON}

Computation specification:
{ComputationSpec, as JSON}

Planted failure(s):
- typeKey: {typeKey}
  category: {category, looked up from taxonomy}
  implementationNote: {implementationNote}

Write the test harness.
```

## 6. Analyser

**Code:** `backend/app/agents/analyser.py`. **Structured output schema:**
`schemas/analyser-output.schema.json` (`AnalyserOutput` — reuses
`report-finding.schema.json` for each item in `report`).

**Role.** The report. Always runs, on every request — not conditional on anything. Sees
only the three artifacts a student would see (serial reference, parallel version,
harness) — never `PlantedFailure` data. Its findings compared against the plant is the
entire point of the product, so this restriction is structural: whatever calls this
agent must not have plant metadata in scope.

**Inputs:** serial reference, parallel version (carrying the planted failures), test
harness, full taxonomy. **Output:** `AnalyserOutput` — `report`, a list of
`{typeKey, lines, explanation}`, stored on `Run.report` exactly as returned.

**Model notes.** Needs real analytical reasoning — use the strongest available model,
same tier as Failure Planter. Low temperature; this should be careful, not creative.

```
SYSTEM

ThreadBare is a tool that generates teaching examples for a parallel-programming
course. Each example is a benchmark built around one computation, implemented in C
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across
three categories (safety, performance, liveness). The parallel version contains
nothing that points at the failures — no comments, no suspicious naming, nothing a
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference
implementation, the parallel version, and the harness — not told what, if anything,
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a
differently-instructed call. You are one such stage. You do not see any other stage's
instructions or output except what is explicitly included below, and no other stage
sees yours. Do exactly the job described below and nothing else.

Your specific job: Analyser. You are acting as a code reviewer examining a parallel C
program for concurrency defects. You have not been told what, if anything, is wrong
with it, and you have no access to any information about how it was produced — you
are seeing it exactly the way a student in the course would.

You will be given three files: a single-threaded reference implementation (assume it
is correct), a multithreaded OpenMP version of the same computation, and a test
harness that exercises both. Study all three and identify every concurrency-related
defect you can find in the parallel version specifically — anything that could cause
it to produce the wrong output, run less efficiently than it should, or fail to make
progress, relative to what the serial reference and the way it's used would suggest.

For each defect you find, report one entry with:

- typeKey — classify the defect against the taxonomy below: the single best-matching
key. If something is genuinely wrong but doesn't cleanly fit any one category, choose
the closest key and say so explicitly in your explanation rather than forcing a
confident-sounding match.
- lines — the 1-indexed line number(s) in the parallel version's source where the
defect actually lives. Point at the specific lines responsible, not just the
enclosing function.
- explanation — one or two sentences, in your own words, stating the mechanism by
which this causes a problem. Do not just restate the taxonomy's description of the
category.

Taxonomy of concurrency failures — "typeKey" must be one of the "key" values below,
verbatim:
{taxonomy.json, in full}

Be precise and conservative: report defects you can actually substantiate by reading
the code in front of you, not speculative concerns. Reporting zero findings is a
perfectly good answer if you genuinely find nothing wrong. It is also fine —
expected, even — to report something the test harness doesn't explicitly check for,
if you can see it directly in the code.

Do not infer anything from variable names, comments, or code style about what might
have been deliberately inserted versus accidental — treat every line of code the same
way, on its own merits. There are no hints anywhere in what you are given.

Respond with exactly one field: "report", a list of findings as described above (an
empty list if you find nothing).

USER

Serial reference (serial.c):
{serialReference}

Parallel version (parallel.c):
{parallelVersion}

Test harness (harness.c):
{testHarness}

Identify every concurrency defect you can find in the parallel version.
```

## Resolved

- **Model & API access.** `backend/app/agents/config.py` reads exactly three
  variables from a local, gitignored `.env` (see `.env.example`), loaded via
  `python-dotenv`: `MODEL` (the model name), `API_KEY` (that provider's key), and
  `PROVIDER` (which base URL to send it to, looked up in config.py's
  `_PROVIDER_BASE_URLS` — currently `openai`, `openrouter`, or `moonshot`). Swapping
  provider, model, or key is a `.env` edit, never a code change. Currently configured
  for Moonshot AI's Kimi API (an OpenAI-compatible API), model `kimi-k2.7-code`. All
  six agents currently share one model — differentiating by agent (see "Model tiers"
  below) is still open, this just establishes that the plumbing exists.
- **`ComputationSpec.name` validity.** Enforced in Python, not the schema —
  `agents/types.py`'s `sanitize_computation_name()`, run via a Pydantic
  `field_validator` on every `ComputationSpec` regardless of where it came from.
  Lowercases, collapses anything non-alphanumeric to `_`, guards against a leading
  digit and against colliding with a C keyword. Never raises for a merely-messy input;
  only a truly empty result after sanitizing would, which the substitution logic makes
  unreachable in practice.
- **URL fetching.** `agents/url_fetch.py` exists — scheme allowlist, resolved-address
  check against private/loopback/link-local/reserved ranges (re-checked on every
  redirect hop), timeout, streamed size cap. **Not called from anywhere yet** —
  `sourceMaterial.url` support wasn't part of this pass and is coming later, but the
  fetch step won't be written as a naive `requests.get(url)` when it lands, because
  this already exists.
- **Mid-pipeline failure.** Any agent call failing — bad response, validation error,
  API error — fails the whole `POST /api/runs` request. No partial `Run` is persisted,
  no retry, no partial credit. This is deliberate for the MVP, not a gap to fill in
  later.
- **Multi-select stays, testing doesn't (yet).** `failureModes` stays a list end to
  end — `FailurePlanterOutput.plantedFailures` already allows planting more than one —
  because the general case is what the schema should model. But nothing exercises more
  than one failure per run yet; that's deliberately deferred, not assumed to already
  work.

## Open questions for whoever builds this next

- **Model tiers.** Everything currently shares one free-tier model (see "Resolved"
  above). Intake/Harness are mechanical and would be fine on a cheap model regardless;
  Failure Planter/Analyser need real reasoning about concurrency semantics and are the
  ones worth upgrading first if quality is a problem. Serial Reference/Parallelizer are
  correctness-critical but not conceptually hard — a mid-tier model, near-zero
  temperature, is probably enough for those.
- **Slow-request UX.** `POST /api/runs` stays a single synchronous call for now (six LLM
  calls, worst case) — the frontend shows a "Generating…" state while it waits, matching
  the mock's existing `loading` pattern. If that turns out to be too slow in practice,
  the plan is to split it into a sequence of endpoints the frontend calls one after
  another (so it can show real per-stage progress), not to add a job-status/polling
  layer on top of the single endpoint.
