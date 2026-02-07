#!/usr/bin/env python3
"""
RawrXD IDE — Agentic Interface Backend (serve.py)
=================================================
Standalone Python HTTP server that bridges the HTML chatbot frontend
to local Ollama and GGUF model files. Use this when the Win32IDE's
built-in C++ server is not running.

Endpoints:
  GET  /models   — Scan D:/OllamaModels for .gguf files + query Ollama
  POST /ask      — Forward chat query to Ollama /api/generate
  GET  /gui      — Serve gui/ide_chatbot.html
  GET  /health   — Health check

Usage:
  python gui/serve.py                    # Default port 11435
  python gui/serve.py --port 8080        # Custom port
  python gui/serve.py --ollama-url http://localhost:11434  # Custom Ollama
"""

import http.server
import json
import os
import sys
import urllib.request
import urllib.error
import argparse
import glob
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
DEFAULT_OLLAMA_URL = "http://localhost:11434"

# Resolve project root (parent of gui/)
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
HTML_PATH = SCRIPT_DIR / "ide_chatbot.html"


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
    """Aggregate all model sources."""
    models = []
    models.extend(scan_gguf_models())
    models.extend(scan_blobs())
    models.extend(query_ollama_models(ollama_url))
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


# ============================================================================
# HTTP Handler
# ============================================================================

class RawrXDHandler(http.server.BaseHTTPRequestHandler):
    ollama_url = DEFAULT_OLLAMA_URL

    def log_message(self, format, *args):
        """Override to add color to terminal output."""
        sys.stderr.write(f"\033[36m[RawrXD]\033[0m {args[0]} {args[1]} {args[2]}\n")

    def send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_cors_headers()
        self.end_headers()

    def do_GET(self):
        if self.path == "/health" or self.path == "/":
            self._json_response(200, {"status": "ok", "server": "RawrXD-serve.py"})

        elif self.path == "/models":
            models = get_all_models(self.ollama_url)
            self._json_response(200, {"models": models})

        elif self.path in ("/gui", "/gui/"):
            self._serve_html()

        elif self.path == "/status":
            models = get_all_models(self.ollama_url)
            self._json_response(200, {
                "ready": True,
                "model_loaded": False,
                "backend": "rawrxd-serve.py",
                "available_models": len(models),
            })

        else:
            self._json_response(404, {"error": "not_found", "message": f"Unknown: {self.path}"})

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8", errors="replace") if content_length else ""

        if self.path == "/ask":
            try:
                data = json.loads(body) if body else {}
            except json.JSONDecodeError:
                self._json_response(400, {"error": "Invalid JSON"})
                return

            question = data.get("question", "")
            model = data.get("model", "")
            context = data.get("context", 4096)

            if not question:
                self._json_response(400, {"error": "No question provided"})
                return

            answer = ask_ollama(question, model, self.ollama_url, context)
            self._json_response(200, {"answer": answer})

        else:
            self._json_response(404, {"error": "not_found"})

    def _json_response(self, status, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_cors_headers()
        self.end_headers()
        self.wfile.write(body)

    def _serve_html(self):
        if not HTML_PATH.exists():
            self._json_response(404, {"error": "gui/ide_chatbot.html not found"})
            return

        content = HTML_PATH.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_cors_headers()
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

    server = http.server.HTTPServer(("0.0.0.0", args.port), RawrXDHandler)

    # Startup banner
    print(f"\033[1;33m")
    print(f"  ╔══════════════════════════════════════════╗")
    print(f"  ║  🦖 RawrXD IDE — Agentic Interface      ║")
    print(f"  ╠══════════════════════════════════════════╣")
    print(f"  ║  Server:  http://localhost:{args.port:<14}║")
    print(f"  ║  GUI:     http://localhost:{args.port}/gui{' ' * (9 - len(str(args.port)))}║")
    print(f"  ║  Ollama:  {args.ollama_url:<31}║")
    print(f"  ╚══════════════════════════════════════════╝")
    print(f"\033[0m")

    models = get_all_models(args.ollama_url)
    print(f"  Found {len(models)} models:")
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
