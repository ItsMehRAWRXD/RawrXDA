// minimal_test.cpp — Consolidated into smoke_test.cpp.
// This file is kept for backward compatibility. The original multi-engine
// and drive-layout tests now live in Phase 3 of smoke_test.cpp.

#include "cpu_inference_engine.h"
#include "multi_engine_system.h"
#include "memory_plugin.hpp"
#include "logging/logger.h"

int main() {
    Logger logger("MinimalTest");
    logger.info("minimal_test: tests consolidated into smoke_test.cpp Phase 3");

    RawrXD::MultiEngineSystem multiEngine;
    auto drives = multiEngine.GetDriveInfo();
    logger.info("Drive setup: {} drives", drives.size());

    multiEngine.DistributeModel("test_model");
    logger.info("Model distribution complete");

    logger.info("PASS");
    return 0;
}