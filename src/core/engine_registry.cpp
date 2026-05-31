// ============================================================================
// engine_registry.cpp — Minimal EngineRegistry for standalone inference
// ============================================================================
// Used by RawrXD-InferenceEngine and any target that links sovereign_engines.cpp
// but not runtime_core.cpp. Defines only EngineRegistry::get and register_engine.
// Full runtime (init_runtime, set_mode, etc.) remains in runtime_core.cpp.
// ============================================================================

#include "../engine_iface.h"
#include <string>
#include <unordered_map>

// LAZY SINGLETON PATTERN to avoid SIOF
inline std::unordered_map<std::string, Engine*>& GetEngines() {
    static std::unordered_map<std::string, Engine*>* inst = new std::unordered_map<std::string, Engine*>();
    return *inst;
}
#define s_engines GetEngines()

Engine* EngineRegistry::get(const std::string& name) {
    auto it = s_engines.find(name);
    return (it != s_engines.end()) ? it->second : nullptr;
}

void EngineRegistry::register_engine(Engine* e) {
    if (e)
        s_engines[e->name()] = e;
}
