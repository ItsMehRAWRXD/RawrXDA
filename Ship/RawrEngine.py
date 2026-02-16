#!/usr/bin/env python3
"""
RawrEngine.py — Universal Access HTTP API (stdlib-only)

This is a lightweight RawrEngine-compatible HTTP server intended for:
  - Docker / headless deployments
  - Web UI access via CORS + optional API key auth

It can proxy chat/model requests to an upstream OpenAI-compatible inference server
(recommended: Ollama on `http://localhost:11434`).

Endpoints (subset used by web_interface/index.html):
  - GET  /status
  - GET  /v1/models
  - GET  /api/tools
  - POST /api/agentic/config
  - POST /api/chat            (supports stream=true; OpenAI-style SSE passthrough)
  - POST /api/agent/wish      (mode: ask|plan|full)

Env:
  - RAWRXD_HOST (default: 0.0.0.0)
  - RAWRXD_PORT (default: 23959)
  - RAWRXD_CORS_ORIGINS (csv)
  - RAWRXD_API_KEYS (csv)
  - RAWRXD_REQUIRE_AUTH (truthy)
  - RAWRXD_INFERENCE_URL (default: http://localhost:11434)
  - RAWRXD_MODEL_ID (default model id)
"""

from __future__ import annotations

import json
import os
import time
import socket
import urllib.request
import urllib.error
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Dict, List, Optional, Tuple


def _split_csv(v: str) -> List[str]:
    return [x.strip() for x in (v or "").split(",") if x.strip()]


def _truthy(v: Optional[str]) -> bool:
    if not v:
        return False
    return v.strip().lower() in ("1", "true", "yes", "on")


def _json_bytes(obj: Any) -> bytes:
    return json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def _read_body(handler: BaseHTTPRequestHandler) -> bytes:
    length = int(handler.headers.get("Content-Length", "0") or "0")
    if length <= 0:
        return b""
    return handler.rfile.read(length)


def _safe_origin(origin: str) -> str:
    return (origin or "").strip()


def _origin_allowed(origin: str, allowed: List[str]) -> bool:
    origin = _safe_origin(origin)
    if not origin or origin == "null":
        return False
    if "*" in allowed:
        return True
    if origin in allowed:
        return True
    if origin.startswith("http://localhost") or origin.startswith("http://127.0.0.1"):
        return True
    if origin.startswith("https://localhost") or origin.startswith("https://127.0.0.1"):
        return True
    return False


def _extract_api_key(headers) -> str:
    key = (headers.get("X-API-Key") or "").strip()
    if key:
        return key
    auth = (headers.get("Authorization") or "").strip()
    if auth.lower().startswith("bearer "):
        return auth[7:].strip()
    return auth


class RawrEngineState:
    def __init__(self) -> None:
        self.started_at = time.time()
        self.selected_model_id = os.getenv("RAWRXD_MODEL_ID", "").strip()
        self.allowed_origins = _split_csv(os.getenv("RAWRXD_CORS_ORIGINS", "")) or [
            "http://localhost",
            "http://127.0.0.1",
            "https://rawrxd.local",
            "app://rawrxd",
        ]
        self.api_keys = set(_split_csv(os.getenv("RAWRXD_API_KEYS", "")))
        self.require_auth = _truthy(os.getenv("RAWRXD_REQUIRE_AUTH"))
        self.inference_url = (os.getenv("RAWRXD_INFERENCE_URL", "") or "http://localhost:11434").rstrip("/")

    def uptime_s(self) -> int:
        return int(time.time() - self.started_at)


STATE = RawrEngineState()


class Handler(BaseHTTPRequestHandler):
    server_version = "RawrEngine/ua-stdlib"

    def _cors_headers(self) -> List[Tuple[str, str]]:
        origin = _safe_origin(self.headers.get("Origin", ""))
        if origin and _origin_allowed(origin, STATE.allowed_origins):
            return [
                ("Access-Control-Allow-Origin", origin),
                ("Access-Control-Allow-Credentials", "true"),
                ("Vary", "Origin"),
                ("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"),
                ("Access-Control-Allow-Headers", "Content-Type, X-API-Key, Authorization"),
            ]
        return [
            ("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"),
            ("Access-Control-Allow-Headers", "Content-Type, X-API-Key, Authorization"),
        ]

    def _send_json(self, status: int, obj: Any) -> None:
        data = _json_bytes(obj)
        self.send_response(status)
        for k, v in self._cors_headers():
            self.send_header(k, v)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _send_sse_headers(self) -> None:
        self.send_response(200)
        for k, v in self._cors_headers():
            self.send_header(k, v)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close")
        self.end_headers()

    def _auth_ok(self) -> Tuple[bool, Optional[Tuple[int, Dict[str, Any]]]]:
        # /status is always public
        if self.path == "/status":
            return True, None

        auth_configured = STATE.require_auth or bool(STATE.api_keys)
        if not auth_configured:
            return True, None

        # Allow loopback clients without key (local IDE/dev)
        ip = (self.client_address[0] if self.client_address else "") or ""
        if ip.startswith("127.") or ip == "::1":
            return True, None

        key = _extract_api_key(self.headers)
        if not key:
            return False, (401, {"error": "api_key_required"})
        if key not in STATE.api_keys:
            return False, (403, {"error": "invalid_api_key"})
        return True, None

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        for k, v in self._cors_headers():
            self.send_header(k, v)
        self.send_header("Access-Control-Max-Age", "86400")
        self.end_headers()

    def do_GET(self) -> None:
        ok, err = self._auth_ok()
        if not ok and err:
            code, payload = err
            self._send_json(code, payload)
            return

        if self.path == "/status":
            self._send_json(200, {
                "ready": True,
                "uptime": STATE.uptime_s(),
                "backend": "rawrengine-stdlib",
                "inference_url": STATE.inference_url,
            })
            return

        if self.path == "/v1/models":
            self._send_json(200, {"models": self._get_models()})
            return

        if self.path == "/api/tools":
            self._send_json(200, {"tools": self._get_tools()})
            return

        self._send_json(404, {"error": "not_found"})

    def do_POST(self) -> None:
        ok, err = self._auth_ok()
        if not ok and err:
            code, payload = err
            self._send_json(code, payload)
            return

        body_bytes = _read_body(self)
        try:
            body = json.loads(body_bytes.decode("utf-8") or "{}") if body_bytes else {}
        except Exception:
            self._send_json(400, {"error": "invalid_json"})
            return

        if self.path == "/api/agentic/config":
            model = (body.get("model") or "").strip()
            STATE.selected_model_id = model
            self._send_json(200, {"success": True, "model": STATE.selected_model_id})
            return

        if self.path == "/api/chat":
            stream = bool(body.get("stream"))
            if stream:
                self._proxy_chat_stream(body)
            else:
                self._proxy_chat(body)
            return

        if self.path == "/api/agent/wish":
            mode = (body.get("mode") or "ask").strip().lower()
            wish = (body.get("wish") or body.get("prompt") or "").strip()
            if not wish:
                self._send_json(400, {"error": "missing_wish"})
                return
            if mode == "plan":
                plan_prompt = "Write a short, concrete implementation plan as 5-12 bullet steps.\n\nGoal:\n" + wish
                resp = self._simple_infer(plan_prompt)
                steps = self._parse_plan_steps(resp)
                self._send_json(200, {"plan": steps, "requires_confirmation": False})
                return

            # ask/full: run as chat
            msg = "User wish:\n" + wish
            self._proxy_chat({"message": msg, "model": body.get("model") or STATE.selected_model_id})
            return

        self._send_json(404, {"error": "not_found"})

    # ---------------------------
    # Backend helpers
    # ---------------------------

    def _get_models(self) -> List[Dict[str, Any]]:
        # Prefer upstream /v1/models (OpenAI compat)
        upstream = STATE.inference_url
        try:
            req = urllib.request.Request(upstream + "/v1/models", method="GET")
            with urllib.request.urlopen(req, timeout=3) as r:
                data = json.loads(r.read().decode("utf-8"))
                # OpenAI format: { data: [{id:...}, ...] }
                if isinstance(data, dict) and isinstance(data.get("data"), list):
                    models = []
                    for m in data["data"]:
                        mid = (m.get("id") if isinstance(m, dict) else None) or ""
                        if mid:
                            models.append({"id": mid})
                    if models:
                        return models
        except Exception:
            pass

        # Fallback to Ollama /api/tags
        try:
            req = urllib.request.Request(upstream + "/api/tags", method="GET")
            with urllib.request.urlopen(req, timeout=3) as r:
                data = json.loads(r.read().decode("utf-8"))
                models = []
                for m in (data.get("models") or []):
                    name = m.get("name") if isinstance(m, dict) else None
                    if name:
                        models.append({"id": name})
                if models:
                    return models
        except Exception:
            pass

        # Local fallback
        mid = STATE.selected_model_id or "rawrxd-default"
        return [{"id": mid}]

    def _get_tools(self) -> List[Dict[str, Any]]:
        return [
            {"name": "status", "description": "GET /status — gateway status"},
            {"name": "models", "description": "GET /v1/models — model list"},
            {"name": "chat", "description": "POST /api/chat — chat (proxy to inference)"},
            {"name": "agent_wish", "description": "POST /api/agent/wish — ask/plan/full"},
            {"name": "agentic_config", "description": "POST /api/agentic/config — set preferred model id"},
        ]

    def _proxy_chat(self, body: Dict[str, Any]) -> None:
        upstream = STATE.inference_url
        model = (body.get("model") or STATE.selected_model_id or "").strip() or None
        messages = body.get("messages")
        message = (body.get("message") or body.get("prompt") or "").strip()
        if not messages:
            messages = [{"role": "user", "content": message}]

        payload = {
            "model": model or "",
            "messages": messages,
            "stream": False,
        }

        try:
            req = urllib.request.Request(
                upstream + "/v1/chat/completions",
                data=_json_bytes(payload),
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=120) as r:
                data = json.loads(r.read().decode("utf-8"))
                content = ""
                try:
                    content = data["choices"][0]["message"]["content"]
                except Exception:
                    content = json.dumps(data)
                self._send_json(200, {"response": content})
                return
        except urllib.error.HTTPError as e:
            self._send_json(502, {"error": "upstream_http_error", "status": e.code})
            return
        except Exception:
            self._send_json(503, {"error": "inference_unavailable", "hint": f"Set RAWRXD_INFERENCE_URL (currently {STATE.inference_url})"})
            return

    def _proxy_chat_stream(self, body: Dict[str, Any]) -> None:
        upstream = STATE.inference_url
        model = (body.get("model") or STATE.selected_model_id or "").strip() or None
        messages = body.get("messages")
        message = (body.get("message") or body.get("prompt") or "").strip()
        if not messages:
            messages = [{"role": "user", "content": message}]

        payload = {
            "model": model or "",
            "messages": messages,
            "stream": True,
        }

        self._send_sse_headers()
        try:
            req = urllib.request.Request(
                upstream + "/v1/chat/completions",
                data=_json_bytes(payload),
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=120) as r:
                # Pass through SSE lines
                while True:
                    line = r.readline()
                    if not line:
                        break
                    self.wfile.write(line)
                    self.wfile.flush()
        except Exception:
            self.wfile.write(b"data: {\"error\":\"inference_unavailable\"}\r\n\r\n")
            self.wfile.write(b"data: [DONE]\r\n\r\n")
            self.wfile.flush()

    def _simple_infer(self, prompt: str) -> str:
        upstream = STATE.inference_url
        # Prefer /api/generate (Ollama-style) for simple one-shot prompts.
        payload = {"model": STATE.selected_model_id or "", "prompt": prompt, "stream": False}
        try:
            req = urllib.request.Request(
                upstream + "/api/generate",
                data=_json_bytes(payload),
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=120) as r:
                data = json.loads(r.read().decode("utf-8"))
                return (data.get("response") or "").strip()
        except Exception:
            return ""

    def _parse_plan_steps(self, text: str) -> List[str]:
        if not text:
            return []
        steps: List[str] = []
        for raw in text.splitlines():
            s = raw.strip()
            if not s:
                continue
            if s[0].isdigit():
                # "1." / "1)"
                i = 0
                while i < len(s) and s[i].isdigit():
                    i += 1
                if i < len(s) and s[i] in (".", ")"):
                    s = s[i + 1 :].strip()
            elif s.startswith(("-", "*")):
                s = s[1:].strip()
            if s:
                steps.append(s)
        return steps

    def log_message(self, format: str, *args: Any) -> None:
        # quiet by default (supervisor handles logs)
        return


def main() -> None:
    host = os.getenv("RAWRXD_HOST", "0.0.0.0")
    port = int(os.getenv("RAWRXD_PORT", "23959"))

    httpd = ThreadingHTTPServer((host, port), Handler)
    httpd.daemon_threads = True
    try:
        httpd.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

