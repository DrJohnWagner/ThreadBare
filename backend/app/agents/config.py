"""Model/client configuration for the generation pipeline.

Uses the OpenAI API directly. The key lives in a local .env — never committed, see
.gitignore — loaded via python-dotenv's find_dotenv() so it works regardless of which
directory the process is started from.
"""

import os
from functools import lru_cache

from dotenv import find_dotenv, load_dotenv
from openai import OpenAI

load_dotenv(find_dotenv())

MODEL = os.environ.get("THREADBARE_MODEL", "gpt-5-nano")


@lru_cache
def get_client() -> OpenAI:
    """Build the OpenAI client.

    Cached so every agent call reuses one client rather than reconnecting. Raises a
    clear error at call time (not import time) if the key is missing, so importing
    this module — or any agent module — never fails just because .env isn't set up
    yet; only actually trying to call the API does.
    """
    api_key = os.environ.get("OPENAI_API_KEY")
    if not api_key:
        raise RuntimeError(
            "OPENAI_API_KEY is not set. Add it to a .env file at the project "
            "root (see .env.example if present) — never commit the key itself."
        )
    return OpenAI(api_key=api_key)
