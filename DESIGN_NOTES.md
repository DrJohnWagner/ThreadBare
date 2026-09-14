# DESIGN.html vs README — working note

`DESIGN.html` is a standalone mock built in Claude with no backend: everything it shows
is fixture data. Its "nothing is generated" copy describes that limitation, not a
product decision — don't read it as scope.

## Settled — not open questions

- Product name is **ThreadBare**, not BugForge. Rename throughout when porting.
- MVP targets **C/C++ with OpenMP only**. The language selector doesn't need to become a
  real dropdown for v1 — one fixed value is correct, not a placeholder gap.
- Tab labels ("Buggy Code" / "Test Harness" / "Explanation") are wrong and get renamed to
  match the README's artifact vocabulary.

## Open — build these into the real frontend, don't just port the mock

1. **The blind analyser.** The README's core claim: the bug report comes from an agent
   that never saw what was planted, so planted-vs-found is itself a signal. The mock's
   explanation panel is documentation of the plant, not an independent finding — no
   second pass, no diff. This has to actually exist; it can't be more fixture data.
2. **Four artifacts, three tabs.** README artifacts: serial reference, parallel version,
   test harness, bug report. The mock only surfaces buggy code + harness + explanation
   on-screen — the serial reference lives only inside the exported zip. Design the real
   tab structure around all four.
3. **Source-material input is inert.** The code/text/URL fields in Step 1 exist in the
   UI and get built into a request payload, but never affect which example is served.
   The real version needs this to actually drive generation.
4. **Bug-report framing.** Whatever replaces the current per-bug explanation should read
   as a *finding* — distinct in voice and structure from the planted-bug metadata
   (title, type, difficulty). The gap between the two is supposed to carry information,
   per the README; collapsing them into one blurb loses that.

## Carry over as-is

Category taxonomy (3 families, 19 items), category colors and grouping, download-as-zip
behavior, and the page layout (Lab / Dashboard / About / History) aren't in question.
