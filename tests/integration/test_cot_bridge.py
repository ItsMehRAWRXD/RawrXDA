#!/usr/bin/env python3
"""
test_cot_bridge.py — Contract-test the CoT HTTP bridge end-to-end
=================================================================

Action Item #1: Golden integration test for the full CoT pipeline:
    /api/cot → WinHTTP proxy → Flask → synthesize()

Exit criteria:
    - 3 requests return 200
    - JSON validates against schema v1
    - The "final" field is NEVER empty

Endpoints tested:
    1. GET  /api/cot/health
    2. GET  /api/cot/metrics
    3. POST /api/cot  (trivial + non-trivial payloads)

Usage:
    python tests/integration/test_cot_bridge.py [--host HOST] [--port PORT]
    python -m pytest tests/integration/test_cot_bridge.py -v

Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
"""

import json
import sys
import time
import argparse
import unittest
import requests
from typing import Dict, Any, Optional

# =============================================================================
# Configuration
# =============================================================================
DEFAULT_COT_HOST = "http://localhost:5000"   # Direct Python Flask
DEFAULT_BRIDGE_HOST = "http://localhost:11434"  # C++ API server (WinHTTP proxy)

# Schema version we enforce (Action Item #2)
REQUIRED_SCHEMA_VERSION = "1"

# Required top-level keys in a CoT response
REQUIRED_RESPONSE_KEYS = {"final", "steps", "meta", "schemaVersion"}
REQUIRED_META_KEYS = {"latencyMs", "route", "preset", "depth"}
REQUIRED_STEP_KEYS = {"role", "content", "latency_ms"}


def validate_cot_response(data: Dict[str, Any], context: str = "") -> list:
    """
    Validate a CoT response against schema v1.
    Returns a list of error strings (empty = valid).
    """
    errors = []
    prefix = f"[{context}] " if context else ""

    # Check top-level keys
    for key in REQUIRED_RESPONSE_KEYS:
        if key not in data:
            errors.append(f"{prefix}Missing required key: '{key}'")

    # schemaVersion must match
    if data.get("schemaVersion") != REQUIRED_SCHEMA_VERSION:
        errors.append(f"{prefix}schemaVersion mismatch: expected '{REQUIRED_SCHEMA_VERSION}', "
                      f"got '{data.get('schemaVersion')}'")

    # final must be non-empty
    final = data.get("final", "")
    if not final or not str(final).strip():
        errors.append(f"{prefix}'final' field is empty or missing")

    # steps must be a list
    steps = data.get("steps", [])
    if not isinstance(steps, list):
        errors.append(f"{prefix}'steps' must be an array, got {type(steps).__name__}")
    else:
        for i, step in enumerate(steps):
            if not isinstance(step, dict):
                errors.append(f"{prefix}steps[{i}] must be an object")
                continue
            for k in REQUIRED_STEP_KEYS:
                if k not in step:
                    errors.append(f"{prefix}steps[{i}] missing key: '{k}'")

    # meta must have required keys
    meta = data.get("meta", {})
    if not isinstance(meta, dict):
        errors.append(f"{prefix}'meta' must be an object")
    else:
        for k in REQUIRED_META_KEYS:
            if k not in meta:
                errors.append(f"{prefix}meta missing key: '{k}'")

    return errors


class CotBridgeContractTest(unittest.TestCase):
    """
    Golden integration test suite for the CoT HTTP bridge.
    Tests direct Flask endpoints and (optionally) the C++ WinHTTP proxy.
    """

    host: str = DEFAULT_COT_HOST
    bridge_host: Optional[str] = None
    timeout: int = 30

    @classmethod
    def setUpClass(cls):
        """Parse command-line args for host configuration."""
        # Allow override via environment or command line
        import os
        cls.host = os.environ.get("COT_HOST", DEFAULT_COT_HOST)
        cls.bridge_host = os.environ.get("COT_BRIDGE_HOST", None)
        cls.timeout = int(os.environ.get("COT_TIMEOUT", "30"))

    # =========================================================================
    # Test 1: Health Endpoint
    # =========================================================================
    def test_01_health_endpoint(self):
        """GET /api/cot/health must return 200 with status field."""
        url = f"{self.host}/api/cot/health"
        resp = requests.get(url, timeout=self.timeout)

        self.assertEqual(resp.status_code, 200,
                         f"Health endpoint returned {resp.status_code}: {resp.text}")

        data = resp.json()
        self.assertIn("status", data, "Health response missing 'status' key")
        self.assertIn(data["status"], ("ok", "degraded"),
                      f"Unexpected health status: {data['status']}")
        self.assertIn("engine", data, "Health response missing 'engine' key")

        print(f"  [PASS] /api/cot/health → status={data['status']}, "
              f"engine={data.get('engine')}")

    # =========================================================================
    # Test 2: Metrics Endpoint
    # =========================================================================
    def test_02_metrics_endpoint(self):
        """GET /api/cot/metrics must return 200 with counter fields."""
        url = f"{self.host}/api/cot/metrics"
        resp = requests.get(url, timeout=self.timeout)

        self.assertEqual(resp.status_code, 200,
                         f"Metrics endpoint returned {resp.status_code}: {resp.text}")

        data = resp.json()
        expected_keys = ["total_requests", "trivial_bypassed", "full_chain_runs"]
        for key in expected_keys:
            self.assertIn(key, data, f"Metrics missing key: '{key}'")

        print(f"  [PASS] /api/cot/metrics → "
              f"total={data.get('total_requests')}, "
              f"trivial={data.get('trivial_bypassed')}, "
              f"full={data.get('full_chain_runs')}")

    # =========================================================================
    # Test 3: CoT with Trivial Payload
    # =========================================================================
    def test_03_cot_trivial_payload(self):
        """POST /api/cot with trivial input → 200, valid schema, non-empty final."""
        url = f"{self.host}/api/cot"
        payload = {"message": "Hello"}

        resp = requests.post(url, json=payload, timeout=self.timeout)
        self.assertEqual(resp.status_code, 200,
                         f"CoT (trivial) returned {resp.status_code}: {resp.text}")

        data = resp.json()

        # Validate against schema v1 (if upgraded) or legacy format
        if "schemaVersion" in data:
            errors = validate_cot_response(data, context="trivial")
            self.assertEqual(errors, [], f"Schema validation errors: {errors}")
        else:
            # Legacy format: check final_answer + steps
            final = data.get("final_answer", data.get("final", ""))
            self.assertTrue(final and str(final).strip(),
                            "Trivial CoT returned empty final_answer")
            self.assertIn("steps", data, "Missing 'steps' in response")
            self.assertIsInstance(data["steps"], list, "'steps' is not a list")

        final_val = data.get("final", data.get("final_answer", ""))
        print(f"  [PASS] /api/cot (trivial 'Hello') → "
              f"final_len={len(str(final_val))}, "
              f"steps={len(data.get('steps', []))}, "
              f"trivial={data.get('trivial', data.get('meta', {}).get('trivial', '?'))}")

    # =========================================================================
    # Test 4: CoT with Non-Trivial Payload
    # =========================================================================
    def test_04_cot_nontrivial_payload(self):
        """POST /api/cot with complex input → 200, valid schema, non-empty final."""
        url = f"{self.host}/api/cot"
        payload = {
            "message": "Review this code for buffer overflow vulnerabilities: "
                       "void process(char* input) { char buf[64]; strcpy(buf, input); }"
        }

        resp = requests.post(url, json=payload, timeout=self.timeout)
        self.assertEqual(resp.status_code, 200,
                         f"CoT (non-trivial) returned {resp.status_code}: {resp.text}")

        data = resp.json()

        # Validate against schema v1 (if upgraded) or legacy format
        if "schemaVersion" in data:
            errors = validate_cot_response(data, context="non-trivial")
            self.assertEqual(errors, [], f"Schema validation errors: {errors}")
        else:
            final = data.get("final_answer", data.get("final", ""))
            self.assertTrue(final and str(final).strip(),
                            "Non-trivial CoT returned empty final_answer")
            self.assertIn("steps", data, "Missing 'steps' in response")

        final_val = data.get("final", data.get("final_answer", ""))
        print(f"  [PASS] /api/cot (non-trivial) → "
              f"final_len={len(str(final_val))}, "
              f"steps={len(data.get('steps', []))}")

    # =========================================================================
    # Test 5: Empty message rejection
    # =========================================================================
    def test_05_cot_empty_message_rejected(self):
        """POST /api/cot with empty message → 400."""
        url = f"{self.host}/api/cot"
        payload = {"message": ""}

        resp = requests.post(url, json=payload, timeout=self.timeout)
        self.assertEqual(resp.status_code, 400,
                         f"Empty message should return 400, got {resp.status_code}")

        data = resp.json()
        self.assertIn("error", data, "Error response missing 'error' key")

        print(f"  [PASS] /api/cot (empty) → 400, error='{data.get('error')}'")

    # =========================================================================
    # Test 6: Final field is NEVER empty (stress variant)
    # =========================================================================
    def test_06_final_never_empty(self):
        """Multiple payloads: verify 'final' is never empty."""
        url = f"{self.host}/api/cot"
        test_messages = [
            "hi",
            "What is 2+2?",
            "Explain the difference between a mutex and a semaphore",
        ]

        for msg in test_messages:
            resp = requests.post(url, json={"message": msg}, timeout=self.timeout)
            self.assertEqual(resp.status_code, 200, f"Failed for message: '{msg}'")

            data = resp.json()
            final = data.get("final", data.get("final_answer", ""))
            self.assertTrue(
                final and str(final).strip(),
                f"EMPTY FINAL for message '{msg}': response={json.dumps(data, indent=2)[:200]}"
            )

        print(f"  [PASS] final-never-empty check passed for {len(test_messages)} messages")


# =============================================================================
# Bridge Tests (via C++ WinHTTP proxy)
# =============================================================================
class CotBridgeProxyTest(unittest.TestCase):
    """
    Tests the C++ WinHTTP proxy path: C++ API Server → Python Flask.
    Only runs if COT_BRIDGE_HOST is set.
    """

    bridge_host: Optional[str] = None
    timeout: int = 30

    @classmethod
    def setUpClass(cls):
        import os
        cls.bridge_host = os.environ.get("COT_BRIDGE_HOST", DEFAULT_BRIDGE_HOST)
        cls.timeout = int(os.environ.get("COT_TIMEOUT", "30"))

        # Quick check if bridge is reachable
        try:
            requests.get(f"{cls.bridge_host}/api/cot/health", timeout=3)
        except requests.exceptions.ConnectionError:
            raise unittest.SkipTest(
                f"C++ bridge not reachable at {cls.bridge_host}")

    def test_01_bridge_health(self):
        """GET /api/cot/health via C++ bridge."""
        url = f"{self.bridge_host}/api/cot/health"
        resp = requests.get(url, timeout=self.timeout)
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertIn("status", data)
        print(f"  [PASS] Bridge /api/cot/health → {data['status']}")

    def test_02_bridge_cot_trivial(self):
        """POST /api/cot via C++ bridge with trivial input."""
        url = f"{self.bridge_host}/api/cot"
        resp = requests.post(url, json={"message": "Hello"}, timeout=self.timeout)
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        final = data.get("final", data.get("final_answer", ""))
        self.assertTrue(final and str(final).strip(), "Empty final via bridge")
        print(f"  [PASS] Bridge /api/cot (trivial) → final_len={len(str(final))}")

    def test_03_bridge_cot_nontrivial(self):
        """POST /api/cot via C++ bridge with complex input."""
        url = f"{self.bridge_host}/api/cot"
        payload = {"message": "Analyze the security implications of using eval() in JavaScript"}
        resp = requests.post(url, json=payload, timeout=self.timeout)
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        final = data.get("final", data.get("final_answer", ""))
        self.assertTrue(final and str(final).strip(), "Empty final via bridge")
        print(f"  [PASS] Bridge /api/cot (non-trivial) → final_len={len(str(final))}")


# =============================================================================
# CLI Runner
# =============================================================================
def main():
    parser = argparse.ArgumentParser(description="CoT Bridge Integration Tests")
    parser.add_argument("--host", default=DEFAULT_COT_HOST,
                        help="Python Flask host (default: %(default)s)")
    parser.add_argument("--bridge-host", default=None,
                        help="C++ bridge host (default: skip bridge tests)")
    parser.add_argument("--timeout", type=int, default=30,
                        help="Request timeout in seconds (default: %(default)s)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output")
    args = parser.parse_args()

    import os
    os.environ["COT_HOST"] = args.host
    os.environ["COT_TIMEOUT"] = str(args.timeout)
    if args.bridge_host:
        os.environ["COT_BRIDGE_HOST"] = args.bridge_host

    print(f"\n{'='*60}")
    print(f"CoT Bridge Contract Tests")
    print(f"  Flask:  {args.host}")
    print(f"  Bridge: {args.bridge_host or '(skipped)'}")
    print(f"  Timeout: {args.timeout}s")
    print(f"{'='*60}\n")

    verbosity = 2 if args.verbose else 1
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    suite.addTests(loader.loadTestsFromTestCase(CotBridgeContractTest))
    if args.bridge_host:
        suite.addTests(loader.loadTestsFromTestCase(CotBridgeProxyTest))

    runner = unittest.TextTestRunner(verbosity=verbosity)
    result = runner.run(suite)

    # Exit code: 0 if all passed, 1 if any failed
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
    main()
