"""Building blocks shared by every agent's prompt.

Each agent call is an independent, stateless OpenAI request — there is no conversation
history and no memory of any other agent's system prompt. Every agent's SYSTEM_PROMPT
therefore starts with THREADBARE_CONTEXT, spelling out what ThreadBare is and how the
pipeline as a whole works, rather than assuming a model reading "the Serial Reference
agent" already knows what that means. It doesn't — it has never seen any other prompt
in this file or this package.
"""

import json

from ..taxonomy import get_taxonomy

THREADBARE_CONTEXT = """ThreadBare is a tool that generates teaching examples for a parallel-programming \
course. Each example is a benchmark built around one computation, implemented in C \
with OpenMP for shared-memory multithreading. A finished example has four parts:

1. A correct, single-threaded reference implementation of the computation.
2. A parallel OpenMP version of the same computation that deliberately contains one or \
more concurrency failures, chosen from a fixed taxonomy of 19 failure types across \
three categories (safety, performance, liveness). The parallel version contains \
nothing that points at the failures — no comments, no suspicious naming, nothing a \
student could grep for.
3. A test harness: a standalone program that runs both versions and demonstrates \
whether a failure is actually present.
4. A report: a list of findings from a reviewer who was given only the reference \
implementation, the parallel version, and the harness — not told what, if anything, \
was deliberately planted.

Producing these four parts is broken into separate stages, each carried out by a \
differently-instructed call. You are one such stage. You do not see any other stage's \
instructions or output except what is explicitly included below, and no other stage \
sees yours. Do exactly the job described below and nothing else."""


def format_taxonomy() -> str:
    """Render the full taxonomy (19 items, 3 categories) as JSON text for a prompt."""
    taxonomy = get_taxonomy()
    return json.dumps(taxonomy.model_dump(by_alias=True), indent=2)
