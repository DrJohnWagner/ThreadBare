# schemas/

JSON Schema (2020-12) contracts for everything that crosses the frontend/backend
boundary, plus the taxonomy content itself. This directory is the source of truth —
`ENGINEERING.md`'s TypeScript interfaces mirror these by hand for readability, but once
codegen is wired up, these files are what's authoritative and the hand-written
interfaces should be generated, not maintained in parallel.

## Files

- `taxonomy.schema.json` / `taxonomy.json` — the 19-item failure-mode taxonomy. The
  `.json` file is real content, not a placeholder; both sides load it rather than
  hardcoding their own copy of the 19 items.
- `generation-request.schema.json` — body of `POST /api/runs`.
- `planted-bug.schema.json`, `bug-report-finding.schema.json` — the two halves of the
  planted-vs-found comparison that's the point of the product.
- `run.schema.json` — full generation result (`POST /api/runs`, `GET /api/runs/{id}`).
- `history-record.schema.json` — lightweight list entry (`GET /api/runs`).
- `dashboard-stats.schema.json` — pre-aggregated stats (`GET /api/dashboard`).

## Conventions

- `additionalProperties: false` everywhere — a typo'd field should fail validation, not
  silently pass through.
- Cross-references use relative `$ref`s (e.g. `run.schema.json` refers to
  `"generation-request.schema.json"`) — resolvable by any tool given this directory.
- Anything whose valid values live in `taxonomy.json` (failure-mode keys) is typed as a
  plain `string`/array of strings in these schemas, not an `enum` — enumerating them here
  would just recreate the drift risk this directory exists to remove. Validate those
  against the loaded taxonomy at the application level, on both sides.
- `language` is `"const": "c-openmp"`, not an `enum` of one — makes it visible in a diff
  the day a second target is added.

## Intended usage (not wired up yet)

- **Backend (Python)**: generate Pydantic models from these schemas — e.g.
  `datamodel-code-generator --input schemas --input-file-type jsonschema`. Validates
  request bodies and lets FastAPI's own OpenAPI docs stay honest against the same
  contract the frontend uses.
- **Frontend (TypeScript)**: generate types with `json-schema-to-typescript` or
  `quicktype`, one file per schema, into `frontend/src/types/generated/`. Replaces the
  hand-written interfaces in `ENGINEERING.md` once this is set up.

Wiring up either generator is a follow-up task, not done here — this directory is the
contract; the build-time codegen step is still to be added on both sides.
