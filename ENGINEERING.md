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
- **Bug report** — written by an agent that never saw what was planted; sees only the
  three artifacts above.

The comparison between what was planted and what the report found is the point of the
product. It is a first-class piece of the data model, not a text blurb:

```ts
type FailureCategory = 'incorrect' | 'slow' | 'nonterminating'

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

interface PlantedBug {
  typeKey: string                // TaxonomyItem key, ground truth
  implementationNote: string     // where/how it was inserted — never shown to the analyser
}

interface BugReportFinding {
  description: string            // the analyser's own words
  matchedTypeKey: string | null  // taxonomy key this finding maps to, if any
}

interface Run {
  id: string
  createdAt: string
  request: GenerationRequest
  serialReference: string
  parallelVersion: string
  parallelVersionFixed: string   // corrected version, produced up front, revealed on click
  testHarness: string
  plantedBugs: PlantedBug[]
  bugReport: BugReportFinding[]
  fixed: boolean                 // has "Fix Bugs" been revealed for this run? starts false
}

interface HistoryRecord {        // lightweight — list view, no code bodies
  id: string
  createdAt: string
  language: string
  requestedFailureModes: string[]  // what was asked for
  plantedTypeKeys: string[]        // what was actually planted — flattened from PlantedBug[]
  fixed: boolean
}

interface DashboardStats {
  totalRuns: number
  fixedCount: number
  fixRate: number | null           // null when totalRuns === 0, not 0 — "no data" isn't "0%"
  byCategory: Record<FailureCategory, number>
  byType: Record<string, number>   // taxonomy item key -> count
}
```

`matchesPlanted` (finding vs. ground truth) is computed client-side by comparing
`matchedTypeKey` against `plantedBugs` — no need for the backend to pre-compute it.
`DashboardStats` itself, unlike that comparison, *is* computed server-side — see below.

Two fields from the mock are gone, not just flagged: `language` breakdowns and
`difficulty`. Language is a fixed single value for the whole MVP, so a per-language
breakdown would always show one 100% bucket — no information, not worth a chart.
`difficulty` (Beginner/Intermediate/Advanced in the mock) was an attribute of the mock's
six hand-picked examples; nothing in the README's pipeline assesses difficulty for a
generated run, and there's no plan to build that assessment, so the concept is dropped
outright rather than carried as an open question.

## API contract (backend not yet built — this is what the frontend calls)

Persistence: an in-memory list of runs on the backend process, lost on restart. No
database for MVP. Enough to back History and Dashboard for real, since a class session
doesn't span a backend reboot.

| Method & path              | Body                  | Returns              | Notes |
|------------------------------|------------------------|-----------------------|-------|
| `POST /api/runs`            | `GenerationRequest`    | `Run`                 | Invokes the full agent chain synchronously. The slow call. |
| `GET /api/runs`             | —                      | `HistoryRecord[]`     | List for History page. |
| `GET /api/runs/:id`         | —                      | `Run`                 | Expanding a History row. |
| `PATCH /api/runs/:id`       | `{ fixed: true }`      | `HistoryRecord`       | Called when "Fix Bugs" is revealed — lets Dashboard's fix-rate reflect reality. |
| `DELETE /api/runs`          | —                      | `204`                 | "Clear history." No per-row delete in v1, matching the mock. |
| `GET /api/dashboard`        | —                      | `DashboardStats`      | Pre-aggregated. See below. |
| `GET /api/runs/:id/download`| —                      | `application/zip`     | One run's artifacts. Replaces the mock's client-side zip builder. |
| `GET /api/runs/export`      | —                      | `application/zip`     | All runs, one archive — "Export all as .zip" on History. |

**No separate "fix" or "analyse" generation endpoint.** The fixed/corrected version and
the bug report are both produced by the one `POST /api/runs` call; "Fix Bugs" reveals
`Run.parallelVersionFixed`, already in hand from that response, and then fires
`PATCH /api/runs/:id` in the background purely so the backend's own record is accurate
for Dashboard/History — matching the mock's already-established pattern of treating this
as best-effort ("shown, but couldn't be saved" is an acceptable outcome, not an error
that blocks the reveal).

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
bugs… → Building harness… → Running blind analysis…" via polling or SSE) is a real
improvement but explicitly deferred — it needs a backend job/status model that doesn't
exist yet, and CLAUDE.md's instruction to keep surface area small for a beginner team
argues against building it before the plain version works.

## Source directory structure

```
frontend/src/
  api/
    runs.ts              generateRun, listRuns, getRun, markFixed, clearRuns — fetch wrappers
    dashboard.ts          getDashboardStats
    downloadUrl.ts         builds the href for the download-zip anchors — no fetch involved
  types/
    taxonomy.ts           TaxonomyItem, FailureCategory
    run.ts                GenerationRequest, PlantedBug, BugReportFinding, Run, HistoryRecord
  data/
    taxonomy.ts            the 19-item taxonomy — ported verbatim from DESIGN.html's CATEGORIES/BUG_TYPES
  components/
    Tag.tsx, PrimaryButton.tsx, GhostButton.tsx, NumberBadge.tsx   generic UI atoms, ported as-is
    icons.tsx                                                       CodeIcon/TextIcon/LinkIcon/ChevronIcon
    CodeBlock.tsx                                                   line-numbered code viewer, ported as-is
    ErrorCategorySelector.tsx, ErrorItem.tsx                        taxonomy picker, ported as-is
    ArtifactTabs.tsx           new — the 4-tab result viewer (replaces the mock's inline sub-tabs)
    BugReportPanel.tsx         new — replaces BugListItem; renders findings vs. planted, see below
    HistoryRow.tsx, StatCard.tsx, BreakdownCard.tsx                 ported as-is
  pages/
    LabPage.tsx, HistoryPage.tsx, DashboardPage.tsx, AboutPage.tsx
  App.tsx                    shell: brand, nav, error banner, page switch
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
  (`--color-accent`, `--color-incorrect-soft`, etc.) — same palette, no per-render style
  object allocation, and every component gets it via `className` + a stylesheet.
- Each component/page gets its own colocated `.css` file, same pattern as `App.css`.
- Layout (the mock's flex/grid arrangements) is reimplemented in plain CSS with the same
  breakpoints, not copied as Tailwind classes.

## State management

- Form state (Lab page inputs) — plain `useState`, matches the mock.
- Data fetching — three small custom hooks, one per domain: `useGenerateRun`,
  `useHistory`, `useDashboardStats`. Each wraps its `api/runs.ts` call and owns its own
  loading/error state. No Redux, no Zustand, no React Query — CLAUDE.md rules those out,
  and a four-page app with one slow mutation and two list reads doesn't need them.
- Nav state and the global error banner live in `App.tsx` and are passed down one level
  to whichever page is active — shallow enough that Context isn't justified yet.

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
| Test Harness  | **Test Harness**      | Unchanged |
| Explanation   | **Bug Report**        | Renamed and redesigned — see below |

**Bug Report tab** (`BugReportPanel`) is the piece the mock is missing entirely. Layout:
the analyser's findings (`Run.bugReport`) are shown first, in the analyser's own words,
with no reference to what was planted — preserving the "guess before you look" moment.
A second, separately-revealed section ("What was actually planted") shows `plantedBugs`
and highlights which findings matched. This keeps the mock's existing collapse-behind-a-
toggle pattern (`BugListItem`'s "Why?" disclosure) but applies it to the planted/found
seam instead of to a single canned explanation.

"Fix Bugs" stays a reveal of `Run.parallelVersionFixed`, next to or inside this tab —
no new request fires.

### History

Same shape as the mock, backed by `GET /api/runs` (list) and `GET /api/runs/:id` (row
expansion) instead of `window.storage` — that API is specific to the Claude Artifact
sandbox the mock was built in and doesn't exist in a deployed app. Each row's "Download"
link and the page's "Export all as .zip" button both become plain links to
`GET /api/runs/:id/download` and `GET /api/runs/export` respectively — no client-side
zip-building code at all.

### Dashboard

In scope for MVP, not deferred — sourced from `GET /api/dashboard`. Stat tiles: total
runs, fixed count, fix rate. Breakdown cards: by category, by taxonomy type. "By
language" and "By difficulty" from the mock's version are both gone — see the note
under `DashboardStats` above.

### About

Copy gets rewritten to drop the mock's "nothing is generated or executed" framing (no
longer true) and instead describe the real pipeline and the blind-analyser mechanism, so
it matches the README rather than contradicting it.

## Error handling and loading states

- `POST /api/runs`: button-level loading state (already the mock's pattern), plus a
  distinct message for "no example matches your filters" vs. a genuine network/backend
  failure (top-level error banner, also already the mock's pattern).
- No automatic retry/backoff. One manual retry (click Generate again) is enough for an
  MVP teaching tool — building retry logic here would be solving a problem nobody has yet.

## Testing

No test suite for MVP beyond `tsc` (already wired into `npm run build`) and manual
verification via `npm run dev`. Revisit with Vitest + React Testing Library only if the
team decides it's worth the setup cost — not assumed here.

## Open for confirmation

- **Is the `PATCH /api/runs/:id` fix-tracking call worth having** for a fix-rate stat
  that's a nice-to-have, not the product's core claim — versus just not tracking it and
  dropping "fix rate" from `DashboardStats`. Leaning toward keeping it: it's a one-field
  PATCH, and "which examples students most often need the answer for" seems like
  genuinely useful instructor signal, not decoration.
