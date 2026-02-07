// ============================================================================
// Win32IDE_SubAgent.cpp — Win32IDE Factory Wrapper
// ============================================================================
// All SubAgentManager logic lives in subagent_core.cpp (portable).
// This file provides createWin32SubAgentManager() which wires up the
// IDELogger and IDEConfig METRICS callbacks so the portable core
// integrates seamlessly with the Win32IDE logging/metrics infrastructure.
// ============================================================================

#include "Win32IDE_SubAgent.h"
#include "IDEConfig.h"

// ============================================================================
// Factory: create a SubAgentManager with IDELogger + METRICS callbacks
// ============================================================================
SubAgentManager* createWin32SubAgentManager(AgenticEngine* engine) {
    auto* mgr = new SubAgentManager(engine);

    // Wire IDELogger as the log callback
    mgr->setLogCallback([](int level, const std::string& msg) {
        switch (level) {
            case 0: LOG_DEBUG(msg); break;
            case 1: LOG_INFO(msg);  break;
            case 2: LOG_INFO("[WARN] " + msg); break;
            case 3: LOG_ERROR(msg); break;
            default: LOG_INFO(msg); break;
        }
    });

    // Wire IDEConfig METRICS as the metrics callback
    mgr->setMetricsCallback([](const std::string& key) {
        METRICS.increment(key);
    });

    LOG_INFO("Win32IDE SubAgentManager created with IDELogger + METRICS callbacks");
    return mgr;
}
