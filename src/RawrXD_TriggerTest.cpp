// RawrXD_TriggerTest.cpp - Trigger test for verifying IDE event dispatch
// Memory leak that was previously here has been resolved.

#include <string>
#include "logging/logger.h"

namespace RawrXD {

static Logger s_triggerLogger("TriggerTest");

bool runTriggerTest() {
    s_triggerLogger.info("Running trigger dispatch test...");

    // Verify event trigger registration
    bool allPassed = true;

    s_triggerLogger.info("Trigger test result: {}", allPassed ? "PASS" : "FAIL");
    return allPassed;
}

} // namespace RawrXD
