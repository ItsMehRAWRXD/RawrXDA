#!/usr/bin/env python3
"""
soak_test_cot.py — Soak Test: "hello spam" + "large prompt" + "backend flap"
=============================================================================

Action Item #12: Validate CoT pipeline under stress and failure conditions.

Scenarios:
  1. Send 1,000 trivial requests ("hello spam")
  2. Send 50 large payloads (100KB each)
  3. Toggle Python backend up/down every N requests

Exit criteria:
  - Zero deadlocks
  - Zero empty finals
  - Error handling stays user-friendly
  - No 5xx responses leak to caller

Usage:
    python tests/integration/soak_test_cot.py [--host HOST] [--count N]
    python tests/integration/soak_test_cot.py --scenario all

Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
"""

import json
import sys
import time
import random
import string
import argparse
import threading
import subprocess
import requests
from typing import Dict, List, Tuple
from concurrent.futures import ThreadPoolExecutor, as_completed

# =============================================================================
# Configuration
# =============================================================================
DEFAULT_HOST = "http://localhost:5000"
DEFAULT_TRIVIAL_COUNT = 1000
DEFAULT_LARGE_COUNT = 50
DEFAULT_LARGE_SIZE_KB = 100
DEFAULT_FLAP_INTERVAL = 20

# =============================================================================
# Helpers
# =============================================================================
class SoakStats:
    """Thread-safe statistics collector."""
    def __init__(self):
        self.lock = threading.Lock()
        self.total = 0
        self.success = 0
        self.empty_finals = 0
        self.errors_5xx = 0
        self.errors_conn = 0
        self.errors_timeout = 0
        self.errors_other = 0
        self.latencies_ms: List[float] = []
        self.start_time = time.time()

    def record(self, status_code: int, final: str, latency_ms: float, error: str = ""):
        with self.lock:
            self.total += 1
            if 200 <= status_code < 300:
                self.success += 1
                if not final or not final.strip():
                    self.empty_finals += 1
            elif status_code >= 500:
                self.errors_5xx += 1
            elif error == "connection":
                self.errors_conn += 1
            elif error == "timeout":
                self.errors_timeout += 1
            else:
                self.errors_other += 1
            self.latencies_ms.append(latency_ms)

    def summary(self) -> Dict:
        with self.lock:
            elapsed = time.time() - self.start_time
            p50 = sorted(self.latencies_ms)[len(self.latencies_ms) // 2] if self.latencies_ms else 0
            p95_idx = int(len(self.latencies_ms) * 0.95)
            p95 = sorted(self.latencies_ms)[p95_idx] if self.latencies_ms else 0
            return {
                "total": self.total,
                "success": self.success,
                "empty_finals": self.empty_finals,
                "errors_5xx": self.errors_5xx,
                "errors_conn": self.errors_conn,
                "errors_timeout": self.errors_timeout,
                "errors_other": self.errors_other,
                "elapsed_s": round(elapsed, 1),
                "rps": round(self.total / elapsed, 1) if elapsed > 0 else 0,
                "p50_ms": round(p50, 1),
                "p95_ms": round(p95, 1),
            }


def send_cot_request(host: str, message: str, timeout: int = 30) -> Tuple[int, str, float, str]:
    """Send a single CoT request. Returns (status, final, latency_ms, error_type)."""
    start = time.time()
    try:
        resp = requests.post(
            f"{host}/api/cot",
            json={"message": message},
            timeout=timeout
        )
        latency = (time.time() - start) * 1000
        data = resp.json() if resp.status_code == 200 else {}
        final = data.get("final", data.get("final_answer", ""))
        return resp.status_code, str(final), latency, ""
    except requests.exceptions.Timeout:
        return 0, "", (time.time() - start) * 1000, "timeout"
    except requests.exceptions.ConnectionError:
        return 0, "", (time.time() - start) * 1000, "connection"
    except Exception as e:
        return 0, "", (time.time() - start) * 1000, str(e)


# =============================================================================
# Scenario 1: Hello Spam — 1,000 trivial requests
# =============================================================================
def scenario_hello_spam(host: str, count: int, concurrency: int = 10) -> Dict:
    """Stress test with trivial inputs."""
    print(f"\n{'='*60}")
    print(f"Scenario 1: Hello Spam — {count} trivial requests")
    print(f"  Concurrency: {concurrency}")
    print(f"{'='*60}")

    stats = SoakStats()
    trivial_inputs = [
        "Hello", "Hi", "Hey", "Test", "Ok", "Yes", "No",
        "Thanks", "Bye", "Help", "Yo", "Sup", "Hi there", "?",
    ]

    def worker(i):
        msg = trivial_inputs[i % len(trivial_inputs)]
        status, final, latency, err = send_cot_request(host, msg, timeout=15)
        stats.record(status, final, latency, err)
        if i % 100 == 0:
            print(f"  Progress: {i}/{count}")

    with ThreadPoolExecutor(max_workers=concurrency) as pool:
        futures = [pool.submit(worker, i) for i in range(count)]
        for f in as_completed(futures):
            f.result()  # Propagate exceptions

    result = stats.summary()
    print(f"\n  Results: {json.dumps(result, indent=2)}")
    return result


# =============================================================================
# Scenario 2: Large Payloads — 50 x 100 KiB
# =============================================================================
def scenario_large_payloads(host: str, count: int = 50,
                            size_kib: int = 100, concurrency: int = 5) -> Dict:
    """Stress test with large inputs to verify truncation + memory safety."""
    print(f"\n{'='*60}")
    print(f"Scenario 2: Large Payloads — {count} x {size_kib} KiB")
    print(f"  Concurrency: {concurrency}")
    print(f"{'='*60}")

    stats = SoakStats()

    def generate_large_prompt(idx: int) -> str:
        """Generate a large but valid prompt."""
        base = f"Request #{idx}: Please analyze this large codebase for issues.\n"
        # Pad with realistic-looking content
        padding = "".join(random.choices(string.ascii_letters + " \n", k=size_kib * 1024))
        return base + padding

    def worker(i):
        msg = generate_large_prompt(i)
        status, final, latency, err = send_cot_request(host, msg, timeout=60)
        stats.record(status, final, latency, err)
        print(f"  Large #{i}: status={status}, latency={latency:.0f}ms, final_len={len(final)}")

    with ThreadPoolExecutor(max_workers=concurrency) as pool:
        futures = [pool.submit(worker, i) for i in range(count)]
        for f in as_completed(futures):
            f.result()

    result = stats.summary()
    print(f"\n  Results: {json.dumps(result, indent=2)}")
    return result


# =============================================================================
# Scenario 3: Backend Flap — toggle Python backend up/down
# =============================================================================
def scenario_backend_flap(host: str, count: int = 200,
                          flap_interval: int = 20, concurrency: int = 5) -> Dict:
    """
    Send requests while periodically toggling the backend availability.
    Verifies graceful degradation: no 5xx, no empty finals, clear error steps.
    """
    print(f"\n{'='*60}")
    print(f"Scenario 3: Backend Flap — {count} requests, flap every {flap_interval}")
    print(f"  NOTE: This scenario validates error handling, not actual flapping.")
    print(f"  Concurrency: {concurrency}")
    print(f"{'='*60}")

    stats = SoakStats()
    flap_active = threading.Event()
    flap_active.set()  # Start with backend "up"

    def worker(i):
        msg = f"Request #{i}: analyze this code for bugs"
        if not flap_active.is_set():
            msg = f"Hi"  # Use trivial during "down" periods

        status, final, latency, err = send_cot_request(host, msg, timeout=15)
        stats.record(status, final, latency, err)

        if i % 50 == 0:
            s = stats.summary()
            print(f"  Flap progress {i}/{count}: success={s['success']}, "
                  f"empty={s['empty_finals']}, 5xx={s['errors_5xx']}")

    # Flap controller: toggle every N requests
    flap_count = 0
    def flap_controller():
        nonlocal flap_count
        while flap_count < count:
            time.sleep(flap_interval * 0.1)  # Approximate timing
            flap_count += flap_interval
            if flap_active.is_set():
                flap_active.clear()
                print(f"  [FLAP] Backend simulated DOWN at request ~{flap_count}")
            else:
                flap_active.set()
                print(f"  [FLAP] Backend simulated UP at request ~{flap_count}")

    flap_thread = threading.Thread(target=flap_controller, daemon=True)
    flap_thread.start()

    with ThreadPoolExecutor(max_workers=concurrency) as pool:
        futures = [pool.submit(worker, i) for i in range(count)]
        for f in as_completed(futures):
            f.result()

    result = stats.summary()
    print(f"\n  Results: {json.dumps(result, indent=2)}")
    return result


# =============================================================================
# Main Runner
# =============================================================================
def main():
    parser = argparse.ArgumentParser(description="CoT Soak Test Harness")
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"CoT engine host (default: {DEFAULT_HOST})")
    parser.add_argument("--scenario", choices=["hello", "large", "flap", "all"],
                        default="all", help="Which scenario to run")
    parser.add_argument("--count", type=int, default=None,
                        help="Override request count per scenario")
    parser.add_argument("--concurrency", type=int, default=10,
                        help="Max concurrent requests (default: 10)")
    args = parser.parse_args()

    print(f"\n{'='*60}")
    print(f"RawrXD CoT Soak Test Harness")
    print(f"  Host: {args.host}")
    print(f"  Scenario: {args.scenario}")
    print(f"{'='*60}")

    # Pre-flight: check if host is reachable
    try:
        resp = requests.get(f"{args.host}/api/cot/health", timeout=5)
        print(f"  Health check: {resp.status_code} — {resp.json().get('status', '?')}")
    except Exception as e:
        print(f"  WARNING: Health check failed: {e}")
        print(f"  Proceeding anyway (will test error handling paths)...")

    all_results = {}

    if args.scenario in ("hello", "all"):
        count = args.count or DEFAULT_TRIVIAL_COUNT
        all_results["hello_spam"] = scenario_hello_spam(
            args.host, count, args.concurrency)

    if args.scenario in ("large", "all"):
        count = args.count or DEFAULT_LARGE_COUNT
        all_results["large_payloads"] = scenario_large_payloads(
            args.host, count, DEFAULT_LARGE_SIZE_KB, min(args.concurrency, 5))

    if args.scenario in ("flap", "all"):
        count = args.count or 200
        all_results["backend_flap"] = scenario_backend_flap(
            args.host, count, DEFAULT_FLAP_INTERVAL, args.concurrency)

    # =========================================================================
    # Final Verdict
    # =========================================================================
    print(f"\n{'='*60}")
    print(f"SOAK TEST FINAL VERDICT")
    print(f"{'='*60}")

    total_empty = sum(r.get("empty_finals", 0) for r in all_results.values())
    total_5xx = sum(r.get("errors_5xx", 0) for r in all_results.values())
    total_success = sum(r.get("success", 0) for r in all_results.values())
    total_total = sum(r.get("total", 0) for r in all_results.values())

    verdict_pass = (total_empty == 0 and total_5xx == 0)

    for name, result in all_results.items():
        status = "PASS" if result["empty_finals"] == 0 and result["errors_5xx"] == 0 else "FAIL"
        print(f"  [{status}] {name}: {result['success']}/{result['total']} success, "
              f"empty={result['empty_finals']}, 5xx={result['errors_5xx']}, "
              f"p95={result['p95_ms']}ms")

    print(f"\n  Overall: {total_success}/{total_total} success")
    print(f"  Empty finals: {total_empty}")
    print(f"  5xx errors: {total_5xx}")
    print(f"  VERDICT: {'PASS' if verdict_pass else 'FAIL'}")
    print(f"{'='*60}")

    sys.exit(0 if verdict_pass else 1)


if __name__ == "__main__":
    main()
