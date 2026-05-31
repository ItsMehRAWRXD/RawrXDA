// ============================================================================
// engine_iface.cpp — Engine Registry Implementation
// ============================================================================
#include "engine_iface.h"
#include <map>
#include <mutex>

namespace {
    std::mutex g_registryMutex;
    std::map<std::string, Engine*> g_engines;
}

Engine* EngineRegistry::get(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_registryMutex);
    auto it = g_engines.find(name);
    if (it != g_engines.end()) {
        return it->second;
    }
    return nullptr;
}

void EngineRegistry::register_engine(Engine* e) {
    if (!e) return;
    std::lock_guard<std::mutex> lock(g_registryMutex);
    g_engines[e->name()] = e;
}
