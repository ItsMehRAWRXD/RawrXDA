#include "tool_registry_init.hpp"
#include <iostream>

void register_rawr_inference() {
    std::cout << "[REGISTRY] Registered RAWR inference tool\n";
}

// register_sovereign_engines — linker fallback for registry-only targets.
// Real implementation in engine/sovereign_engines.cpp registers Engine800B + SovereignSmall
// via EngineRegistry::register_engine(). This fallback logs that the real module is absent.
void register_sovereign_engines() {
    std::cout << "[REGISTRY] register_sovereign_engines fallback — engine module not linked\n";
}
