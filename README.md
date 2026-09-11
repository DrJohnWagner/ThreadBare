# ThreadBare

ThreadBare is a small full-stack app: a React + TypeScript single-page front end and a
Python (FastAPI) back end.

This README is written for students who are new to Python virtual environments and to
React. Follow it top to bottom the first time you set the project up. After that, you'll
only need the "Everyday use" section.

## What's in this repo

```
backend/          Python API (FastAPI)
  app/main.py      the API application
  requirements.txt Python dependencies
frontend/          React + TypeScript SPA (created with Vite)
  src/App.tsx      the page you'll edit — shows the React logo and a counter
.venv/             your Python virtual environment (created below, not in git)
```

## Prerequisites

You need three things installed before you start: VS Code, Python 3.11+, and Node.js 20+.

### 1. VS Code

Download and install from [code.visualstudio.com](https://code.visualstudio.com/).

Open this project folder in VS Code (`File > Open Folder…`). When VS Code opens it, it
will prompt you to install some recommended extensions (Python and ESLint) — click
**Install All**. If it doesn't prompt you, open the Extensions panel
(`Cmd/Ctrl+Shift+X`) and install:

- **Python** (by Microsoft)
- **Pylance** (by Microsoft)
- **ESLint** (by Microsoft/dbaeumer)

### 2. Python

Check whether you already have Python 3.11 or newer:

```sh
python3 --version
```

If that prints `Python 3.11.x` or higher, you're set. If it's older, missing, or you're
on Windows and `python3` isn't recognised, install Python from
[python.org/downloads](https://www.python.org/downloads/). On Windows, tick
**"Add python.exe to PATH"** during install.

You do **not** need Python 3.14 specifically to run this project — anything 3.11+ works
with the instructions below.

### 3. Node.js

Check whether you already have Node.js:

```sh
node --version
npm --version
```

You need Node 20 or newer. If missing or older, install the **LTS** version from
[nodejs.org](https://nodejs.org/).

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
pip install -r backend/requirements.txt
```

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
source .venv/bin/activate   # if not already active; on Windows use .venv\Scripts\Activate.ps1
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

## Troubleshooting

- **`python3: command not found` / `'python3' is not recognized`** — Python isn't
  installed or isn't on your PATH. Reinstall from python.org and make sure the PATH
  option is checked (Windows).
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
