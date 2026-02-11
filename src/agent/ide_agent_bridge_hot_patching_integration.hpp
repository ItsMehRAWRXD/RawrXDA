/**
 * @file ide_agent_bridge_hot_patching_integration.hpp
 * @brief Extended IDEAgentBridge with real-time hallucination correction (Qt-free)
 */
#pragma once

#include "ide_agent_bridge.hpp"
#include "agent_hot_patcher.hpp"
#include "gguf_proxy_server.hpp"
#include <memory>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class IDEAgentBridgeWithHotPatching : public IDEAgentBridge {
public:
    IDEAgentBridgeWithHotPatching() = default;
    ~IDEAgentBridgeWithHotPatching() override = default;

    void initializeWithHotPatching();
    bool startHotPatchingProxy();
    void stopHotPatchingProxy();
    AgentHotPatcher* getHotPatcher() const;
    GGUFProxyServer* getProxyServer() const;
    bool isHotPatchingActive() const;
    nlohmann::json getHotPatchingStatistics() const;
    void setHotPatchingEnabled(bool enabled);
    void loadCorrectionPatterns(const std::string& databasePath);
    void loadBehaviorPatches(const std::string& databasePath);

    std::string proxyPort() const { return m_proxyPort; }
    void setProxyPort(const std::string& port) { m_proxyPort = port; }
    std::string ggufEndpoint() const { return m_ggufEndpoint; }
    void setGgufEndpoint(const std::string& endpoint) { m_ggufEndpoint = endpoint; }
    void onModelInvokerReplaced();

    // Event handlers (replace Qt slots)
    void handleHallucinationDetected(const HallucinationDetection& detection);
    void handleHallucinationCorrected(const HallucinationDetection& correction);
    void handleNavigationErrorFixed(const NavigationFix& fix);
    void handleBehaviorPatchApplied(const BehaviorPatch& patch);

    // Callbacks (replace Qt signals)
    std::function<void()> onProxyPortChanged;
    std::function<void()> onGgufEndpointChanged;

private:
    std::unique_ptr<AgentHotPatcher> m_hotPatcher;
    std::unique_ptr<GGUFProxyServer> m_proxyServer;
    bool m_hotPatchingEnabled = false;
    std::string m_proxyPort = "11435";
    std::string m_ggufEndpoint = "localhost:11434";
    void logCorrection(const HallucinationDetection& correction);
    void logNavigationFix(const NavigationFix& fix);
};
