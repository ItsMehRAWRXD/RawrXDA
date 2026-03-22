#include "rawrxd/runtime/RuntimeSurfaceBootstrap.hpp"

#include "rawrxd/runtime/CompressedPoolBudget.hpp"
#include "rawrxd/runtime/FourLaneGate.hpp"
#include "rawrxd/runtime/LlamaExllamaParityRegister.hpp"
#include "rawrxd/runtime/ModularModelLoader.hpp"
#include "rawrxd/runtime/QuantSignedLayout.hpp"
#include "rawrxd/runtime/RocmDynamicLoader.hpp"

#include "../logging/Logger.h"

#include <string>

namespace RawrXD::Runtime {

void bootstrapRuntimeSurface() {
    RawrXD::Logging::Logger::instance().info("[RuntimeSurface] bootstrap begin (no startup scan/index)",
                                             "RuntimeSurface");

    Quant::LogQuantLayoutLegendOnce();

    (void)RocmDynamicLoader::instance().load();

    CompressedPoolBudget::instance().setBudgetBytes(0);

    ModularModelLoader::instance().setResidentLayerBudget(0);

    const std::size_t n = LlamaExllamaParity::roleTableCount();
    RawrXD::Logging::Logger::instance().info(
        "[RuntimeSurface] llama.cpp/exllamaV2 parity register entries=" + std::to_string(n),
        "RuntimeSurface");

    (void)FourLaneGate::instance();

    RawrXD::Logging::Logger::instance().info(
        "[RuntimeSurface] four-lane gate ready (max 4 of 5 lanes unless explicit module)", "RuntimeSurface");

    RawrXD::Logging::Logger::instance().info("[RuntimeSurface] bootstrap complete", "RuntimeSurface");
}

}  // namespace RawrXD::Runtime
