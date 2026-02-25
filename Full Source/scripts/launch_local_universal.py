#!/usr/bin/env python3
"""
RawrXD local universal launcher.

Runs RawrEngine and the web interface together on localhost with no hosting.
"""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import webbrowser
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Launch RawrXD universal access locally")
    parser.add_argument("--backend-host", default="127.0.0.1", help="Backend bind host")
    parser.add_argument("--backend-port", type=int, default=23959, help="Backend bind port")
    parser.add_argument("--web-host", default="127.0.0.1", help="Web bind host")
    parser.add_argument("--web-port", type=int, default=8088, help="Web bind port")
    parser.add_argument("--backend-only", action="store_true", help="Start backend only")
    parser.add_argument("--web-only", action="store_true", help="Start web only")
    parser.add_argument("--cli", action="store_true", help="Launch local CLI after startup")
    parser.add_argument("--open-browser", action="store_true", help="Open browser after startup")
    parser.add_argument("--ready-timeout", type=float, default=20.0, help="Seconds to wait for services")
    return parser.parse_args()


def build_backend_command(args: argparse.Namespace) -> list[str]:
    backend = ROOT / "Ship" / "RawrEngine.py"
    if backend.exists():
        return [sys.executable, str(backend), "--host", args.backend_host, "--port", str(args.backend_port)]
    legacy = ROOT / "Ship" / "chat_server.py"
    if legacy.exists():
        return [sys.executable, str(legacy), "--port", str(args.backend_port)]
    raise FileNotFoundError("No backend entrypoint found (expected Ship/RawrEngine.py or Ship/chat_server.py)")


def build_web_command(args: argparse.Namespace) -> list[str]:
    web_dir = ROOT / "web_interface"
    if not web_dir.exists():
        raise FileNotFoundError("web_interface directory not found")
    return [
        sys.executable,
        "-m",
        "http.server",
        str(args.web_port),
        "--bind",
        args.web_host,
        "--directory",
        str(web_dir),
    ]


def build_cli_command(backend_url: str) -> list[str]:
    cli_path = ROOT / "Ship" / "rawrxd_cli_local.py"
    if not cli_path.exists():
        raise FileNotFoundError("CLI entrypoint not found at Ship/rawrxd_cli_local.py")
    return [sys.executable, str(cli_path), "--api-url", backend_url]


def wait_for_http(url: str, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=1.0) as response:
                if 200 <= response.status < 500:
                    return True
        except Exception:
            time.sleep(0.2)
    return False


def spawn(cmd: Sequence[str]) -> subprocess.Popen:
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    return subprocess.Popen(cmd, cwd=str(ROOT), env=env)


def terminate_process(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def main() -> int:
    args = parse_args()

    if args.backend_only and args.web_only:
        print("Cannot combine --backend-only and --web-only", file=sys.stderr)
        return 2

    backend_url = f"http://{args.backend_host}:{args.backend_port}"
    web_url = f"http://{args.web_host}:{args.web_port}"
    launch_url = f"{web_url}/?api={urllib.parse.quote(backend_url, safe=':/')}"

    processes: list[tuple[str, subprocess.Popen]] = []

    try:
        if not args.web_only:
            backend_cmd = build_backend_command(args)
            print(f"[local-launch] starting backend: {' '.join(backend_cmd)}")
            backend_proc = spawn(backend_cmd)
            processes.append(("backend", backend_proc))

            if not wait_for_http(f"{backend_url}/status", args.ready_timeout):
                raise RuntimeError(f"Backend did not become ready at {backend_url}/status")

        if not args.backend_only:
            web_cmd = build_web_command(args)
            print(f"[local-launch] starting web: {' '.join(web_cmd)}")
            web_proc = spawn(web_cmd)
            processes.append(("web", web_proc))

            if not wait_for_http(web_url, args.ready_timeout):
                raise RuntimeError(f"Web server did not become ready at {web_url}")

        if not args.web_only:
            print(f"[local-launch] backend ready: {backend_url}")
        if not args.backend_only:
            print(f"[local-launch] web ready: {launch_url}")
            print("[local-launch] tip: URL includes ?api=... override for clean local startup.")

        if args.open_browser and not args.backend_only:
            webbrowser.open(launch_url, new=2)

        if args.cli and not args.web_only:
            cli_cmd = build_cli_command(backend_url)
            print(f"[local-launch] opening CLI: {' '.join(cli_cmd)}")
            subprocess.run(cli_cmd, cwd=str(ROOT), check=False)
            return 0

        print("[local-launch] press Ctrl+C to stop")
        while True:
            for name, proc in processes:
                rc = proc.poll()
                if rc is not None:
                    raise RuntimeError(f"{name} process exited unexpectedly with code {rc}")
            time.sleep(0.8)
    except KeyboardInterrupt:
        print("\n[local-launch] shutdown requested")
    except Exception as exc:
        print(f"[local-launch] error: {exc}", file=sys.stderr)
        return_code = 1
    else:
        return_code = 0
    finally:
        for _name, proc in reversed(processes):
            terminate_process(proc)

    return return_code


if __name__ == "__main__":
    if os.name == "nt":
        signal.signal(signal.SIGINT, signal.default_int_handler)
    raise SystemExit(main())
