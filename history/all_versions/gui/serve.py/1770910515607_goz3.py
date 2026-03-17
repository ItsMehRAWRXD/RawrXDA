#!/usr/bin/env python3
"""
RawrXD IDE — Agentic Interface Backend (serve.py)
=================================================
Standalone Python HTTP server that bridges the HTML chatbot frontend
to local Ollama and GGUF model files. Use this when the Win32IDE's
built-in C++ server is not running.

Endpoints:
  GET  /models                — Scan D:/OllamaModels for .gguf + query Ollama
  POST /ask                   — Forward chat query to Ollama /api/generate
  POST /v1/chat/completions   — OpenAI-compatible proxy to Ollama
  POST /api/generate          — Ollama-compatible streaming proxy
  POST /api/chat              — Direct Ollama /api/chat proxy
  GET  /gui                   — Serve gui/ide_chatbot.html
  GET  /health                — Health check
  GET  /status                — Server status + stats
  GET  /api/failures          — Failure intelligence data
  GET  /api/agents/status     — Agent subsystem status
  GET  /api/agents/history    — Agent event history
  POST /api/agents/replay     — Replay a failed agent event
  POST /api/read-file         — Read a local file by path
  POST /api/write-file        — Write/create file (up to 50MB)
  POST /api/delete-file       — Delete file or empty dir
  POST /api/rename-file       — Rename/move file
  POST /api/mkdir             — Create directories recursively
  POST /api/list-directory    — List directory contents
  POST /api/search-files      — Recursive text/regex search
  POST /api/git/status        — Git status (porcelain + branch + remotes)
  POST /api/git/diff          — Git diff (staged or unstaged)
  POST /api/git/log           — Git log (configurable limit)
  POST /api/git/add           — Git add (stage files)
  POST /api/git/commit        — Git commit with message
  POST /api/git/push          — Git push to remote
  POST /api/git/pull          — Git pull from remote
  POST /api/terminal/exec     — Execute shell command (with safety guards)
  GET  /api/agent/tools       — List available agentic tools
  POST /api/agent/execute-tool — Execute a tool call from the AI agent

Usage:
  python gui/serve.py                    # Default port 11435
  python gui/serve.py --port 8080        # Custom port
  python gui/serve.py --ollama-url http://localhost:11434  # Custom Ollama
"""

import http.server
import json
import os
import sys
import time
import threading
import urllib.request
import urllib.error
import urllib.parse
import argparse
import glob
import subprocess
import shutil
import fnmatch
import re
from pathlib import Path

# ============================================================================
# Configuration
# ============================================================================

GGUF_SCAN_DIRS = [
    "D:/OllamaModels",
    os.path.expanduser("~/.ollama/models"),
]

BLOB_DIR = "D:/OllamaModels/blobs"

DEFAULT_PORT = 11435
DEFAULT_OLLAMA_URL = "http://127.0.0.1:11434"

# Phase 4: Security & Hardening Configuration
MAX_REQUEST_BODY = 10 * 1024 * 1024    # 10 MB max request body
RATE_LIMIT_PER_MINUTE = 60              # Requests per minute per IP
RATE_LIMIT_WINDOW = 60                  # Window in seconds
ALLOWED_ORIGINS = [
    "http://localhost:*",
    "http://127.0.0.1:*",
]

# Resolve project root (parent of gui/)
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
HTML_PATH = SCRIPT_DIR / "ide_chatbot.html"


# ============================================================================
# Phase 4: Rate Limiter (per-IP, sliding window)
# ============================================================================

_rate_lock = threading.Lock()
_rate_buckets = {}   # { ip: [timestamps] }


def _check_rate_limit(ip):
    """Return True if request is allowed, False if rate limited."""
    now = time.time()
    with _rate_lock:
        if ip not in _rate_buckets:
            _rate_buckets[ip] = []
        bucket = _rate_buckets[ip]
        # Prune old entries outside the window
        _rate_buckets[ip] = [ts for ts in bucket if (now - ts) < RATE_LIMIT_WINDOW]
        bucket = _rate_buckets[ip]
        if len(bucket) >= RATE_LIMIT_PER_MINUTE:
            return False
        bucket.append(now)
        return True


def _match_origin(origin):
    """Check if an Origin header matches the allowed origins patterns."""
    if not origin:
        return True  # No origin header (same-origin or non-browser)
    import re
    for pattern in ALLOWED_ORIGINS:
        regex = "^" + re.escape(pattern).replace(r"\*", r".*") + "$"
        if re.match(regex, origin, re.IGNORECASE):
            return True
    return False


# ============================================================================
# Server Statistics & Agentic State (in-memory)
# ============================================================================

_stats_lock = threading.Lock()
_server_stats = {
    "start_time": time.time(),
    "total_requests": 0,
    "total_tokens": 0,
    "total_chat_completions": 0,
    "total_legacy_asks": 0,
    "total_stream_requests": 0,
}

_failure_lock = threading.Lock()
_failure_log = []       # list of failure event dicts
_failure_id_counter = 0

_agent_lock = threading.Lock()
_agent_events = []      # list of agent event dicts
_agent_status = {
    "failure_detector": {"active": True, "last_check": None, "detections": 0},
    "puppeteer": {"active": True, "corrections": 0, "last_correction": None},
    "proxy_hotpatcher": {"active": True, "patches_applied": 0},
    "unified_manager": {"memory_patches": 0, "byte_patches": 0, "server_patches": 0},
}


# ---------------------------------------------------------------------------
# Model-count cache — avoids re-enumerating Ollama + GGUF on every /status
# ---------------------------------------------------------------------------
_model_cache_lock = threading.Lock()
_model_cache = {"ts": 0.0, "count": 0, "models": None}
_MODEL_CACHE_TTL = 10  # seconds

def _get_cached_model_count(ollama_url):
    """Return cached model count (refreshed every 10s in background)."""
    now = time.time()
    with _model_cache_lock:
        if now - _model_cache["ts"] < _MODEL_CACHE_TTL and _model_cache["models"] is not None:
            return _model_cache["count"]
    # Cache miss — refresh (outside lock to avoid blocking other threads)
    models = get_all_models(ollama_url)
    with _model_cache_lock:
        _model_cache["ts"] = time.time()
        _model_cache["count"] = len(models)
        _model_cache["models"] = models
    return len(models)

def _get_cached_models(ollama_url):
    """Return cached model list (refreshed every 10s)."""
    now = time.time()
    with _model_cache_lock:
        if now - _model_cache["ts"] < _MODEL_CACHE_TTL and _model_cache["models"] is not None:
            return _model_cache["models"]
    models = get_all_models(ollama_url)
    with _model_cache_lock:
        _model_cache["ts"] = time.time()
        _model_cache["count"] = len(models)
        _model_cache["models"] = models
    return models

# Pre-serialized /health response — zero JSON overhead
_HEALTH_BYTES = b'{"status":"ok","server":"RawrXD-serve.py"}'
_HEALTH_LEN = str(len(_HEALTH_BYTES))


def _bump_stat(key, amount=1):
    with _stats_lock:
        _server_stats[key] = _server_stats.get(key, 0) + amount


def _add_failure(failure_type, evidence="", strategy="none", outcome="Detected",
                 prompt_snippet="", session_id=""):
    global _failure_id_counter
    with _failure_lock:
        _failure_id_counter += 1
        event = {
            "id": _failure_id_counter,
            "timestampMs": int(time.time() * 1000),
            "type": failure_type,
            "evidence": evidence,
            "strategy": strategy,
            "outcome": outcome,
            "promptSnippet": prompt_snippet[:200],
            "sessionId": session_id,
            "attempt": 1,
        }
        _failure_log.append(event)
        # Keep max 500 entries
        if len(_failure_log) > 500:
            _failure_log[:] = _failure_log[-500:]
        return event


def _add_agent_event(event_type, detail="", source="serve.py"):
    with _agent_lock:
        _agent_events.append({
            "timestampMs": int(time.time() * 1000),
            "type": event_type,
            "detail": detail,
            "source": source,
        })
        if len(_agent_events) > 500:
            _agent_events[:] = _agent_events[-500:]


def _get_failure_stats():
    with _failure_lock:
        total = len(_failure_log)
        retries = sum(1 for f in _failure_log if f.get("attempt", 1) > 1)
        successes = sum(1 for f in _failure_log if f.get("outcome") == "Corrected")
        declined = sum(1 for f in _failure_log if f.get("outcome") == "Declined")

        # Top reasons
        reason_counts = {}
        for f in _failure_log:
            t = f.get("type", "Unknown")
            reason_counts[t] = reason_counts.get(t, 0) + 1

        top_reasons = sorted(
            [{"type": k, "count": v} for k, v in reason_counts.items()],
            key=lambda x: x["count"], reverse=True
        )

        return {
            "totalFailures": total,
            "totalRetries": retries,
            "successAfterRetry": successes,
            "retriesDeclined": declined,
            "topReasons": top_reasons[:10],
        }


# ============================================================================
# Model Discovery
# ============================================================================

def scan_gguf_models():
    """Scan configured directories for .gguf model files."""
    models = []
    for scan_dir in GGUF_SCAN_DIRS:
        if not os.path.isdir(scan_dir):
            continue
        for fpath in glob.glob(os.path.join(scan_dir, "*.gguf")):
            fname = os.path.basename(fpath)
            size_bytes = os.path.getsize(fpath)
            size_gb = size_bytes / (1024 ** 3)
            display_name = fname.rsplit(".gguf", 1)[0] if fname.endswith(".gguf") else fname
            models.append({
                "name": display_name,
                "type": "gguf",
                "size": f"{size_gb:.1f}GB",
                "path": fpath.replace("\\", "/"),
            })
    return models


def scan_blobs():
    """Scan D:/OllamaModels/blobs for large blob files (model weights)."""
    models = []
    if not os.path.isdir(BLOB_DIR):
        return models
    for fpath in glob.glob(os.path.join(BLOB_DIR, "sha256-*")):
        fname = os.path.basename(fpath)
        size_bytes = os.path.getsize(fpath)
        size_gb = size_bytes / (1024 ** 3)
        if size_gb < 0.1:  # Skip small metadata blobs
            continue
        display_name = f"blob:{fname[:19]}"
        models.append({
            "name": display_name,
            "type": "blob",
            "size": f"{size_gb:.1f}GB",
            "path": fpath.replace("\\", "/"),
        })
    return models


def query_ollama_models(ollama_url):
    """Query Ollama /api/tags for running models."""
    models = []
    try:
        req = urllib.request.Request(f"{ollama_url}/api/tags", method="GET")
        with urllib.request.urlopen(req, timeout=3) as resp:
            data = json.loads(resp.read())
            for m in data.get("models", []):
                name = m.get("name", "unknown")
                size_bytes = m.get("size", 0)
                size_gb = size_bytes / (1024 ** 3) if size_bytes else 0
                models.append({
                    "name": name,
                    "type": "ollama",
                    "size": f"{size_gb:.1f}GB" if size_gb > 0 else "unknown",
                })
    except Exception:
        pass  # Ollama not running — that's fine
    return models


def get_all_models(ollama_url):
    """Aggregate all model sources.  Ollama models listed FIRST so that
    API consumers (test harness, IDE) pick a runnable model by default."""
    models = []
    # Running Ollama models first — they can actually serve inference
    models.extend(query_ollama_models(ollama_url))
    # GGUF / blob files second — informational / for manual loading
    models.extend(scan_gguf_models())
    models.extend(scan_blobs())
    return models


# ============================================================================
# Chat via Ollama
# ============================================================================

def ask_ollama(question, model, ollama_url, context_size=4096):
    """Send a question to Ollama's /api/generate and return the response."""
    if not model:
        return "[Error] No model selected. Pick a model from the dropdown."

    payload = json.dumps({
        "model": model,
        "prompt": question,
        "stream": False,
        "options": {
            "num_ctx": context_size,
        },
    }).encode("utf-8")

    try:
        req = urllib.request.Request(
            f"{ollama_url}/api/generate",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=120) as resp:
            data = json.loads(resp.read())
            return data.get("response", "[No response from model]")
    except urllib.error.URLError as e:
        return f"[Error] Ollama unreachable at {ollama_url}: {e}"
    except Exception as e:
        return f"[Error] {e}"


def proxy_chat_completions(body_bytes, ollama_url, stream=False):
    """Proxy OpenAI-compatible /v1/chat/completions to Ollama.

    Strategy: Try Ollama's OpenAI-compat endpoint first (/v1/chat/completions).
    If 404, fall back to native /api/chat and translate the response.
    """
    # ---- Attempt 1: Ollama OpenAI-compatible endpoint (v0.1.24+) ----
    try:
        req = urllib.request.Request(
            f"{ollama_url}/v1/chat/completions",
            data=body_bytes,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        resp = urllib.request.urlopen(req, timeout=300)
        return resp
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise Exception(f"Ollama error on /v1/chat/completions: HTTP {e.code}")
        # 404 — fall through to /api/chat translation
    except urllib.error.URLError as e:
        raise Exception(f"Ollama unreachable at {ollama_url}: {e}")

    # ---- Attempt 2: Translate to Ollama native /api/chat ----
    try:
        body_json = json.loads(body_bytes)
    except Exception:
        body_json = {}

    ollama_body = {
        "model": body_json.get("model", ""),
        "messages": body_json.get("messages", []),
        "stream": stream,
    }
    # Forward supported generation params
    for key in ("temperature", "top_p", "top_k", "seed", "num_predict"):
        if key in body_json:
            if "options" not in ollama_body:
                ollama_body["options"] = {}
            ollama_body["options"][key] = body_json[key]
    if "max_tokens" in body_json:
        if "options" not in ollama_body:
            ollama_body["options"] = {}
        ollama_body["options"]["num_predict"] = body_json["max_tokens"]

    ollama_data = json.dumps(ollama_body).encode("utf-8")

    try:
        req = urllib.request.Request(
            f"{ollama_url}/api/chat",
            data=ollama_data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        resp = urllib.request.urlopen(req, timeout=300)
    except urllib.error.HTTPError as e:
        # Ollama returns 404 for model-not-found — pass through the error body
        try:
            err_body = json.loads(e.read())
        except Exception:
            err_body = {"error": f"Ollama HTTP {e.code}"}
        raise Exception(json.dumps(err_body))
    except urllib.error.URLError as e:
        raise Exception(f"Ollama unreachable at {ollama_url}/api/chat: {e}")

    if stream:
        # For streaming, return a wrapper that translates NDJSON → SSE
        return _OllamaChatToSSEAdapter(resp)
    else:
        # For non-streaming, read and translate to OpenAI format
        raw = resp.read()
        resp.close()
        try:
            ollama_resp = json.loads(raw)
        except Exception:
            raise Exception(f"Invalid JSON from Ollama /api/chat: {raw[:200]}")

        openai_resp = {
            "id": f"chatcmpl-{int(time.time()*1000)}",
            "object": "chat.completion",
            "created": int(time.time()),
            "model": ollama_resp.get("model", body_json.get("model", "")),
            "choices": [{
                "index": 0,
                "message": ollama_resp.get("message", {"role": "assistant", "content": ""}),
                "finish_reason": "stop",
            }],
            "usage": {
                "prompt_tokens": ollama_resp.get("prompt_eval_count", 0),
                "completion_tokens": ollama_resp.get("eval_count", 0),
                "total_tokens": ollama_resp.get("prompt_eval_count", 0) + ollama_resp.get("eval_count", 0),
            },
        }
        # Return a fake response object with .read() and .close()
        return _BytesResponse(json.dumps(openai_resp).encode("utf-8"))


class _BytesResponse:
    """Minimal file-like response wrapping bytes for proxy return."""
    def __init__(self, data):
        self._data = data
        self._read = False
    def read(self, n=-1):
        if self._read:
            return b""
        self._read = True
        return self._data
    def close(self):
        pass


class _OllamaChatToSSEAdapter:
    """Translates Ollama /api/chat NDJSON stream to OpenAI SSE format."""
    def __init__(self, resp):
        self._resp = resp
        self._buffer = b""
        self._done = False

    def read(self, n=4096):
        if self._done:
            return b""
        chunk = self._resp.read(n)
        if not chunk:
            self._done = True
            return b"data: [DONE]\n\n"

        self._buffer += chunk
        output = b""
        while b"\n" in self._buffer:
            line, self._buffer = self._buffer.split(b"\n", 1)
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
                msg = obj.get("message", {})
                content = msg.get("content", "")
                is_done = obj.get("done", False)

                sse_obj = {
                    "id": f"chatcmpl-{int(time.time()*1000)}",
                    "object": "chat.completion.chunk",
                    "created": int(time.time()),
                    "model": obj.get("model", ""),
                    "choices": [{
                        "index": 0,
                        "delta": {"content": content} if content else {},
                        "finish_reason": "stop" if is_done else None,
                    }],
                }
                output += f"data: {json.dumps(sse_obj)}\n\n".encode("utf-8")

                if is_done:
                    output += b"data: [DONE]\n\n"
                    self._done = True
                    break
            except json.JSONDecodeError:
                continue
        return output

    def close(self):
        self._resp.close()


def proxy_api_generate(body_bytes, ollama_url):
    """Proxy Ollama-native /api/generate for streaming."""
    try:
        req = urllib.request.Request(
            f"{ollama_url}/api/generate",
            data=body_bytes,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        resp = urllib.request.urlopen(req, timeout=300)
        return resp
    except urllib.error.URLError as e:
        raise Exception(f"Ollama unreachable at {ollama_url}/api/generate: {e}")
    except Exception as e:
        raise Exception(f"API generate proxy error: {e}")


# ============================================================================
# HTTP Handler
# ============================================================================

class RawrXDHandler(http.server.BaseHTTPRequestHandler):
    ollama_url = DEFAULT_OLLAMA_URL

    def log_message(self, format, *args):
        """Override to add color to terminal output."""
        sys.stderr.write(f"\033[36m[RawrXD]\033[0m {args[0]} {args[1]} {args[2]}\n")

    def send_cors_headers(self):
        # Phase 4: Restrict CORS to allowed origins
        origin = self.headers.get("Origin", "")
        if _match_origin(origin):
            allowed = origin if origin else "*"
        else:
            allowed = ALLOWED_ORIGINS[0].replace("*", str(DEFAULT_PORT)) if ALLOWED_ORIGINS else ""
        self.send_header("Access-Control-Allow-Origin", allowed)
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Vary", "Origin")

    def send_security_headers(self, allow_framing=False):
        """Phase 4: Add security-hardening response headers.
        
        Args:
            allow_framing: If True, omit X-Frame-Options to permit same-origin
                           iframe embedding (used by test pages embedding /gui).
        """
        self.send_header("X-Content-Type-Options", "nosniff")
        if not allow_framing:
            self.send_header("X-Frame-Options", "SAMEORIGIN")
        # CSP frame-ancestors supersedes X-Frame-Options and is more reliable
        # with sandbox iframes.  Allow self + localhost variants.
        self.send_header(
            "Content-Security-Policy",
            "frame-ancestors 'self' http://localhost:* http://127.0.0.1:*"
        )
        self.send_header("X-XSS-Protection", "1; mode=block")
        self.send_header("Referrer-Policy", "strict-origin-when-cross-origin")
        self.send_header("Cache-Control", "no-store")

    def _get_client_ip(self):
        """Extract client IP for rate limiting."""
        forwarded = self.headers.get("X-Forwarded-For")
        if forwarded:
            return forwarded.split(",")[0].strip()
        return self.client_address[0] if self.client_address else "unknown"

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_cors_headers()
        self.send_security_headers()
        self.end_headers()

    def do_GET(self):
        _bump_stat("total_requests")

        # Phase 4: Rate limit check
        client_ip = self._get_client_ip()
        if not _check_rate_limit(client_ip):
            self._json_response(429, {
                "error": "rate_limited",
                "message": f"Too many requests. Limit: {RATE_LIMIT_PER_MINUTE}/min",
            })
            _add_failure("rate_limit", f"GET {self.path} from {client_ip}",
                         "throttle", "Blocked")
            return

        try:
            self._handle_get()
        except Exception as e:
            self._json_response(500, {
                "error": "internal_error",
                "message": "An unexpected error occurred.",
            })
            _add_failure("internal_error", str(e)[:200], "none", "Failed",
                         self.path[:100], "serve.py")

    def _handle_get(self):

        if self.path == "/health" or self.path == "/":
            # Fast-path: pre-serialized bytes, no JSON overhead
            try:
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", _HEALTH_LEN)
                self.send_cors_headers()
                self.send_security_headers()
                self.end_headers()
                self.wfile.write(_HEALTH_BYTES)
            except (ConnectionAbortedError, ConnectionResetError, BrokenPipeError):
                pass

        elif self.path == "/models":
            models = _get_cached_models(self.ollama_url)
            self._json_response(200, {"models": models})

        elif self.path in ("/gui", "/gui/"):
            self._serve_html()

        elif self.path in ("/test", "/test/", "/test-jumper"):
            self._serve_test_jumper()

        elif self.path in ("/test-harness", "/test-harness/"):
            self._serve_test_harness()

        elif self.path == "/status":
            model_count = _get_cached_model_count(self.ollama_url)
            uptime = int(time.time() - _server_stats["start_time"])
            with _stats_lock:
                stats_copy = dict(_server_stats)
            self._json_response(200, {
                "ready": True,
                "model_loaded": False,
                "backend": "rawrxd-serve.py",
                "server": "RawrXD-serve.py",
                "available_models": model_count,
                "total_requests": stats_copy["total_requests"],
                "total_tokens": stats_copy["total_tokens"],
                "total_chat_completions": stats_copy["total_chat_completions"],
                "uptime_seconds": uptime,
            })

        elif self.path.startswith("/api/failures"):
            # Parse ?limit=N
            limit = 200
            if "?" in self.path:
                qs = self.path.split("?", 1)[1]
                for part in qs.split("&"):
                    if part.startswith("limit="):
                        try:
                            limit = int(part.split("=")[1])
                        except ValueError:
                            pass

            with _failure_lock:
                failures = list(_failure_log[-limit:])
            stats = _get_failure_stats()
            self._json_response(200, {"failures": failures, "stats": stats})

        elif self.path == "/api/agents/status":
            with _agent_lock:
                status_copy = json.loads(json.dumps(_agent_status))
            self._json_response(200, {
                "agents": status_copy,
                "server_uptime": int(time.time() - _server_stats["start_time"]),
                "total_events": len(_agent_events),
            })

        elif self.path.startswith("/api/agents/history"):
            limit = 100
            if "?" in self.path:
                qs = self.path.split("?", 1)[1]
                for part in qs.split("&"):
                    if part.startswith("limit="):
                        try:
                            limit = int(part.split("=")[1])
                        except ValueError:
                            pass
            with _agent_lock:
                events = list(_agent_events[-limit:])
            self._json_response(200, {"events": events})

        elif self.path == "/api/tags":
            # Proxy to Ollama /api/tags for compatibility
            try:
                req = urllib.request.Request(f"{self.ollama_url}/api/tags", method="GET")
                with urllib.request.urlopen(req, timeout=5) as resp:
                    data = resp.read()
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(data)))
                    self.send_cors_headers()
                    self.send_security_headers()
                    self.end_headers()
                    self.wfile.write(data)
            except Exception as e:
                self._json_response(502, {"error": f"Ollama unreachable: {e}"})

        elif self.path.startswith("/api/read-file"):
            # Read a local file by path (for IDE file-path auto-attach)
            # Accept path as query param: /api/read-file?path=D:/rawrxd/src/file.cpp
            file_path = None
            if "?" in self.path:
                qs = self.path.split("?", 1)[1]
                for part in qs.split("&"):
                    if part.startswith("path="):
                        file_path = urllib.parse.unquote(part[5:])
            if file_path:
                self._read_local_file(file_path)
            else:
                self._json_response(400, {"error": "Missing 'path' query parameter"})

        else:
            self._json_response(404, {"error": "not_found", "message": f"Unknown: {self.path}"})

    def do_POST(self):
        _bump_stat("total_requests")

        # Phase 4: Rate limit check
        client_ip = self._get_client_ip()
        if not _check_rate_limit(client_ip):
            self._json_response(429, {
                "error": "rate_limited",
                "message": f"Too many requests. Limit: {RATE_LIMIT_PER_MINUTE}/min",
            })
            _add_failure("rate_limit", f"POST {self.path} from {client_ip}",
                         "throttle", "Blocked")
            return

        # Phase 4: Request body size limit
        content_length = int(self.headers.get("Content-Length", 0))
        if content_length > MAX_REQUEST_BODY:
            self._json_response(413, {
                "error": "payload_too_large",
                "message": f"Request body too large ({content_length} bytes). Max: {MAX_REQUEST_BODY}",
            })
            _add_failure("payload_too_large", f"{content_length} bytes on {self.path}",
                         "reject", "Blocked")
            return

        try:
            body = self.rfile.read(content_length) if content_length else b""
            body_str = body.decode("utf-8", errors="replace") if body else ""
            self._handle_post(body, body_str)
        except Exception as e:
            self._json_response(500, {
                "error": "internal_error",
                "message": "An unexpected error occurred.",
            })
            _add_failure("internal_error", str(e)[:200], "none", "Failed",
                         self.path[:100], "serve.py")

    def _handle_post(self, body, body_str):

        if self.path == "/v1/chat/completions":
            # Proxy to Ollama's OpenAI-compatible endpoint
            _bump_stat("total_chat_completions")
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return

            is_stream = data.get("stream", False)

            try:
                ollama_resp = proxy_chat_completions(body, self.ollama_url, stream=is_stream)
            except Exception as e:
                _add_failure("endpoint_error", str(e), "proxy", "Failed",
                             body_str[:200], "serve.py")
                self._json_response(502, {
                    "error": {"message": str(e), "type": "proxy_error"}
                })
                return

            if is_stream:
                _bump_stat("total_stream_requests")
                # Stream response back to client
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "close")
                self.send_header("X-Accel-Buffering", "no")
                self.send_cors_headers()
                self.send_security_headers()
                self.end_headers()

                try:
                    while True:
                        chunk = ollama_resp.read(4096)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    pass
                except Exception:
                    pass
                finally:
                    ollama_resp.close()
                    self.close_connection = True
            else:
                # Non-streaming — read full response and forward
                try:
                    resp_data = ollama_resp.read()
                    ollama_resp.close()

                    # Count tokens from response
                    try:
                        resp_json = json.loads(resp_data)
                        usage = resp_json.get("usage", {})
                        _bump_stat("total_tokens", usage.get("total_tokens", 0))
                    except Exception:
                        pass

                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(resp_data)))
                    self.send_cors_headers()
                    self.send_security_headers()
                    self.end_headers()
                    self.wfile.write(resp_data)
                except Exception as e:
                    self._json_response(502, {
                        "error": {"message": f"Response read failed: {e}", "type": "proxy_error"}
                    })

        elif self.path == "/api/generate":
            # Proxy to Ollama's native /api/generate (supports streaming)
            _bump_stat("total_stream_requests")

            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                data = {}

            is_stream = data.get("stream", True)

            try:
                ollama_resp = proxy_api_generate(body, self.ollama_url)
            except Exception as e:
                _add_failure("endpoint_error", str(e), "proxy", "Failed",
                             body_str[:200], "serve.py")
                self._json_response(502, {"error": str(e)})
                return

            if not is_stream:
                # Non-streaming: read full response and send with Content-Length
                try:
                    resp_data = ollama_resp.read()
                    ollama_resp.close()
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(resp_data)))
                    self.send_cors_headers()
                    self.send_security_headers()
                    self.end_headers()
                    self.wfile.write(resp_data)
                except Exception as e:
                    self._json_response(502, {"error": str(e)})
            else:
                # Streaming: relay NDJSON chunks
                self.send_response(200)
                self.send_header("Content-Type", "application/x-ndjson")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "close")
                self.send_cors_headers()
                self.send_security_headers()
                self.end_headers()

                try:
                    while True:
                        chunk = ollama_resp.read(4096)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    pass
                except Exception:
                    pass
                finally:
                    ollama_resp.close()
                    self.close_connection = True

        elif self.path == "/api/chat":
            # Direct proxy to Ollama /api/chat (native format, no translation)
            _bump_stat("total_api_chat")
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return

            is_stream = data.get("stream", True)

            try:
                ollama_data = json.dumps(data).encode("utf-8")
                req = urllib.request.Request(
                    f"{self.ollama_url}/api/chat",
                    data=ollama_data,
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                ollama_resp = urllib.request.urlopen(req, timeout=300)
            except urllib.error.HTTPError as e:
                try:
                    err_body = e.read().decode("utf-8", errors="replace")
                except Exception:
                    err_body = f"HTTP {e.code}"
                _add_failure("endpoint_error", err_body, "proxy", "Failed",
                             body_str[:200], "serve.py")
                self._json_response(e.code, {"error": err_body})
                return
            except Exception as e:
                self._json_response(502, {"error": str(e)})
                return

            if is_stream:
                self.send_response(200)
                self.send_header("Content-Type", "application/x-ndjson")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "close")
                self.send_header("X-Accel-Buffering", "no")
                self.send_cors_headers()
                self.send_security_headers()
                self.end_headers()

                try:
                    while True:
                        chunk = ollama_resp.read(4096)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    pass
                except Exception:
                    pass
                finally:
                    ollama_resp.close()
                    self.close_connection = True
            else:
                try:
                    resp_data = ollama_resp.read()
                    ollama_resp.close()
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(resp_data)))
                    self.send_cors_headers()
                    self.send_security_headers()
                    self.end_headers()
                    self.wfile.write(resp_data)
                except Exception as e:
                    self._json_response(502, {"error": str(e)})

        elif self.path == "/ask":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return

            _bump_stat("total_legacy_asks")
            question = data.get("question", "")
            model = data.get("model", "")
            context = data.get("context", 4096)

            if not question:
                self._json_response(400, {"error": "No question provided"})
                return

            answer = ask_ollama(question, model, self.ollama_url, context)
            self._json_response(200, {"answer": answer})

        elif self.path == "/api/agents/replay":
            # Replay a failed event
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return

            failure_id = data.get("failure_id")
            action = data.get("action", "retry")  # retry | skip | escalate

            if not failure_id:
                self._json_response(400, {"error": "Missing failure_id"})
                return

            # Find the failure
            with _failure_lock:
                target = None
                for f in _failure_log:
                    if f.get("id") == failure_id:
                        target = f
                        break

            if not target:
                self._json_response(404, {"error": f"Failure {failure_id} not found"})
                return

            if action == "skip":
                with _failure_lock:
                    target["outcome"] = "Declined"
                _add_agent_event("replay_skip", f"Failure #{failure_id} skipped by user")
                self._json_response(200, {
                    "status": "skipped",
                    "failure_id": failure_id,
                    "outcome": "Declined",
                })
            elif action == "retry":
                # Mark as retried — in a real system this would re-send the prompt
                with _failure_lock:
                    target["attempt"] = target.get("attempt", 1) + 1
                    target["outcome"] = "Corrected"
                with _agent_lock:
                    _agent_status["puppeteer"]["corrections"] += 1
                    _agent_status["puppeteer"]["last_correction"] = int(time.time() * 1000)
                _add_agent_event("replay_retry", f"Failure #{failure_id} retried (attempt {target['attempt']})")
                self._json_response(200, {
                    "status": "retried",
                    "failure_id": failure_id,
                    "outcome": "Corrected",
                    "attempt": target["attempt"],
                })
            elif action == "escalate":
                with _failure_lock:
                    target["outcome"] = "Escalated"
                _add_agent_event("replay_escalate", f"Failure #{failure_id} escalated by user")
                self._json_response(200, {
                    "status": "escalated",
                    "failure_id": failure_id,
                    "outcome": "Escalated",
                })
            else:
                self._json_response(400, {"error": f"Unknown action: {action}"})

        elif self.path == "/api/read-file":
            # Read a local file by path (POST version for IDE file-path auto-attach)
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            file_path = data.get("path")
            if file_path:
                self._read_local_file(file_path)
            else:
                self._json_response(400, {"error": "Missing 'path' field"})

        # ==================================================================
        # FILE OPERATIONS — Real production endpoints for IDE file management
        # ==================================================================

        elif self.path == "/api/write-file":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            file_path = data.get("path")
            content = data.get("content", "")
            create_dirs = data.get("createDirs", False)
            if not file_path:
                self._json_response(400, {"error": "Missing 'path' field"})
                return
            try:
                resolved = Path(file_path).resolve()
                # Block network paths
                if str(resolved).startswith("\\\\") or str(resolved).startswith("//"):
                    self._json_response(403, {"error": "Network paths forbidden"})
                    return
                # Size guard: 50MB
                if len(content.encode("utf-8")) > 50 * 1024 * 1024:
                    self._json_response(413, {"error": "Content exceeds 50MB limit"})
                    return
                if create_dirs:
                    resolved.parent.mkdir(parents=True, exist_ok=True)
                resolved.write_text(content, encoding="utf-8")
                _bump_stat("total_file_writes")
                self._json_response(200, {
                    "ok": True,
                    "path": str(resolved),
                    "size": len(content),
                    "created": not resolved.exists() or True,
                })
            except PermissionError:
                self._json_response(403, {"error": f"Permission denied: {file_path}"})
            except Exception as e:
                self._json_response(500, {"error": f"Write failed: {e}"})

        elif self.path == "/api/delete-file":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            file_path = data.get("path")
            force = data.get("force", False)
            if not file_path:
                self._json_response(400, {"error": "Missing 'path' field"})
                return
            try:
                resolved = Path(file_path).resolve()
                if not resolved.exists():
                    self._json_response(404, {"error": f"Not found: {file_path}"})
                    return
                if resolved.is_file():
                    resolved.unlink()
                elif resolved.is_dir():
                    if force:
                        shutil.rmtree(str(resolved))
                    else:
                        resolved.rmdir()  # only empty dirs
                _bump_stat("total_file_deletes")
                self._json_response(200, {"ok": True, "deleted": str(resolved)})
            except PermissionError:
                self._json_response(403, {"error": f"Permission denied: {file_path}"})
            except OSError as e:
                self._json_response(400, {"error": f"Delete failed: {e}"})

        elif self.path == "/api/rename-file":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            old_path = data.get("old_path") or data.get("path")
            new_path = data.get("new_path") or data.get("newPath")
            if not old_path or not new_path:
                self._json_response(400, {"error": "Missing 'old_path' or 'new_path'"})
                return
            try:
                src = Path(old_path).resolve()
                dst = Path(new_path).resolve()
                if not src.exists():
                    self._json_response(404, {"error": f"Source not found: {old_path}"})
                    return
                dst.parent.mkdir(parents=True, exist_ok=True)
                src.rename(dst)
                _bump_stat("total_file_renames")
                self._json_response(200, {
                    "ok": True,
                    "old_path": str(src),
                    "new_path": str(dst),
                })
            except PermissionError:
                self._json_response(403, {"error": f"Permission denied"})
            except Exception as e:
                self._json_response(500, {"error": f"Rename failed: {e}"})

        elif self.path == "/api/mkdir":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            dir_path = data.get("path")
            if not dir_path:
                self._json_response(400, {"error": "Missing 'path' field"})
                return
            try:
                resolved = Path(dir_path).resolve()
                resolved.mkdir(parents=True, exist_ok=True)
                self._json_response(200, {"ok": True, "path": str(resolved)})
            except PermissionError:
                self._json_response(403, {"error": f"Permission denied: {dir_path}"})
            except Exception as e:
                self._json_response(500, {"error": f"mkdir failed: {e}"})

        elif self.path in ("/api/list-directory", "/api/list-dir"):
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            dir_path = data.get("path", str(PROJECT_ROOT))
            max_entries = data.get("limit", 50000)
            show_hidden = data.get("showHidden", False)
            try:
                resolved = Path(dir_path).resolve()
                if not resolved.exists():
                    self._json_response(404, {"error": f"Directory not found: {dir_path}"})
                    return
                if not resolved.is_dir():
                    self._json_response(400, {"error": f"Not a directory: {dir_path}"})
                    return
                entries = []
                count = 0
                for item in sorted(resolved.iterdir()):
                    if count >= max_entries:
                        break
                    name = item.name
                    if not show_hidden and name.startswith("."):
                        continue
                    try:
                        stat = item.stat()
                        entries.append({
                            "name": name,
                            "path": str(item),
                            "isDir": item.is_dir(),
                            "size": stat.st_size if item.is_file() else 0,
                            "modified": int(stat.st_mtime * 1000),
                        })
                        count += 1
                    except (PermissionError, OSError):
                        entries.append({
                            "name": name,
                            "path": str(item),
                            "isDir": item.is_dir(),
                            "size": 0,
                            "modified": 0,
                            "error": "access_denied",
                        })
                        count += 1
                self._json_response(200, {
                    "ok": True,
                    "path": str(resolved),
                    "entries": entries,
                    "total": count,
                    "truncated": count >= max_entries,
                })
            except PermissionError:
                self._json_response(403, {"error": f"Permission denied: {dir_path}"})
            except Exception as e:
                self._json_response(500, {"error": f"List failed: {e}"})

        elif self.path == "/api/search-files":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            query = data.get("query", "")
            search_path = data.get("path", str(PROJECT_ROOT))
            is_regex = data.get("isRegex", False)
            case_sensitive = data.get("caseSensitive", False)
            max_results = data.get("maxResults", 500)
            file_pattern = data.get("filePattern", "*")
            if not query:
                self._json_response(400, {"error": "Missing 'query' field"})
                return
            try:
                resolved = Path(search_path).resolve()
                if not resolved.is_dir():
                    self._json_response(404, {"error": f"Directory not found: {search_path}"})
                    return
                flags = 0 if case_sensitive else re.IGNORECASE
                if is_regex:
                    pattern = re.compile(query, flags)
                else:
                    pattern = re.compile(re.escape(query), flags)
                results = []
                files_searched = 0
                # Walk directory tree
                for root, dirs, files in os.walk(str(resolved)):
                    # Skip hidden/build dirs
                    dirs[:] = [d for d in dirs if not d.startswith(".") and d not in
                               ("node_modules", "__pycache__", ".git", "build", "Release", "Debug")]
                    for fname in files:
                        if len(results) >= max_results:
                            break
                        if file_pattern != "*" and not fnmatch.fnmatch(fname, file_pattern):
                            continue
                        fpath = os.path.join(root, fname)
                        try:
                            # Skip binary/large files
                            fsize = os.path.getsize(fpath)
                            if fsize > 5 * 1024 * 1024:  # 5MB
                                continue
                            with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                                files_searched += 1
                                for line_num, line in enumerate(f, 1):
                                    if len(results) >= max_results:
                                        break
                                    match = pattern.search(line)
                                    if match:
                                        results.append({
                                            "file": fpath.replace("\\", "/"),
                                            "line": line_num,
                                            "column": match.start() + 1,
                                            "text": line.rstrip("\n\r")[:500],
                                            "matchLength": match.end() - match.start(),
                                        })
                        except (PermissionError, OSError, UnicodeDecodeError):
                            continue
                    if len(results) >= max_results:
                        break
                self._json_response(200, {
                    "ok": True,
                    "results": results,
                    "total": len(results),
                    "filesSearched": files_searched,
                    "truncated": len(results) >= max_results,
                })
            except re.error as e:
                self._json_response(400, {"error": f"Invalid regex: {e}"})
            except Exception as e:
                self._json_response(500, {"error": f"Search failed: {e}"})

        # ==================================================================
        # GIT OPERATIONS — Real subprocess-based git integration
        # ==================================================================

        elif self.path == "/api/git/status":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                data = {}
            repo_path = data.get("path", str(PROJECT_ROOT))
            try:
                result = subprocess.run(
                    ["git", "status", "--porcelain", "-b"],
                    cwd=repo_path, capture_output=True, text=True, timeout=15
                )
                if result.returncode != 0:
                    self._json_response(500, {"error": f"git status failed: {result.stderr}"})
                    return
                lines = result.stdout.strip().split("\n") if result.stdout.strip() else []
                branch = ""
                changes = []
                for line in lines:
                    if line.startswith("## "):
                        branch = line[3:].split("...")[0]
                    elif line:
                        status_code = line[:2]
                        file_name = line[3:]
                        changes.append({
                            "status": status_code.strip(),
                            "file": file_name,
                            "staged": status_code[0] != " " and status_code[0] != "?",
                        })
                # Get remote info
                remote_result = subprocess.run(
                    ["git", "remote", "-v"],
                    cwd=repo_path, capture_output=True, text=True, timeout=10
                )
                remotes = []
                if remote_result.returncode == 0:
                    for rline in remote_result.stdout.strip().split("\n"):
                        if rline and "(push)" in rline:
                            parts = rline.split()
                            if len(parts) >= 2:
                                remotes.append({"name": parts[0], "url": parts[1]})
                self._json_response(200, {
                    "ok": True,
                    "branch": branch,
                    "changes": changes,
                    "totalChanges": len(changes),
                    "remotes": remotes,
                    "clean": len(changes) == 0,
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except subprocess.TimeoutExpired:
                self._json_response(504, {"error": "git status timed out"})
            except Exception as e:
                self._json_response(500, {"error": f"git status failed: {e}"})

        elif self.path == "/api/git/diff":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                data = {}
            repo_path = data.get("path", str(PROJECT_ROOT))
            staged = data.get("staged", False)
            file_filter = data.get("file", None)
            cmd = ["git", "diff"]
            if staged:
                cmd.append("--cached")
            cmd.append("--stat")
            if file_filter:
                cmd.extend(["--", file_filter])
            try:
                # Get stat summary
                stat_result = subprocess.run(
                    cmd, cwd=repo_path, capture_output=True, text=True, timeout=30
                )
                # Get full diff
                full_cmd = ["git", "diff"]
                if staged:
                    full_cmd.append("--cached")
                if file_filter:
                    full_cmd.extend(["--", file_filter])
                diff_result = subprocess.run(
                    full_cmd, cwd=repo_path, capture_output=True, text=True, timeout=30
                )
                self._json_response(200, {
                    "ok": True,
                    "stat": stat_result.stdout,
                    "diff": diff_result.stdout[:500000],  # 500KB limit
                    "truncated": len(diff_result.stdout) > 500000,
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except subprocess.TimeoutExpired:
                self._json_response(504, {"error": "git diff timed out"})
            except Exception as e:
                self._json_response(500, {"error": f"git diff failed: {e}"})

        elif self.path == "/api/git/log":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                data = {}
            repo_path = data.get("path", str(PROJECT_ROOT))
            limit = data.get("limit", 50)
            try:
                result = subprocess.run(
                    ["git", "log", f"-{limit}",
                     "--pretty=format:%H|%h|%an|%ae|%ai|%s"],
                    cwd=repo_path, capture_output=True, text=True, timeout=15
                )
                if result.returncode != 0:
                    self._json_response(500, {"error": f"git log failed: {result.stderr}"})
                    return
                commits = []
                for line in result.stdout.strip().split("\n"):
                    if not line:
                        continue
                    parts = line.split("|", 5)
                    if len(parts) >= 6:
                        commits.append({
                            "hash": parts[0],
                            "shortHash": parts[1],
                            "author": parts[2],
                            "email": parts[3],
                            "date": parts[4],
                            "message": parts[5],
                        })
                self._json_response(200, {
                    "ok": True,
                    "commits": commits,
                    "total": len(commits),
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except subprocess.TimeoutExpired:
                self._json_response(504, {"error": "git log timed out"})
            except Exception as e:
                self._json_response(500, {"error": f"git log failed: {e}"})

        elif self.path == "/api/git/add":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            repo_path = data.get("path", str(PROJECT_ROOT))
            files = data.get("files", ["."])  # default: stage all
            try:
                cmd = ["git", "add"] + files
                result = subprocess.run(
                    cmd, cwd=repo_path, capture_output=True, text=True, timeout=30
                )
                if result.returncode != 0:
                    self._json_response(500, {"error": f"git add failed: {result.stderr}"})
                    return
                self._json_response(200, {
                    "ok": True,
                    "staged": files,
                    "message": f"Staged {len(files)} item(s)",
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except Exception as e:
                self._json_response(500, {"error": f"git add failed: {e}"})

        elif self.path == "/api/git/commit":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            repo_path = data.get("path", str(PROJECT_ROOT))
            message = data.get("message", "")
            if not message:
                self._json_response(400, {"error": "Missing 'message' field"})
                return
            try:
                result = subprocess.run(
                    ["git", "commit", "-m", message],
                    cwd=repo_path, capture_output=True, text=True, timeout=30
                )
                if result.returncode != 0:
                    stderr = result.stderr.strip()
                    if "nothing to commit" in result.stdout.lower() or "nothing to commit" in stderr.lower():
                        self._json_response(200, {
                            "ok": True,
                            "noop": True,
                            "message": "Nothing to commit, working tree clean",
                        })
                    else:
                        self._json_response(500, {"error": f"git commit failed: {stderr or result.stdout}"})
                    return
                # Parse commit hash from output
                commit_hash = ""
                for line in result.stdout.split("\n"):
                    if line.strip().startswith("["):
                        # e.g. [main abc1234] commit message
                        parts = line.strip().split()
                        if len(parts) >= 2:
                            commit_hash = parts[1].rstrip("]")
                self._json_response(200, {
                    "ok": True,
                    "message": message,
                    "hash": commit_hash,
                    "output": result.stdout.strip(),
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except Exception as e:
                self._json_response(500, {"error": f"git commit failed: {e}"})

        elif self.path == "/api/git/push":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                data = {}
            repo_path = data.get("path", str(PROJECT_ROOT))
            remote = data.get("remote", "origin")
            branch = data.get("branch", "")
            force = data.get("force", False)
            try:
                cmd = ["git", "push", remote]
                if branch:
                    cmd.append(branch)
                if force:
                    cmd.append("--force")
                result = subprocess.run(
                    cmd, cwd=repo_path, capture_output=True, text=True, timeout=120
                )
                if result.returncode != 0:
                    self._json_response(500, {
                        "error": f"git push failed: {result.stderr.strip()}",
                        "stdout": result.stdout.strip(),
                    })
                    return
                self._json_response(200, {
                    "ok": True,
                    "remote": remote,
                    "branch": branch or "(current)",
                    "output": (result.stdout + result.stderr).strip(),
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except subprocess.TimeoutExpired:
                self._json_response(504, {"error": "git push timed out (120s)"})
            except Exception as e:
                self._json_response(500, {"error": f"git push failed: {e}"})

        elif self.path == "/api/git/pull":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                data = {}
            repo_path = data.get("path", str(PROJECT_ROOT))
            remote = data.get("remote", "origin")
            branch = data.get("branch", "")
            try:
                cmd = ["git", "pull", remote]
                if branch:
                    cmd.append(branch)
                result = subprocess.run(
                    cmd, cwd=repo_path, capture_output=True, text=True, timeout=120
                )
                if result.returncode != 0:
                    self._json_response(500, {"error": f"git pull failed: {result.stderr.strip()}"})
                    return
                self._json_response(200, {
                    "ok": True,
                    "output": result.stdout.strip(),
                })
            except FileNotFoundError:
                self._json_response(500, {"error": "git not found in PATH"})
            except subprocess.TimeoutExpired:
                self._json_response(504, {"error": "git pull timed out (120s)"})
            except Exception as e:
                self._json_response(500, {"error": f"git pull failed: {e}"})

        # ==================================================================
        # TERMINAL — Real subprocess execution for agentic shell commands
        # ==================================================================

        elif self.path == "/api/terminal/exec":
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            command = data.get("command", "")
            cwd = data.get("cwd", str(PROJECT_ROOT))
            timeout_sec = min(data.get("timeout", 60), 300)  # max 5 min
            if not command:
                self._json_response(400, {"error": "Missing 'command' field"})
                return
            # Security: block dangerous commands
            dangerous = ["format ", "del /s", "rd /s", "rm -rf /", "mkfs",
                         "shutdown", "reboot", "poweroff", ":(){", "fork bomb"]
            cmd_lower = command.lower().strip()
            for d in dangerous:
                if d in cmd_lower:
                    self._json_response(403, {
                        "error": f"Blocked dangerous command pattern: {d}"
                    })
                    return
            try:
                _bump_stat("total_terminal_execs")
                result = subprocess.run(
                    command, shell=True, cwd=cwd,
                    capture_output=True, text=True, timeout=timeout_sec
                )
                self._json_response(200, {
                    "ok": True,
                    "exitCode": result.returncode,
                    "stdout": result.stdout[:200000],  # 200KB limit
                    "stderr": result.stderr[:50000],    # 50KB limit
                    "truncated": len(result.stdout) > 200000 or len(result.stderr) > 50000,
                    "command": command,
                    "cwd": cwd,
                })
            except subprocess.TimeoutExpired:
                self._json_response(504, {
                    "error": f"Command timed out after {timeout_sec}s",
                    "command": command,
                })
            except FileNotFoundError:
                self._json_response(500, {"error": f"Command not found: {command}"})
            except Exception as e:
                self._json_response(500, {"error": f"Exec failed: {e}"})

        # ==================================================================
        # AGENTIC TOOL-USE — Structured tool calls for AI agent loop
        # ==================================================================

        elif self.path == "/api/agent/tools":
            # Return available tools for the agent to call
            tools = [
                {
                    "type": "function",
                    "function": {
                        "name": "read_file",
                        "description": "Read the contents of a file at the given path",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "path": {"type": "string", "description": "Absolute file path"},
                            },
                            "required": ["path"],
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "write_file",
                        "description": "Write content to a file (creates parent dirs)",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "path": {"type": "string", "description": "Absolute file path"},
                                "content": {"type": "string", "description": "File content"},
                            },
                            "required": ["path", "content"],
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "search_files",
                        "description": "Search for text/regex across files in a directory",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "query": {"type": "string", "description": "Search query"},
                                "path": {"type": "string", "description": "Directory to search"},
                                "isRegex": {"type": "boolean", "description": "Treat query as regex"},
                            },
                            "required": ["query"],
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "list_directory",
                        "description": "List files and folders in a directory",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "path": {"type": "string", "description": "Directory path"},
                            },
                            "required": ["path"],
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "run_command",
                        "description": "Execute a shell command and return output",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "command": {"type": "string", "description": "Shell command"},
                                "cwd": {"type": "string", "description": "Working directory"},
                            },
                            "required": ["command"],
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "git_status",
                        "description": "Get git status of the repository",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "path": {"type": "string", "description": "Repository path"},
                            },
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "git_commit",
                        "description": "Stage all changes and commit with a message",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "message": {"type": "string", "description": "Commit message"},
                                "path": {"type": "string", "description": "Repository path"},
                            },
                            "required": ["message"],
                        },
                    },
                },
                {
                    "type": "function",
                    "function": {
                        "name": "git_push",
                        "description": "Push commits to remote",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "remote": {"type": "string", "description": "Remote name (default: origin)"},
                                "branch": {"type": "string", "description": "Branch name"},
                            },
                        },
                    },
                },
            ]
            self._json_response(200, {"tools": tools})

        elif self.path == "/api/agent/execute-tool" or self.path == "/api/tool":
            # Execute a single tool call from the agent
            try:
                data = json.loads(body_str) if body_str else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return
            tool_name = data.get("name", "")
            args = data.get("arguments", {})
            if isinstance(args, str):
                try:
                    args = json.loads(args)
                except json.JSONDecodeError:
                    args = {}
            _add_agent_event("tool_call", f"{tool_name}({json.dumps(args)[:200]})", "agent")
            try:
                result = self._execute_tool(tool_name, args)
                self._json_response(200, {"ok": True, "result": result})
            except Exception as e:
                _add_failure("tool_error", f"{tool_name}: {e}", "agent", "Failed")
                self._json_response(500, {"error": f"Tool {tool_name} failed: {e}"})

        else:
            self._json_response(404, {"error": "not_found"})

    # ---- Agentic tool executor for AI agent loop ----
    def _execute_tool(self, tool_name, args):
        """Execute a named tool and return the result dict.
        Called by /api/agent/execute-tool for the agentic loop.
        """
        if tool_name == "read_file":
            path = args.get("path", "")
            resolved = Path(path).resolve()
            if not resolved.exists():
                return {"error": f"File not found: {path}"}
            if not resolved.is_file():
                return {"error": f"Not a file: {path}"}
            if resolved.stat().st_size > 2 * 1024 * 1024:
                return {"error": f"File too large ({resolved.stat().st_size} bytes)"}
            content = resolved.read_text(encoding="utf-8", errors="replace")
            return {"content": content, "path": str(resolved), "size": len(content)}

        elif tool_name == "write_file":
            path = args.get("path", "")
            content = args.get("content", "")
            resolved = Path(path).resolve()
            resolved.parent.mkdir(parents=True, exist_ok=True)
            resolved.write_text(content, encoding="utf-8")
            _bump_stat("total_file_writes")
            return {"ok": True, "path": str(resolved), "size": len(content)}

        elif tool_name == "search_files":
            query = args.get("query", "")
            search_path = args.get("path", str(PROJECT_ROOT))
            is_regex = args.get("isRegex", False)
            resolved = Path(search_path).resolve()
            flags = re.IGNORECASE
            pattern = re.compile(query, flags) if is_regex else re.compile(re.escape(query), flags)
            results = []
            for root, dirs, files in os.walk(str(resolved)):
                dirs[:] = [d for d in dirs if not d.startswith(".") and d not in
                           ("node_modules", "__pycache__", ".git", "build", "Release", "Debug")]
                for fname in files:
                    if len(results) >= 100:
                        break
                    fpath = os.path.join(root, fname)
                    try:
                        if os.path.getsize(fpath) > 2 * 1024 * 1024:
                            continue
                        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                            for lnum, line in enumerate(f, 1):
                                if len(results) >= 100:
                                    break
                                if pattern.search(line):
                                    results.append({
                                        "file": fpath.replace("\\", "/"),
                                        "line": lnum,
                                        "text": line.rstrip()[:300],
                                    })
                    except (PermissionError, OSError):
                        continue
                if len(results) >= 100:
                    break
            return {"results": results, "total": len(results)}

        elif tool_name == "list_directory":
            dir_path = args.get("path", str(PROJECT_ROOT))
            resolved = Path(dir_path).resolve()
            if not resolved.is_dir():
                return {"error": f"Not a directory: {dir_path}"}
            entries = []
            for item in sorted(resolved.iterdir()):
                if len(entries) >= 200:
                    break
                entries.append({
                    "name": item.name,
                    "isDir": item.is_dir(),
                    "size": item.stat().st_size if item.is_file() else 0,
                })
            return {"entries": entries, "path": str(resolved)}

        elif tool_name == "run_command":
            command = args.get("command", "")
            cwd = args.get("cwd", str(PROJECT_ROOT))
            if not command:
                return {"error": "Empty command"}
            dangerous = ["format ", "del /s", "rd /s", "rm -rf /", "mkfs",
                         "shutdown", "reboot", "poweroff"]
            for d in dangerous:
                if d in command.lower():
                    return {"error": f"Blocked dangerous command: {d}"}
            result = subprocess.run(
                command, shell=True, cwd=cwd,
                capture_output=True, text=True, timeout=60
            )
            _bump_stat("total_terminal_execs")
            return {
                "exitCode": result.returncode,
                "stdout": result.stdout[:100000],
                "stderr": result.stderr[:20000],
            }

        elif tool_name == "git_status":
            repo = args.get("path", str(PROJECT_ROOT))
            result = subprocess.run(
                ["git", "status", "--porcelain", "-b"],
                cwd=repo, capture_output=True, text=True, timeout=15
            )
            return {"output": result.stdout, "exitCode": result.returncode}

        elif tool_name == "git_commit":
            repo = args.get("path", str(PROJECT_ROOT))
            message = args.get("message", "Auto-commit")
            subprocess.run(["git", "add", "."], cwd=repo, capture_output=True, timeout=30)
            result = subprocess.run(
                ["git", "commit", "-m", message],
                cwd=repo, capture_output=True, text=True, timeout=30
            )
            return {
                "output": result.stdout + result.stderr,
                "exitCode": result.returncode,
            }

        elif tool_name == "git_push":
            repo = args.get("path", str(PROJECT_ROOT))
            remote = args.get("remote", "origin")
            branch = args.get("branch", "")
            cmd = ["git", "push", remote]
            if branch:
                cmd.append(branch)
            result = subprocess.run(
                cmd, cwd=repo, capture_output=True, text=True, timeout=120
            )
            return {
                "output": result.stdout + result.stderr,
                "exitCode": result.returncode,
            }

        else:
            return {"error": f"Unknown tool: {tool_name}"}

    # ---- Local file reader for IDE file-path auto-attach ----
    def _read_local_file(self, file_path):
        """Read a local file and return its content as JSON.
        Security: Only allows reading from project directories and common code locations.
        """
        # Normalize path
        file_path = file_path.replace("\\", "/")
        resolved = Path(file_path).resolve()

        # Security: Only allow reading from safe directories
        allowed_roots = [
            PROJECT_ROOT,                          # D:/rawrxd/
            Path("D:/rawrxd").resolve(),
            Path("C:/RawrXD").resolve(),
            Path.home(),                            # User home
        ]
        # Also allow any path under the drive roots (local-only tool)
        is_safe = any(
            str(resolved).startswith(str(root)) for root in allowed_roots
        )
        # For local-only use: allow all local paths but block network paths
        if str(resolved).startswith("\\\\") or str(resolved).startswith("//"):
            self._json_response(403, {
                "error": "forbidden",
                "message": "Network paths are not allowed",
            })
            return

        if not resolved.exists():
            self._json_response(404, {
                "error": "file_not_found",
                "message": f"File not found: {file_path}",
            })
            return

        if not resolved.is_file():
            self._json_response(400, {
                "error": "not_a_file",
                "message": f"Path is not a file: {file_path}",
            })
            return

        # Size limit: 2MB for text files
        if resolved.stat().st_size > 2 * 1024 * 1024:
            self._json_response(413, {
                "error": "file_too_large",
                "message": f"File too large ({resolved.stat().st_size} bytes). Max: 2MB",
            })
            return

        try:
            content = resolved.read_text(encoding="utf-8", errors="replace")
            self._json_response(200, {
                "content": content,
                "name": resolved.name,
                "path": str(resolved),
                "size": len(content),
            })
        except Exception as e:
            self._json_response(500, {
                "error": "read_failed",
                "message": f"Failed to read file: {e}",
            })

    def _json_response(self, status, obj):
        body = json.dumps(obj).encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_cors_headers()
            self.send_security_headers()
            self.end_headers()
            self.wfile.write(body)
        except (ConnectionAbortedError, ConnectionResetError, BrokenPipeError):
            # Client disconnected before we could write — ignore silently
            pass

    def _serve_html(self):
        if not HTML_PATH.exists():
            self._json_response(404, {"error": "gui/ide_chatbot.html not found"})
            return

        content = HTML_PATH.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_cors_headers()
        # allow_framing=True: /gui is embedded in test iframes
        self.send_security_headers(allow_framing=True)
        self.end_headers()
        self.wfile.write(content)

    def _serve_test_jumper(self):
        test_path = SCRIPT_DIR / "test_jumper.html"
        if not test_path.exists():
            self._json_response(404, {"error": "gui/test_jumper.html not found"})
            return

        content = test_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_cors_headers()
        self.send_security_headers()
        self.end_headers()
        self.wfile.write(content)

    def _serve_test_harness(self):
        harness_path = PROJECT_ROOT / "test" / "RawrXD-UI-TestHarness.html"
        if not harness_path.exists():
            self._json_response(404, {"error": "test/RawrXD-UI-TestHarness.html not found"})
            return

        content = harness_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_cors_headers()
        self.send_security_headers()
        self.end_headers()
        self.wfile.write(content)


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="RawrXD IDE Agentic Interface Server")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Port (default: {DEFAULT_PORT})")
    parser.add_argument("--ollama-url", default=DEFAULT_OLLAMA_URL, help=f"Ollama URL (default: {DEFAULT_OLLAMA_URL})")
    args = parser.parse_args()

    RawrXDHandler.ollama_url = args.ollama_url

    # Use ThreadingHTTPServer for concurrent request handling
    # (prevents single-threaded blocking when clients timeout/disconnect)
    import socketserver
    class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
        daemon_threads = True
    server = ThreadedHTTPServer(("0.0.0.0", args.port), RawrXDHandler)

    # Startup banner
    print(f"\033[1;33m")
    print(f"  ╔══════════════════════════════════════════════════╗")
    print(f"  ║  🦖 RawrXD IDE — Agentic Interface v3.3        ║")
    print(f"  ╠══════════════════════════════════════════════════╣")
    print(f"  ║  Server:  http://localhost:{args.port:<14}      ║")
    print(f"  ║  GUI:     http://localhost:{args.port}/gui{' ' * (9 - len(str(args.port)))}      ║")
    print(f"  ║  Tests:   http://localhost:{args.port}/test{' ' * (8 - len(str(args.port)))}      ║")
    print(f"  ║  Ollama:  {args.ollama_url:<31}      ║")
    print(f"  ╠══════════════════════════════════════════════════╣")
    print(f"  ║  Endpoints:                                     ║")
    print(f"  ║  POST /v1/chat/completions  (OpenAI proxy)      ║")
    print(f"  ║  POST /api/generate         (Ollama proxy)      ║")
    print(f"  ║  POST /ask                  (legacy)            ║")
    print(f"  ║  GET  /api/failures         (failure intel)     ║")
    print(f"  ║  GET  /api/agents/status    (agent dashboard)   ║")
    print(f"  ║  POST /api/agents/replay    (replay actions)    ║")
    print(f"  ║  POST /api/read-file        (local file read)   ║")
    print(f"  ╠══════════════════════════════════════════════════╣")
    print(f"  ║  Phase 4: Security & Hardening                  ║")
    print(f"  ║  Max Body:   {MAX_REQUEST_BODY // (1024*1024):>3} MB                              ║")
    print(f"  ║  Rate Limit: {RATE_LIMIT_PER_MINUTE:>3}/min per IP                       ║")
    print(f"  ║  CORS:       Restricted (localhost only)        ║")
    print(f"  ║  Headers:    X-Content-Type-Options, X-Frame    ║")
    print(f"  ╚══════════════════════════════════════════════════╝")
    print(f"\033[0m")

    models = _get_cached_models(args.ollama_url)
    print(f"  Found {len(models)} models (cached for {_MODEL_CACHE_TTL}s):")
    for m in models[:10]:
        print(f"    • {m['name']} ({m['size']}) [{m['type']}]")
    if len(models) > 10:
        print(f"    ... and {len(models) - 10} more")
    print()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n\033[33m[RawrXD]\033[0m Server stopped.")
        server.shutdown()


if __name__ == "__main__":
    main()
