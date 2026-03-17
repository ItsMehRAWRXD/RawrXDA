/**
 * @file self_test_gate.cpp
 * @brief Self-test gate – Qt-free (C++20 / Win32)
 *
 * Entry point that runs all self-tests and returns pass/fail.
 * Intended for CI or pre-commit hooks.
 */

#include "self_test_gate.hpp"
#include "self_test.hpp"
#include <cstdio>

bool runSelfTestGate() {
    fprintf(stderr, "[SelfTestGate] Starting self-test gate...\n");

    SelfTest tester;

    // Optional: wire a log callback for structured output
    tester.setLogCb([](void* /*ctx*/, const char* line) {
        fprintf(stdout, "%s\n", line);
    }, nullptr);

    bool pass = tester.runAll();

    fprintf(stderr, "[SelfTestGate] Result: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}
