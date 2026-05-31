#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "SovereignToolBridge.h"
#include "SovereignMemoryBridge.h"
#include "SovereignKAIROSBridge.h"

namespace RawrXD::Runtime {

class SovereignOrchestrator {
public:
    static SovereignOrchestrator& instance();

    bool initialize() {
        if (m_initialized) return true;

        bool success = true;
        // Boot order: Memory -> KAIROS -> ToolEngine
        success &= SovereignMemoryBridge::instance().initialize();
        success &= SovereignKAIROSBridge::instance().initialize();
        success &= SovereignToolBridge::instance().initialize();

        if (success) {
            m_initialized = true;
            m_state = "IDLE";
            SovereignMemoryBridge::instance().recordDecision("ORCHESTRATOR_INIT", "Sovereign V2 Core Online (Parity Achieved)");
        }
        return success;
    }

    bool shutdown() {
        if (!m_initialized) return true;
        
        SovereignToolBridge::instance().shutdown();
        SovereignKAIROSBridge::instance().shutdown();
        SovereignMemoryBridge::instance().shutdown();

        m_initialized = false;
        m_state = "SHUTDOWN";
        return true;
    }

    // High-level "Plan and Execute" interface
    uint32_t planTask(const std::string& taskDescription);
    bool checkTask(uint32_t taskId, ToolStatus& status);

private:
    SovereignOrchestrator() : m_initialized(false), m_state("OFFLINE") {}
    ~SovereignOrchestrator() { shutdown(); }

    bool m_initialized;
    std::string m_state;
    std::mutex m_mutex;
    std::map<uint32_t, std::string> m_tasks;
};

} // namespace RawrXD::Runtime
