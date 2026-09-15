import os

from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from openai import OpenAI
from pydantic import BaseModel, Field

load_dotenv()

app = FastAPI(title="ThreadBare API")

app.add_middleware(
    CORSMiddleware,
    # Vite sometimes falls back to a different port (5174, 5175, ...) if
    # 5173 is already taken, so allow any localhost port during development.
    allow_origin_regex=r"http://localhost:\d+",
    allow_methods=["*"],
    allow_headers=["*"],
)

# OpenRouter exposes an OpenAI-compatible API, so the official openai client
# works as-is — we just point it at OpenRouter's base URL and pass an
# OpenRouter key instead of an OpenAI one.
openrouter_client = OpenAI(
    base_url="https://openrouter.ai/api/v1",
    api_key=os.environ.get("OPENROUTER_API_KEY"),
)

POEM_MODEL = "nvidia/nemotron-3.5-lightning:free"


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


class GreetRequest(BaseModel):
    name: str = Field(min_length=1)


@app.post("/api/greet")
def greet(request: GreetRequest) -> dict[str, str]:
    return {"message": f"Hello, {request.name}!"}


@app.post("/api/poem")
def poem(request: GreetRequest) -> dict[str, str]:
    try:
        completion = openrouter_client.chat.completions.create(
            model=POEM_MODEL,
            messages=[
                {
                    "role": "user",
                    "content": (
                        "Write a short, safe-for-work poem about a person "
                        f"named {request.name}."
                    ),
                }
            ],
        )
    except Exception as error:
        raise HTTPException(
            status_code=502, detail="Could not reach the LLM provider."
        ) from error

    return {"message": completion.choices[0].message.content}
