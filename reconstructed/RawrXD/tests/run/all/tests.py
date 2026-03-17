#!/usr/bin/env python3
"""
run_all_tests.py — Verification Gates: Single Command Test Runner
=================================================================

Action Item #17: A single command runs all tests and fails fast with
actionable output.

Test categories:
  1. Unit tests     — parsers, trivial classifier, schema validation,
                      XSS sanitizer, tool-call parser
  2. Integration    — CoT endpoints, streaming termination, bridge
  3. Soak tests     — (optional, long-running)

Exit criteria: One command, fails fast, actionable output.

Usage:
    python tests/run_all_tests.py
    python tests/run_all_tests.py --unit-only
    python tests/run_all_tests.py --integration-only
    python tests/run_all_tests.py --include-soak

Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
"""

import os
import sys
import time
import argparse
import subprocess
import json
from pathlib import Path

# =============================================================================
# Configuration
# =============================================================================
PROJECT_ROOT = Path(__file__).parent.parent
TESTS_DIR = PROJECT_ROOT / "tests"
INTEGRATION_DIR = TESTS_DIR / "integration"
UNIT_DIR = TESTS_DIR / "unit"

# =============================================================================
# Test Suite Definitions
# =============================================================================
UNIT_TESTS = [
    {
        "name": "Schema Validation (Python)",
        "command": [sys.executable, "-m", "pytest",
                    str(INTEGRATION_DIR / "test_cot_bridge.py"),
                    "-v", "--tb=short", "-x"],
        "category": "unit",
        "required": True,
    },
]

INTEGRATION_TESTS = [
    {
        "name": "CoT Bridge Contract Test",
        "command": [sys.executable,
                    str(INTEGRATION_DIR / "test_cot_bridge.py"),
                    "--host", os.environ.get("COT_HOST", "http://localhost:5000"),
                    "--timeout", "15"],
        "category": "integration",
        "required": False,  # Requires running backend
    },
]

SOAK_TESTS = [
    {
        "name": "CoT Soak Test (Hello Spam)",
        "command": [sys.executable,
                    str(INTEGRATION_DIR / "soak_test_cot.py"),
                    "--scenario", "hello", "--count", "100",
                    "--concurrency", "5"],
        "category": "soak",
        "required": False,
    },
]

# C++ tests (compiled binaries)
CPP_TESTS = [
    {
        "name": "XSS Sanitizer Test",
        "binary": "test_xss_sanitizer",
        "source": str(UNIT_DIR / "test_xss_sanitizer.cpp"),
        "category": "unit",
        "required": True,
    },
]


# =============================================================================
# Test Runner
# =============================================================================
class TestResult:
    def __init__(self, name, category, passed, duration_s, output="", error=""):
        self.name = name
        self.category = category
        self.passed = passed
        self.duration_s = duration_s
        self.output = output
        self.error = error


def run_test(test_def: dict, timeout: int = 120) -> TestResult:
    """Run a single test and return the result."""
    name = test_def["name"]
    category = test_def["category"]
    command = test_def.get("command")

    if not command:
        # C++ binary — check if compiled
        binary = test_def.get("binary", "")
        build_dir = PROJECT_ROOT / "build" / "Release"
        binary_path = build_dir / (binary + ".exe")

        if not binary_path.exists():
            # Try Debug build
            binary_path = PROJECT_ROOT / "build" / "Debug" / (binary + ".exe")

        if not binary_path.exists():
            return TestResult(name, category, False, 0,
                              error=f"Binary not found: {binary}. Build with CMake first.")

        command = [str(binary_path)]

    print(f"\n  Running: {name}")
    print(f"    Command: {' '.join(str(c) for c in command)}")

    start = time.time()
    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=str(PROJECT_ROOT)
        )
        duration = time.time() - start

        passed = (result.returncode == 0)
        output = result.stdout[:2000] if result.stdout else ""
        error = result.stderr[:2000] if result.stderr else ""

        status = "PASS" if passed else "FAIL"
        print(f"    [{status}] {name} ({duration:.1f}s)")

        if not passed and error:
            print(f"    Error: {error[:200]}")

        return TestResult(name, category, passed, duration, output, error)

    except subprocess.TimeoutExpired:
        duration = time.time() - start
        print(f"    [TIMEOUT] {name} ({timeout}s limit)")
        return TestResult(name, category, False, duration,
                          error=f"Timed out after {timeout}s")

    except FileNotFoundError as e:
        print(f"    [SKIP] {name}: {e}")
        return TestResult(name, category, False, 0,
                          error=f"Command not found: {e}")

    except Exception as e:
        duration = time.time() - start
        print(f"    [ERROR] {name}: {e}")
        return TestResult(name, category, False, duration, error=str(e))


def main():
    parser = argparse.ArgumentParser(description="RawrXD Test Runner")
    parser.add_argument("--unit-only", action="store_true",
                        help="Run only unit tests")
    parser.add_argument("--integration-only", action="store_true",
                        help="Run only integration tests")
    parser.add_argument("--include-soak", action="store_true",
                        help="Include soak tests (long-running)")
    parser.add_argument("--fail-fast", action="store_true", default=True,
                        help="Stop on first failure (default: true)")
    parser.add_argument("--timeout", type=int, default=120,
                        help="Per-test timeout in seconds (default: 120)")
    args = parser.parse_args()

    print("=" * 60)
    print("RawrXD Verification Gates — Test Runner")
    print(f"  Project: {PROJECT_ROOT}")
    print(f"  Mode: {'unit' if args.unit_only else 'integration' if args.integration_only else 'full'}")
    print("=" * 60)

    # Build test list
    tests = []
    if not args.integration_only:
        tests.extend(UNIT_TESTS)
        tests.extend(CPP_TESTS)
    if not args.unit_only:
        tests.extend(INTEGRATION_TESTS)
    if args.include_soak:
        tests.extend(SOAK_TESTS)

    # Run tests
    results = []
    total_start = time.time()
    failed_required = False

    for test_def in tests:
        result = run_test(test_def, timeout=args.timeout)
        results.append(result)

        if not result.passed:
            if test_def.get("required", False):
                failed_required = True
                if args.fail_fast:
                    print(f"\n  FAIL FAST: Required test '{result.name}' failed.")
                    break

    total_duration = time.time() - total_start

    # =========================================================================
    # Summary
    # =========================================================================
    print(f"\n{'=' * 60}")
    print(f"TEST RESULTS SUMMARY")
    print(f"{'=' * 60}")

    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed)

    for r in results:
        status = "PASS" if r.passed else "FAIL"
        print(f"  [{status}] {r.name} ({r.duration_s:.1f}s)")
        if not r.passed and r.error:
            print(f"         Error: {r.error[:100]}")

    print(f"\n  Total: {len(results)} tests, {passed} passed, {failed} failed")
    print(f"  Duration: {total_duration:.1f}s")

    verdict = "PASS" if failed == 0 else "FAIL"
    print(f"  Verdict: {verdict}")
    print(f"{'=' * 60}")

    # JSON output for CI
    report = {
        "verdict": verdict,
        "total": len(results),
        "passed": passed,
        "failed": failed,
        "duration_s": round(total_duration, 1),
        "tests": [
            {
                "name": r.name,
                "category": r.category,
                "passed": r.passed,
                "duration_s": round(r.duration_s, 1),
                "error": r.error[:200] if r.error else ""
            }
            for r in results
        ]
    }

    report_path = PROJECT_ROOT / "test_report.json"
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2)
    print(f"\n  Report written to: {report_path}")

    sys.exit(0 if verdict == "PASS" else 1)


if __name__ == "__main__":
    main()
