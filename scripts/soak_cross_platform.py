#!/usr/bin/env python3
"""Cross-platform soak harness. Windows+Linux+macOS+Docker. Evidence log: JSON."""

import json
import os
import platform
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path


def _utc_now():
    return datetime.now(timezone.utc)

HOST = os.environ.get("RAWRXD_HOST", "http://127.0.0.1:23959")
DURATION_HOURS = float(os.environ.get("SOAK_HOURS", "0.05"))
ITERATIONS = int(os.environ.get("SOAK_ITERATIONS", "50"))
LOG_DIR = Path(os.environ.get("SOAK_LOG_DIR", "."))
LOG_DIR.mkdir(parents=True, exist_ok=True)


def detect_platform():
    if os.environ.get("DOCKER") == "1" or Path("/.dockerenv").exists():
        return "docker"
    try:
        if Path("/proc/1/cgroup").exists():
            if "docker" in Path("/proc/1/cgroup").read_text(errors="ignore"):
                return "docker"
    except Exception:
        pass
    p = platform.system().lower()
    return {"windows": "win", "darwin": "macos", "linux": "linux"}.get(p, p)


def http_get(url, timeout=5):
    req = urllib.request.Request(url, method="GET")
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            body = r.read().decode("utf-8", errors="replace")
            return r.status, body, (time.perf_counter() - start) * 1000
    except urllib.error.HTTPError as e:
        return e.code, str(e), (time.perf_counter() - start) * 1000
    except Exception as e:
        return 0, str(e), (time.perf_counter() - start) * 1000


def http_post(url, data, timeout=30):
    req = urllib.request.Request(
        url,
        data=json.dumps(data).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            body = r.read().decode("utf-8", errors="replace")
            return r.status, body, (time.perf_counter() - start) * 1000
    except urllib.error.HTTPError as e:
        return e.code, str(e), (time.perf_counter() - start) * 1000
    except Exception as e:
        return 0, str(e), (time.perf_counter() - start) * 1000


def run_soak():
    plat = detect_platform()
    ts = _utc_now().strftime("%Y%m%d_%H%M%SZ")
    log_path = LOG_DIR / f"soak_evidence_{plat}_{ts}.json"

    evidence = {
        "platform": plat,
        "host": HOST,
        "start": _utc_now().isoformat(),
        "duration_hours": DURATION_HOURS,
        "iterations": ITERATIONS,
        "status_oks": 0,
        "status_fails": 0,
        "models_oks": 0,
        "models_fails": 0,
        "chat_oks": 0,
        "chat_fails": 0,
        "latencies_ms": [],
        "errors": [],
    }

    deadline = time.perf_counter() + DURATION_HOURS * 3600 if DURATION_HOURS > 0 else None
    n = 0

    while True:
        n += 1
        if deadline and time.perf_counter() >= deadline:
            break
        if ITERATIONS > 0 and n > ITERATIONS:
            break

        s, b, lat = http_get(f"{HOST}/status")
        evidence["latencies_ms"].append(lat)
        if 200 <= s < 300:
            evidence["status_oks"] += 1
        else:
            evidence["status_fails"] += 1
            evidence["errors"].append({"iter": n, "endpoint": "status", "status": s, "body": b[:200]})

        s, b, lat = http_get(f"{HOST}/v1/models")
        if 200 <= s < 300:
            evidence["models_oks"] += 1
        else:
            evidence["models_fails"] += 1

        s, b, lat = http_post(
            f"{HOST}/api/chat",
            {"model": "rawrxd-local", "messages": [{"role": "user", "content": "ping"}], "stream": False},
            timeout=15,
        )
        if 200 <= s < 300:
            evidence["chat_oks"] += 1
        else:
            evidence["chat_fails"] += 1

        if n % 10 == 0:
            print(f"[{plat}] iter={n} status_ok={evidence['status_oks']} chat_ok={evidence['chat_oks']}")

    evidence["end"] = _utc_now().isoformat()
    evidence["total_iters"] = n
    evidence["pass"] = evidence["status_fails"] == 0 and (evidence["chat_oks"] > 0 or evidence["status_oks"] > 0)
    if evidence["latencies_ms"]:
        evidence["p95_ms"] = round(sorted(evidence["latencies_ms"])[int(len(evidence["latencies_ms"]) * 0.95)], 1)

    with open(log_path, "w") as f:
        json.dump(evidence, f, indent=2)

    print(f"Evidence log: {log_path}")
    print(f"PASS={evidence['pass']} status_ok={evidence['status_oks']} chat_ok={evidence['chat_oks']}")
    return 0 if evidence["pass"] else 1


if __name__ == "__main__":
    sys.exit(run_soak())
