#!/usr/bin/env python3
"""
RawrEngine — Unified Backend for RawrXD Universal Access
=========================================================
Production-ready HTTP API server that bridges:
  - Web interface (web_interface/index.html)
  - Local IDE (Win32/MASM)
  - External AI providers (Ollama, OpenAI-compatible)

Endpoints (compatible with web_interface/index.html):
  GET  /status              -> Connection check
  GET  /v1/models           -> List available models
  GET  /api/tools           -> List available tools
  POST /api/chat            -> Chat with SSE streaming
  POST /api/agentic/config  -> Update agent configuration
  POST /api/agent/wish      -> Agent plan/execute mode
  GET  /health              -> Health check
  GET  /models              -> Legacy model list
  POST /ask                 -> Legacy ask endpoint
  POST /scan                -> Force rescan models

Zero external dependencies — uses only Python stdlib.
Cross-platform (Linux, macOS, Windows).
"""

import http.server
import json
import os
import sys
import time
import glob
import threading
import traceback
from pathlib import Path
from urllib.parse import urlparse, parse_qs

# Configuration from environment or defaults
HOST = os.environ.get("RAWRXD_HOST", "0.0.0.0")
PORT = int(os.environ.get("RAWRXD_PORT", "23959"))
CORS_ORIGINS = os.environ.get("RAWRXD_CORS_ORIGINS", "*")
API_KEYS = set(filter(None, os.environ.get("RAWRXD_API_KEYS", "").split(",")))
REQUIRE_AUTH = os.environ.get("RAWRXD_REQUIRE_AUTH", "0") == "1"

# Cross-platform model directories
MODEL_DIRS = []
if sys.platform == "win32":
    MODEL_DIRS = [
        r"D:\OllamaModels",
        r"D:\models",
        r"C:\models",
        os.path.expanduser(r"~\.ollama\models"),
    ]
else:
    MODEL_DIRS = [
        os.path.expanduser("~/.ollama/models"),
        "/opt/models",
        "./models",
    ]

# Allow override via environment
if os.environ.get("RAWRXD_MODEL_PATH"):
    MODEL_DIRS.insert(0, os.environ["RAWRXD_MODEL_PATH"])

# State
cached_models = []
models_last_scan = 0
MODEL_CACHE_TTL = 30

agent_config = {
    "model": "",
    "mode": "ask",
    "temperature": 0.7,
    "max_tokens": 2048,
}

AVAILABLE_TOOLS = [
    {"name": "read_file", "description": "Read a file from the workspace"},
    {"name": "write_file", "description": "Write content to a file"},
    {"name": "run_command", "description": "Execute a shell command"},
    {"name": "search_code", "description": "Search for patterns in codebase"},
    {"name": "list_files", "description": "List files in a directory"},
    {"name": "edit_file", "description": "Apply targeted edits to a file"},
]

VERSION = "4.0.0"


def format_file_size(size_bytes):
    if size_bytes >= 1073741824:
        return f"{size_bytes / 1073741824:.1f} GB"
    elif size_bytes >= 1048576:
        return f"{size_bytes / 1048576:.1f} MB"
    else:
        return f"{size_bytes / 1024:.1f} KB"


def load_ollama_manifests(base_dir):
    manifest_map = {}
    manifests_dir = os.path.join(base_dir, "manifests", "registry.ollama.ai")
    if not os.path.exists(manifests_dir):
        return manifest_map

    for root, dirs, files in os.walk(manifests_dir):
        for filename in files:
            manifest_path = os.path.join(root, filename)
            try:
                with open(manifest_path, "r", encoding="utf-8") as f:
                    manifest = json.load(f)
                if not isinstance(manifest, dict):
                    continue
                rel_path = os.path.relpath(manifest_path, manifests_dir)
                path_parts = rel_path.replace("\\", "/").split("/")
                if len(path_parts) >= 2:
                    namespace = path_parts[0]
                    model_name = path_parts[1]
                    tag = path_parts[2] if len(path_parts) > 2 else "latest"
                    display_name = f"{namespace}/{model_name}:{tag}"
                    if "layers" in manifest and isinstance(manifest["layers"], list):
                        for layer in manifest["layers"]:
                            if isinstance(layer, dict) and "digest" in layer:
                                blob_hash = layer["digest"].replace("sha256:", "sha256-")
                                manifest_map[blob_hash] = display_name
            except (json.JSONDecodeError, OSError, KeyError, TypeError):
                pass
    return manifest_map


def scan_model_directories():
    global cached_models, models_last_scan
    models = []
    file_patterns = ["*.gguf", "*.bin", "*.safetensors"]
    ollama_manifests = {}

    for dir_path in MODEL_DIRS:
        if os.path.exists(dir_path):
            ollama_manifests.update(load_ollama_manifests(dir_path))

    for dir_path in MODEL_DIRS:
        if not os.path.exists(dir_path):
            continue
        for root, dirs, files in os.walk(dir_path):
            for filename in files:
                filepath = os.path.join(root, filename)
                ext = filename.rsplit(".", 1)[-1].lower() if "." in filename else ""
                is_ollama_blob = filename.startswith("sha256-") and "blobs" in root.lower()
                is_model_file = any(
                    filename.lower().endswith(p.replace("*", "")) for p in file_patterns
                )

                if is_model_file or is_ollama_blob:
                    try:
                        size = os.path.getsize(filepath)
                        if is_ollama_blob:
                            display_name = ollama_manifests.get(
                                filename, filename[:24] + "..."
                            )
                            model_type = "ollama"
                        else:
                            display_name = filename
                            model_type = ext
                        models.append(
                            {
                                "id": display_name,
                                "name": display_name,
                                "path": filepath,
                                "type": model_type,
                                "size": format_file_size(size),
                                "size_bytes": size,
                            }
                        )
                    except OSError:
                        pass

    cached_models = models
    models_last_scan = time.time()
    log(f"Scan complete: {len(models)} models across {len(MODEL_DIRS)} directories")
    return models


def get_models():
    global cached_models, models_last_scan
    if time.time() - models_last_scan > MODEL_CACHE_TTL:
        return scan_model_directories()
    return cached_models


def query_ollama(model, messages, stream=False):
    """Query Ollama API on localhost:11434"""
    import urllib.request
    import urllib.error

    # Format prompt from messages
    prompt = ""
    for msg in messages:
        role = msg.get("role", "user")
        content = msg.get("content", "")
        if role == "system":
            prompt += f"<|system|>\n{content}\n"
        elif role == "user":
            prompt += f"<|user|>\n{content}\n"
        elif role == "assistant":
            prompt += f"<|assistant|>\n{content}\n"
    prompt += "<|assistant|>\n"

    try:
        url = "http://localhost:11434/api/generate"
        payload = {
            "model": model or "llama3.2:3b",
            "prompt": prompt,
            "stream": False,
        }

        req = urllib.request.Request(
            url,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        with urllib.request.urlopen(req, timeout=120) as response:
            data = json.loads(response.read().decode("utf-8"))
            return data.get("response", "").strip()
    except Exception as e:
        log(f"Ollama query failed: {e}")
        return None


def log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] [RawrEngine] {msg}")


class RawrEngineHandler(http.server.BaseHTTPRequestHandler):
    """Unified HTTP handler for all RawrXD endpoints"""

    def log_message(self, format, *args):
        log(f"{self.client_address[0]} {format % args}")

    def send_cors_headers(self):
        origin = self.headers.get("Origin", "*")
        if CORS_ORIGINS == "*":
            self.send_header("Access-Control-Allow-Origin", origin)
        elif origin and any(o in origin for o in CORS_ORIGINS.split(",")):
            self.send_header("Access-Control-Allow-Origin", origin)
        else:
            self.send_header("Access-Control-Allow-Origin", "")
        self.send_header(
            "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"
        )
        self.send_header(
            "Access-Control-Allow-Headers",
            "Content-Type, X-API-Key, Authorization, Cache-Control",
        )
        self.send_header("Access-Control-Allow-Credentials", "true")
        self.send_header("Access-Control-Max-Age", "86400")

    def send_json(self, data, status=200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(body))
        self.send_cors_headers()
        self.end_headers()
        self.wfile.write(body)

    def check_auth(self):
        if not REQUIRE_AUTH:
            return True
        if self.client_address[0] in ("127.0.0.1", "::1", "localhost"):
            return True
        key = self.headers.get("X-API-Key") or self.headers.get(
            "Authorization", ""
        ).replace("Bearer ", "")
        if not key:
            self.send_json({"error": "API key required"}, 401)
            return False
        if API_KEYS and key not in API_KEYS:
            self.send_json({"error": "Invalid API key"}, 403)
            return False
        return True

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_cors_headers()
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        path = urlparse(self.path).path

        if path == "/status":
            self.handle_status()
        elif path == "/health":
            self.handle_health()
        elif path == "/v1/models":
            self.handle_v1_models()
        elif path == "/models":
            self.handle_legacy_models()
        elif path == "/api/tools":
            self.handle_tools()
        elif path == "/config":
            self.handle_config()
        elif path == "/":
            self.handle_root()
        else:
            self.send_json({"error": "Not Found", "path": path}, 404)

    def do_POST(self):
        if not self.check_auth():
            return

        path = urlparse(self.path).path
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8") if content_length > 0 else ""

        try:
            data = json.loads(body) if body else {}
        except json.JSONDecodeError:
            self.send_json({"error": "Invalid JSON"}, 400)
            return

        if path == "/api/chat":
            self.handle_chat(data)
        elif path == "/api/agentic/config":
            self.handle_agentic_config(data)
        elif path == "/api/agent/wish":
            self.handle_agent_wish(data)
        elif path == "/ask" or path == "/":
            self.handle_legacy_ask(data)
        elif path == "/scan":
            self.handle_scan()
        elif path == "/api/generate":
            self.handle_generate(data)
        else:
            self.send_json({"error": "Not Found", "path": path}, 404)

    # ---- GET handlers ----

    def handle_status(self):
        models = get_models()
        self.send_json(
            {
                "status": "online",
                "version": VERSION,
                "backend": "rawr-engine-python",
                "models_loaded": len(models),
                "port": PORT,
                "agent_mode": agent_config["mode"],
                "current_model": agent_config["model"],
            }
        )

    def handle_health(self):
        models = get_models()
        self.send_json(
            {
                "status": "online",
                "port": PORT,
                "models": len(models),
                "version": VERSION,
                "backend": "rawr-engine-python",
            }
        )

    def handle_v1_models(self):
        models = get_models()
        self.send_json(
            {
                "models": [
                    {
                        "id": m["id"],
                        "object": "model",
                        "owned_by": "local",
                        "type": m["type"],
                        "size": m["size"],
                    }
                    for m in models
                ]
            }
        )

    def handle_legacy_models(self):
        models = get_models()
        self.send_json({"models": models})

    def handle_tools(self):
        self.send_json({"tools": AVAILABLE_TOOLS})

    def handle_config(self):
        self.send_json(
            {
                "model_dirs": MODEL_DIRS,
                "port": PORT,
                "cache_ttl": MODEL_CACHE_TTL,
                "agent_config": agent_config,
                "cors_origins": CORS_ORIGINS,
                "require_auth": REQUIRE_AUTH,
            }
        )

    def handle_root(self):
        self.send_json(
            {
                "name": "RawrXD RawrEngine",
                "version": VERSION,
                "endpoints": [
                    "GET  /status",
                    "GET  /health",
                    "GET  /v1/models",
                    "GET  /models",
                    "GET  /api/tools",
                    "GET  /config",
                    "POST /api/chat",
                    "POST /api/agentic/config",
                    "POST /api/agent/wish",
                    "POST /api/generate",
                    "POST /ask",
                    "POST /scan",
                ],
            }
        )

    # ---- POST handlers ----

    def handle_chat(self, data):
        """Handle /api/chat with SSE streaming support"""
        model = data.get("model", agent_config["model"])
        messages = data.get("messages", [])
        stream = data.get("stream", False)

        if not messages:
            self.send_json({"error": "No messages provided"}, 400)
            return

        # Try Ollama first
        response_text = query_ollama(model, messages)

        if response_text is None:
            # Fallback: echo-based response for development/demo
            last_msg = messages[-1].get("content", "") if messages else ""
            response_text = (
                f"[RawrEngine] No inference backend connected. "
                f"Install Ollama (`ollama serve`) or configure an external provider.\n\n"
                f"Your message: {last_msg}"
            )

        if stream:
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.send_cors_headers()
            self.end_headers()

            # Stream response word by word
            words = response_text.split(" ")
            for i, word in enumerate(words):
                chunk = word + (" " if i < len(words) - 1 else "")
                sse_data = json.dumps(
                    {
                        "choices": [
                            {
                                "delta": {"content": chunk},
                                "index": 0,
                                "finish_reason": None,
                            }
                        ]
                    }
                )
                try:
                    self.wfile.write(f"data: {sse_data}\n\n".encode("utf-8"))
                    self.wfile.flush()
                except BrokenPipeError:
                    return

            # Send done event
            done_data = json.dumps(
                {
                    "choices": [
                        {"delta": {}, "index": 0, "finish_reason": "stop"}
                    ]
                }
            )
            try:
                self.wfile.write(f"data: {done_data}\n\n".encode("utf-8"))
                self.wfile.write(b"data: [DONE]\n\n")
                self.wfile.flush()
            except BrokenPipeError:
                pass
        else:
            self.send_json(
                {
                    "choices": [
                        {
                            "message": {
                                "role": "assistant",
                                "content": response_text,
                            },
                            "index": 0,
                            "finish_reason": "stop",
                        }
                    ],
                    "model": model,
                    "usage": {
                        "prompt_tokens": sum(
                            len(m.get("content", "").split()) for m in messages
                        ),
                        "completion_tokens": len(response_text.split()),
                        "total_tokens": sum(
                            len(m.get("content", "").split()) for m in messages
                        )
                        + len(response_text.split()),
                    },
                }
            )

    def handle_agentic_config(self, data):
        """Handle /api/agentic/config — update agent configuration"""
        if "model" in data:
            agent_config["model"] = data["model"]
        if "mode" in data:
            agent_config["mode"] = data["mode"]
        if "temperature" in data:
            agent_config["temperature"] = data["temperature"]
        if "max_tokens" in data:
            agent_config["max_tokens"] = data["max_tokens"]

        log(f"Agent config updated: {agent_config}")
        self.send_json({"ok": True, "config": agent_config})

    def handle_agent_wish(self, data):
        """Handle /api/agent/wish — Plan or execute agent tasks"""
        wish = data.get("wish", "")
        mode = data.get("mode", "ask")
        model = data.get("model", agent_config["model"])
        auto_execute = data.get("auto_execute", False)

        if not wish:
            self.send_json({"error": "No wish provided"}, 400)
            return

        if mode == "plan":
            # Generate a plan from the wish
            plan_steps = self._generate_plan(wish)
            self.send_json(
                {
                    "plan": plan_steps,
                    "requires_confirmation": True,
                    "wish": wish,
                    "model": model,
                }
            )
        elif mode == "full":
            # Execute the wish fully
            response = self._execute_wish(wish, model, auto_execute)
            self.send_json(response)
        else:
            # Ask mode: just get a response
            messages = [{"role": "user", "content": wish}]
            response_text = query_ollama(model, messages)
            if response_text is None:
                response_text = (
                    f"[RawrEngine] Agent mode: {mode}. "
                    f"No inference backend available. Wish: {wish}"
                )
            self.send_json({"response": response_text, "model": model, "mode": mode})

    def handle_legacy_ask(self, data):
        """Handle legacy /ask endpoint"""
        question = data.get("question", data.get("prompt", ""))
        model = data.get("model", "")
        system_prompt = data.get("system_prompt", "You are a helpful assistant.")

        if not question:
            self.send_json({"error": "No question provided"}, 400)
            return

        messages = [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": question},
        ]
        answer = query_ollama(model, messages)

        if answer:
            self.send_json({"answer": answer})
        else:
            self.send_json(
                {
                    "answer": (
                        "Error: Could not reach inference backend. "
                        "Make sure Ollama is running (`ollama serve`) "
                        "or configure an external provider."
                    ),
                    "error": True,
                },
                503,
            )

    def handle_generate(self, data):
        """Handle /api/generate endpoint"""
        prompt = data.get("prompt", "")
        model = data.get("model", agent_config["model"])
        max_tokens = data.get("max_tokens", 128)

        if not prompt:
            self.send_json({"error": "No prompt provided"}, 400)
            return

        messages = [{"role": "user", "content": prompt}]
        response_text = query_ollama(model, messages)

        if response_text is None:
            response_text = (
                f"[RawrEngine] No inference backend connected. Prompt: {prompt[:100]}"
            )

        self.send_json(
            {
                "response": response_text,
                "tokens_generated": len(response_text.split()),
                "status": "success",
            }
        )

    def handle_scan(self):
        """Force rescan of model directories"""
        models = scan_model_directories()
        self.send_json({"models": models, "count": len(models)})

    # ---- Agent helpers ----

    def _generate_plan(self, wish):
        """Generate a plan from a wish description"""
        # Try Ollama for intelligent planning
        messages = [
            {
                "role": "system",
                "content": (
                    "You are a planning agent. Given a task, break it into numbered steps. "
                    "Return ONLY the steps, one per line, numbered. No extra text."
                ),
            },
            {"role": "user", "content": wish},
        ]
        response = query_ollama(agent_config["model"], messages)

        if response:
            lines = [
                line.strip()
                for line in response.strip().split("\n")
                if line.strip()
            ]
            return lines if lines else [f"Execute: {wish}"]

        # Fallback: basic decomposition
        return [
            f"Analyze request: {wish}",
            "Identify required files and dependencies",
            "Plan modifications",
            "Execute changes",
            "Verify results",
        ]

    def _execute_wish(self, wish, model, auto_execute):
        """Execute a wish in full agent mode"""
        plan = self._generate_plan(wish)

        tool_calls = []
        if auto_execute:
            # Simulate tool usage based on wish keywords
            wish_lower = wish.lower()
            if any(w in wish_lower for w in ["file", "read", "open", "show"]):
                tool_calls.append(
                    {"tool": "read_file", "params": {"path": "."}, "status": "pending"}
                )
            if any(w in wish_lower for w in ["write", "create", "edit", "change", "fix"]):
                tool_calls.append(
                    {
                        "tool": "edit_file",
                        "params": {"description": wish},
                        "status": "pending",
                    }
                )
            if any(w in wish_lower for w in ["run", "execute", "build", "test"]):
                tool_calls.append(
                    {
                        "tool": "run_command",
                        "params": {"command": wish},
                        "status": "pending",
                    }
                )
            if any(w in wish_lower for w in ["search", "find", "grep", "look"]):
                tool_calls.append(
                    {
                        "tool": "search_code",
                        "params": {"pattern": wish},
                        "status": "pending",
                    }
                )

        # Try to get a response from the model
        messages = [
            {
                "role": "system",
                "content": (
                    "You are an AI coding agent. Execute the user's request. "
                    "Describe what you would do step by step."
                ),
            },
            {"role": "user", "content": wish},
        ]
        response_text = query_ollama(model, messages)

        if response_text is None:
            response_text = (
                f"[RawrEngine] Agent plan generated for: {wish}\n\n"
                + "\n".join(f"  {i+1}. {step}" for i, step in enumerate(plan))
                + "\n\nConnect an inference backend to execute with full AI capabilities."
            )

        return {
            "response": response_text,
            "plan": plan,
            "tool_calls": tool_calls,
            "model": model,
            "mode": "full",
            "auto_executed": auto_execute,
        }


class ThreadedHTTPServer(http.server.ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    print(
        f"""
================================================================
  RawrXD RawrEngine v{VERSION}
  Universal Access Backend — Cross-Platform
  Zero Dependencies — Pure Python stdlib
================================================================
"""
    )

    # Initial scan
    scan_model_directories()

    print(f"Model directories:")
    for d in MODEL_DIRS:
        exists = "+" if os.path.exists(d) else "-"
        print(f"  [{exists}] {d}")

    print(f"\nEndpoints:")
    print(f"  GET  /status              — Connection check")
    print(f"  GET  /health              — Health status")
    print(f"  GET  /v1/models           — List models (OpenAI format)")
    print(f"  GET  /models              — List models (legacy)")
    print(f"  GET  /api/tools           — List tools")
    print(f"  GET  /config              — Server configuration")
    print(f"  POST /api/chat            — Chat with SSE streaming")
    print(f"  POST /api/agentic/config  — Update agent config")
    print(f"  POST /api/agent/wish      — Agent plan/execute")
    print(f"  POST /api/generate        — Generate completion")
    print(f"  POST /ask                 — Legacy ask")
    print(f"  POST /scan                — Force rescan")

    if REQUIRE_AUTH:
        print(f"\nAuth: REQUIRED (keys configured: {len(API_KEYS)})")
    else:
        print(f"\nAuth: Disabled (localhost bypass)")

    print(f"CORS: {CORS_ORIGINS}")
    print(f"\n================================================================")

    server = ThreadedHTTPServer((HOST, PORT), RawrEngineHandler)
    print(f"[+] Server listening on http://{HOST}:{PORT}")
    print(f"[+] Web UI: Open web_interface/index.html and connect to http://localhost:{PORT}")
    print(f"[*] Press Ctrl+C to stop\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[!] Shutdown signal received")
        server.shutdown()


if __name__ == "__main__":
    main()
