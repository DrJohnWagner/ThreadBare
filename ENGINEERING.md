# ENGINEERING.md — ThreadBare MVP frontend

Engineering design for the real `frontend/` (Vite + React + TypeScript), replacing
`DESIGN.html` as the spec. `DESIGN.html` stays in the repo as the visual reference;
this document is what actually gets built, and resolves the gaps `DESIGN_NOTES.md`
raised. MVP scope: **C/C++ with OpenMP only**, single-user, instructor-facing.

## Product model

The frontend's job is to expose one operation — request a benchmark, get back four
artifacts — plus a history of past requests. From the README:

- **Serial reference** — the single-threaded implementation, the oracle.
- **Parallel version** — the same computation, multithreaded, carrying the planted
  failure modes, no comments pointing at them.
- **Test harness** — drives both and compares them.
- **Report** — structured JSON from an agent that never saw what was planted; sees only
  the three artifacts above. A list of findings, each with a failure type, the line(s)
  it points at, and a short explanation — not prose.

The comparison between what was planted and what the report found is the point of the
product. It is a first-class piece of the data model, not a text blurb:

```ts
type FailureCategory = 'safety' | 'performance' | 'liveness'

interface TaxonomyItem {
  key: string
  label: string
  category: FailureCategory
  summary: string
  description: string
}

interface GenerationRequest {
  language: 'c-openmp'          // literal type, not a free string — MVP has exactly one value
  failureModes: string[]        // TaxonomyItem keys the user asked to have planted; [] = generator's choice
  sourceMaterial?: { code?: string; text?: string; url?: string }
}

interface PlantedFailure {
  typeKey: string                // TaxonomyItem key, ground truth
  implementationNote: string     // where/how it was inserted — never shown to the analyser
}

interface ReportFinding {
  typeKey: string       // taxonomy key this finding is classified as
  lines: number[]       // 1-indexed line(s) into parallelVersion this finding points at
  explanation: string   // short, the analyser's own words — not an essay
}

interface Run {
  id: string
  createdAt: string
  request: GenerationRequest
  serialReference: string
  parallelVersion: string
  parallelVersionFixed: string   // corrected version, produced up front alongside everything else
  testHarness: string
  plantedFailures: PlantedFailure[]
  report: ReportFinding[]
}

interface HistoryRecord {        // lightweight — list view, no code bodies
  id: string
  createdAt: string
  language: string
  requestedFailureModes: string[]  // what was asked for
  plantedTypeKeys: string[]        // what was actually planted — flattened from PlantedFailure[]
}

interface DashboardStats {
  totalRuns: number
  byCategory: Record<FailureCategory, number>
  byType: Record<string, number>   // taxonomy item key -> count
}
```

`matchesPlanted` (finding vs. ground truth) is computed client-side by comparing each
finding's `typeKey` against `plantedFailures` — no need for the backend to pre-compute it.
`DashboardStats` itself, unlike that comparison, *is* computed server-side — see below.

Two fields from the mock are gone, not just flagged: `language` breakdowns and
`difficulty`. Language is a fixed single value for the whole MVP, so a per-language
breakdown would always show one 100% bucket — no information, not worth a chart.
`difficulty` (Beginner/Intermediate/Advanced in the mock) was an attribute of the mock's
six hand-picked examples; nothing in the README's pipeline assesses difficulty for a
generated run, and there's no plan to build that assessment, so the concept is dropped
outright rather than carried as an open question.

## API contract

Implemented for real, as of `feat/backend-generation-pipeline` — `POST /api/runs`
runs the full six-agent generation pipeline (`backend/app/agents/pipeline.py`; see
`AGENTS.md`) against a live model and returns the assembled `Run`. Every other
endpoint is fully real: in-memory store, real zip-building, real dashboard
aggregation.

Persistence: an in-memory dict of runs on the backend process (`backend/app/store.py`),
lost on restart. No database for MVP. Enough to back History and Dashboard for real,
since a class session doesn't span a backend reboot.

| Method & path              | Body                  | Returns              | Notes |
|------------------------------|------------------------|-----------------------|-------|
| `GET /api/taxonomy`         | —                      | `Taxonomy`            | Serves `schemas/taxonomy.json` straight off disk — added so the frontend never needs its own copy of the 19 items. |
| `POST /api/runs`            | `GenerationRequest`    | `Run`                 | Invokes the full agent chain synchronously. The slow call. |
| `GET /api/runs`             | —                      | `HistoryRecord[]`     | List for History page. |
| `GET /api/runs/:id`         | —                      | `Run`                 | Expanding a History row. |
| `DELETE /api/runs`          | —                      | `204`                 | "Clear history." No per-row delete in v1, matching the mock. |
| `GET /api/dashboard`        | —                      | `DashboardStats`      | Pre-aggregated. See below. |
| `GET /api/runs/:id/download`| —                      | `application/zip`     | One run's artifacts. Replaces the mock's client-side zip builder. |
| `GET /api/runs/export`      | —                      | `application/zip`     | All runs, one archive — "Export all as .zip" on History. |

**No separate "fix" or "analyse" generation endpoint.** The corrected version and the
report are both produced by the one `POST /api/runs` call — `Run.parallelVersionFixed`
is simply part of that response, shown in the UI alongside the buggy version rather than
gated behind any "reveal" action. There used to be a `PATCH /api/runs/:id` endpoint and
a `fixed` flag tracking whether a user had clicked to reveal it; both are retired — the
UI feature they backed (a one-click Fix reveal, plus the Dashboard fix-rate stat it fed)
was removed, and there was no reason to keep the flag around with nothing left to set it.

**Dashboard is server-aggregated, not client-derived.** The backend already holds the
full run list in memory; it's cheaper and more correct for it to compute
`DashboardStats` once than to ship every `HistoryRecord` to the browser and recompute
counts on every Dashboard visit. This reverses this document's earlier draft, which had
Dashboard deriving stats client-side — that assumption was based on the mock, which had
no backend to do the aggregating.

**Zip building moves to the backend entirely.** The mock's `zip.ts` (a hand-rolled,
dependency-free CRC32 + PK ZIP writer, ~100 lines) exists only because the mock had no
server. With a real backend, it's the natural place to build the archive — one
implementation instead of duplicating "which files go in the zip" logic in two places
(single-run download and "export all"), and it deletes real complexity from the frontend
rather than relocating it. The frontend triggers both via a plain `<a href=... download>`
to the endpoint — no fetch-and-blob dance needed for a same-origin GET.

`POST /api/runs` can plausibly take tens of seconds (it's a chain of LLM calls). MVP
answer: a single synchronous request with a generic "Generating…" state, matching the
mock's `loading` boolean. Staged progress (e.g. "Writing serial reference… → Planting
failures… → Building harness… → Running blind analysis…" via polling or SSE) is a real
improvement but explicitly deferred — it needs a backend job/status model that doesn't
exist yet, and CLAUDE.md's instruction to keep surface area small for a beginner team
argues against building it before the plain version works.

## Source directory structure

```
frontend/src/
  api/
    client.ts               fetch wrapper: base URL, JSON parsing, error normalization
    taxonomy.ts              getTaxonomy — GET /api/taxonomy
    runs.ts                  generateRun, listRuns, getRun, clearRuns
    dashboard.ts             getDashboardStats
    downloadUrl.ts           builds the href for the download-zip anchors — no fetch involved
  types/
    taxonomy.ts           TaxonomyItem, TaxonomyCategory, FailureCategory
    run.ts                GenerationRequest, PlantedFailure, ReportFinding, Run, HistoryRecord, DashboardStats
  lib/
    taxonomyLookup.ts       labelForType, categoryForType, labelForCategory, colorForCategory, softColorForCategory — the one place taxonomy lookups live, instead of duplicated per-component
  components/
    Tag.tsx, PrimaryButton.tsx, GhostButton.tsx, NumberBadge.tsx   generic UI atoms, ported as-is
    icons.tsx                                                       CodeIcon/TextIcon/LinkIcon/ChevronIcon/Logo
    CodeBlock.tsx                                                   line-numbered code viewer, ported as-is
    FailureCategorySelector.tsx, FailureItem.tsx                    taxonomy picker, as tabbed panels (one per category); takes taxonomy data as a prop (from useTaxonomy) instead of importing a static list
    Tabs.tsx                   generic tab-bar-and-panel widget — used by both the Lab results view and FailureCategorySelector
    ReportPanel.tsx             new — replaces BugListItem; renders findings vs. planted, see below
    HistoryRow.tsx, StatCard.tsx, BreakdownCard.tsx                 ported as-is
  pages/
    LabPage.tsx, HistoryPage.tsx, DashboardPage.tsx, AboutPage.tsx
  App.tsx                    shell: brand, nav, taxonomy-load error, page switch
  index.css                  theme tokens as CSS custom properties (see Styling)
  main.tsx                   unchanged
```

The existing `App.tsx`/`App.css` (Vite's counter demo) are deleted, not extended, once
`LabPage` exists.

## Styling

The mock uses Tailwind utility classes plus a JS `C` color object passed through inline
`style={{}}` props. We do not adopt Tailwind. CLAUDE.md already commits this project to
colocated plain CSS matching the existing `App.tsx`/`App.css` convention, and adding a
utility-CSS framework's config and class-name vocabulary is exactly the kind of extra
surface area that rule exists to avoid for a team that has never used React. Instead:

- The mock's `C` object becomes CSS custom properties on `:root` in `index.css`
  (`--color-accent`, `--color-safety-soft`, etc.) — same palette, no per-render style
  object allocation, and every component gets it via `className` + a stylesheet.
- Each component/page gets its own colocated `.css` file, same pattern as `App.css`.
- Layout (the mock's flex/grid arrangements) is reimplemented in plain CSS with the same
  breakpoints, not copied as Tailwind classes.

## State management

- Form state (Lab page inputs) — plain `useState`, matches the mock.
- Data fetching — small custom hooks, one per domain: `useTaxonomy`, `useGenerateRun`,
  `useHistory`, `useDashboardStats`. Each wraps its `api/*.ts` call and owns its own
  loading/error state. No Redux, no Zustand, no React Query — CLAUDE.md rules those out,
  and a four-page app with one slow mutation and three list/lookup reads doesn't need
  them. `useTaxonomy` runs once at the `App` level (the Lab page can't render its error
  picker without it) and is passed down, rather than every page re-fetching it.
- Nav state lives in `App.tsx` and is passed down one level to whichever page is
  active — shallow enough that Context isn't justified yet. There's no general-purpose
  error banner anymore; `App.tsx` only surfaces one specific failure (the taxonomy
  fetch, since nothing else can render without it) rather than a catch-all `error`/
  `setError` threaded through every page. That generic version existed only to support
  the Fix-reveal flow's error case, which no longer exists.

## Navigation

No React Router. `App.tsx` holds `useState<'lab' | 'history' | 'dashboard' | 'about'>`
and renders the matching page, exactly like the mock. CLAUDE.md already rules out adding
a routing library without being asked. Trade-off, explicit: no deep links, no
back-button support, no bookmarkable URLs. Acceptable for a single-instructor local tool;
revisit only if that stops being true.

## Pages

### Lab

Steps 1–3 (source material, language, error-category picker) and the request-preview
panel port over close to as-is. Two changes:

- **Language** is rendered as a static label ("C/C++ with OpenMP"), not a `<select>` —
  a dropdown with one option implies a choice that doesn't exist yet. `GenerationRequest.language`
  stays a literal type so adding a second target later is a type-level change, not a
  silent runtime one.
- **Source material fields are wired for real** — sent as `GenerationRequest.sourceMaterial`
  and actually used by the backend, unlike the mock where they were inert by construction
  (no backend existed to use them).

Result tabs are renamed to match the README's four artifacts, and a tab is added:

| Mock tab      | Real tab            | Change |
|---------------|----------------------|--------|
| *(none)*      | **Serial Reference** | New — was buried inside the zip download only |
| Buggy Code    | **Parallel Version**  | Renamed |
| Test Harness  | **Test Harness**      | Unchanged in name; now renders real driver code (its own `main()`, linked against `serial.c`/`parallel.c`), not a prose description |
| Explanation   | **Report**            | Renamed (not "Bug Report" — see below) and redesigned |

Not "Bug Report" — the taxonomy already dropped "bug"-flavored naming once (Safety /
Performance / Liveness failures, not "Incorrect"), and a performance or liveness failure
isn't a "bug" in the colloquial sense either. "Report" matches that.

**Report tab** (`ReportPanel`) is the piece the mock is missing entirely. `Run.report` is
structured JSON, not prose: a list of findings, each with a `typeKey`, one or more
`lines` into `parallelVersion`, and a short `explanation` — mirroring how `PlantedFailure`
already carries a `typeKey` and an `implementationNote`, so planted and found use the
same vocabulary. The analyser's findings are shown first, in the analyser's own words,
with no reference to what was planted — preserving the "guess before you look" moment.
A second, separately-revealed section ("What was actually planted") shows `plantedFailures`
and highlights which findings matched. This keeps the mock's existing collapse-behind-a-
toggle pattern (`BugListItem`'s "Why?" disclosure) but applies it to the planted/found
seam instead of to a single canned explanation.

`Run.parallelVersionFixed` is not shown on the Lab page at all — it's part of the
`Run` object from the start, same as everything else, but there's no tab or reveal
action for it here. It surfaces on the History page instead (see below), where it's
always shown alongside the buggy version rather than gated behind any click.

### History

Same shape as the mock, backed by `GET /api/runs` (list) and `GET /api/runs/:id` (row
expansion) instead of `window.storage` — that API is specific to the Claude Artifact
sandbox the mock was built in and doesn't exist in a deployed app. Each row's "Download"
link and the page's "Export all as .zip" button both become plain links to
`GET /api/runs/:id/download` and `GET /api/runs/export` respectively — no client-side
zip-building code at all.

### Dashboard

In scope for MVP, not deferred — sourced from `GET /api/dashboard`. Stat tile: total
runs. Breakdown cards: by category, by taxonomy type. "By language" and "By difficulty"
from the mock's version are both gone — see the note under `DashboardStats` above. Fix
count/rate are gone too, retired along with the Fix reveal feature they measured.

### About

Copy gets rewritten to drop the mock's "nothing is generated or executed" framing (no
longer true) and instead describe the real pipeline and the blind-analyser mechanism, so
it matches the README rather than contradicting it.

## Error handling and loading states

- `POST /api/runs`: button-level loading state (already the mock's pattern), plus a
  distinct message for "no example matches your filters" vs. a genuine network/backend
  failure — both shown inline on the Lab page via `useGenerateRun`'s own `error` state,
  not a global banner (see the note below on `App.tsx`).
- No automatic retry/backoff. One manual retry (click Generate again) is enough for an
  MVP teaching tool — building retry logic here would be solving a problem nobody has yet.

## Testing

No test suite for MVP beyond `tsc` (already wired into `npm run build`) and manual
verification via `npm run dev`. Revisit with Vitest + React Testing Library only if the
team decides it's worth the setup cost — not assumed here.

