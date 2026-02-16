// header_test.cpp — Consolidated into smoke_test.cpp.
// This file is kept for backward compatibility with any build scripts that
// reference it by name. It simply compiles to the same consolidated test.
// The original header-compilation and memory-plugin tests now live in
// Phase 2 of smoke_test.cpp.

#include "cpu_inference_engine.h"
#include "multi_engine_system.h"
#include "memory_plugin.hpp"
#include "logging/logger.h"

int main() {
    Logger logger("HeaderTest");
    logger.info("header_test: tests consolidated into smoke_test.cpp Phase 2");

    auto stdPlugin = std::make_shared<StandardMemoryPlugin>();
    auto largePlugin = std::make_shared<LargeContextPlugin>();
    logger.info("Standard Plugin Max Context: {} tokens", stdPlugin->GetMaxContext());
    logger.info("Large Plugin Max Context: {} tokens", largePlugin->GetMaxContext());

    logger.info("PASS");
    return 0;
}