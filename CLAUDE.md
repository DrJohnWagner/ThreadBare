# ThreadBare — instructions for Claude

Stack: React 19 + TypeScript SPA (`frontend/`, Vite) and a FastAPI backend (`backend/`).

## Who's working here

Four students, mixed experience: some have never used git, none have used React before.
They will often be driving Claude Code directly. Assume no prior git vocabulary — don't
say "rebase" or "fast-forward" without a one-clause explanation the first time in a
session.

## Git — hard constraints

- Never commit or push directly to `main`. Every change goes through a feature branch and
  a GitHub PR, even for a one-line fix.
- Before branching: `git fetch origin && git checkout main && git pull`, so the new
  branch starts from current `main`, not a stale local copy.
- One branch per unit of work — see naming scheme below.
- Small commits over large ones. Each commit should leave the app in a working state
  (frontend builds, backend imports) — don't commit mid-break.
- Never force-push. Never rewrite history (`rebase -i`, `commit --amend`, `push -f`) on a
  branch that's already pushed, without asking first — someone else may have pulled it.
- Merge to `main` via a GitHub PR, not local `git merge`. If a student asks to "merge my
  branch in," the answer is "open a PR," not `git checkout main && git merge`.
- Resolving a conflict: read both sides of every conflicting hunk and say in plain
  language what each side was trying to do before picking or combining — never resolve by
  blindly keeping "ours" or "theirs."
- Before any command that can discard uncommitted work (`checkout`, `restore`, `reset`,
  `clean`), run `git status` first and stash (`git stash -u`) or commit anything found —
  a lost afternoon of a student's work is a worse outcome here than almost anywhere else
  this instruction applies.
- If it's unclear which branch a student is on, run `git branch --show-current` and state
  it before doing anything else.
- If a git action would touch `main` directly, touch another student's branch, or discard
  work, stop and confirm in plain language before running it — don't just run it.

## React — keep the surface area small

- Function components + hooks only, matching `App.tsx`. Don't introduce Redux, Zustand,
  React Router, CSS-in-JS, or a component library unless a student explicitly asks — more
  surface area means more for a beginner to get stuck on.
- The first time a session uses a hook or pattern not already present in the codebase
  (`useEffect`, `useContext`, custom hooks, etc.), say in one sentence what it does before
  using it.
- Keep components small; colocate styles the way `App.tsx`/`App.css` already do.

## Code style

Four-space indentation everywhere, enforced by tooling, not by hand:

- TypeScript/TSX: ESLint runs Prettier as a rule (`frontend/eslint.config.js`,
  `frontend/.prettierrc.json`, tabWidth 4). Run `npm run lint:fix` after editing, or
  trust it (VS Code fixes on save with the recommended extensions installed).
- Python: Black (`backend/requirements-dev.txt`). Run `black backend` after editing.

Don't hand-indent to "look right" and skip the formatter — run it, don't eyeball it.

## General

- When a student is trying to learn the step themselves ("how do I...", "what does this
  do"), explain and let them run it. When they clearly just want the outcome ("fix this",
  "add X"), do it directly — don't force a lesson on someone who didn't ask for one.
- State non-obvious git commands and what they'll do before running them; don't narrate
  routine reads (`status`, `diff`, `log`).
