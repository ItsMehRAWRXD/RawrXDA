// smoke_test.cpp — Consolidated smoke test merging ideas from:
//   smoke_test.cpp  (GenerateAnything service tests)
//   header_test.cpp (header compilation + memory plugin instantiation)
//   minimal_test.cpp (multi-engine system + drive setup + model distribution)
// All three sets of tests live here now.

#include <string>
#include <memory>
#include "universal_generator_service.h"
#include "cpu_inference_engine.h"
#include "multi_engine_system.h"
#include "memory_plugin.hpp"
#include "logging/logger.h"

int main() {
    Logger logger("SmokeTest");

    // ---- Phase 1: Universal Generator Service (from smoke_test.cpp) ----
    logger.info("=== Phase 1: Universal Generator Service ===");

    logger.info("[TEST 1] Generate Web App Project");
    std::string result1 = GenerateAnything("generate_project",
        R"({"name":"TestApp","type":"web","path":"./test_output"})");
    logger.info("Result: {}", result1);

    logger.info("[TEST 2] Generate Cookie Recipe Guide");
    std::string result2 = GenerateAnything("generate_guide", "chocolate chip cookies");
    logger.info("Result: {}", result2);

    logger.info("[TEST 3] Load Model");
    std::string result3 = GenerateAnything("load_model",
        R"({"path":"./models/test.gguf"})");
    logger.info("Result: {}", result3);

    // ---- Phase 2: Header compilation + Memory Plugins (from header_test.cpp) ----
    logger.info("=== Phase 2: Header Compilation & Memory Plugins ===");

    logger.info("CPU Inference Engine header compiled");
    logger.info("Multi-Engine System header compiled");
    logger.info("Memory Plugin header compiled");

    auto stdPlugin = std::make_shared<StandardMemoryPlugin>();
    auto largePlugin = std::make_shared<LargeContextPlugin>();
    logger.info("Standard Plugin created — Max Context: {} tokens", stdPlugin->GetMaxContext());
    logger.info("Large Plugin created — Max Context: {} tokens", largePlugin->GetMaxContext());

    // ---- Phase 3: Multi-Engine + Drive Layout (from minimal_test.cpp) ----
    logger.info("=== Phase 3: Multi-Engine System & Drive Layout ===");

    RawrXD::MultiEngineSystem multiEngine;
    logger.info("Multi-Engine System object created");

    auto drives = multiEngine.GetDriveInfo();
    logger.info("Drive setup configured ({} drives)", drives.size());

    multiEngine.DistributeModel("test_model");
    logger.info("Model distribution across drives complete");

    logger.info("=== ALL SMOKE TESTS COMPLETE ===");
    return 0;
}
