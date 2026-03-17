// ============================================================================
// File: src/agent/ide_agent_bridge_hot_patching_integration.hpp
// 
// Purpose: Integration of hot patching into IDEAgentBridge
// Seamless wiring for transparent hallucination correction
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include "ide_agent_bridge.hpp"
#include "agent_hot_patcher.hpp"
#include "gguf_proxy_server.hpp"

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace RawrXD {

/**
 * @class IDEAgentBridgeWithHotPatching
 * @brief Extended IDEAgentBridge with real-time hallucination correction
 * 
 * NATIVE C++20 IMPLEMENTATION (Zero Qt)
 */
class IDEAgentBridgeWithHotPatching : public IDEAgentBridge {
public:
    explicit IDEAgentBridgeWithHotPatching();
    ~IDEAgentBridgeWithHotPatching() override;

    /**
     * Initialize bridge with hot patching system
     * @note This MUST be called before any agent operations
     */
    void initializeWithHotPatching();

    /**
     * Start hot patching proxy server
     * @return true if started successfully
     */
    bool startHotPatchingProxy();

    /**
     * Stop hot patching proxy server
     */
    void stopHotPatchingProxy();

    /**
     * Get hot patcher instance
     */
    AgentHotPatcher* getHotPatcher() const;

    /**
     * Get proxy server instance
     */
    GGUFProxyServer* getProxyServer() const;

    /**
     * Check if hot patching is active
     */
    bool isHotPatchingActive() const;

    /**
     * Get hot patching statistics
     */
    nlohmann::json getHotPatchingStatistics() const;

    /**
     * Enable/disable hot patching at runtime
     */
    void setHotPatchingEnabled(bool enabled);

    /**
     * Load correction patterns from database
     */
    void loadCorrectionPatterns(const std::string& databasePath);

    /**
     * Load behavior patches from database
     */
    void loadBehaviorPatches(const std::string& databasePath);

protected:
    std::unique_ptr<AgentHotPatcher> m_hotPatcher;
    std::unique_ptr<GGUFProxyServer> m_proxyServer;
    bool m_hotPatchingActive = false;
};

} // namespace RawrXD

#endif // IDE_AGENT_BRIDGE_HOT_PATCHING_INTEGRATION_HPP
