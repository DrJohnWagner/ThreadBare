"""Model/client configuration for the generation pipeline.

Every agent hits one OpenAI-compatible endpoint, chosen entirely from .env — swapping
providers, models, or keys never requires touching this file. .env supplies exactly
three variables: MODEL (the model name to request), API_KEY (that provider's key), and
PROVIDER (which base URL to send it to — one of the names in _PROVIDER_BASE_URLS
below). The key lives in a local .env — never committed, see .gitignore — loaded via
python-dotenv's find_dotenv() so it works regardless of which directory the process is
started from.
"""

import os
from functools import lru_cache

from dotenv import find_dotenv, load_dotenv
from openai import OpenAI

load_dotenv(find_dotenv())

MODEL = os.environ.get("MODEL", "kimi-k2.7-code")
PROVIDER = os.environ.get("PROVIDER", "openai")
# Optional cap on output tokens, reasoning included; unset means the provider's
# default. Kimi K2.6's default runs out mid-reply on the longer agents.
MAX_COMPLETION_TOKENS = int(os.environ.get("MAX_COMPLETION_TOKENS") or 0) or None

# base_url for each supported PROVIDER value; "openai" maps to None so the OpenAI SDK
# uses its own built-in default rather than this file hardcoding it.
_PROVIDER_BASE_URLS: dict[str, str | None] = {
    "openai": None,
    "openrouter": "https://openrouter.ai/api/v1",
    "moonshot": "https://api.moonshot.ai/v1",
}


@lru_cache
def get_client() -> OpenAI:
    """Build the OpenAI-compatible client for whichever provider .env names.

    Cached so every agent call reuses one client rather than reconnecting. Raises a
    clear error at call time (not import time) if a required variable is missing or
    unrecognized, so importing this module — or any agent module — never fails just
    because .env isn't set up yet; only actually trying to call the API does.
    """
    api_key = os.environ.get("API_KEY")
    if not api_key:
        raise RuntimeError(
            "API_KEY is not set. Add it to a .env file at the project root (see "
            ".env.example) — never commit the key itself."
        )
    if PROVIDER not in _PROVIDER_BASE_URLS:
        raise RuntimeError(
            f"PROVIDER={PROVIDER!r} is not recognized. Add it to _PROVIDER_BASE_URLS "
            f"in config.py, or use one of: {', '.join(sorted(_PROVIDER_BASE_URLS))}."
        )
    return OpenAI(api_key=api_key, base_url=_PROVIDER_BASE_URLS[PROVIDER])
