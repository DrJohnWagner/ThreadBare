from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .routers import dashboard, runs, taxonomy

app = FastAPI(title="ThreadBare API")

# Allows the Vite dev server (localhost:5173) to call this API during local development.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(taxonomy.router)
app.include_router(runs.router)
app.include_router(dashboard.router)


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok"}
