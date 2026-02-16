#!/usr/bin/env python3
"""
RawrXD Local CLI Client.

No dependencies. Talks to the local RawrEngine HTTP API.
"""

from __future__ import annotations

import argparse
import json
import shlex
import sys
import textwrap
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import Any


def _json_dumps(payload: dict[str, Any]) -> bytes:
    return json.dumps(payload).encode("utf-8")


@dataclass
class CliState:
    base_url: str
    api_key: str
    model: str
    mode: str = "ask"

    def headers(self, *, json_body: bool = False) -> dict[str, str]:
        output: dict[str, str] = {}
        if self.api_key:
            output["X-API-Key"] = self.api_key
        if json_body:
            output["Content-Type"] = "application/json"
        return output

    def request(self, method: str, path: str, payload: dict[str, Any] | None = None) -> dict[str, Any]:
        data = None if payload is None else _json_dumps(payload)
        req = urllib.request.Request(
            urllib.parse.urljoin(self.base_url.rstrip("/") + "/", path.lstrip("/")),
            data=data,
            headers=self.headers(json_body=payload is not None),
            method=method.upper(),
        )
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                raw = response.read().decode("utf-8")
                if not raw.strip():
                    return {}
                parsed = json.loads(raw)
                return parsed if isinstance(parsed, dict) else {"data": parsed}
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HTTP {exc.code}: {body}") from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(f"Connection failed: {exc}") from exc


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="RawrXD local CLI")
    parser.add_argument("--api-url", default="http://127.0.0.1:23959", help="RawrEngine base URL")
    parser.add_argument("--api-key", default="", help="Optional API key")
    parser.add_argument("--model", default="rawrxd-local", help="Initial model id")
    parser.add_argument("--command", default="", help="Single command to execute and exit")
    return parser


def print_help() -> None:
    print(
        textwrap.dedent(
            """
            Commands:
              /help
              /status
              /models
              /model <id>
              /mode <ask|plan|full>
              /chat <prompt>
              /plan <prompt>
              /full <prompt>
              /list [path]
              /read <path>
              /write <path> <content...>
              /search <query> [path]
              /exec <shell command...>
              /quit
            """
        ).strip()
    )


def print_json(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, indent=2, ensure_ascii=True))


def run_chat(state: CliState, prompt: str, mode: str) -> None:
    if mode == "ask":
        response = state.request(
            "POST",
            "/api/chat",
            {
                "model": state.model,
                "stream": False,
                "messages": [{"role": "user", "content": prompt}],
            },
        )
        try:
            message = response["choices"][0]["message"]["content"]
        except Exception:
            print_json(response)
            return
        print(message)
        return

    response = state.request(
        "POST",
        "/api/agent/wish",
        {
            "wish": prompt,
            "mode": mode,
            "model": state.model,
            "auto_execute": mode == "full",
        },
    )
    if mode == "plan" and isinstance(response.get("plan"), list):
        for index, step in enumerate(response["plan"], start=1):
            print(f"{index}. {step}")
        return
    if response.get("response"):
        print(response["response"])
        return
    print_json(response)


def execute_command(state: CliState, raw_line: str) -> bool:
    line = raw_line.strip()
    if not line:
        return True

    if not line.startswith("/"):
        run_chat(state, line, state.mode)
        return True

    try:
        tokens = shlex.split(line)
    except ValueError as exc:
        print(f"Parse error: {exc}")
        return True

    cmd = tokens[0][1:].lower()
    args = tokens[1:]

    if cmd in {"quit", "exit"}:
        return False
    if cmd in {"help", "h", "?"}:
        print_help()
        return True

    try:
        if cmd == "status":
            print_json(state.request("GET", "/status"))
        elif cmd == "models":
            response = state.request("GET", "/v1/models")
            models = response.get("models", [])
            for model in models:
                model_id = model.get("id") if isinstance(model, dict) else str(model)
                marker = "*" if model_id == state.model else " "
                print(f"{marker} {model_id}")
        elif cmd == "model":
            if not args:
                print("Usage: /model <id>")
                return True
            state.model = args[0]
            response = state.request("POST", "/api/agentic/config", {"model": state.model})
            print(f"Model set to: {response.get('model', state.model)}")
        elif cmd == "mode":
            if not args or args[0] not in {"ask", "plan", "full"}:
                print("Usage: /mode <ask|plan|full>")
                return True
            state.mode = args[0]
            print(f"Mode set to: {state.mode}")
        elif cmd in {"chat", "ask"}:
            if not args:
                print("Usage: /chat <prompt>")
                return True
            run_chat(state, " ".join(args), "ask")
        elif cmd == "plan":
            if not args:
                print("Usage: /plan <prompt>")
                return True
            run_chat(state, " ".join(args), "plan")
        elif cmd == "full":
            if not args:
                print("Usage: /full <prompt>")
                return True
            run_chat(state, " ".join(args), "full")
        elif cmd == "list":
            rel_path = args[0] if args else "."
            query = urllib.parse.urlencode({"path": rel_path, "depth": 2})
            response = state.request("GET", f"/api/fs/list?{query}")
            print(f"root: {response.get('root')} | count: {response.get('count')} | truncated: {response.get('truncated')}")
            for item in response.get("items", [])[:80]:
                item_type = "D" if item.get("type") == "dir" else "F"
                print(f"[{item_type}] {item.get('path')}")
        elif cmd == "read":
            if not args:
                print("Usage: /read <path>")
                return True
            response = state.request("POST", "/api/fs/read", {"path": args[0]})
            print(response.get("content", ""))
            if response.get("truncated"):
                print("\n[output truncated]")
        elif cmd == "write":
            if len(args) < 2:
                print("Usage: /write <path> <content...>")
                return True
            path = args[0]
            content = " ".join(args[1:])
            response = state.request("POST", "/api/fs/write", {"path": path, "content": content})
            print_json(response)
        elif cmd == "search":
            if not args:
                print("Usage: /search <query> [path]")
                return True
            query = args[0]
            rel_path = args[1] if len(args) > 1 else "."
            response = state.request("POST", "/api/search", {"query": query, "path": rel_path})
            print(f"results: {response.get('count')} | scanned_files: {response.get('scanned_files')}")
            for item in response.get("results", [])[:80]:
                print(f"{item.get('path')}:{item.get('line')}:{item.get('column')}  {item.get('preview')}")
        elif cmd == "exec":
            if not args:
                print("Usage: /exec <shell command...>")
                return True
            response = state.request("POST", "/api/terminal/exec", {"command": " ".join(args), "cwd": "."})
            print(f"exit={response.get('exit_code')} duration={response.get('duration_ms')}ms")
            if response.get("stdout"):
                print(response["stdout"])
            if response.get("stderr"):
                print(response["stderr"], file=sys.stderr)
        else:
            print(f"Unknown command: /{cmd}")
            print("Type /help")
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
    return True


def interactive_loop(state: CliState) -> int:
    print("RawrXD Local CLI connected. Type /help for commands.")
    while True:
        try:
            line = input(f"[{state.mode}|{state.model}]> ")
        except EOFError:
            print("")
            return 0
        except KeyboardInterrupt:
            print("")
            return 0
        if not execute_command(state, line):
            return 0


def main() -> int:
    args = build_parser().parse_args()
    state = CliState(
        base_url=args.api_url,
        api_key=args.api_key,
        model=args.model,
    )
    if args.command:
        execute_command(state, args.command)
        return 0
    return interactive_loop(state)


if __name__ == "__main__":
    raise SystemExit(main())
