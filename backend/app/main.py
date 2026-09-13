from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

app = FastAPI(title="ThreadBare API")

# Allows the Vite dev server (localhost:5173) to call this API during local development.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


class GreetRequest(BaseModel):
    name: str = Field(min_length=1)


@app.post("/api/greet")
def greet(request: GreetRequest) -> dict[str, str]:
    return {"message": f"Hello, {request.name}!"}
