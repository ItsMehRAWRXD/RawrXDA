#!/usr/bin/env python3
"""
Mock Backend: HTTP API Server for Sovereign Inference IDE

Serves /status endpoint matching EngineService.ts expectations.
Simulates engine state transitions: IDLE -> LOADING -> READY -> FAULT

Usage:
    python mock_backend.py [--port 11435] [--auto-cycle]

Auto-cycle mode progresses through states every 2 seconds for testing.
"""

import json
import os
import time
import threading
import argparse
import sys
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import uuid
from pathlib import Path


# Global state machine
class EngineStateMachine:
    IDLE = 0
    LOADING = 1
    READY = 2
    FAULT = 3
    
    def __init__(self):
        self.state = self.IDLE
        self.agentic_paused = False
        self.session_id = str(uuid.uuid4())
        self.status_seq = 0
        self.startup_epoch_ms = int(time.time() * 1000)
        self.last_update_epoch_ms = self.startup_epoch_ms
        self.lock = threading.Lock()
        self.cycle_thread = None
        self.running = True
        self.process_id = 11435
        self.active_model = "default-7b"
        self.available_models = [
            {"id": "default-7b", "name": "Default 7B", "is_loaded": True, "vram_req_mb": 4096},
            {"id": "code-13b", "name": "Code 13B", "is_loaded": False, "vram_req_mb": 8192},
            {"id": "vision-opt-1b", "name": "Vision OPT 1B", "is_loaded": False, "vram_req_mb": 1024},
        ]
        self.fault_policy = {
            "session_id": self.session_id,
            "status_seq": self.status_seq,
            "fault_class": "HEARTBEAT_UNAVAILABLE",
            "suggested_action": "REINITIALIZE_ENGINE",
            "process_id": self.process_id,
            "active_model": self.active_model,
            "source": "ENGINE_REPORTED",
        }

    def get_models(self):
        with self.lock:
            return {
                "current": self.active_model,
                "available": [m["id"] for m in self.available_models],
            }

    def select_model(self, model_id):
        """Switch active model and trigger LOADING -> READY cycle."""
        with self.lock:
            known = [m["id"] for m in self.available_models]
            if model_id not in known:
                return False

            old = self.active_model
            self.active_model = model_id
            print(f"[Control] Model switch: {old} -> {model_id}")

        # Kick a background LOADING -> READY cycle so the UI observes state 1 -> 2.
        def _load_cycle():
            time.sleep(0.1)
            self.transition_to(self.LOADING)
            time.sleep(1.5)
            self.transition_to(self.READY)

        threading.Thread(target=_load_cycle, daemon=True).start()
        return True

    def restart(self):
        with self.lock:
            self.state = self.IDLE
            self.session_id = str(uuid.uuid4())
            self.status_seq += 1
            self.last_update_epoch_ms = int(time.time() * 1000)
            self.fault_policy = {
                "session_id": self.session_id,
                "status_seq": self.status_seq,
                "fault_class": "HEALTHY",
                "suggested_action": "NONE",
                "process_id": self.process_id,
                "active_model": self.active_model,
                "source": "ENGINE_REPORTED",
            }
            print(f"[Control] Engine restart simulated, new session={self.session_id}")
        
    def get_status(self):
        with self.lock:
            status = {
                "session_id": self.session_id,
                "status_seq": self.status_seq,
                "agentic_paused": self.agentic_paused,
                "loader_context": {
                    "state": self.state,
                    "suggested_action": "NONE" if self.state != self.FAULT else "RETRY_SAME",
                    "can_retry": True,
                    "retry_budget_rem": 5,
                    "terminal_fault": False,
                    "fault_class": "NONE" if self.state != self.FAULT else "TRANSIENT"
                },
                "last_error_tag": "NONE",
                "recommended_model": "default-7b"
            }
            return status

    def set_agentic_pause(self, paused):
        with self.lock:
            self.agentic_paused = bool(paused)
            self.status_seq += 1
            self.last_update_epoch_ms = int(time.time() * 1000)
            return {
                "status": "ok",
                "agentic_paused": self.agentic_paused,
                "status_seq": self.status_seq,
                "session_id": self.session_id,
            }

    def get_fault_policy(self):
        with self.lock:
            policy = dict(self.fault_policy)
            policy["session_id"] = self.session_id
            policy["status_seq"] = self.status_seq
            policy["active_model"] = self.active_model
            policy["source"] = "ENGINE_REPORTED"

            if self.state == self.FAULT:
                policy["fault_class"] = "TRANSIENT"
                policy["suggested_action"] = "RETRY_SAME"

            return policy
    
    def transition_to(self, new_state):
        with self.lock:
            old_state = self.state
            self.state = new_state
            self.status_seq += 1
            self.last_update_epoch_ms = int(time.time() * 1000)
            self.fault_policy = {
                "session_id": self.session_id,
                "status_seq": self.status_seq,
                "fault_class": "HEALTHY" if new_state != self.FAULT else "TRANSIENT",
                "suggested_action": "NONE" if new_state != self.FAULT else "RETRY_SAME",
                "process_id": self.process_id,
                "active_model": self.active_model,
                "source": "ENGINE_REPORTED",
            }
            state_names = {0: "IDLE", 1: "LOADING", 2: "READY", 3: "FAULT"}
            print(f"[Engine] State: {state_names.get(old_state, '?')} -> {state_names.get(new_state, '?')}")
    
    def auto_cycle(self, include_fault=False):
        """Cycle through states automatically for testing."""
        states = [self.IDLE, self.LOADING, self.READY]
        if include_fault:
            states.append(self.FAULT)
        current = 0
        while self.running:
            time.sleep(2)
            if self.running:
                self.transition_to(states[current % len(states)])
                current += 1


# HTTP Request Handler
class StatusHandler(BaseHTTPRequestHandler):
    engine = None  # Will be set by server
    workspace_root = None  # Set at startup

    def _safe_path(self, raw_path):
        raw = str(raw_path or ".")
        if Path(raw).is_absolute():
            raise ValueError("Absolute paths are not allowed")

        root = Path(self.workspace_root).resolve()
        rel = raw.replace("\\", "/").lstrip("/")
        if ".." in Path(rel).parts:
            raise ValueError("Path traversal is not allowed")

        candidate = (root / rel).resolve()
        try:
            candidate.relative_to(root)
        except ValueError:
            raise ValueError("Path escapes workspace root")
        return candidate

    def _send_json(self, code, payload):
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(json.dumps(payload, indent=2).encode("utf-8"))

    def _shutdown_process(self, delay_s=0.1):
        time.sleep(delay_s)
        os._exit(1)

    def _stream_inference(self, prompt):
        # Deterministic token stream to exercise IDE data plane behavior.
        # Test override: include [TOKENS=<N>] in prompt to stream exactly N tokens.
        token_target = None
        marker_start = prompt.find("[TOKENS=")
        if marker_start >= 0:
            marker_end = prompt.find("]", marker_start)
            if marker_end > marker_start:
                raw_value = prompt[marker_start + 8:marker_end]
                try:
                    parsed = int(raw_value)
                    if parsed > 0:
                        token_target = min(parsed, 50000)
                except ValueError:
                    token_target = None

        if token_target is not None:
            tokens = ["tok" for _ in range(token_target)]
        else:
            generated = (
                f"Sovereign stream accepted your prompt: {prompt}. "
                "Engine authority remains active. "
                "Streaming tokens now through the data plane."
            )
            tokens = generated.split(" ")
        started = time.time()

        delay_s = 0.08
        if token_target is not None and token_target >= 1000:
            delay_s = 0.0005

        try:
            for idx, token in enumerate(tokens, start=1):
                event = {
                    "type": "token",
                    "token": token + (" " if idx < len(tokens) else ""),
                    "token_count": idx,
                }
                self.wfile.write(f"data: {json.dumps(event)}\r\n\r\n".encode("utf-8"))
                self.wfile.flush()
                if delay_s > 0:
                    time.sleep(delay_s)

            elapsed_ms = max(1, int((time.time() - started) * 1000))
            done_event = {
                "type": "done",
                "token_count": len(tokens),
                "elapsed_ms": elapsed_ms,
                "tokens_per_sec": round((len(tokens) * 1000.0) / elapsed_ms, 2),
            }
            self.wfile.write(f"data: {json.dumps(done_event)}\r\n\r\n".encode("utf-8"))
            self.wfile.flush()
            print(f"[HTTP] inference stream -> {len(tokens)} tokens, {done_event['tokens_per_sec']} tps")
        except (BrokenPipeError, ConnectionResetError):
            print("[HTTP] Stream client disconnected")
    
    def do_GET(self):
        """Handle GET /status"""
        parsed_path = urlparse(self.path)
        
        if parsed_path.path == "/status":
            status = self.engine.get_status()
            response = json.dumps(status, indent=2)
            
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(response.encode())
            
            print(f"[HTTP] GET /status -> State {status['loader_context']['state']}")
        elif parsed_path.path == "/fault":
            policy = self.engine.get_fault_policy()

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps(policy, indent=2).encode("utf-8"))
            print(f"[HTTP] GET /fault -> {policy['fault_class']} / {policy['suggested_action']}")
        elif parsed_path.path == "/chaos/freeze":
            print("[Chaos] Freeze requested, terminating mock backend process")
            threading.Thread(target=self._shutdown_process, daemon=True).start()

            self.send_response(202)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps({
                "status": "accepted",
                "action": "process_terminating",
                "mode": "heartbeat_loss"
            }).encode("utf-8"))
        elif parsed_path.path == "/models":
            payload = self.engine.get_models()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps(payload, indent=2).encode("utf-8"))
            print(f"[HTTP] GET /models -> current={payload['current']}, count={len(payload['available'])}")        
        elif parsed_path.path == "/tool/list_dir":
            q = parse_qs(parsed_path.query)
            raw_path = q.get("path", ["."])[0]
            try:
                path = self._safe_path(raw_path)
                if not path.exists() or not path.is_dir():
                    self._send_json(404, {"error": "Directory not found", "path": raw_path})
                    return

                entries = []
                for child in sorted(path.iterdir(), key=lambda p: p.name.lower()):
                    entries.append({
                        "name": child.name,
                        "type": "dir" if child.is_dir() else "file",
                    })

                self._send_json(200, {
                    "path": raw_path,
                    "entries": entries,
                })
            except Exception as exc:
                self._send_json(400, {"error": str(exc), "path": raw_path})
            return
        elif parsed_path.path == "/tool/read_file":
            q = parse_qs(parsed_path.query)
            raw_path = q.get("path", ["."])[0]
            start_line = int(q.get("start", ["1"])[0])
            end_line = int(q.get("end", ["200"])[0])

            try:
                path = self._safe_path(raw_path)
                if not path.exists() or not path.is_file():
                    self._send_json(404, {"error": "File not found", "path": raw_path})
                    return

                start_line = max(1, start_line)
                end_line = max(start_line, min(end_line, start_line + 2000))

                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
                start_idx = start_line - 1
                end_idx = min(len(lines), end_line)
                sliced = lines[start_idx:end_idx]

                self._send_json(200, {
                    "path": raw_path,
                    "content": "\n".join(sliced),
                    "start_line": start_line,
                    "end_line": end_idx,
                    "total_lines": len(lines),
                })
            except Exception as exc:
                self._send_json(400, {"error": str(exc), "path": raw_path})
            return
        elif parsed_path.path == "/tool/search_code":
            q = parse_qs(parsed_path.query)
            query = (q.get("query", [""])[0] or "").strip()
            if not query:
                self._send_json(400, {"error": "query required"})
                return

            root = Path(self.workspace_root).resolve()
            matches = []
            max_matches = 200
            excluded_dirs = {".git", "node_modules", "dist", "build", "__pycache__"}
            allowed_suffix = {".ts", ".tsx", ".js", ".jsx", ".py", ".md", ".json", ".yml", ".yaml", ".css", ".cpp", ".h"}

            for path in root.rglob("*"):
                if len(matches) >= max_matches:
                    break
                if any(part in excluded_dirs for part in path.parts):
                    continue
                if not path.is_file():
                    continue
                if path.suffix.lower() not in allowed_suffix:
                    continue

                try:
                    rel = str(path.relative_to(root)).replace("\\", "/")
                    for idx, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
                        if query.lower() in line.lower():
                            matches.append({
                                "path": rel,
                                "line_number": idx,
                                "line": line.strip()[:400],
                            })
                            if len(matches) >= max_matches:
                                break
                except Exception:
                    continue

            self._send_json(200, {
                "query": query,
                "matches": matches,
                "truncated": len(matches) >= max_matches,
            })
            return
        elif parsed_path.path == "/system/logs":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps({
                "logs": [
                    f"session={self.engine.session_id}",
                    f"state={self.engine.state}",
                    f"status_seq={self.engine.status_seq}"
                ]
            }).encode("utf-8"))
        elif parsed_path.path == "/inference/sse":
            status = self.engine.get_status()
            query = parse_qs(parsed_path.query)
            prompt = query.get("prompt", ["No prompt provided."])[0]

            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()

            if status.get("agentic_paused"):
                error_event = {
                    "type": "error",
                    "message": "ENGINE_PAUSED_FOR_HITL"
                }
                self.wfile.write(f"data: {json.dumps(error_event)}\r\n\r\n".encode("utf-8"))
                self.wfile.flush()
                return

            if status["loader_context"]["state"] != EngineStateMachine.READY:
                error_event = {
                    "type": "error",
                    "message": f"ENGINE_NOT_READY state={status['loader_context']['state']}"
                }
                self.wfile.write(f"data: {json.dumps(error_event)}\r\n\r\n".encode("utf-8"))
                self.wfile.flush()
                return

            self._stream_inference(prompt)
        else:
            self.send_response(404)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Not Found")

    def do_POST(self):
        """Handle POST /inference/stream"""
        parsed_path = urlparse(self.path)

        if parsed_path.path == "/model/select":
            length = int(self.headers.get("Content-Length", 0))
            raw = self.rfile.read(length) if length > 0 else b"{}"
            try:
                body = json.loads(raw.decode("utf-8"))
                # Accept both 'model' (spec) and legacy 'model_id' key.
                model_id = str(body.get("model") or body.get("model_id") or "").strip()
            except Exception:
                model_id = ""

            if not model_id:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"error": "model required"}).encode())
                return

            if self.engine.get_status().get("agentic_paused"):
                self.send_response(423)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
                self.send_header("Access-Control-Allow-Headers", "Content-Type")
                self.end_headers()
                self.wfile.write(json.dumps({
                    "error": "ENGINE_PAUSED_FOR_HITL",
                    "action": "resume_or_resolve_pending_approval",
                }).encode())
                return

            ok = self.engine.select_model(model_id)
            code = 200 if ok else 404
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps({
                "status": "ok" if ok else "not_found",
                "current": self.engine.active_model,
            }).encode())
            print(f"[HTTP] POST /model/select {model_id} -> {'ok' if ok else 'not found'}")
            return

        if parsed_path.path == "/control/restart":
            self.engine.restart()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps({
                "status": "ok",
                "action": "engine_restart_simulated",
                "session_id": self.engine.session_id,
            }).encode("utf-8"))
            return

        if parsed_path.path == "/control/pause":
            payload = self.engine.set_agentic_pause(True)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps(payload).encode("utf-8"))
            return

        if parsed_path.path == "/control/resume":
            payload = self.engine.set_agentic_pause(False)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(json.dumps(payload).encode("utf-8"))
            return

        if parsed_path.path == "/tool/write_file":
            if self.engine.get_status().get("agentic_paused"):
                self.send_response(423)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
                self.send_header("Access-Control-Allow-Headers", "Content-Type")
                self.end_headers()
                self.wfile.write(json.dumps({
                    "error": "ENGINE_PAUSED_FOR_HITL",
                    "action": "resume_or_resolve_pending_approval",
                }).encode("utf-8"))
                return

            length = int(self.headers.get("Content-Length", 0))
            raw = self.rfile.read(length) if length > 0 else b"{}"
            try:
                body = json.loads(raw.decode("utf-8"))
            except Exception:
                self._send_json(400, {"error": "Invalid JSON body"})
                return

            write_path = str(body.get("path") or "").strip()
            if not write_path:
                self._send_json(400, {"error": "path is required"})
                return

            content = body.get("content", "")
            if not isinstance(content, str):
                self._send_json(400, {"error": "content must be a string"})
                return

            if len(content.encode("utf-8")) > 1_000_000:
                self._send_json(413, {"error": "content exceeds 1MB limit"})
                return

            append = bool(body.get("append", False))
            create_dirs = bool(body.get("create_dirs", True))

            try:
                target = self._safe_path(write_path)
                if target.exists() and target.is_dir():
                    self._send_json(400, {"error": "path points to a directory", "path": write_path})
                    return

                if create_dirs:
                    target.parent.mkdir(parents=True, exist_ok=True)

                if not target.parent.exists():
                    self._send_json(400, {"error": "parent directory does not exist", "path": write_path})
                    return

                mode = "a" if append else "w"
                with open(target, mode, encoding="utf-8", newline="") as handle:
                    written = handle.write(content)

                self._send_json(200, {
                    "path": write_path,
                    "bytes_written": len(content.encode("utf-8")),
                    "chars_written": written,
                    "appended": append,
                })
            except Exception as exc:
                self._send_json(400, {"error": str(exc), "path": write_path})
            return

        if parsed_path.path != "/inference/stream":
            self.send_response(404)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Not Found")
            return

        status = self.engine.get_status()
        if status.get("agentic_paused"):
            self.send_response(423)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({
                "error": "ENGINE_PAUSED_FOR_HITL",
            }).encode())
            return

        if status["loader_context"]["state"] != EngineStateMachine.READY:
            self.send_response(409)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({
                "error": "ENGINE_NOT_READY",
                "state": status["loader_context"]["state"]
            }).encode())
            return

        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length > 0 else b"{}"

        try:
            payload = json.loads(raw.decode("utf-8"))
            prompt = str(payload.get("prompt", "")).strip()
        except Exception:
            prompt = ""

        if not prompt:
            prompt = "No prompt provided."

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

        self._stream_inference(prompt)
    
    def do_OPTIONS(self):
        """Handle CORS preflight."""
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
    
    def log_message(self, format, *args):
        """Suppress default logging."""
        pass


def main():
    parser = argparse.ArgumentParser(description="Mock Backend HTTP API Server")
    parser.add_argument("--port", type=int, default=11435, help="HTTP server port")
    parser.add_argument("--auto-cycle", action="store_true", help="Auto-cycle through states")
    parser.add_argument("--include-fault", action="store_true", help="Include FAULT in auto-cycle")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address")
    args = parser.parse_args()
    
    # Create engine state machine
    engine = EngineStateMachine()
    StatusHandler.engine = engine
    StatusHandler.workspace_root = os.path.dirname(os.path.abspath(__file__))
    
    # Start auto-cycling thread if requested
    if args.auto_cycle:
        engine.cycle_thread = threading.Thread(target=engine.auto_cycle, kwargs={"include_fault": args.include_fault}, daemon=True)
        engine.cycle_thread.start()
        if args.include_fault:
            print("[Engine] Auto-cycling enabled (IDLE -> LOADING -> READY -> FAULT, repeat)")
        else:
            print("[Engine] Auto-cycling enabled (IDLE -> LOADING -> READY, repeat)")
    
    # Start HTTP server
    server = ThreadingHTTPServer((args.host, args.port), StatusHandler)
    print(f"[HTTP] Server listening on http://localhost:{args.port}")
    print(f"[HTTP] /status endpoint ready")
    print(f"[HTTP] Session ID: {engine.session_id}")
    print("[HTTP] Press Ctrl+C to stop")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[HTTP] Shutting down...")
        engine.running = False
        server.shutdown()
        sys.exit(0)


if __name__ == "__main__":
    main()
