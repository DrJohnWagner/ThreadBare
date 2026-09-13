# ThreadBare

Generates **parallel programs with deliberate concurrency bugs**, for teaching.

Give ThreadBare a starting point — source code, a URL, or a plain-English description of a
computation — then choose a target language and threading model and pick which failure
modes to plant. A chain of LLM agents returns four artifacts:

| artifact | what it is |
|---|---|
| **serial reference** | the single-threaded implementation, and the oracle everything else is compared against |
| **parallel version** | the same computation, multithreaded, carrying the failure modes you selected — with nothing in the code pointing at them |
| **test harness** | drives both versions and compares them, so a planted failure is observed rather than asserted |
| **bug report** | what a separate LLM finds when handed the three files above and asked what is wrong |

The report is written by an agent that **did not plant the bugs**. It sees only the serial
code, the parallel code and the harness — the same evidence a student gets — which makes the
report a finding rather than a transcript. The gap between what was planted and what was
found is then information in its own right: a planted bug the analyser missed is either well
hidden or not actually present, and a bug it found that nobody planted is a real defect the
generator introduced by accident.

Failure modes come from a **19-item taxonomy in three families** — Incorrect (wrong results),
Slow (correct but doesn't scale), Non-terminating (never finishes). The taxonomy drives
selection, the report, and the coverage view across saved benchmarks.

**Input and target are independent.** Source material in Rust can produce a benchmark in
C/C++ with OpenMP; a prose description can produce one in any supported target. What you
supply defines the *computation*, not the language it comes back in.

Vite + React + TypeScript on the front end, Python/FastAPI behind it.

## Scope of this version

Shared-memory multithreading only. Distributed models such as MPI come later; the taxonomy
is written to absorb them without restructuring.

The audience is an **instructor building teaching examples** — programs with a known intended
defect, a harness that demonstrates it, and an explanation to mark against. The saved-benchmark
and coverage machinery is being built toward research use, evaluating bug-finding tools against
a collection with known answers, but that is not what this version is for.

This README is written for students who are new to Python virtual environments and to
React. Follow it top to bottom the first time you set the project up. After that, you'll
only need the "Everyday use" section.

## What's in this repo

```
backend/                Python API (FastAPI)
  app/main.py           the API application
  requirements.txt      runtime Python dependencies
  requirements-dev.txt  requirements.txt + Black (the code formatter)
frontend/               React + TypeScript SPA (created with Vite)
  src/App.tsx           the page you'll edit — shows the React logo and a counter
  .prettierrc.json      formatting rules ESLint applies via Prettier
pyproject.toml          Black's configuration (applies to backend/)
.venv/                  your Python virtual environment (created below, not in git)
```

## Prerequisites

You need four things installed before you start: Git, VS Code, Python 3.11+, and
Node.js 20+.

### 1. Git

Check whether you already have it:

```sh
git --version
```

If that prints a version number, skip to VS Code below.

**macOS:** running the command above with no Git installed triggers a macOS prompt to
install the "Command Line Tools" — accept it, wait for the install to finish, then run
`git --version` again to confirm. If you use Homebrew instead: `brew install git`.

**Windows:**

Download and run the installer from [git-scm.com/download/win](https://git-scm.com/download/win) —
accept the defaults on every screen. This also installs **Git Bash**, a terminal that
understands the `sh` (bash) commands used throughout this README, which the default
Windows Command Prompt does not. If you use `winget` instead:
`winget install --id Git.Git -e --source winget`.

After installing, close and reopen your terminal (or open Git Bash), then confirm with
`git --version`.

### 2. VS Code

Download and install from [code.visualstudio.com](https://code.visualstudio.com/) — same
installer page for macOS and Windows.

Open this project folder in VS Code (`File > Open Folder…`). When VS Code opens it, it
will prompt you to install some recommended extensions (Python and ESLint) — click
**Install All**. If it doesn't prompt you, open the Extensions panel
(`Cmd/Ctrl+Shift+X`) and install:

- **Python** (by Microsoft)
- **Pylance** (by Microsoft)
- **ESLint** (by Microsoft/dbaeumer)

### 3. Python

Check whether you already have Python 3.11 or newer:

```sh
python3 --version
```

If that prints `Python 3.11.x` or higher, you're set. Otherwise:

**macOS:** install from [python.org/downloads/macos](https://www.python.org/downloads/macos/)
(download the `.pkg` and run it), or with Homebrew: `brew install python@3.12`.

**Windows:** install from [python.org/downloads/windows](https://www.python.org/downloads/windows/)
— download the installer and run it. On the first install screen, tick
**"Add python.exe to PATH"** before clicking Install; this is the step people most often
miss. Or with winget: `winget install --id Python.Python.3.12 -e`. After installing on
Windows, `python3` may not exist — use `python --version` instead to check.

You do **not** need Python 3.14 specifically to run this project — anything 3.11+ works
with the instructions below.

### 4. Node.js

Check whether you already have Node.js:

```sh
node --version
npm --version
```

You need Node 20 or newer. If missing or older:

**macOS:** download the **LTS** installer from [nodejs.org](https://nodejs.org/) and run
it, or with Homebrew: `brew install node@20`.

**Windows:** download the **LTS** installer from [nodejs.org](https://nodejs.org/) and
run it (accept the defaults, including the optional Chocolatey/native-module step — you
can leave that unchecked), or with winget: `winget install --id OpenJS.NodeJS.LTS -e`.

After installing, close and reopen your terminal, then confirm with `node --version`.

## First-time setup

Do these steps once, in a terminal opened at the root of this project (in VS Code:
`Terminal > New Terminal`).

### Step 1 — Create the Python virtual environment

A virtual environment ("venv") is a private, isolated copy of Python for this project
only, so the packages you install here don't clash with anything else on your machine.

```sh
python3 -m venv .venv
```

This creates a `.venv/` folder in the project. It's already excluded from git — don't
commit it.

### Step 2 — Activate the virtual environment

Activating puts `.venv`'s Python and pip first on your PATH, so commands like `python`
and `pip` use the project's copy instead of your system's.

**macOS / Linux:**

```sh
source .venv/bin/activate
```

**Windows (PowerShell):**

```powershell
.venv\Scripts\Activate.ps1
```

**Windows (cmd.exe):**

```bat
.venv\Scripts\activate.bat
```

You'll know it worked because your terminal prompt now starts with `(.venv)`. You need
to do this activation step every time you open a new terminal to work on this project —
it does not stay active permanently.

If VS Code asks "Select a Python Interpreter" or shows a notification about the
workspace's Python environment, pick the one at `.venv/bin/python` (or
`.venv\Scripts\python.exe` on Windows). This project already tells VS Code to use that
interpreter by default via `.vscode/settings.json`.

### Step 3 — Install the Python dependencies

With the venv activated:

```sh
pip install -r backend/requirements-dev.txt
```

(`requirements-dev.txt` pulls in `requirements.txt` plus Black, the code formatter — see
"Code style" below. If you only ever want the app's own runtime dependencies, without
Black, install `backend/requirements.txt` instead.)

### Step 4 — Install the frontend dependencies

```sh
cd frontend
npm install
cd ..
```

This downloads React, Vite, TypeScript and everything else listed in
`frontend/package.json` into `frontend/node_modules/` (also not committed to git).

## Everyday use

Once first-time setup is done, each time you come back to work on the project:

1. Open the project folder in VS Code.
2. Open a terminal and activate the venv (Step 2 above) if you're running Python
   commands directly.
3. Start the backend and frontend (each in its own terminal — see below).

### Run the backend API

```sh
source .venv/bin/activate   # if not already active; on Windows use .venv\Scripts\activate.bat
uvicorn backend.app.main:app --reload
```

The API runs at `http://127.0.0.1:8000`. Visit `http://127.0.0.1:8000/api/health` in
your browser — you should see `{"status":"ok"}`. `--reload` restarts the server
automatically whenever you save a change to a Python file.

### Run the frontend

In a **second** terminal:

```sh
cd frontend
npm run dev
```

Open the URL it prints (usually `http://localhost:5173`) in your browser. You should
see the React logo and a "count is 0" button — click it and the count goes up. Edit
`frontend/src/App.tsx` and save; the page updates automatically without a manual
refresh.

## Code style

All code in this repo uses four-space indentation. Two tools enforce this automatically
— you shouldn't need to think about indentation by hand.

- **TypeScript / TSX** — ESLint checks correctness, and runs
  [Prettier](https://prettier.io/) as one of its rules (`frontend/eslint.config.js`,
  `frontend/.prettierrc.json`) to check formatting, including indentation. Run
  `npm run lint` (from `frontend/`) to check, or `npm run lint:fix` to auto-fix what it
  can.
- **Python** — [Black](https://black.readthedocs.io/) formats every file, no
  configuration needed (it's not a style you argue with — that's the point). With the
  venv active, run `black backend` to format, or `black --check backend` to check
  without changing anything.

If you installed the recommended VS Code extensions (see Prerequisites), both run
automatically on save: ESLint fixes TypeScript issues it can auto-fix, and the Black
Formatter extension reformats Python files.

## Troubleshooting

- **`git: command not found` / `'git' is not recognized`** — Git isn't installed or your
  terminal was opened before the install finished. Close and reopen the terminal; if that
  doesn't fix it, reinstall (see Prerequisites above).
- **`python3: command not found` / `'python3' is not recognized`** — Python isn't
  installed or isn't on your PATH. Reinstall from python.org and make sure the PATH
  option is checked (Windows). On Windows, also try `python --version` — the launcher is
  often named `python`, not `python3`.
- **`(.venv)` doesn't appear in your prompt after activating** — make sure you ran the
  activate command from the project root, and that `.venv/` exists (re-run Step 1 if
  not).
- **VS Code's terminal keeps using the wrong Python** — open the Command Palette
  (`Cmd/Ctrl+Shift+P`), run `Python: Select Interpreter`, and choose the one inside
  `.venv`.
- **`npm install` fails or hangs** — check your Node version (`node --version`); if it's
  below 20, install the current LTS release from nodejs.org and try again.
- **Port already in use** — if `5173` or `8000` is taken, stop whatever else is using it,
  or pass a different port (`npm run dev -- --port 5174`, `uvicorn ... --port 8001`).
