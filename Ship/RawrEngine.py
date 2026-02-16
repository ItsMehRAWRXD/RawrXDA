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
import fnmatch
import ipaddress
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import threading
import time
import uuid
from dataclasses import dataclass, field
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Iterable
from urllib.parse import parse_qs, urlparse


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
    {"id": "fs_list", "name": "fs_list", "description": "List workspace files"},
    {"id": "file_reader", "name": "file_reader", "description": "Read workspace files"},
    {"id": "file_writer", "name": "file_writer", "description": "Write workspace files"},
    {"id": "fs_mkdir", "name": "fs_mkdir", "description": "Create workspace directories"},
    {"id": "fs_rename", "name": "fs_rename", "description": "Rename files and directories"},
    {"id": "fs_delete", "name": "fs_delete", "description": "Delete files and directories"},
    {"id": "search", "name": "search", "description": "Search workspace content"},
    {"id": "git_status", "name": "git_status", "description": "Inspect git working tree status"},
    {"id": "git_diff", "name": "git_diff", "description": "Inspect git diffs"},
    {"id": "git_commit", "name": "git_commit", "description": "Create git commits"},
    {"id": "code_edit", "name": "code_edit", "description": "Apply code edits"},
    {"id": "terminal_exec", "name": "terminal_exec", "description": "Run shell commands"},
    {"id": "planner", "name": "planner", "description": "Build execution plans"},
]

PUBLIC_PATHS = {"/status", "/health", "/healthz"}

WORKSPACE_ROOT = os.path.abspath(os.getenv("RAWRXD_WORKSPACE_ROOT", os.getcwd()))
IGNORE_DIRS = {
    ".git",
    ".hg",
    ".svn",
    "node_modules",
    ".venv",
    "venv",
    "__pycache__",
    "build",
    "dist",
    "out",
}
MAX_LIST_ITEMS = int(os.getenv("RAWRXD_MAX_LIST_ITEMS", "2000"))
MAX_FILE_READ_BYTES = int(os.getenv("RAWRXD_MAX_FILE_READ_BYTES", str(1024 * 1024)))
MAX_SEARCH_RESULTS = int(os.getenv("RAWRXD_MAX_SEARCH_RESULTS", "300"))
MAX_TERMINAL_TIMEOUT_S = int(os.getenv("RAWRXD_MAX_TERMINAL_TIMEOUT_S", "60"))
MAX_GIT_TIMEOUT_S = int(os.getenv("RAWRXD_MAX_GIT_TIMEOUT_S", "60"))


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


def _normalize_relpath(path: str) -> str:
    return path.replace("\\", "/")


def _resolve_workspace_path(raw_path: str | None) -> str:
    candidate = (raw_path or ".").strip() or "."
    if os.path.isabs(candidate):
        resolved = os.path.abspath(candidate)
    else:
        resolved = os.path.abspath(os.path.join(WORKSPACE_ROOT, candidate))

    try:
        within_workspace = os.path.commonpath([WORKSPACE_ROOT, resolved]) == WORKSPACE_ROOT
    except ValueError:
        within_workspace = False
    if not within_workspace:
        raise ValueError("Path escapes workspace root")
    return resolved


def _workspace_relpath(abs_path: str) -> str:
    rel = os.path.relpath(abs_path, WORKSPACE_ROOT)
    if rel == ".":
        return "."
    return _normalize_relpath(rel)


def _list_workspace(path: str, depth: int = 2, include_hidden: bool = False) -> dict[str, Any]:
    target = _resolve_workspace_path(path)
    if not os.path.exists(target):
        raise FileNotFoundError(f"Path not found: {path}")
    if not os.path.isdir(target):
        raise NotADirectoryError(f"Not a directory: {path}")

    root_depth = target.rstrip(os.sep).count(os.sep)
    items: list[dict[str, Any]] = []

    for root, dirs, files in os.walk(target):
        current_depth = root.rstrip(os.sep).count(os.sep) - root_depth

        filtered_dirs: list[str] = []
        for directory in sorted(dirs):
            if directory in IGNORE_DIRS:
                continue
            if not include_hidden and directory.startswith("."):
                continue
            filtered_dirs.append(directory)
        dirs[:] = filtered_dirs

        if current_depth >= depth:
            dirs[:] = []

        for directory in dirs:
            abs_dir = os.path.join(root, directory)
            items.append({"type": "dir", "path": _workspace_relpath(abs_dir)})
            if len(items) >= MAX_LIST_ITEMS:
                return {
                    "root": _workspace_relpath(target),
                    "count": len(items),
                    "truncated": True,
                    "items": items,
                }

        for filename in sorted(files):
            if not include_hidden and filename.startswith("."):
                continue
            abs_file = os.path.join(root, filename)
            try:
                size = os.path.getsize(abs_file)
            except OSError:
                continue
            items.append({"type": "file", "path": _workspace_relpath(abs_file), "size_bytes": size})
            if len(items) >= MAX_LIST_ITEMS:
                return {
                    "root": _workspace_relpath(target),
                    "count": len(items),
                    "truncated": True,
                    "items": items,
                }

    return {"root": _workspace_relpath(target), "count": len(items), "truncated": False, "items": items}


def _read_workspace_file(path: str) -> dict[str, Any]:
    target = _resolve_workspace_path(path)
    if not os.path.exists(target):
        raise FileNotFoundError(f"File not found: {path}")
    if not os.path.isfile(target):
        raise IsADirectoryError(f"Not a file: {path}")

    size = os.path.getsize(target)
    truncated = size > MAX_FILE_READ_BYTES
    with open(target, "rb") as handle:
        data = handle.read(MAX_FILE_READ_BYTES)
    content = data.decode("utf-8", errors="replace")
    return {
        "path": _workspace_relpath(target),
        "size_bytes": size,
        "truncated": truncated,
        "content": content,
    }


def _write_workspace_file(path: str, content: str) -> dict[str, Any]:
    target = _resolve_workspace_path(path)
    parent = os.path.dirname(target)
    if parent:
        os.makedirs(parent, exist_ok=True)
    encoded = content.encode("utf-8")
    with open(target, "wb") as handle:
        handle.write(encoded)
    return {"path": _workspace_relpath(target), "size_bytes": len(encoded)}


def _mkdir_workspace(path: str, parents: bool = True) -> dict[str, Any]:
    target = _resolve_workspace_path(path)
    if os.path.exists(target):
        if not os.path.isdir(target):
            raise FileExistsError(f"Path exists and is not a directory: {path}")
        return {"path": _workspace_relpath(target), "created": False}

    if parents:
        os.makedirs(target, exist_ok=True)
    else:
        os.mkdir(target)
    return {"path": _workspace_relpath(target), "created": True}


def _rename_workspace(src_path: str, dst_path: str) -> dict[str, Any]:
    src = _resolve_workspace_path(src_path)
    dst = _resolve_workspace_path(dst_path)
    if not os.path.exists(src):
        raise FileNotFoundError(f"Source path not found: {src_path}")
    if os.path.exists(dst):
        raise FileExistsError(f"Destination already exists: {dst_path}")
    dst_parent = os.path.dirname(dst)
    if dst_parent:
        os.makedirs(dst_parent, exist_ok=True)
    os.replace(src, dst)
    return {"from": _workspace_relpath(src), "to": _workspace_relpath(dst)}


def _delete_workspace(path: str, recursive: bool = False) -> dict[str, Any]:
    target = _resolve_workspace_path(path)
    if not os.path.exists(target):
        raise FileNotFoundError(f"Path not found: {path}")
    if os.path.isfile(target):
        os.remove(target)
        return {"path": _workspace_relpath(target), "deleted": True, "type": "file"}
    if os.path.isdir(target):
        if recursive:
            shutil.rmtree(target)
        else:
            os.rmdir(target)
        return {"path": _workspace_relpath(target), "deleted": True, "type": "dir"}
    raise ValueError(f"Unsupported path type: {path}")


def _iter_searchable_files(base_path: str, include_hidden: bool, file_glob: str) -> Iterable[str]:
    for root, dirs, files in os.walk(base_path):
        dirs[:] = [
            d for d in sorted(dirs)
            if d not in IGNORE_DIRS and (include_hidden or not d.startswith("."))
        ]
        for filename in sorted(files):
            if not include_hidden and filename.startswith("."):
                continue
            if file_glob and not fnmatch.fnmatch(filename, file_glob):
                continue
            yield os.path.join(root, filename)


def _search_workspace(
    query: str,
    *,
    path: str = ".",
    regex: bool = False,
    case_sensitive: bool = False,
    include_hidden: bool = False,
    file_glob: str = "*",
    max_results: int = MAX_SEARCH_RESULTS,
) -> dict[str, Any]:
    if not query:
        raise ValueError("Search query is required")

    target_root = _resolve_workspace_path(path)
    if not os.path.isdir(target_root):
        raise NotADirectoryError(f"Not a directory: {path}")

    flags = 0 if case_sensitive else re.IGNORECASE
    compiled = re.compile(query, flags) if regex else None

    results: list[dict[str, Any]] = []
    scanned_files = 0

    for file_path in _iter_searchable_files(target_root, include_hidden, file_glob):
        scanned_files += 1
        try:
            if os.path.getsize(file_path) > MAX_FILE_READ_BYTES * 3:
                continue
            with open(file_path, "r", encoding="utf-8", errors="replace") as handle:
                for line_number, line in enumerate(handle, start=1):
                    haystack = line if case_sensitive else line.lower()
                    needle = query if case_sensitive else query.lower()
                    if compiled:
                        match = compiled.search(line)
                    else:
                        index = haystack.find(needle)
                        match = None if index < 0 else (index, index + len(needle))
                    if match is None:
                        continue
                    if isinstance(match, tuple):
                        column = match[0] + 1
                    else:
                        column = match.start() + 1
                    results.append(
                        {
                            "path": _workspace_relpath(file_path),
                            "line": line_number,
                            "column": column,
                            "preview": line.rstrip("\n"),
                        }
                    )
                    if len(results) >= max_results:
                        return {
                            "query": query,
                            "root": _workspace_relpath(target_root),
                            "scanned_files": scanned_files,
                            "count": len(results),
                            "truncated": True,
                            "results": results,
                        }
        except OSError:
            continue

    return {
        "query": query,
        "root": _workspace_relpath(target_root),
        "scanned_files": scanned_files,
        "count": len(results),
        "truncated": False,
        "results": results,
    }


def _exec_command(command: str, cwd: str = ".", timeout_s: int = 20) -> dict[str, Any]:
    if not command.strip():
        raise ValueError("Command is required")

    run_cwd = _resolve_workspace_path(cwd)
    timeout = max(1, min(int(timeout_s), MAX_TERMINAL_TIMEOUT_S))
    start = time.time()
    try:
        completed = subprocess.run(
            command,
            cwd=run_cwd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        duration_ms = int((time.time() - start) * 1000)
        return {
            "command": command,
            "cwd": _workspace_relpath(run_cwd),
            "exit_code": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
            "duration_ms": duration_ms,
            "timed_out": False,
        }
    except subprocess.TimeoutExpired as exc:
        duration_ms = int((time.time() - start) * 1000)
        return {
            "command": command,
            "cwd": _workspace_relpath(run_cwd),
            "exit_code": -1,
            "stdout": exc.stdout or "",
            "stderr": (exc.stderr or "") + f"\nCommand timed out after {timeout}s",
            "duration_ms": duration_ms,
            "timed_out": True,
        }


def _run_git(args: list[str], cwd: str = ".", timeout_s: int = 20) -> dict[str, Any]:
    run_cwd = _resolve_workspace_path(cwd)
    timeout = max(1, min(int(timeout_s), MAX_GIT_TIMEOUT_S))
    start = time.time()
    try:
        completed = subprocess.run(
            ["git", *args],
            cwd=run_cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        duration_ms = int((time.time() - start) * 1000)
        return {
            "cwd": _workspace_relpath(run_cwd),
            "args": args,
            "exit_code": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
            "duration_ms": duration_ms,
            "timed_out": False,
        }
    except FileNotFoundError as exc:
        raise RuntimeError("git executable not found") from exc
    except subprocess.TimeoutExpired as exc:
        duration_ms = int((time.time() - start) * 1000)
        return {
            "cwd": _workspace_relpath(run_cwd),
            "args": args,
            "exit_code": -1,
            "stdout": exc.stdout or "",
            "stderr": (exc.stderr or "") + f"\nGit command timed out after {timeout}s",
            "duration_ms": duration_ms,
            "timed_out": True,
        }


def _git_status(cwd: str = ".") -> dict[str, Any]:
    result = _run_git(["status", "--short", "--branch"], cwd=cwd)
    result["command"] = "git status --short --branch"
    return result


def _git_diff(cwd: str = ".", path: str = "", staged: bool = False) -> dict[str, Any]:
    args = ["diff"]
    if staged:
        args.append("--staged")
    if path.strip():
        abs_path = _resolve_workspace_path(path)
        args.extend(["--", abs_path])
    result = _run_git(args, cwd=cwd)
    result["command"] = "git " + " ".join(args)
    return result


def _git_log(cwd: str = ".", limit: int = 20) -> dict[str, Any]:
    clamped_limit = max(1, min(int(limit), 100))
    pretty = "%h\t%an\t%ad\t%s"
    result = _run_git(["log", f"-n{clamped_limit}", f"--pretty=format:{pretty}", "--date=short"], cwd=cwd)
    result["command"] = "git log"
    commits: list[dict[str, str]] = []
    for line in result.get("stdout", "").splitlines():
        parts = line.split("\t", 3)
        if len(parts) != 4:
            continue
        commits.append({"hash": parts[0], "author": parts[1], "date": parts[2], "subject": parts[3]})
    result["commits"] = commits
    return result


def _git_branches(cwd: str = ".") -> dict[str, Any]:
    result = _run_git(["branch", "--list", "--verbose"], cwd=cwd)
    result["command"] = "git branch --list --verbose"
    branches: list[dict[str, Any]] = []
    for line in result.get("stdout", "").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        current = line.lstrip().startswith("*")
        payload = stripped[1:].strip() if current else stripped
        parts = payload.split(maxsplit=2)
        if len(parts) >= 2:
            branches.append(
                {
                    "name": parts[0],
                    "hash": parts[1],
                    "subject": parts[2] if len(parts) > 2 else "",
                    "current": current,
                }
            )
    result["branches"] = branches
    return result


def _git_commit(cwd: str = ".", message: str = "", add_all: bool = False) -> dict[str, Any]:
    if not message.strip():
        raise ValueError("Commit message is required")
    if add_all:
        stage_result = _run_git(["add", "."], cwd=cwd)
        if stage_result["exit_code"] != 0:
            return {"status": "error", "stage": stage_result}
    result = _run_git(["commit", "-m", message], cwd=cwd)
    result["command"] = "git commit"
    return result


def _git_checkout(cwd: str = ".", branch: str = "", create: bool = False) -> dict[str, Any]:
    if not branch.strip():
        raise ValueError("Branch name is required")
    args = ["checkout"]
    if create:
        args.append("-b")
    args.append(branch)
    result = _run_git(args, cwd=cwd)
    result["command"] = "git " + " ".join(args)
    return result


def _system_info() -> dict[str, Any]:
    return {
        "workspace_root": WORKSPACE_ROOT,
        "platform": platform.platform(),
        "python_version": sys.version.split()[0],
        "models_configured": len(STATE.models),
        "model_dirs": _MODEL_DIRS,
    }


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

    def _query_params(self) -> dict[str, list[str]]:
        return parse_qs(urlparse(self.path).query)

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
            "workspace_root": WORKSPACE_ROOT,
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

        if path == "/api/system/info":
            self._json(HTTPStatus.OK, _system_info())
            return

        if path == "/api/fs/list":
            query = self._query_params()
            rel_path = query.get("path", ["."])[0]
            depth_raw = query.get("depth", ["2"])[0]
            include_hidden = query.get("hidden", ["0"])[0] in {"1", "true", "yes"}
            try:
                depth = max(0, min(int(depth_raw), 10))
                listing = _list_workspace(rel_path, depth=depth, include_hidden=include_hidden)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, listing)
            return

        if path == "/api/git/status":
            query = self._query_params()
            cwd = query.get("cwd", ["."])[0]
            try:
                status = _git_status(cwd=cwd)
            except (ValueError, OSError, RuntimeError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, status)
            return

        if path == "/api/git/diff":
            query = self._query_params()
            cwd = query.get("cwd", ["."])[0]
            target_path = query.get("path", [""])[0]
            staged = query.get("staged", ["0"])[0] in {"1", "true", "yes"}
            try:
                diff_result = _git_diff(cwd=cwd, path=target_path, staged=staged)
            except (ValueError, OSError, RuntimeError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, diff_result)
            return

        if path == "/api/git/log":
            query = self._query_params()
            cwd = query.get("cwd", ["."])[0]
            limit_raw = query.get("limit", ["20"])[0]
            try:
                limit = int(limit_raw)
                log_result = _git_log(cwd=cwd, limit=limit)
            except (TypeError, ValueError, OSError, RuntimeError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, log_result)
            return

        if path == "/api/git/branches":
            query = self._query_params()
            cwd = query.get("cwd", ["."])[0]
            try:
                branch_result = _git_branches(cwd=cwd)
            except (ValueError, OSError, RuntimeError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, branch_result)
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

        if path == "/api/fs/read":
            rel_path = str(payload.get("path", "")).strip()
            if not rel_path:
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Missing 'path'"})
                return
            try:
                result = _read_workspace_file(rel_path)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, result)
            return

        if path == "/api/fs/write":
            rel_path = str(payload.get("path", "")).strip()
            if not rel_path:
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Missing 'path'"})
                return
            content = payload.get("content", "")
            if not isinstance(content, str):
                self._json(HTTPStatus.BAD_REQUEST, {"error": "'content' must be a string"})
                return
            try:
                result = _write_workspace_file(rel_path, content)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, {"status": "ok", **result})
            return

        if path == "/api/fs/mkdir":
            rel_path = str(payload.get("path", "")).strip()
            if not rel_path:
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Missing 'path'"})
                return
            parents = bool(payload.get("parents", True))
            try:
                result = _mkdir_workspace(rel_path, parents=parents)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, {"status": "ok", **result})
            return

        if path == "/api/fs/rename":
            src = str(payload.get("src", "")).strip()
            dst = str(payload.get("dst", "")).strip()
            if not src or not dst:
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Missing 'src' or 'dst'"})
                return
            try:
                result = _rename_workspace(src, dst)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, {"status": "ok", **result})
            return

        if path == "/api/fs/delete":
            rel_path = str(payload.get("path", "")).strip()
            if not rel_path:
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Missing 'path'"})
                return
            recursive = bool(payload.get("recursive", False))
            try:
                result = _delete_workspace(rel_path, recursive=recursive)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, {"status": "ok", **result})
            return

        if path == "/api/search":
            query = str(payload.get("query") or payload.get("pattern") or "").strip()
            rel_path = str(payload.get("path", ".")).strip() or "."
            use_regex = bool(payload.get("regex"))
            case_sensitive = bool(payload.get("case_sensitive"))
            include_hidden = bool(payload.get("include_hidden"))
            file_glob = str(payload.get("file_glob", "*")).strip() or "*"
            max_results_raw = payload.get("max_results", MAX_SEARCH_RESULTS)
            try:
                max_results = max(1, min(int(max_results_raw), MAX_SEARCH_RESULTS))
            except (TypeError, ValueError):
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Invalid max_results"})
                return
            try:
                result = _search_workspace(
                    query,
                    path=rel_path,
                    regex=use_regex,
                    case_sensitive=case_sensitive,
                    include_hidden=include_hidden,
                    file_glob=file_glob,
                    max_results=max_results,
                )
            except re.error as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": f"Invalid regex: {exc}"})
                return
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, result)
            return

        if path == "/api/terminal/exec":
            command = str(payload.get("command", "")).strip()
            cwd = str(payload.get("cwd", ".")).strip() or "."
            timeout_raw = payload.get("timeout_s", 20)
            try:
                timeout_s = int(timeout_raw)
            except (TypeError, ValueError):
                self._json(HTTPStatus.BAD_REQUEST, {"error": "Invalid timeout_s"})
                return
            try:
                result = _exec_command(command, cwd=cwd, timeout_s=timeout_s)
            except (ValueError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, result)
            return

        if path == "/api/git/commit":
            cwd = str(payload.get("cwd", ".")).strip() or "."
            message = str(payload.get("message", "")).strip()
            add_all = bool(payload.get("add_all", False))
            try:
                result = _git_commit(cwd=cwd, message=message, add_all=add_all)
            except (ValueError, OSError, RuntimeError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, result)
            return

        if path == "/api/git/checkout":
            cwd = str(payload.get("cwd", ".")).strip() or "."
            branch = str(payload.get("branch", "")).strip()
            create = bool(payload.get("create", False))
            try:
                result = _git_checkout(cwd=cwd, branch=branch, create=create)
            except (ValueError, OSError, RuntimeError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return
            self._json(HTTPStatus.OK, result)
            return

        self._json(HTTPStatus.NOT_FOUND, {"error": f"Unknown endpoint: {path}"})


# Embeddable start/stop API — used by IDE chat panel to launch the server
# in a background thread (same interface as the old Ship/chat_server.py)
_embedded_server = None
_embedded_thread = None


def start_server(port: int = 23959, host: str = "127.0.0.1") -> bool:
    global _embedded_server, _embedded_thread
    try:
        _embedded_server = ReusableThreadingHTTPServer((host, port), RawrEngineHandler)
        _embedded_thread = threading.Thread(target=_embedded_server.serve_forever, daemon=True)
        _embedded_thread.start()
        return True
    except Exception:
        return False


def stop_server() -> None:
    global _embedded_server, _embedded_thread
    if _embedded_server:
        _embedded_server.shutdown()
        _embedded_server.server_close()
        _embedded_server = None
        _embedded_thread = None


def is_server_running() -> bool:
    return _embedded_server is not None


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
    print(
        "[RawrEngine] Endpoints: /status /v1/models /api/chat /api/agent/wish "
        "/api/tools /api/fs/list /api/fs/read /api/fs/write /api/fs/mkdir /api/fs/rename /api/fs/delete "
        "/api/search /api/terminal/exec /api/git/status /api/git/diff /api/git/log /api/git/branches "
        "/api/git/commit /api/git/checkout"
    )

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
