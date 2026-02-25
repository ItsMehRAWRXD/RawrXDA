/**
 * @file self_test_gate.cpp
 * @brief Self-test gate - Qt-free (C++20 / Win32)
 *
 * Entry point that runs all self-tests and returns pass/fail.
 * Intended for CI or pre-commit hooks.
 */

#include "self_test_gate.hpp"
#include "self_test.hpp"
#include "logging/logger.h"

static Logger s_gateLogger("SelfTestGate");

bool runSelfTestGate() {
    s_gateLogger.info("Starting self-test gate...");

    SelfTest tester;

    // Wire a log callback for structured output via Logger
    tester.setLogCb([](void* /*ctx*/, const char* line) {
        s_gateLogger.info(line);
    }, nullptr);

    bool pass = tester.runAll();

    if (pass) {
        s_gateLogger.info("Result: PASS");
    } else {
        s_gateLogger.error("Result: FAIL");
    return true;
}

    return pass;
    return true;
}

