#!/usr/bin/env python3
"""
RawrXD Cross-Platform Soak Test Runner
=======================================
Runs continuous health, API, and load tests against a RawrEngine instance.
Produces JSONL evidence logs suitable for compliance/audit.

Works on: Linux, macOS, Windows, Docker.
Zero dependencies — Python 3.8+ stdlib only.

Usage:
  python3 scripts/soak_test.py                          # 1-hour soak, 30s interval
  python3 scripts/soak_test.py --hours 24 --interval 60 # 24-hour soak, 60s interval
  python3 scripts/soak_test.py --iterations 100         # exactly 100 passes

Environment variables (all optional):
  RAWRXD_SOAK_URL       Base URL        (default: http://localhost:23959)
  RAWRXD_SOAK_HOURS     Duration hours  (default: 1)
  RAWRXD_SOAK_INTERVAL  Seconds between passes (default: 30)
  DOCKER                Set to 1 inside containers
"""

import argparse
import json
import os
import platform
import socket
import sys
import time
import urllib.request
import urllib.error
from datetime import datetime, timezone
from pathlib import Path

_config = {
    "base_url": os.environ.get("RAWRXD_SOAK_URL", "http://localhost:23959"),
}
LOG_DIR = Path(os.environ.get("RAWRXD_SOAK_LOGDIR", "data"))

# All endpoints the web interface relies on
ENDPOINTS = [
    ("GET",  "/status"),
    ("GET",  "/health"),
    ("GET",  "/v1/models"),
    ("GET",  "/api/tools"),
    ("POST", "/api/chat",           {"model": "rawrxd-local", "messages": [{"role": "user", "content": "ping"}], "stream": False}),
    ("POST", "/api/agentic/config", {"model": "rawrxd-local"}),
    ("POST", "/api/agent/wish",     {"wish": "list files", "mode": "plan"}),
    ("POST", "/api/agent/wish",     {"wish": "explain code", "mode": "full", "auto_execute": False}),
    ("POST", "/api/generate",       {"prompt": "hello", "model": "rawrxd-local"}),
]

CORS_ORIGINS = [
    "http://localhost:3000",
    "http://127.0.0.1:8080",
    "https://rawrxd.local",
]


def detect_platform():
    p = platform.system().lower()
    if os.environ.get("DOCKER") == "1":
        return f"docker-{p}"
    return p


def now_iso():
    return datetime.now(timezone.utc).isoformat()


def http_request(method, path, body=None, headers=None):
    url = f"{_config['base_url']}{path}"
    hdrs = {"Content-Type": "application/json", "Accept": "application/json"}
    if headers:
        hdrs.update(headers)
    data = json.dumps(body).encode("utf-8") if body else None
    req = urllib.request.Request(url, data=data, headers=hdrs, method=method)
    start = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            elapsed_ms = int((time.monotonic() - start) * 1000)
            resp_body = resp.read().decode("utf-8")
            return {
                "status": resp.status,
                "elapsed_ms": elapsed_ms,
                "body_bytes": len(resp_body),
                "ok": True,
                "error": None,
            }
    except urllib.error.HTTPError as e:
        elapsed_ms = int((time.monotonic() - start) * 1000)
        return {
            "status": e.code,
            "elapsed_ms": elapsed_ms,
            "body_bytes": 0,
            "ok": False,
            "error": f"HTTP {e.code}: {e.reason}",
        }
    except Exception as e:
        elapsed_ms = int((time.monotonic() - start) * 1000)
        return {
            "status": 0,
            "elapsed_ms": elapsed_ms,
            "body_bytes": 0,
            "ok": False,
            "error": str(e),
        }


def test_cors(origin):
    url = f"{_config['base_url']}/api/chat"
    req = urllib.request.Request(url, method="OPTIONS", headers={"Origin": origin})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            acao = resp.headers.get("Access-Control-Allow-Origin", "")
            return {"origin": origin, "allowed": bool(acao), "acao_header": acao, "ok": True}
    except Exception as e:
        return {"origin": origin, "allowed": False, "error": str(e), "ok": False}


def test_sse_streaming():
    url = f"{_config['base_url']}/api/chat"
    body = json.dumps({"model": "rawrxd-local", "messages": [{"role": "user", "content": "stream test"}], "stream": True}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"}, method="POST")
    start = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            chunks = 0
            got_done = False
            for line in resp:
                decoded = line.decode("utf-8").strip()
                if decoded.startswith("data: "):
                    payload = decoded[6:]
                    if payload == "[DONE]":
                        got_done = True
                    else:
                        chunks += 1
            elapsed_ms = int((time.monotonic() - start) * 1000)
            return {"ok": True, "chunks": chunks, "got_done": got_done, "elapsed_ms": elapsed_ms}
    except Exception as e:
        elapsed_ms = int((time.monotonic() - start) * 1000)
        return {"ok": False, "error": str(e), "elapsed_ms": elapsed_ms}


def run_pass(pass_num):
    results = {
        "ts": now_iso(),
        "pass": pass_num,
        "platform": detect_platform(),
        "hostname": socket.gethostname(),
        "python": platform.python_version(),
        "target": _config["base_url"],
        "endpoints": [],
        "cors": [],
        "sse": None,
    }

    # Test each endpoint
    for entry in ENDPOINTS:
        method = entry[0]
        path = entry[1]
        body = entry[2] if len(entry) > 2 else None
        r = http_request(method, path, body)
        r["method"] = method
        r["path"] = path
        results["endpoints"].append(r)

    # Test CORS
    for origin in CORS_ORIGINS:
        results["cors"].append(test_cors(origin))

    # Test SSE streaming
    results["sse"] = test_sse_streaming()

    # Summary
    total = len(results["endpoints"])
    passed = sum(1 for e in results["endpoints"] if e["ok"])
    cors_ok = sum(1 for c in results["cors"] if c["ok"])
    results["summary"] = {
        "endpoints_total": total,
        "endpoints_passed": passed,
        "endpoints_failed": total - passed,
        "cors_tested": len(results["cors"]),
        "cors_ok": cors_ok,
        "sse_ok": results["sse"]["ok"],
        "all_green": passed == total and results["sse"]["ok"],
    }

    return results


def main():
    parser = argparse.ArgumentParser(description="RawrXD Cross-Platform Soak Test")
    parser.add_argument("--hours", type=float, default=float(os.environ.get("RAWRXD_SOAK_HOURS", "1")))
    parser.add_argument("--interval", type=int, default=int(os.environ.get("RAWRXD_SOAK_INTERVAL", "30")))
    parser.add_argument("--iterations", type=int, default=0)
    parser.add_argument("--url", type=str, default=_config["base_url"])
    args = parser.parse_args()

    _config["base_url"] = args.url

    LOG_DIR.mkdir(parents=True, exist_ok=True)
    log_file = LOG_DIR / f"soak_evidence_{datetime.now().strftime('%Y%m%d_%H%M%S')}_{detect_platform()}.jsonl"

    print(f"[Soak] Platform:  {detect_platform()}")
    print(f"[Soak] Target:    {_config['base_url']}")
    print(f"[Soak] Duration:  {args.hours}h / {args.iterations} iterations")
    print(f"[Soak] Interval:  {args.interval}s")
    print(f"[Soak] Log:       {log_file}")
    print()

    end_time = time.time() + (args.hours * 3600) if args.iterations == 0 else None
    pass_num = 0

    with open(log_file, "w") as f:
        start_entry = {"ts": now_iso(), "event": "start", "platform": detect_platform(), "target": _config["base_url"], "config": {"hours": args.hours, "interval": args.interval, "iterations": args.iterations}}
        f.write(json.dumps(start_entry) + "\n")
        f.flush()

        try:
            while True:
                pass_num += 1
                results = run_pass(pass_num)
                f.write(json.dumps(results) + "\n")
                f.flush()

                s = results["summary"]
                status = "PASS" if s["all_green"] else "FAIL"
                print(f"[Soak] Pass {pass_num:4d}: {status}  endpoints={s['endpoints_passed']}/{s['endpoints_total']}  cors={s['cors_ok']}/{s['cors_tested']}  sse={'ok' if s['sse_ok'] else 'FAIL'}  ", end="")
                # Print any failures
                for ep in results["endpoints"]:
                    if not ep["ok"]:
                        print(f" [{ep['method']} {ep['path']}: {ep['error']}]", end="")
                print()

                if args.iterations > 0 and pass_num >= args.iterations:
                    break
                if end_time and time.time() >= end_time:
                    break

                time.sleep(args.interval)

        except KeyboardInterrupt:
            print("\n[Soak] Interrupted by user")

        end_entry = {"ts": now_iso(), "event": "end", "platform": detect_platform(), "passes": pass_num, "log_file": str(log_file)}
        f.write(json.dumps(end_entry) + "\n")

    print(f"\n[Soak] Complete. {pass_num} passes. Evidence: {log_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
