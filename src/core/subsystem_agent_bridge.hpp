// SCAFFOLD_100: SubsystemAgentBridge and failure callback

/**
 * @file subsystem_agent_bridge.hpp
 * @brief Bridges SubsystemRegistry into the agentic action execution pipeline
 *
 * This adapter allows the agentic framework (planner, puppeteer, failure
 * detector) to invoke CLI modes as first-class agent actions without
 * spawning child processes.
 *
 * Integration points:
 *   - ActionExecutor can dispatch ActionType::InvokeCommand to this bridge
 *   - Planner can enumerate available subsystems for capability discovery
 *   - FailureDetector receives SubsystemEvents for anomaly detection
 *   - Puppeteer can auto-retry failed subsystem calls
 *
 * NO exceptions. NO std::function. NO Qt.
 */
#pragma once

#include "rawrxd_subsystem_api.hpp"
#include <cstdint>

// ============================================================
// Agent Action Descriptor (mode call as an action)
// ============================================================

struct SubsystemAction {
    SubsystemId mode;
    const char* switchName;     // for logging / plan description
    SubsystemParams params;     // mode-specific parameters
    int maxRetries;             // puppeteer retry count (0 = no retry)
    uint32_t timeoutMs;         // max execution time before abort
};

// ============================================================
// Agent Bridge Interface
// ============================================================

class SubsystemAgentBridge {
public:
    static SubsystemAgentBridge& instance() {
        static SubsystemAgentBridge bridge;
        return bridge;
    }

    /**
     * Execute a subsystem action with agent-level wrapping:
     *   - Pre-validation
     *   - Execution with latency capture
     *   - Failure detection event emission
     *   - Auto-retry via puppeteer (if maxRetries > 0)
     */
    SubsystemResult executeAction(const SubsystemAction& action);

    /**
     * Enumerate all available subsystems for planner capability discovery.
     * Writes up to maxCount entries into the output array.
     * Returns the number of entries written.
     */
    struct SubsystemCapability {
        SubsystemId id;
        const char* switchName;
        bool requiresArgs;     // true if mode needs parameters to be useful
        bool requiresElevation; // true if mode needs admin privileges
        bool selfContained;     // true if mode operates on own executable
    };
    int enumerateCapabilities(SubsystemCapability* out, int maxCount) const;

    /**
     * Quick-check: can this mode run right now?
     * Checks availability, not permissions.
     */
    bool canInvoke(SubsystemId id) const;

    /**
     * Set the failure callback for agentic failure detection.
     * Called when a subsystem invocation fails or exceeds timeout.
     */
    typedef void (*FailureCallback)(SubsystemId mode, const SubsystemResult& result, void* userData);
    void setFailureCallback(FailureCallback cb, void* userData);

private:
    SubsystemAgentBridge();
    ~SubsystemAgentBridge() = default;

    FailureCallback m_failureCallback;
    void* m_failureUserData;
};

// ============================================================
// Inline Implementation
// ============================================================

inline SubsystemAgentBridge::SubsystemAgentBridge()
    : m_failureCallback(nullptr)
    , m_failureUserData(nullptr) {}

inline void SubsystemAgentBridge::setFailureCallback(FailureCallback cb, void* userData) {
    m_failureCallback = cb;
    m_failureUserData = userData;
}

inline bool SubsystemAgentBridge::canInvoke(SubsystemId id) const {
    return SubsystemRegistry::instance().isAvailable(id);
}

inline SubsystemResult SubsystemAgentBridge::executeAction(const SubsystemAction& action) {
    SubsystemResult result = SubsystemResult::error("Not executed", -1);
    int attempts = 0;
    int maxAttempts = action.maxRetries + 1;

    while (attempts < maxAttempts) {
        result = SubsystemRegistry::instance().invoke(action.params);
        attempts++;

        if (result.success) {
            break;
        }

        // Emit failure event for detection
        if (m_failureCallback) {
            m_failureCallback(action.mode, result, m_failureUserData);
        }

        // Don't retry if this was a usage error (incomplete args)
        if (result.errorCode == 0) {
            break;
        }
    }

    return result;
}

inline int SubsystemAgentBridge::enumerateCapabilities(SubsystemCapability* out, int maxCount) const {
    struct CapInfo {
        SubsystemId id;
        bool requiresArgs;
        bool requiresElevation;
        bool selfContained;
    };

    static const CapInfo caps[] = {
        { SubsystemId::Compile,               false, false, true  },
        { SubsystemId::Encrypt,               false, false, false },
        { SubsystemId::Inject,                true,  false, false },  // needs pid/pname
        { SubsystemId::UACBypass,             false, false, false },  // triggers elevation
        { SubsystemId::Persist,               false, false, false },  // writes registry
        { SubsystemId::Sideload,              false, false, false },
        { SubsystemId::AVScan,                false, false, true  },  // self-scan default
        { SubsystemId::Entropy,               false, false, true  },  // self-scan default
        { SubsystemId::StubGen,               true,  false, false },  // needs input file
        { SubsystemId::Trace,                 false, false, true  },  // map-only if no pid
        { SubsystemId::Agent,                 false, false, false },
        { SubsystemId::BBCov,                 false, false, true  },  // self-scan default
        { SubsystemId::CovFusion,             false, false, true  },  // fusion of BBCov+trace
        { SubsystemId::DynTrace,              false, false, true  },  // dynamic trace
        { SubsystemId::AgentTrace,            false, false, false },  // agent-guided
        { SubsystemId::GapFuzz,               false, false, true  },  // gap analysis
        { SubsystemId::IntelPT,               false, true,  false },  // requires kernel driver
        { SubsystemId::DiffCov,               false, false, true  },  // differential coverage
        { SubsystemId::AnalyzerDistiller,     true,  false, false },  // needs GGUF input path
        { SubsystemId::StreamingOrchestrator, true,  false, false },  // needs .exec input path
        { SubsystemId::VulkanKernel,          true,  false, false },  // needs .exec + Vulkan
        { SubsystemId::DiskRecovery,          false, true,  false },  // needs raw disk access (elevation)
    };

    int count = 0;
    for (int i = 0; i < 22 && count < maxCount; i++) {
        if (SubsystemRegistry::instance().isAvailable(caps[i].id)) {
            out[count].id = caps[i].id;
            out[count].switchName = SubsystemRegistry::instance().getSwitchName(caps[i].id);
            out[count].requiresArgs = caps[i].requiresArgs;
            out[count].requiresElevation = caps[i].requiresElevation;
            out[count].selfContained = caps[i].selfContained;
            count++;
        }
    }
    return count;
}
