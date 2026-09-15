"""Shared mechanics for calling an agent — the one place that knows how to ask the
model for structured output and turn the result into a typed, validated object.

Deliberately does NOT use the OpenAI SDK's `.chat.completions.parse()` convenience
method. Empirically, when this project was calling OpenRouter instead of OpenAI
directly, `.parse()` silently returned `message.parsed = None` (no exception, no
`.refusal`) even when the model's raw `message.content` was valid JSON that matched
the schema exactly — confirmed by parsing that same content by hand. So: ask for
structured output via a plain `response_format` JSON Schema (a hint to the model, not
something this code trusts blindly), then parse and validate the returned text
ourselves — the same thing `.parse()` claims to do, just visible and under our own
control. Kept even after moving to OpenAI directly, since it's strictly more visible
on failure and costs nothing extra.

Does not pass `temperature` — gpt-5-nano rejects any value other than its default (1)
with a 400. If a future model swap needs per-agent temperature control again, it has
to come back as a parameter here, gated on whether the configured model supports it.
"""

import json

from pydantic import BaseModel, ValidationError

from .config import MODEL, get_client


class AgentCallError(RuntimeError):
    """Raised when an agent call fails outright: a refusal, empty content, invalid
    JSON, or JSON that doesn't validate against the expected output shape. Callers
    should let this propagate — see AGENTS.md's "Mid-pipeline failure" decision.
    """


def _forbid_additional_properties(node: object) -> None:
    """Recursively set `additionalProperties: false` on every object schema.

    OpenAI's strict Structured Outputs mode requires this on every nested object —
    not just the root — including each entry under `$defs`. Pydantic's
    `model_json_schema()` doesn't set it anywhere, so this walks the whole tree
    (`$defs`, `properties`, `items`, `anyOf`/`allOf`/`oneOf`) and adds it everywhere
    an object schema appears.
    """
    if isinstance(node, dict):
        if node.get("type") == "object":
            node["additionalProperties"] = False
        for key in ("$defs", "properties"):
            for child in node.get(key, {}).values():
                _forbid_additional_properties(child)
        for key in ("anyOf", "allOf", "oneOf"):
            for child in node.get(key, []):
                _forbid_additional_properties(child)
        if "items" in node:
            _forbid_additional_properties(node["items"])


def _schema_for(output_model: type[BaseModel]) -> dict:
    schema = output_model.model_json_schema()
    _forbid_additional_properties(schema)
    return schema


def call_agent[T: BaseModel](
    *,
    system_prompt: str,
    user_prompt: str,
    output_model: type[T],
) -> T:
    client = get_client()
    completion = client.chat.completions.create(
        model=MODEL,
        messages=[
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ],
        response_format={
            "type": "json_schema",
            "json_schema": {
                "name": output_model.__name__,
                "schema": _schema_for(output_model),
                "strict": True,
            },
        },
    )

    choice = completion.choices[0]
    message = choice.message
    if message.refusal:
        raise AgentCallError(f"model refused to respond: {message.refusal}")
    if not message.content:
        raise AgentCallError(
            f"model returned no content (finish_reason={choice.finish_reason!r})"
        )

    try:
        data = json.loads(message.content)
    except json.JSONDecodeError as exc:
        raise AgentCallError(f"model response was not valid JSON: {exc}") from exc

    try:
        return output_model.model_validate(data)
    except ValidationError as exc:
        raise AgentCallError(
            f"model response did not match {output_model.__name__}: {exc}"
        ) from exc
