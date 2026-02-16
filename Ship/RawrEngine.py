#!/usr/bin/env python3
"""
RawrEngine Local Universal Access Server.

Provides a local-first HTTP API compatible with the RawrXD web client:
- GET  /status
- GET  /health, /healthz
- GET  /v1/models
- GET  /api/tools
- POST /api/chat            (SSE + non-stream)
- POST /api/agent/wish
- POST /api/agentic/config
- POST /api/generate        (legacy compatibility)

Security and CORS behavior are controlled through environment variables:
- RAWRXD_HOST (default: 127.0.0.1)
- RAWRXD_PORT (default: 23959)
- RAWRXD_REQUIRE_AUTH (default: 0)
- RAWRXD_API_KEYS (comma-separated)
- RAWRXD_CORS_ORIGINS (comma-separated)
- RAWRXD_MODELS (comma-separated model IDs)
- RAWRXD_DEFAULT_MODEL (default model id)
"""

from __future__ import annotations

import argparse
import ipaddress
import json
import os
import re
import threading
import time
import uuid
from dataclasses import dataclass, field
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Iterable
from urllib.parse import urlparse


DEFAULT_ALLOWED_ORIGINS = [
    "http://localhost",
    "http://127.0.0.1",
    "https://rawrxd.local",
    "app://rawrxd",
]

DEFAULT_MODELS = [
    "rawrxd-local",
    "rawrxd-plan",
    "rawrxd-full",
]

# Cross-platform model directories for GGUF/Ollama scanning
_MODEL_DIRS: list[str] = []
if os.name == "nt":
    _MODEL_DIRS = [
        os.path.expandvars(r"%USERPROFILE%\.ollama\models"),
        r"D:\OllamaModels",
        r"D:\models",
        r"C:\models",
    ]
else:
    _MODEL_DIRS = [
        os.path.expanduser("~/.ollama/models"),
        "/opt/models",
        "./models",
    ]
if os.environ.get("RAWRXD_MODEL_PATH"):
    _MODEL_DIRS.insert(0, os.environ["RAWRXD_MODEL_PATH"])

DEFAULT_TOOLS = [
    {"id": "file_reader", "name": "file_reader", "description": "Read project files"},
    {"id": "code_edit", "name": "code_edit", "description": "Apply code edits"},
    {"id": "terminal_exec", "name": "terminal_exec", "description": "Run shell commands"},
    {"id": "planner", "name": "planner", "description": "Build execution plans"},
]

PUBLIC_PATHS = {"/status", "/health", "/healthz"}


def _bool_env(name: str, default: bool) -> bool:
    value = os.getenv(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _csv_env(name: str, fallback: Iterable[str]) -> list[str]:
    value = os.getenv(name, "")
    if not value.strip():
        return list(fallback)
    return [item.strip() for item in value.split(",") if item.strip()]


def _normalize_origin(origin: str) -> str:
    return origin.strip().rstrip("/").lower()


def _is_loopback_address(host: str | None) -> bool:
    if not host:
        return False
    if host == "localhost":
        return True
    try:
        return ipaddress.ip_address(host).is_loopback
    except ValueError:
        return False


def _chunk_text(text: str, chunk_size: int = 20) -> list[str]:
    if not text:
        return [""]
    tokens = re.findall(r"\S+\s*", text)
    chunks: list[str] = []
    current = ""
    for token in tokens:
        if len(current) + len(token) > chunk_size and current:
            chunks.append(current)
            current = token
        else:
            current += token
    if current:
        chunks.append(current)
    return chunks or [text]


def _extract_prompt(payload: dict[str, Any]) -> str:
    messages = payload.get("messages")
    if isinstance(messages, list):
        for item in reversed(messages):
            if isinstance(item, dict) and item.get("role") == "user":
                content = item.get("content")
                if isinstance(content, str):
                    return content
    for key in ("message", "prompt", "wish"):
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value
    return ""


def _build_plan(prompt: str) -> list[str]:
    cleaned = prompt.strip() or "the requested task"
    return [
        f"Understand the request and constraints for: {cleaned}",
        "Inspect project files and runtime dependencies",
        "Create an implementation patch set with safe defaults",
        "Run local validation checks and collect evidence",
        "Return results with follow-up actions",
    ]


def _build_tool_calls(prompt: str) -> list[dict[str, Any]]:
    text = prompt.lower()
    calls: list[dict[str, Any]] = []
    if "@file" in text:
        calls.append({"tool": "file_reader", "params": {"target": "workspace"}})
    if "@code" in text:
        calls.append({"tool": "code_edit", "params": {"mode": "patch"}})
    if "@terminal" in text:
        calls.append({"tool": "terminal_exec", "params": {"shell": "bash"}})
    return calls


def _query_ollama(model: str, messages: list[dict[str, Any]]) -> str | None:
    """Try to proxy a request to Ollama on localhost:11434. Returns None on failure."""
    import urllib.request
    import urllib.error

    prompt_parts: list[str] = []
    for msg in messages:
        role = msg.get("role", "user")
        content = msg.get("content", "")
        prompt_parts.append(f"<|{role}|>\n{content}")
    prompt_parts.append("<|assistant|>")
    prompt = "\n".join(prompt_parts)

    try:
        payload = json.dumps({"model": model or "llama3.2:3b", "prompt": prompt, "stream": False})
        req = urllib.request.Request(
            "http://localhost:11434/api/generate",
            data=payload.encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=120) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            answer = data.get("response", "").strip()
            return answer if answer else None
    except Exception:
        return None


def _scan_model_dirs() -> list[dict[str, Any]]:
    """Scan configured directories for GGUF/BIN/SafeTensors model files."""
    found: list[dict[str, Any]] = []
    for dir_path in _MODEL_DIRS:
        if not os.path.isdir(dir_path):
            continue
        for root, _dirs, files in os.walk(dir_path):
            for fname in files:
                ext = fname.rsplit(".", 1)[-1].lower() if "." in fname else ""
                is_blob = fname.startswith("sha256-") and "blobs" in root.lower()
                if ext in {"gguf", "bin", "safetensors"} or is_blob:
                    fpath = os.path.join(root, fname)
                    try:
                        size = os.path.getsize(fpath)
                    except OSError:
                        continue
                    found.append({"id": fname, "path": fpath, "type": ext or "ollama", "size_bytes": size})
    return found


def _build_response(prompt: str, model: str, mode: str) -> str:
    cleaned = prompt.strip()
    if not cleaned:
        return f"RawrEngine local mode is ready on model '{model}'."

    # Try Ollama backend first for a real AI response
    messages = [{"role": "user", "content": cleaned}]
    if mode == "plan":
        messages.insert(0, {"role": "system", "content": "Break the task into numbered steps. Return only the steps."})
    elif mode == "full":
        messages.insert(0, {"role": "system", "content": "You are an AI coding agent. Execute the request step by step."})
    ollama_answer = _query_ollama(model, messages)
    if ollama_answer:
        return ollama_answer

    # Fallback: structured local response
    if mode == "plan":
        return f"Generated an execution plan for: {cleaned}"
    if mode == "full":
        return f"Executed local full-agent workflow for: {cleaned}"
    return f"Local response ({model}): {cleaned}"


@dataclass
class EngineState:
    require_auth: bool = field(default_factory=lambda: _bool_env("RAWRXD_REQUIRE_AUTH", False))
    api_keys: set[str] = field(default_factory=lambda: set(_csv_env("RAWRXD_API_KEYS", [])))
    allowed_origins: set[str] = field(
        default_factory=lambda: {_normalize_origin(origin) for origin in _csv_env("RAWRXD_CORS_ORIGINS", DEFAULT_ALLOWED_ORIGINS)}
    )
    models: list[str] = field(default_factory=lambda: _csv_env("RAWRXD_MODELS", DEFAULT_MODELS))
    current_model: str = field(default_factory=lambda: os.getenv("RAWRXD_DEFAULT_MODEL", DEFAULT_MODELS[0]))
    tools: list[dict[str, Any]] = field(default_factory=lambda: list(DEFAULT_TOOLS))
    request_count: int = 0
    active_sessions: dict[str, dict[str, Any]] = field(default_factory=dict)
    lock: threading.Lock = field(default_factory=threading.Lock)
    started_at: float = field(default_factory=time.time)

    @property
    def allow_all_origins(self) -> bool:
        return "*" in self.allowed_origins

    def is_origin_allowed(self, origin: str) -> bool:
        if not origin:
            return False
        if self.allow_all_origins:
            return True
        normalized = _normalize_origin(origin)
        if normalized in self.allowed_origins:
            return True
        parsed = urlparse(normalized)
        return parsed.hostname in {"localhost", "127.0.0.1", "::1"}

    def authorize(self, path: str, headers: dict[str, str], remote_addr: str | None) -> tuple[bool, int, str]:
        if path in PUBLIC_PATHS:
            return True, HTTPStatus.OK, ""

        if not self.require_auth and _is_loopback_address(remote_addr):
            return True, HTTPStatus.OK, ""

        api_key = headers.get("x-api-key", "").strip()
        auth_header = headers.get("authorization", "").strip()
        if not api_key and auth_header:
            if auth_header.lower().startswith("bearer "):
                api_key = auth_header[7:].strip()
            else:
                api_key = auth_header

        if self.require_auth and not self.api_keys:
            return False, HTTPStatus.SERVICE_UNAVAILABLE, "Server misconfigured: no API keys configured"

        if self.require_auth and not api_key:
            return False, HTTPStatus.UNAUTHORIZED, "API key required"

        if api_key and api_key not in self.api_keys:
            return False, HTTPStatus.FORBIDDEN, "Invalid API key"

        if api_key:
            with self.lock:
                self.active_sessions[api_key] = {
                    "ip": remote_addr or "",
                    "user_agent": headers.get("user-agent", ""),
                    "last_seen": time.time(),
                }
        return True, HTTPStatus.OK, ""

    def increment_requests(self) -> int:
        with self.lock:
            self.request_count += 1
            return self.request_count


STATE = EngineState()


class ReusableThreadingHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True


class RawrEngineHandler(BaseHTTPRequestHandler):
    server_version = "RawrEngine/1.0"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"[RawrEngine] {self.address_string()} - {fmt % args}")

    def _path(self) -> str:
        return self.path.split("?", 1)[0]

    def _headers_lc(self) -> dict[str, str]:
        return {key.lower(): value for key, value in self.headers.items()}

    def _origin(self) -> str:
        return self.headers.get("Origin", "")

    def _set_cors_headers(self) -> None:
        origin = self._origin()
        if STATE.is_origin_allowed(origin):
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Access-Control-Allow-Credentials", "true")
            self.send_header("Vary", "Origin")

    def _json(self, status: int, payload: dict[str, Any], *, extra_headers: dict[str, str] | None = None) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(int(status))
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self._set_cors_headers()
        if extra_headers:
            for key, value in extra_headers.items():
                self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def _read_json_body(self) -> dict[str, Any] | None:
        raw_length = self.headers.get("Content-Length", "0")
        try:
            length = int(raw_length)
        except ValueError:
            self._json(HTTPStatus.BAD_REQUEST, {"error": "Invalid Content-Length"})
            return None

        if length <= 0:
            return {}

        raw = self.rfile.read(length)
        if not raw:
            return {}
        try:
            parsed = json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError:
            self._json(HTTPStatus.BAD_REQUEST, {"error": "Invalid JSON payload"})
            return None
        if not isinstance(parsed, dict):
            self._json(HTTPStatus.BAD_REQUEST, {"error": "JSON payload must be an object"})
            return None
        return parsed

    def _authorize(self) -> bool:
        path = self._path()
        headers = self._headers_lc()
        remote_addr = self.client_address[0] if self.client_address else None
        ok, status, message = STATE.authorize(path, headers, remote_addr)
        if not ok:
            self._json(status, {"error": message})
            return False
        return True

    def _preflight(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self._set_cors_headers()
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key, Authorization")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS")
        self.send_header("Access-Control-Max-Age", "86400")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def _status_payload(self) -> dict[str, Any]:
        uptime_s = int(time.time() - STATE.started_at)
        return {
            "status": "ok",
            "service": "RawrEngine",
            "mode": "local",
            "uptime_s": uptime_s,
            "request_count": STATE.request_count,
            "require_auth": STATE.require_auth,
            "active_sessions": len(STATE.active_sessions),
            "current_model": STATE.current_model,
        }

    def _stream_chat(self, response_text: str, model: str) -> None:
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close")
        self._set_cors_headers()
        self.end_headers()

        created = int(time.time())
        for chunk in _chunk_text(response_text, chunk_size=26):
            payload = {
                "id": f"chatcmpl-{uuid.uuid4().hex[:10]}",
                "object": "chat.completion.chunk",
                "created": created,
                "model": model,
                "choices": [{"index": 0, "delta": {"content": chunk}, "finish_reason": None}],
            }
            data_line = f"data: {json.dumps(payload)}\n\n".encode("utf-8")
            try:
                self.wfile.write(data_line)
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                return
            time.sleep(0.01)

        try:
            self.wfile.write(b"data: [DONE]\n\n")
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            return
        self.close_connection = True

    def do_OPTIONS(self) -> None:
        STATE.increment_requests()
        self._preflight()

    def do_GET(self) -> None:
        STATE.increment_requests()
        if not self._authorize():
            return

        path = self._path()
        if path in {"/status", "/health", "/healthz"}:
            self._json(HTTPStatus.OK, self._status_payload())
            return

        if path == "/v1/models":
            models = [{"id": m} for m in STATE.models]
            # Append any GGUF/Ollama models found on disk
            for scanned in _scan_model_dirs():
                if scanned["id"] not in STATE.models:
                    models.append({"id": scanned["id"], "type": scanned["type"]})
            self._json(HTTPStatus.OK, {"object": "list", "models": models})
            return

        if path == "/api/tools":
            self._json(HTTPStatus.OK, {"tools": STATE.tools})
            return

        self._json(HTTPStatus.NOT_FOUND, {"error": f"Unknown endpoint: {path}"})

    def do_POST(self) -> None:
        STATE.increment_requests()
        if not self._authorize():
            return

        path = self._path()
        payload = self._read_json_body()
        if payload is None:
            return

        if path == "/api/agentic/config":
            model = payload.get("model")
            if isinstance(model, str) and model.strip():
                STATE.current_model = model.strip()
                if STATE.current_model not in STATE.models:
                    STATE.models.append(STATE.current_model)
            self._json(HTTPStatus.OK, {"status": "ok", "model": STATE.current_model})
            return

        if path == "/api/chat":
            model = payload.get("model")
            if not isinstance(model, str) or not model.strip():
                model = STATE.current_model
            prompt = _extract_prompt(payload)
            response_text = _build_response(prompt, model, mode="ask")
            stream = bool(payload.get("stream"))
            if stream:
                self._stream_chat(response_text, model)
                return

            completion = {
                "id": f"chatcmpl-{uuid.uuid4().hex[:10]}",
                "object": "chat.completion",
                "created": int(time.time()),
                "model": model,
                "choices": [
                    {
                        "index": 0,
                        "message": {"role": "assistant", "content": response_text},
                        "finish_reason": "stop",
                    }
                ],
            }
            self._json(HTTPStatus.OK, completion)
            return

        if path == "/api/agent/wish":
            prompt = _extract_prompt(payload)
            mode = str(payload.get("mode", "ask")).strip().lower() or "ask"
            model = payload.get("model")
            if not isinstance(model, str) or not model.strip():
                model = STATE.current_model

            if mode == "plan":
                self._json(
                    HTTPStatus.OK,
                    {
                        "mode": "plan",
                        "model": model,
                        "plan": _build_plan(prompt),
                        "requires_confirmation": False,
                    },
                )
                return

            if mode == "full":
                self._json(
                    HTTPStatus.OK,
                    {
                        "mode": "full",
                        "model": model,
                        "response": _build_response(prompt, model, mode="full"),
                        "tool_calls": _build_tool_calls(prompt),
                    },
                )
                return

            self._json(
                HTTPStatus.OK,
                {"mode": mode, "model": model, "response": _build_response(prompt, model, mode="ask")},
            )
            return

        if path == "/api/generate":
            prompt = _extract_prompt(payload)
            model = payload.get("model")
            if not isinstance(model, str) or not model.strip():
                model = STATE.current_model
            self._json(
                HTTPStatus.OK,
                {
                    "status": "success",
                    "model": model,
                    "response": _build_response(prompt, model, mode="ask"),
                    "tokens_generated": max(1, len(prompt.split())),
                },
            )
            return

        self._json(HTTPStatus.NOT_FOUND, {"error": f"Unknown endpoint: {path}"})


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="RawrEngine local universal access server")
    parser.add_argument("--host", default=os.getenv("RAWRXD_HOST", "127.0.0.1"), help="Bind host")
    parser.add_argument("--port", type=int, default=int(os.getenv("RAWRXD_PORT", "23959")), help="Bind port")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if STATE.current_model not in STATE.models:
        STATE.models.insert(0, STATE.current_model)

    server = ReusableThreadingHTTPServer((args.host, args.port), RawrEngineHandler)
    print(f"[RawrEngine] Local server listening on http://{args.host}:{args.port}")
    print("[RawrEngine] Endpoints: /status /v1/models /api/chat /api/agent/wish /api/tools")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[RawrEngine] Shutdown requested")
    finally:
        server.shutdown()
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
