#!/usr/bin/env python3
"""Smoke test for HexMag swarm endpoints.

- /ask: verifies synchronous Q&A returns JSON with an answer.
- /agent: verifies streaming goal execution reaches goal.satisfied.

Designed for CI usage; defaults to the test port 8001.
"""
from __future__ import annotations

import json
import sys
from typing import Optional

import httpx

SWARM_HOST = "http://localhost:8001"
REQUEST_TIMEOUT = 45  # seconds


def request_json(path: str, payload: Optional[dict[str, object]] = None) -> dict[str, object]:
    url = f"{SWARM_HOST}{path}"
    try:
        response = httpx.post(url, json=payload, timeout=REQUEST_TIMEOUT)
        response.raise_for_status()
    except httpx.RequestError as exc:  # network/connection issues
        raise RuntimeError(f"Request to {url} failed: {exc}") from exc
    except httpx.HTTPStatusError as exc:  # non-2xx responses
        raise RuntimeError(
            f"Endpoint {url} returned {exc.response.status_code}: {exc.response.text.strip()}"
        ) from exc

    content_type = response.headers.get("content-type", "")
    if "application/json" not in content_type:
        raise RuntimeError(f"Unexpected content-type for {url}: {content_type}")

    try:
        return response.json()
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"Invalid JSON from {url}: {response.text[:200]}") from exc


def test_ask() -> None:
    print("Running /ask smoke test...")
    payload = {
        "question": "What is the core benefit of the HexMag architecture?",
        "code": "class BotState: pass",
    }
    data = request_json("/ask", payload)
    answer = data.get("answer")
    if not isinstance(answer, str) or not answer.strip():
        raise AssertionError(f"/ask response missing non-empty answer field: {data}")
    print(f"/ask succeeded; answer snippet: {answer[:80]!r}")


def test_agent() -> None:
    print("Running /agent smoke test...")
    payload = {
        "goal": "List the first three prime numbers and mark goal satisfied.",
        "max_time": 30,
    }
    url = f"{SWARM_HOST}/agent"

    try:
        with httpx.stream("POST", url, json=payload, timeout=REQUEST_TIMEOUT) as response:
            response.raise_for_status()
            satisfied = False
            for line in response.iter_lines():
                if not line or not line.startswith("data:"):
                    continue
                try:
                    message = json.loads(line[5:].strip())
                except json.JSONDecodeError:
                    continue
                if message.get("kind") == "goal.satisfied":
                    satisfied = True
                    break
    except httpx.RequestError as exc:
        raise RuntimeError(f"Streaming request to {url} failed: {exc}") from exc
    except httpx.HTTPStatusError as exc:
        raise RuntimeError(
            f"Streaming endpoint {url} returned {exc.response.status_code}: {exc.response.text.strip()}"
        ) from exc

    if not satisfied:
        raise AssertionError("/agent stream ended without goal.satisfied event")
    print("/agent succeeded; goal.satisfied observed.")


def main() -> int:
    try:
        print("Starting HexMag smoke tests...")
        test_ask()
        test_agent()
        print("All smoke tests passed.")
        return 0
    except Exception as exc:
        print(f"Smoke tests failed: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
